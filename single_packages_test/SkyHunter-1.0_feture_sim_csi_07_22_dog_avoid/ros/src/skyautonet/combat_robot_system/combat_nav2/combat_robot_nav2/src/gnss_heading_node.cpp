// gnss_heading_node.cpp
// C++ 포팅: script/gnss_heading.py (Nav2HeadingProvider) feature-parity 재구현.
//   - NMEA(THS/GGA/VTG/GST) 파싱 → /fix, /gps/heading_imu, /edge_heading, /vel 발행
//   - 시리얼은 termios raw + CLOCAL(DTR/모뎀라인 무시, python의 dsrdtr=False 대응)
//   - flock(LOCK_EX|LOCK_NB) 으로 배타 오픈(=pyserial exclusive=True): 두 번째
//     인스턴스가 조용히 바이트를 훔치지 못하고 즉시 실패 → gnss garble 원천 차단.
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float64.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/nav_sat_status.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/file.h>

namespace {

// $....*XX : body 문자 XOR == cksum(hex)
bool nmea_checksum_ok(const std::string & s)
{
  if (s.empty() || s[0] != '$') return false;
  auto star = s.find('*');
  if (star == std::string::npos || star + 3 > s.size()) return false;
  unsigned calc = 0;
  for (size_t i = 1; i < star; ++i) calc ^= static_cast<unsigned char>(s[i]);
  try {
    int given = std::stoi(s.substr(star + 1, 2), nullptr, 16);
    return static_cast<int>(calc) == given;
  } catch (...) {
    return false;
  }
}

// ddmm.mmmm + 반구 → 십진 도. 실패 시 nullopt.
std::optional<double> nmea_to_decimal(const std::string & coord, const std::string & hemi)
{
  if (coord.empty() || hemi.empty()) return std::nullopt;
  auto dot = coord.find('.');
  if (dot == std::string::npos || dot < 2) return std::nullopt;
  try {
    size_t deg_len = dot - 2;
    double deg = std::stod(coord.substr(0, deg_len));
    double minutes = std::stod(coord.substr(deg_len));
    double dec = deg + minutes / 60.0;
    if (hemi == "S" || hemi == "W") dec = -dec;
    return dec;
  } catch (...) {
    return std::nullopt;
  }
}

std::vector<std::string> split(const std::string & s, char d)
{
  std::vector<std::string> out;
  std::string cur;
  std::istringstream ss(s);
  while (std::getline(ss, cur, d)) out.push_back(cur);
  // getline 은 trailing delimiter 뒤 빈 필드를 누락하므로 보정
  if (!s.empty() && s.back() == d) out.push_back("");
  return out;
}

// "$GPxxx,..*hh" → 본문(*앞) 만 반환
std::string body_before_star(const std::string & s)
{
  auto star = s.find('*');
  return star == std::string::npos ? s : s.substr(0, star);
}

double parse_or(const std::string & f, double def)
{
  if (f.empty()) return def;
  try { return std::stod(f); } catch (...) { return def; }
}

}  // namespace

class Nav2HeadingProvider : public rclcpp::Node
{
public:
  Nav2HeadingProvider()
  : Node("nav2_heading_provider")
  {
    port_ = declare_parameter<std::string>("port", "/dev/ttyUSB1");
    baud_ = declare_parameter<int>("baud", 921600);
    heading_frame_ = declare_parameter<std::string>("heading_frame_id", "gps");
    gps_frame_ = declare_parameter<std::string>("gps_frame_id", "gps");
    gst_timeout_ = declare_parameter<double>("gst_timeout_sec", 2.0);
    antenna_offset_ = declare_parameter<double>("antenna_yaw_offset_deg", 0.0);
    heading_min_speed_ = declare_parameter<double>("heading_min_speed_mps", 0.5);
    min_sigma_h_ = declare_parameter<double>("min_sigma_horizontal", 0.10);
    min_sigma_v_ = declare_parameter<double>("min_sigma_vertical", 0.15);
    min_fix_quality_ = declare_parameter<int>("min_fix_quality", 1);

    pub_float_ = create_publisher<std_msgs::msg::Float64>("/edge_heading", 10);
    pub_imu_ = create_publisher<sensor_msgs::msg::Imu>("/gps/heading_imu", 10);
    pub_fix_ = create_publisher<sensor_msgs::msg::NavSatFix>("/fix", 10);
    pub_vel_ = create_publisher<geometry_msgs::msg::TwistStamped>("/vel", 10);

    status_timer_ = create_wall_timer(
      std::chrono::seconds(5), std::bind(&Nav2HeadingProvider::status_timer, this));

    open_serial();  // 실패 시 예외 → 노드 생성 실패(=python raise 와 동일)

    running_ = true;
    thread_ = std::thread(&Nav2HeadingProvider::serial_loop, this);

    RCLCPP_INFO(get_logger(),
      "✅ GNSS Provider started (heading_frame=%s, antenna_yaw_offset=%.2f°, "
      "min_sigma_h=%.3fm, min_fix_quality=%d)",
      heading_frame_.c_str(), antenna_offset_, min_sigma_h_, min_fix_quality_);
  }

