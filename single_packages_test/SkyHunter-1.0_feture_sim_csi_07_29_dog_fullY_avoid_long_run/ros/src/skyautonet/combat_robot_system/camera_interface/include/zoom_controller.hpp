#pragma once

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/int32.hpp>

#include <string>
#include <atomic>

namespace camera_interface
{

class ZoomController : public rclcpp::Node
{
public:
  explicit ZoomController(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
  ~ZoomController() override;

private:
  // 초기화
  void InitRosParam();
  void InitPelcoSerial();
  void ClosePelco();
  void InitSubscriptions();

  // 콜백 / 처리
  void OnDistance(const std_msgs::msg::Float64::SharedPtr msg); // Float64 전용
  void ProcessDistance(double d);
  void OnPresetCmd(const std_msgs::msg::Int32::SharedPtr msg);  // rqt 수동 프리셋

  // Pelco-D
  void SendPelcoFrame(uint8_t cmd1, uint8_t cmd2, uint8_t data1, uint8_t data2,
                      int repeats, int delay_ms);
  void PelcoGotoPreset(int preset_id, bool send_stop, int stop_delay_ms);

  // 유틸
  int GetBaudConst(int baud);
  int PresetFromDistance(double d);
  int GetZoomLevelFromPreset(int preset_id);
  bool AllowChangeWithHysteresis(double old_d, double new_d, int old_preset, int new_preset) const;

private:
  // 파라미터
  std::string device_path_{"/dev/ttyUSB0"};
  int baud_rate_{9600};
  int camera_id_{1};
  std::string distance_topic_{"/distance"};
  std::string preset_cmd_topic_{"/zoom/preset"};
  std::string zoom_level_topic_{"/zoom_level"};

  int  pelco_repeat_tx_{3};
  int  pelco_repeat_delay_ms_{40};
  bool pelco_post_stop_{true};
  int  pelco_post_stop_delay_ms_{30};

  double hysteresis_m_{0.0}; // 0.0이면 비활성

  // 매핑 파라미터
  std::vector<double> zoom_mapping_thresholds_;
  std::vector<int64_t> zoom_mapping_presets_; // ROS2 param은 int64_t 벡터로 읽힘
  std::vector<int64_t> zoom_levels_;

  // 상태
  int rs485_fd_{-1};
  std::atomic<int>    last_preset_{0};        // 0 == none
  std::atomic<double> last_distance_{-1.0};   // 미정

  // ROS
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr sub_distance_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr   sub_preset_cmd_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr      pub_zoom_level_;
};

} // namespace camera_interface
