#ifndef TELEOP_CONTROLLER_HPP
#define TELEOP_CONTROLLER_HPP

#include <rclcpp/rclcpp.hpp>
#include <modbus/modbus.h>

#include "combat_robot_msgs/msg/drive_command.hpp"
#include "combat_robot_msgs/msg/operation_state.hpp"

namespace teleop_controller {
using combat_robot_msgs::msg::DriveCommand;
using combat_robot_msgs::msg::OperationState;
using std::placeholders::_1;

const double MAX_RPM = 3000.0;
const double MAX_SPEED_MPS = 2.222;  // 8km/h

class RobotController : public rclcpp::Node {
 public:
  explicit RobotController(const rclcpp::NodeOptions& options);
  ~RobotController();

 private:
  void init_ros();
  void init_serial();
  rclcpp::TimerBase::SharedPtr timer_;
  void on_timer();

  rclcpp::Subscription<DriveCommand>::SharedPtr sub_cmd_vel_;
  void on_cmd_vel(const DriveCommand::ConstSharedPtr& msg);

  // Subscribe to operation state
  rclcpp::Subscription<OperationState>::SharedPtr sub_operation_state_;
  void on_operation_state(const OperationState::SharedPtr msg);

  double wheel_base_;
  double max_linear_velocity_;
  double max_angular_velocity_;
  std::string device_path_;

  modbus_t* modbus_;

  // current operation state
  int32_t current_operation_state_ = 0;


  double speedToRegisterValue(double speed_mps);

};
}  // namespace teleop_controller
#endif