  ~Nav2HeadingProvider() override
  {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    if (fd_ >= 0) ::close(fd_);
  }

private:
  void open_serial()
  {
    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
      std::string e = std::strerror(errno);
      RCLCPP_ERROR(get_logger(), "❌ Serial open failed: %s (%s)", port_.c_str(), e.c_str());
      throw std::runtime_error("serial open failed: " + e);
    }
    // 배타 잠금(pyserial exclusive=True 와 동일). 이미 다른 프로세스가 잡고 있으면 실패.
    if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
      std::string e = std::strerror(errno);
      ::close(fd_);
      fd_ = -1;
      RCLCPP_ERROR(get_logger(),
        "❌ Serial open failed: Could not exclusively lock port %s: %s",
        port_.c_str(), e.c_str());
      throw std::runtime_error("serial exclusive lock failed: " + e);
    }

    // ★ termios 설정 전에 DTR/RTS 먼저 해제. tcsetattr(baud 설정)가 DTR 을 순간
    //   assert 해 GPS 모듈을 교란(garble)시키므로, 설정 전에 미리 내려둔다.
    {
      int mbits = TIOCM_DTR | TIOCM_RTS;
      ioctl(fd_, TIOCMBIC, &mbits);
    }

    termios tio{};
    if (tcgetattr(fd_, &tio) != 0) {
      std::string e = std::strerror(errno);
      ::close(fd_); fd_ = -1;
      throw std::runtime_error("tcgetattr failed: " + e);
    }
    cfmakeraw(&tio);
    speed_t sp = baud_to_speed(baud_);
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);
    // CLOCAL: 모뎀 제어선(DTR/DSR/DCD) 무시 → GPS 모듈 안 건드림(=python dsrdtr=False 의도).
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS;             // HW 흐름제어 off
    tio.c_cflag &= ~PARENB;              // 패리티 없음
    tio.c_cflag &= ~CSTOPB;              // stop bit 1
    tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10;               // 1.0s read timeout (python timeout=1.0)
    if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
      std::string e = std::strerror(errno);
      ::close(fd_); fd_ = -1;
      throw std::runtime_error("tcsetattr failed: " + e);
    }
    // blocking read 로 전환(VMIN/VTIME 기반 timeout 이 동작하도록 O_NONBLOCK 해제)
    int fl = fcntl(fd_, F_GETFL);
    fcntl(fd_, F_SETFL, fl & ~O_NONBLOCK);
    // ★ DTR/RTS 명시적 해제. CLOCAL 은 "프로그램이 모뎀선을 무시"할 뿐, open 시
    //   커널이 DTR 출력선을 assert 한다. Prolific+일부 GPS 모듈은 DTR assert 에
    //   반응해 출력이 garble(GGA=0) 됨 → python ser.dtr=False/rts=False 대응.
    int mbits = TIOCM_DTR | TIOCM_RTS;
    ioctl(fd_, TIOCMBIC, &mbits);
    // GPS 모듈이 DTR 교란에서 회복할 시간(250ms) 준 뒤, 그동안 쌓인 garble 바이트를
    // 비운다. 이래야 이후 스트림이 clean NMEA 로 들어온다.
    usleep(250000);
    tcflush(fd_, TCIOFLUSH);
    RCLCPP_INFO(get_logger(), "✅ Serial opened: %s @ %d (DTR/RTS cleared, settled)", port_.c_str(), baud_);
  }

  static speed_t baud_to_speed(int baud)
  {
    switch (baud) {
      case 9600:   return B9600;
      case 19200:  return B19200;
      case 38400:  return B38400;
      case 57600:  return B57600;
      case 115200: return B115200;
      case 230400: return B230400;
      case 460800: return B460800;
      case 921600: return B921600;
      default:     return B921600;
    }
  }

  void serial_loop()
  {
    std::string buf;
    char tmp[512];
    while (running_ && rclcpp::ok()) {
      ssize_t n = ::read(fd_, tmp, sizeof(tmp));
      if (n < 0) {
        if (errno == EAGAIN || errno == EINTR) continue;
        if (running_) RCLCPP_ERROR(get_logger(), "Serial read error: %s", std::strerror(errno));
        break;
      }
      if (n == 0) continue;  // timeout
      buf.append(tmp, static_cast<size_t>(n));
      size_t pos;
      while ((pos = buf.find('\n')) != std::string::npos) {
        std::string line = buf.substr(0, pos);
        buf.erase(0, pos + 1);
        // strip \r 및 앞뒤 공백
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' ||
               line.back() == '\t')) line.pop_back();
        size_t s = line.find_first_not_of(" \t");
        if (s == std::string::npos) continue;
        if (s > 0) line = line.substr(s);
        if (line.empty()) continue;

        line_count_++;
        if (!first_line_seen_) {
          RCLCPP_INFO(get_logger(), "✅ 첫 시리얼 라인: %.60s", line.c_str());
          first_line_seen_ = true;
        }
        if (!nmea_checksum_ok(line)) continue;
        dispatch(line);
      }
      if (buf.size() > 4096) buf.clear();  // 안전장치: 개행 없는 쓰레기 누적 방지
    }
  }

  void dispatch(const std::string & s)
  {
    if (s.size() < 6) return;
    std::string tag = s.substr(3, 3);
    if (tag == "THS") handle_ths(s);
    else if (tag == "GGA") handle_gga(s);
    else if (tag == "VTG") handle_vtg(s);
    else if (tag == "GST") handle_gst(s);
  }

  void handle_ths(const std::string & s)
  {
    auto p = split(body_before_star(s), ',');
    if (p.size() < 3 || p[1].empty() || p[2].find('A') == std::string::npos) return;
    try {
      double raw_heading = std::stod(p[1]);
      double heading_deg = std::fmod(raw_heading - antenna_offset_, 360.0);
      if (heading_deg < 0) heading_deg += 360.0;
      double ros_yaw = deg2rad(90.0 - heading_deg);
      ros_yaw = std::atan2(std::sin(ros_yaw), std::cos(ros_yaw));

      auto stamp = now();

      std_msgs::msg::Float64 f;
      f.data = heading_deg;
      pub_float_->publish(f);

      sensor_msgs::msg::Imu imu;
      imu.header.stamp = stamp;
      imu.header.frame_id = heading_frame_;
      imu.orientation.x = 0.0;
      imu.orientation.y = 0.0;
      imu.orientation.z = std::sin(ros_yaw / 2.0);
      imu.orientation.w = std::cos(ros_yaw / 2.0);
      // ★ 속도게이팅: 정지 시 듀얼안테나 heading 은 스핀(garbage, 실측 ~470°/min)이라
      //   EKF 가 무시하도록 yaw 공분산을 크게. 이동(VTG speed>=임계) 시에만 신뢰(0.001).
      //   (C++ 포팅 때 누락됐던 게이팅 — 정지 중 EKF yaw 폭주→120° 라이다 콘 회전→
      //    costmap stale 마크 360° 누적을 유발하던 근본원인.)
      const bool heading_trusted = last_speed_mps_.load() >= heading_min_speed_;
      imu.orientation_covariance[0] = 999.0;
      imu.orientation_covariance[4] = 999.0;
      imu.orientation_covariance[8] = heading_trusted ? 0.001 : 999.0;
      imu.angular_velocity_covariance[0] = -1.0;
      imu.linear_acceleration_covariance[0] = -1.0;
      pub_imu_->publish(imu);

      ths_count_++;
      if (!first_heading_logged_) {
        RCLCPP_INFO(get_logger(),
          "✅ 첫 heading 발행: raw=%.2f° offset=%.2f° → "
          "robot_heading=%.2f° (yaw_enu=%.2f°)",
          raw_heading, antenna_offset_, heading_deg, rad2deg(ros_yaw));
        first_heading_logged_ = true;
      }
    } catch (const std::exception & e) {
      RCLCPP_WARN(get_logger(), "THS parse error: %s | %s", e.what(), s.c_str());
    }
  }

  void handle_gst(const std::string & s)
  {
    auto p = split(body_before_star(s), ',');
    if (p.size() < 9) return;
    if (p[6].empty() || p[7].empty() || p[8].empty()) return;
    try {
      gst_sigma_lat_ = std::stod(p[6]);
      gst_sigma_lon_ = std::stod(p[7]);
      gst_sigma_alt_ = std::stod(p[8]);
      gst_last_ns_.store(now().nanoseconds());
      gst_count_++;
    } catch (const std::exception & e) {
      RCLCPP_WARN(get_logger(), "GST parse error: %s | %s", e.what(), s.c_str());
    }
  }

  void handle_gga(const std::string & s)
  {
    auto p = split(body_before_star(s), ',');
    if (p.size() < 15) return;
    try {
      int fix_quality = p[6].empty() ? 0 : std::stoi(p[6]);
      last_fix_quality_.store(fix_quality);

      if (fix_quality < min_fix_quality_) {
        gga_dropped_++;
        return;
      }

      auto lat = nmea_to_decimal(p[2], p[3]);
      auto lon = nmea_to_decimal(p[4], p[5]);
      double alt = parse_or(p[9], 0.0);
      double hdop = parse_or(p[8], 99.0);
      if (!lat || !lon) return;

      sensor_msgs::msg::NavSatFix msg;
      msg.header.stamp = now();
      msg.header.frame_id = gps_frame_;

      sensor_msgs::msg::NavSatStatus status;
      if (fix_quality == 0) status.status = sensor_msgs::msg::NavSatStatus::STATUS_NO_FIX;
      else if (fix_quality == 4 || fix_quality == 5)
        status.status = sensor_msgs::msg::NavSatStatus::STATUS_GBAS_FIX;
      else if (fix_quality == 2)
        status.status = sensor_msgs::msg::NavSatStatus::STATUS_SBAS_FIX;
      else status.status = sensor_msgs::msg::NavSatStatus::STATUS_FIX;
      status.service = sensor_msgs::msg::NavSatStatus::SERVICE_GPS |
                       sensor_msgs::msg::NavSatStatus::SERVICE_GLONASS;
      msg.status = status;

      msg.latitude = *lat;
      msg.longitude = *lon;
      msg.altitude = alt;

      double sigma_e = 0, sigma_n = 0, sigma_u = 0;
      bool use_gst = false;
      int64_t last_ns = gst_last_ns_.load();
      if (last_ns >= 0) {
        double age = (now().nanoseconds() - last_ns) / 1e9;
        if (age <= gst_timeout_) {
          sigma_n = gst_sigma_lat_;
          sigma_e = gst_sigma_lon_;
          sigma_u = gst_sigma_alt_;
          use_gst = true;
        }
      }
      if (!use_gst) {
        if (fix_quality == 4 || fix_quality == 5) {
          sigma_e = sigma_n = 0.05; sigma_u = 0.10;
        } else if (fix_quality == 2) {
          sigma_e = sigma_n = std::max(hdop * 1.0, 0.5); sigma_u = sigma_e * 1.5;
        } else {
          sigma_e = sigma_n = std::max(hdop * 2.5, 1.0); sigma_u = sigma_e * 1.5;
        }
      }
      sigma_e = std::max(sigma_e, min_sigma_h_);
      sigma_n = std::max(sigma_n, min_sigma_h_);
      sigma_u = std::max(sigma_u, min_sigma_v_);

      msg.position_covariance[0] = sigma_e * sigma_e;
      msg.position_covariance[4] = sigma_n * sigma_n;
      msg.position_covariance[8] = sigma_u * sigma_u;
      msg.position_covariance_type =
        sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;

      pub_fix_->publish(msg);
      gga_count_++;

      if (!first_fix_logged_) {
        RCLCPP_INFO(get_logger(),
          "✅ 첫 /fix 발행: lat=%.7f lon=%.7f alt=%.2fm q=%d "
          "σ_E=%.3f σ_N=%.3f σ_U=%.3f [%s]",
          *lat, *lon, alt, fix_quality, sigma_e, sigma_n, sigma_u,
          use_gst ? "GST" : "fallback");
        first_fix_logged_ = true;
      }
    } catch (const std::exception & e) {
      RCLCPP_WARN(get_logger(), "GGA parse error: %s | %s", e.what(), s.c_str());
    }
  }

  void handle_vtg(const std::string & s)
  {
    auto p = split(body_before_star(s), ',');
    if (p.size() < 9) return;
    try {
      bool have_course = !p[1].empty();
      double course_true = have_course ? std::stod(p[1]) : 0.0;
      double speed_kmh = parse_or(p[7], 0.0);
      double speed_mps = speed_kmh / 3.6;
      last_speed_mps_.store(speed_mps);   // heading 속도게이팅용 최신 속도

      geometry_msgs::msg::TwistStamped msg;
      msg.header.stamp = now();
      msg.header.frame_id = gps_frame_;
      if (have_course) {
        double course_enu = deg2rad(90.0 - course_true);
        msg.twist.linear.x = speed_mps * std::cos(course_enu);
        msg.twist.linear.y = speed_mps * std::sin(course_enu);
      } else {
        msg.twist.linear.x = speed_mps;
        msg.twist.linear.y = 0.0;
      }
      pub_vel_->publish(msg);
      vtg_count_++;
    } catch (const std::exception & e) {
      RCLCPP_WARN(get_logger(), "VTG parse error: %s | %s", e.what(), s.c_str());
    }
  }

  void status_timer()
  {
    std::string gst_age = "∞";
    int64_t last_ns = gst_last_ns_.load();
    if (last_ns >= 0) {
      double age = (now().nanoseconds() - last_ns) / 1e9;
      char b[32]; std::snprintf(b, sizeof(b), "%.1fs", age); gst_age = b;
    }
    int q = last_fix_quality_.load();
    std::string q_str = (q == kNoQuality) ? "q=?" : ("q=" + std::to_string(q));
    RCLCPP_INFO(get_logger(),
      "[status] line=%d THS=%d GGA=%d (dropped=%d) VTG=%d GST=%d GST_age=%s %s",
      line_count_.exchange(0), ths_count_.exchange(0), gga_count_.exchange(0),
      gga_dropped_.exchange(0), vtg_count_.exchange(0), gst_count_.exchange(0),
      gst_age.c_str(), q_str.c_str());
  }

  static double deg2rad(double d) { return d * M_PI / 180.0; }
  static double rad2deg(double r) { return r * 180.0 / M_PI; }

  // 파라미터
  std::string port_, heading_frame_, gps_frame_;
  int baud_{921600};
  double gst_timeout_{2.0}, antenna_offset_{0.0}, heading_min_speed_{0.5};
  std::atomic<double> last_speed_mps_{0.0};   // VTG 최신 속도(heading 게이팅용)
  double min_sigma_h_{0.10}, min_sigma_v_{0.15};
  int min_fix_quality_{1};

  // 퍼블리셔
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_float_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr pub_imu_;
  rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr pub_fix_;
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr pub_vel_;
  rclcpp::TimerBase::SharedPtr status_timer_;

  // 시리얼 + 스레드
  int fd_{-1};
  std::atomic<bool> running_{false};
  std::thread thread_;

  // GST 캐시(serial thread 에서만 sigma 갱신/사용, last_ns 만 cross-thread)
  double gst_sigma_lat_{0}, gst_sigma_lon_{0}, gst_sigma_alt_{0};
  std::atomic<int64_t> gst_last_ns_{-1};

  // 통계(cross-thread → atomic)
  static constexpr int kNoQuality = std::numeric_limits<int>::min();
  std::atomic<int> line_count_{0}, ths_count_{0}, gga_count_{0};
  std::atomic<int> vtg_count_{0}, gst_count_{0}, gga_dropped_{0};
  std::atomic<int> last_fix_quality_{kNoQuality};
  bool first_line_seen_{false}, first_fix_logged_{false}, first_heading_logged_{false};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<Nav2HeadingProvider>());
  } catch (const std::exception & e) {
    RCLCPP_ERROR(rclcpp::get_logger("nav2_heading_provider"), "fatal: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
