#pragma once
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int8.hpp>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <filesystem>
#include <fstream>

class GunTriggerDriver {
public:
  struct Params {
    int chip{-1};       // pwmchipN 의 N (예: 0 → /sys/class/pwm/pwmchip0)
    int line{-1};       // pwm<N> 의 <N>  (예: 0 → pwm0)
    int us1{1850};      // 1st pulse (µs)
    int us2{2000};      // 2nd pulse (µs)  (<=0 이면 생략)
    int hold_ms{500};   // hold each pulse (ms)
    int trigger_ms{0};  // wait between us1 and us2 (ms, optional)
  };

  explicit GunTriggerDriver(rclcpp::Node* node, const Params& p);
  ~GunTriggerDriver();

  bool requestFire();                     // 1회 발사 요청. 이미 발사중이면 false
  bool busy() const { return firing_.load(std::memory_order_relaxed); }

private:
  void fire_worker_(Params p);

  // sysfs PWM 제어 (lgpio 대체)
  void open_gpio_();     // export + polarity=normal + period + duty=0 + enable=0 (idle)
  void close_gpio_();    // duty=0 + enable=0
  void pwm_us_(int us);  // enable=0 → duty 갱신 → enable=1
  void pwm_stop_();      // enable=0 (완전 OFF, 떨림 방지)

  // sysfs helpers
  bool write_file_(const std::string& path, const std::string& val);
  bool read_file_(const std::string& path, std::string* out);

  // paths
  std::string base_path_;   // /sys/class/pwm/pwmchipX
  std::string pwm_path_;    // /sys/class/pwm/pwmchipX/pwmY
  static constexpr uint32_t PERIOD_NS_ = 20000000; // 50Hz(20ms)

  rclcpp::Node*  node_{nullptr};
  rclcpp::Logger logger_{rclcpp::get_logger("GunTriggerDriver")};
  Params params_;

  std::atomic<bool> firing_{false};
  std::mutex mtx_;
  std::thread worker_;
};

class GunTriggerNode : public rclcpp::Node {
public:
  GunTriggerNode();

private:
  void onCmd(const std_msgs::msg::Int8::SharedPtr msg);
  void publishStatus_(int8_t s);

  GunTriggerDriver::Params p_;
  std::unique_ptr<GunTriggerDriver> driver_;
  std::string cmd_topic_;
  std::string status_topic_;
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_cmd_;
  rclcpp::Publisher<std_msgs::msg::Int8>::SharedPtr    pub_status_;
  rclcpp::TimerBase::SharedPtr status_timer_;
};
