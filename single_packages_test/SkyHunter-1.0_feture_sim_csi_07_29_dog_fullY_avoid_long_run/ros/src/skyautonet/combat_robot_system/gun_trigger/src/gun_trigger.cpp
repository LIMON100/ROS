#include "gun_trigger.hpp"
#include <chrono>
#include <thread>
#include <stdexcept>
#include <sstream>

using namespace std::chrono_literals;

// ================= Driver (sysfs PWM) =================

GunTriggerDriver::GunTriggerDriver(rclcpp::Node* node, const Params& p)
: node_(node), logger_(node->get_logger()), params_(p) {
  if (!node_) throw std::runtime_error("GunTriggerDriver: node is null");
  if (params_.chip < 0 || params_.line < 0)
    throw std::runtime_error("GunTriggerDriver: invalid chip/line");
  open_gpio_();
  RCLCPP_INFO(logger_, "GunTriggerDriver ready (pwmchip=%d, pwm=%d)", params_.chip, params_.line);
}

GunTriggerDriver::~GunTriggerDriver() {
  try {
    pwm_stop_();                 // 완전 OFF
    if (worker_.joinable()) worker_.join();
    close_gpio_();
  } catch (...) {}
}

bool GunTriggerDriver::requestFire() {
  std::lock_guard<std::mutex> lock(mtx_);
  if (firing_) {
    RCLCPP_INFO(logger_, "Already firing. Ignore request.");
    return false;
  }
  firing_ = true;
  auto p = params_;
  if (worker_.joinable()) worker_.join();
  worker_ = std::thread([this, p]() { this->fire_worker_(p); });
  return true;
}

void GunTriggerDriver::fire_worker_(Params p) {
  try {
    // 1st pulse
    pwm_us_(p.us1);
    std::this_thread::sleep_for(std::chrono::milliseconds(p.hold_ms));

    // optional gap
    if (p.trigger_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(p.trigger_ms));
    }

    // 2nd pulse (옵션)
    if (p.us2 > 0) {
      pwm_us_(p.us2);
      std::this_thread::sleep_for(std::chrono::milliseconds(p.hold_ms));
    }

    // 끝나면 완전 OFF (enable=0) — 떨림 방지
    pwm_stop_();

  } catch (const std::exception& e) {
    RCLCPP_ERROR(logger_, "fire_worker_ exception: %s", e.what());
    pwm_stop_();
  } catch (...) {
    RCLCPP_ERROR(logger_, "fire_worker_: unknown exception");
    pwm_stop_();
  }
  firing_ = false;
}

bool GunTriggerDriver::write_file_(const std::string& path, const std::string& val) {
  std::ofstream ofs(path);
  if (!ofs) return false;
  ofs << val;
  ofs.flush();
  return ofs.good();
}

bool GunTriggerDriver::read_file_(const std::string& path, std::string* out) {
  std::ifstream ifs(path);
  if (!ifs) return false;
  std::ostringstream ss; ss << ifs.rdbuf();
  if (out) *out = ss.str();
  return true;
}

void GunTriggerDriver::open_gpio_() {
  base_path_ = "/sys/class/pwm/pwmchip" + std::to_string(params_.chip);
  pwm_path_  = base_path_ + "/pwm" + std::to_string(params_.line);

  if (!std::filesystem::exists(base_path_)) {
    throw std::runtime_error("PWM chip path not found: " + base_path_);
  }
  if (!std::filesystem::exists(pwm_path_)) {
    if (!write_file_(base_path_ + "/export", std::to_string(params_.line))) {
      throw std::runtime_error("Failed to export PWM channel");
    }
    std::this_thread::sleep_for(50ms); // sysfs 생성 대기
  }

  // 설정은 enable=0 상태에서
  write_file_(pwm_path_ + "/enable", "0");

  // polarity=normal 강제 (여기서만 변경 가능)
  if (!write_file_(pwm_path_ + "/polarity", "normal")) {
    throw std::runtime_error("Failed to set polarity=normal (must be set with enable=0).");
  }

  // 50Hz 세팅, duty=0 (idle), enable=0 유지
  if (!write_file_(pwm_path_ + "/period", std::to_string(PERIOD_NS_))) {
    throw std::runtime_error("Failed to set period");
  }
  if (!write_file_(pwm_path_ + "/duty_cycle", "0")) {
    throw std::runtime_error("Failed to set duty_cycle");
  }
  // 여기서는 켜지 않음 — 발사 시점에 켬
}

