#include "zoom_controller.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

using namespace std::chrono_literals;

namespace camera_interface
{

// ========================== 헬퍼 ==========================
int ZoomController::GetBaudConst(int baud) {
  switch (baud) {
    case 2400:   return B2400;
    case 4800:   return B4800;
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    default:     return B9600;
  }
}

// 거리 → 프리셋 매핑
int ZoomController::PresetFromDistance(double d) {
  // thresholds: [500, 300, 200, 100] (내림차순 가정)
  // presets:    [1,   2,   3,   4,   5]
  
  // 1. 임계값 검사
  for (size_t i = 0; i < zoom_mapping_thresholds_.size(); ++i) {
    if (d >= zoom_mapping_thresholds_[i]) {
      if (i < zoom_mapping_presets_.size()) {
        return static_cast<int>(zoom_mapping_presets_[i]);
      }
    }
  }
  
  // 2. 모든 임계값보다 작을 경우 -> 마지막 프리셋 반환
  if (!zoom_mapping_presets_.empty()) {
    return static_cast<int>(zoom_mapping_presets_.back());
  }
  
  return 5; // fallback
}

// 프리셋 → 줌 배율 매핑
int ZoomController::GetZoomLevelFromPreset(int preset_id) {
  // preset_id는 1부터 시작한다고 가정 (Pelco Spec)
  // zoom_levels_: index 0 -> preset 1
  int idx = preset_id - 1;
  if (idx >= 0 && idx < static_cast<int>(zoom_levels_.size())) {
    return static_cast<int>(zoom_levels_[idx]);
  }
  return 1; // fallback
}

// 히스테리시스: 경계 근처 플래핑 방지
bool ZoomController::AllowChangeWithHysteresis(double old_d, double new_d,
                                               int old_preset, int new_preset) const {
  if (hysteresis_m_ <= 0.0) {
    return true;
  }
  if (old_preset == new_preset) {
    return false;
  }
  
  // 경계값(thresholds)을 기준으로 old_d와 new_d가 서로 반대편에 있는지 확인
  for (double edge : zoom_mapping_thresholds_) {
    double was = old_d - edge;
    double now = new_d - edge;
    
    // 경계를 가로질렀다면 (부호가 다름)
    if (was * now < 0.0) {
      // 경계로부터 hysteresis_m_ 만큼 떨어졌는지 확인
      return std::abs(now) >= hysteresis_m_;
    }
  }
  
  // 경계를 가로지르지 않았으면 기본적으로 허용 (하지만 로직상 프리셋이 바뀌었다면 경계를 넘었어야 함)
  // 여기서는 "경계 근처 미세 변동으로 프리셋이 바뀌려는 상황"을 막는 것이 목적이므로
  // 프리셋 변경 조건(PresetFromDistance)을 만족했더라도 여기서 한번 더 거르는 구조임.
  return true;
}

// ========================== 생성/소멸 ==========================
ZoomController::ZoomController(const rclcpp::NodeOptions& options)
: rclcpp::Node("zoom_controller", options) {
  InitRosParam();
  InitPelcoSerial();
  InitSubscriptions();
  RCLCPP_INFO(get_logger(), "[BOOT] ZoomController constructed");
}

ZoomController::~ZoomController() {
  ClosePelco();
}

// ========================== 초기화 ==========================
void ZoomController::InitRosParam() {
  device_path_       = declare_parameter<std::string>("device_path", device_path_);
  baud_rate_         = declare_parameter<int>("baud_rate", baud_rate_);
  camera_id_         = declare_parameter<int>("camera_id", camera_id_);
  distance_topic_    = declare_parameter<std::string>("distance_topic", distance_topic_);
  preset_cmd_topic_  = declare_parameter<std::string>("preset_cmd_topic", preset_cmd_topic_);
  zoom_level_topic_  = declare_parameter<std::string>("zoom_level_topic", zoom_level_topic_);

  pelco_repeat_tx_         = declare_parameter<int>("pelco_repeat_tx", pelco_repeat_tx_);
  pelco_repeat_delay_ms_   = declare_parameter<int>("pelco_repeat_delay_ms", pelco_repeat_delay_ms_);
  pelco_post_stop_         = declare_parameter<bool>("pelco_post_stop", pelco_post_stop_);
  pelco_post_stop_delay_ms_= declare_parameter<int>("pelco_post_stop_delay_ms", pelco_post_stop_delay_ms_);

  hysteresis_m_            = declare_parameter<double>("hysteresis_m", hysteresis_m_);

  // 매핑 파라미터 로드 (기본값은 코드상의 예전 로직과 동일하게 설정)
  zoom_mapping_thresholds_ = declare_parameter<std::vector<double>>("zoom_mapping.thresholds", std::vector<double>{500.0, 300.0, 200.0, 100.0});
  zoom_mapping_presets_    = declare_parameter<std::vector<int64_t>>("zoom_mapping.presets", std::vector<int64_t>{1, 2, 3, 4, 5});
  zoom_levels_             = declare_parameter<std::vector<int64_t>>("zoom_levels", std::vector<int64_t>{30, 16, 8, 4, 1});

  // 유효성 검사
  if (zoom_mapping_presets_.size() != zoom_mapping_thresholds_.size() + 1) {
    RCLCPP_ERROR(get_logger(), "[INIT] Size mismatch: thresholds(%zu) vs presets(%zu). Force default.",
                 zoom_mapping_thresholds_.size(), zoom_mapping_presets_.size());
    zoom_mapping_thresholds_ = {500.0, 300.0, 200.0, 100.0};
    zoom_mapping_presets_    = {1, 2, 3, 4, 5};
  }

  RCLCPP_INFO(get_logger(),
              "[INIT] device=%s baud=%d cam_id=%d topic=%s repeat=%d delay=%dms post_stop=%s post_delay=%dms hysteresis=%.2f",
              device_path_.c_str(), baud_rate_, camera_id_, distance_topic_.c_str(),
              pelco_repeat_tx_, pelco_repeat_delay_ms_,
              pelco_post_stop_ ? "true" : "false", pelco_post_stop_delay_ms_,
              hysteresis_m_);
  RCLCPP_INFO(get_logger(), "[INIT] preset_cmd_topic=%s", preset_cmd_topic_.c_str());
  RCLCPP_INFO(get_logger(), "[INIT] zoom_level_topic=%s", zoom_level_topic_.c_str());
  RCLCPP_INFO(get_logger(), "[INIT] Loaded %zu thresholds, %zu presets, %zu zoom levels",
              zoom_mapping_thresholds_.size(), zoom_mapping_presets_.size(), zoom_levels_.size());
}

void ZoomController::InitPelcoSerial() {
  if (rs485_fd_ >= 0) return;

  RCLCPP_INFO(get_logger(), "[PELCO] open '%s' @ %d", device_path_.c_str(), baud_rate_);

  rs485_fd_ = ::open(device_path_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (rs485_fd_ < 0) {
    RCLCPP_ERROR(get_logger(), "Failed to open %s: %s", device_path_.c_str(), std::strerror(errno));
    return;
  }

  termios tio{};
  if (tcgetattr(rs485_fd_, &tio) < 0) {
    RCLCPP_ERROR(get_logger(), "tcgetattr failed: %s", std::strerror(errno));
    ::close(rs485_fd_); rs485_fd_ = -1;
    return;
  }

  cfmakeraw(&tio);
  int b = GetBaudConst(baud_rate_);
  cfsetispeed(&tio, b);
  cfsetospeed(&tio, b);
  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_cflag &= ~PARENB; // N
  tio.c_cflag &= ~CSTOPB; // 1
  tio.c_cflag &= ~CSIZE;
  tio.c_cflag |= CS8;     // 8

  if (tcsetattr(rs485_fd_, TCSANOW, &tio) < 0) {
    RCLCPP_ERROR(get_logger(), "tcsetattr failed: %s", std::strerror(errno));
    ::close(rs485_fd_); rs485_fd_ = -1;
    return;
  }

  RCLCPP_INFO(get_logger(), "[PELCO] serial ready");
}

void ZoomController::ClosePelco() {
  if (rs485_fd_ >= 0) {
    ::close(rs485_fd_);
    rs485_fd_ = -1;
    RCLCPP_INFO(get_logger(), "[PELCO] closed");
  }
}

void ZoomController::InitSubscriptions() {
  // 퍼블리셔와 호환되도록 SensorDataQoS 사용 (best_effort)
  auto qos = rclcpp::SensorDataQoS();

  // 거리(Float64) 구독
  sub_distance_ = create_subscription<std_msgs::msg::Float64>(
      distance_topic_, qos,
      std::bind(&ZoomController::OnDistance, this, std::placeholders::_1));

  // rqt 수동 프리셋 (Int32)
  sub_preset_cmd_ = create_subscription<std_msgs::msg::Int32>(
      preset_cmd_topic_, rclcpp::QoS(10),
      std::bind(&ZoomController::OnPresetCmd, this, std::placeholders::_1));

  // Zoom Level 발행
  pub_zoom_level_ = create_publisher<std_msgs::msg::Int32>(zoom_level_topic_, 10);

  RCLCPP_INFO(get_logger(), "[ROS ] subscribe '%s' (SensorDataQoS, Float64)", distance_topic_.c_str());
  RCLCPP_INFO(get_logger(), "[ROS ] subscribe '%s' (preset cmd)", preset_cmd_topic_.c_str());
  RCLCPP_INFO(get_logger(), "[ROS ] publish   '%s' (zoom level)", zoom_level_topic_.c_str());
}

// ========================== 콜백/처리 ==========================
void ZoomController::OnDistance(const std_msgs::msg::Float64::SharedPtr msg) {
  if (!msg) return;
  ProcessDistance(msg->data);
}

void ZoomController::ProcessDistance(double d) {
  const int current = last_preset_.load();
  const double prevd = last_distance_.load();
  const bool is_valid = (d > 0.0);

  // 목표 프리셋 결정
  // 무효값일 때는 가장 마지막 구간(Wide)인 것으로 간주 -> zoom_mapping_presets_.back() 사용
  int target_preset_val = 5;
  if (!zoom_mapping_presets_.empty()) {
      target_preset_val = static_cast<int>(zoom_mapping_presets_.back());
  }
  
  const int target = is_valid ? PresetFromDistance(d) : target_preset_val;

  bool apply_change = false;

  // 1. 변경 불필요 (동일 프리셋)
  if (target == current) {
    if (is_valid) {
      RCLCPP_INFO(get_logger(), "[SKIP] same preset=%d (d=%.2f)", target, d);
    }
  }
  // 2. 히스테리시스 체크 (유효값 범위 내 변동 시에만)
  else if (is_valid && current != 0 && prevd >= 0.0 &&
           !AllowChangeWithHysteresis(prevd, d, current, target)) {
    RCLCPP_INFO(get_logger(), "[HYST] hold %d->%d (prev=%.2f, now=%.2f, hys=%.2fm)",
                current, target, prevd, d, hysteresis_m_);
  }
  // 3. 변경 실행 필요
  else {
    apply_change = true;
  }

  // 실행 로직
  if (apply_change) {
    if (is_valid) {
      RCLCPP_INFO(get_logger(), "[CALL] goto preset %d (from %d)", target, current);
    }

    PelcoGotoPreset(target, pelco_post_stop_, pelco_post_stop_delay_ms_);
    last_preset_.store(target);

    if (pub_zoom_level_) {
      std_msgs::msg::Int32 msg;
      msg.data = GetZoomLevelFromPreset(target);
      pub_zoom_level_->publish(msg);
    }

    if (is_valid) {
      RCLCPP_INFO(get_logger(), "[DONE] preset %d sent", target);
    }
  }

  // 상태 업데이트 (항상 수행)
  last_distance_.store(d);
}

void ZoomController::OnPresetCmd(const std_msgs::msg::Int32::SharedPtr msg) {
  if (!msg) return;
  const int target  = std::clamp(msg->data, 1, 255);
  const int current = last_preset_.load();

  if (target == current) {
    RCLCPP_INFO(get_logger(), "[SKIP] manual preset=%d (same as current)", target);
    return;
  }

  RCLCPP_INFO(get_logger(), "[CMD ] goto preset %d (manual)", target);
  PelcoGotoPreset(target, pelco_post_stop_, pelco_post_stop_delay_ms_);
  last_preset_.store(target);

  if (pub_zoom_level_) {
    std_msgs::msg::Int32 msg;
    msg.data = GetZoomLevelFromPreset(target);
    pub_zoom_level_->publish(msg);
  }

  RCLCPP_INFO(get_logger(), "[DONE] preset %d sent (manual)", target);
}

// ========================== Pelco-D ==========================
void ZoomController::SendPelcoFrame(uint8_t cmd1, uint8_t cmd2, uint8_t data1, uint8_t data2,
                                    int repeats, int delay_ms) {
  if (rs485_fd_ < 0) {
    RCLCPP_ERROR(get_logger(), "[TX  ] serial not open");
    return;
  }

  uint8_t addr   = static_cast<uint8_t>(camera_id_ & 0xFF);
  uint8_t chksum = static_cast<uint8_t>((addr + cmd1 + cmd2 + data1 + data2) & 0xFF);
  uint8_t pkt[7] = {0xFF, addr, cmd1, cmd2, data1, data2, chksum};

  //RCLCPP_INFO(get_logger(), "[TX  ] %02X %02X %02X %02X %02X %02X %02X (rep=%d, delay=%dms)",
  //            pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5], pkt[6],
  //            std::max(1,repeats), delay_ms);

  const int reps = std::max(1, repeats);
  for (int i = 0; i < reps; ++i) {
    ssize_t ret = ::write(rs485_fd_, pkt, sizeof(pkt));
    if (ret < 0) {
      RCLCPP_ERROR(get_logger(), "[TX  ] write failed: %s", std::strerror(errno));
    }
    if (delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  }
}

void ZoomController::PelcoGotoPreset(int preset_id, bool send_stop, int stop_delay_ms) {
  const int pid = std::clamp(preset_id, 1, 255);
  // Goto Preset: CMD1=0x00, CMD2=0x07, DATA1=0x00, DATA2=PresetID
  SendPelcoFrame(0x00, 0x07, 0x00, static_cast<uint8_t>(pid),
                 pelco_repeat_tx_, pelco_repeat_delay_ms_);

  if (send_stop) {
    if (stop_delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(stop_delay_ms));
    // STOP: CMD1=0x00, CMD2=0x00, DATA1=0x00, DATA2=0x00
    SendPelcoFrame(0x00, 0x00, 0x00, 0x00, 1, 0);
  }
}

} // namespace camera_interface
