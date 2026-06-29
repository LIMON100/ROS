// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#ifndef SAN_SURVEILLANCE__PAN_TILT_DRIVER_NODE_HPP_
#define SAN_SURVEILLANCE__PAN_TILT_DRIVER_NODE_HPP_

#include <cstdint>
#include <mutex>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float64.hpp>
#include <combat_robot_msgs/msg/pan_tilt_command.hpp>

#include "san_surveillance/pan_tilt_controller.hpp"

namespace san_surveillance
{

/// Bridges /swarm/cmd/pantilt (IDS §5.11, published by surveillance_node)
/// onto per-robot gimbal joint position commands by stepping the pure-logic
/// pan-tilt controller (SDD §8.3/§8.4) at a fixed rate. The sim consumes the
/// rad outputs via gz joint controllers; real HW replaces this executable
/// with the vendor gimbal driver speaking the same topics.
class PanTiltDriverNode : public rclcpp::Node
{
public:
  explicit PanTiltDriverNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

private:
  using PanTiltCommandMsg = combat_robot_msgs::msg::PanTiltCommand;

  void onCommand(const PanTiltCommandMsg::SharedPtr msg);
  void onTick();

  uint32_t robot_id_{1};
  double rate_hz_{50.0};
  float dt_s_{0.02f};
  double cmd_timeout_s_{3.0};

  PanTiltConfig cfg_;
  PanTiltState state_;

  std::mutex cmd_mu_;
  bool have_cmd_{false};
  uint8_t cmd_mode_{0};
  float cmd_pan_deg_{0.0f};
  float cmd_tilt_deg_{0.0f};
  float cmd_sweep_range_deg_{60.0f};
  rclcpp::Time cmd_stamp_{0, 0, RCL_ROS_TIME};

  rclcpp::Subscription<PanTiltCommandMsg>::SharedPtr cmd_sub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pan_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr tilt_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace san_surveillance

#endif  // SAN_SURVEILLANCE__PAN_TILT_DRIVER_NODE_HPP_