void GunTriggerDriver::close_gpio_() {
  if (std::filesystem::exists(pwm_path_)) {
    write_file_(pwm_path_ + "/duty_cycle", "0");
    write_file_(pwm_path_ + "/enable", "0");
    // 필요 시 unexport 원하면 아래 주석 해제
    // write_file_(base_path_ + "/unexport", std::to_string(params_.line));
  }
}

void GunTriggerDriver::pwm_us_(int us) {
  if (us < 0) us = 0;
  uint64_t duty_ns = static_cast<uint64_t>(us) * 1000ULL;
  if (duty_ns >= PERIOD_NS_) duty_ns = PERIOD_NS_ - 1;

  // 항상: enable=0 → duty 갱신 → enable=1 (드라이버/하드웨어 호환성 최우선)
  write_file_(pwm_path_ + "/enable", "0");
  if (!write_file_(pwm_path_ + "/duty_cycle", std::to_string(duty_ns))) {
    throw std::runtime_error("Failed to write duty_cycle");
  }
  write_file_(pwm_path_ + "/enable", "1");
}

void GunTriggerDriver::pwm_stop_() {
  // 완전 OFF — 떨림/헛진동 방지
  write_file_(pwm_path_ + "/enable", "0");
}

// ================= Node (인터페이스/파라미터/토픽 그대로) =================

GunTriggerNode::GunTriggerNode()
: rclcpp::Node("gun_trigger_node")
{
  // 파라미터
  p_.chip       = declare_parameter<int>("chip", 0);   // pwmchip0
  p_.line       = declare_parameter<int>("line", 0);   // pwm0
  p_.us1        = declare_parameter<int>("us1", 1850);
  p_.us2        = declare_parameter<int>("us2", 2000);
  p_.hold_ms    = declare_parameter<int>("hold_ms", 500);
  p_.trigger_ms = declare_parameter<int>("trigger_ms", 0);

  // 토픽
  cmd_topic_    = declare_parameter<std::string>("cmd_topic", "/gun_trigger/cmd");
  status_topic_ = declare_parameter<std::string>("status_topic", "/gun_trigger/status");

  driver_ = std::make_unique<GunTriggerDriver>(this, p_);

  pub_status_ = create_publisher<std_msgs::msg::Int8>(status_topic_, 10);
  sub_cmd_ = create_subscription<std_msgs::msg::Int8>(
    cmd_topic_, 10,
    std::bind(&GunTriggerNode::onCmd, this, std::placeholders::_1));

  // 상태 주기 발행(200ms)
  status_timer_ = create_wall_timer(200ms, [this](){
    publishStatus_(driver_->busy() ? 1 : 0);
  });

  publishStatus_(0);
  RCLCPP_INFO(get_logger(),
    "gun_trigger_node started (pwmchip=%d, pwm=%d | cmd=%s, status=%s)",
    p_.chip, p_.line, cmd_topic_.c_str(), status_topic_.c_str());
}

void GunTriggerNode::onCmd(const std_msgs::msg::Int8::SharedPtr msg) {
  if (!msg) return;
  if (msg->data != 1) {
    RCLCPP_WARN(get_logger(), "Unknown cmd=%d (ignored)", msg->data);
    return;
  }
  if (!driver_->requestFire()) {
    publishStatus_(1); // 이미 발사중
  } else {
    publishStatus_(1); // 발사 시작
  }
}

void GunTriggerNode::publishStatus_(int8_t s) {
  std_msgs::msg::Int8 m; m.data = s;
  pub_status_->publish(m);
}

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<GunTriggerNode>());
  rclcpp::shutdown();
  return 0;
}
