#ifndef PAN_TILT_CONTROLLER_HPP_
#define PAN_TILT_CONTROLLER_HPP_

#include <cmath>
#include <algorithm>
#include <thread>
#include <deque>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "std_msgs/msg/string.hpp"
#include "combat_robot_msgs/msg/pan_tilt_control_command.hpp"
#include "combat_robot_msgs/msg/pan_tilt_state.hpp"

#include "async_comm/serial.h"

using namespace std::placeholders;
using namespace combat_robot_msgs::msg;

#define PAN_TILT_ADDR 0x01

#define GET_ANGLE_HOR 0x0051
#define GET_ANGLE_VER 0x0053

#define REQ_ANGLE_HOR 0x0059
#define REQ_ANGLE_VER 0x005B

#define SET_ANGLE_HOR 0x004B
#define SET_ANGLE_VER 0x004D
#define SET_SPEED     0x5F

// Motion Direction Bits
const uint8_t DIR_RIGHT = 0x0002;
const uint8_t DIR_LEFT  = 0x0004;
const uint8_t DIR_UP    = 0x0008;
const uint8_t DIR_DOWN  = 0x0010;

#define SET_BRAKE 0x0000


namespace combat_robot_system {

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class PanTiltController : public rclcpp_lifecycle::LifecycleNode {
public:
  explicit PanTiltController(const rclcpp::NodeOptions &options);
  ~PanTiltController();

  // Lifecycle Callbacks
  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
    CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);
    CallbackReturn on_error(const rclcpp_lifecycle::State &);

    // Internal Functions
  std::shared_ptr<rclcpp_lifecycle::LifecyclePublisher<PanTiltState>> pub_actuator_state_;

  // Subscribe (Regular)
  rclcpp::Subscription<PanTiltControlCommand>::SharedPtr sub_control_command_;
  void on_control_command(const PanTiltControlCommand::ConstSharedPtr msg);
  
  // Init helpers
  void init_param();
  bool init_serial_comm();

  rclcpp::TimerBase::SharedPtr timer_;
  void on_timer();
  void update_dir_control();
  int cmd_counter_{0};
  // Params
  std::string port_name_;
  int bard_rate_;
  double angle_tolerance_;

  // PID Controller Gains & State
  double pan_proportional_gain_;
  double pan_integral_gain_;
  double pan_derivative_gain_;
  double pan_integral_windup_limit_;
  double pan_integral_ = 0.0;
  double last_pan_error_ = 0.0;

  double tilt_proportional_gain_;
  double tilt_integral_gain_;
  double tilt_derivative_gain_;
  double tilt_integral_windup_limit_;
  double tilt_integral_ = 0.0;
  double last_tilt_error_ = 0.0;

  // Velocity and Acceleration Limits
  double max_pan_velocity_;
  double max_tilt_velocity_;
  double max_pan_acceleration_;
  double max_tilt_acceleration_;
  double current_pan_velocity_ = 0.0;
  double current_tilt_velocity_ = 0.0;

  // Safety and Timeout
  double command_timeout_ms_;
  rclcpp::Time last_command_time_;
  bool enable_startup_homing_;
  double home_pan_angle_;
  double home_tilt_angle_;

  // Convenience
  bool invert_pan_direction_;
  bool invert_tilt_direction_;
  double controller_frequency_;

  int min_pan_speed_;
  int min_tilt_speed_;
  int max_pan_speed_;
  int max_tilt_speed_;

  // Serial
  std::unique_ptr<async_comm::Serial> serial_comm_;
  std::vector<uint8_t> recv_buffer_;

  void on_read_data(const uint8_t* buf, size_t len);
  void send_data();
  void send_data_stop();
  void parsing_data(const uint8_t* buf, size_t len);
  uint8_t calc_check_sum(uint8_t* data);

  void enqueue_command(uint16_t command, uint16_t data = 0x0000);
  std::deque<std::pair<uint16_t, uint16_t>> command_queue_;
  std::mutex queue_mutex_;
  // State
  PanTiltState state_; // output state

  // 목표 각도 및 내부 이동 제어 플래그
  float target_pan_angle_ = 0.0f;
  float target_tilt_angle_ = 0.0f;
  uint8_t target_pan_speed_ = 0;
  uint8_t target_tilt_speed_ = 0;
  bool dir_control_enabled_{false};

  std::map<uint8_t, uint8_t> pan_dir_ = {{0x00, 0x00}, {0x01, DIR_RIGHT}, {0x02, DIR_LEFT}};
  std::map<uint8_t, uint8_t> tilt_dir_ = {{0x00, 0x00}, {0x01, DIR_UP}, {0x02, DIR_DOWN}};
  int pan_offset_{180};
  int tilt_offset_{27}; 

  int min_tilt_angle_{0}; 
  int max_tilt_angle_{350}; 
  int min_pan_angle_{0}; 
  int max_pan_angle_{47}; 
};


}  // namespace combat_robot_system
#endif