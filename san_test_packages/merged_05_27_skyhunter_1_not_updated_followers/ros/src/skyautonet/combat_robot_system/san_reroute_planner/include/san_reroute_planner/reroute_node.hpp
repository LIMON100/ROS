// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — RerouteNode (PATCHED 2026-05-13).
//
// PATCH 2026-05-13 (Reroute deep-dive review):
//   * decodeCostGrid no longer has dead-code path (C4). Raw vs PNG
//     mode is now explicit and bothered to actually attempt PNG
//     decode via stb_image (header-only). A new `raw_grid_mode`
//     parameter forces raw-uint8 interpretation for integration
//     testing.
//   * obstacle_on_path now reflects lethal cells ONLY (M7), not
//     inflated. Inflated triggers cmd_vel slow-down but NOT T1.5 —
//     matches SDD §6.4 intent (inflation layer is a soft hint).
//   * yaw from RobotStatus is now captured and used to compute the
//     correct angular velocity (M10) — previously the absolute path
//     bearing was clamped into the velocity field, leading to
//     erratic rotation when the robot was not facing east.
//   * EvasionConfig.heading_aware controlled by ros param.
//   * StartCellLethal handled explicitly — emergency STOP published.

#ifndef SAN_REROUTE_PLANNER__REROUTE_NODE_HPP_
#define SAN_REROUTE_PLANNER__REROUTE_NODE_HPP_

#include <chrono>
#include <memory>
#include <mutex>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <geometry_msgs/msg/twist.hpp>

#include <combat_robot_msgs/msg/cost_map_update.hpp>
#include <combat_robot_msgs/msg/follower_target_message.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/tier_status_change.hpp>

#include "san_reroute_planner/cost_map_view.hpp"

namespace san_reroute_planner
{

class RerouteNode : public rclcpp::Node
{
public:
  explicit RerouteNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

private:
  void declareParameters();
  void loadParameters();

  void onCostMap(
    const combat_robot_msgs::msg::CostMapUpdate::SharedPtr msg);
  void onFollowerTarget(
    const combat_robot_msgs::msg::FollowerTargetMessage::SharedPtr msg);
  void onRobotStatus(
    const combat_robot_msgs::msg::RobotStatus::SharedPtr msg);
  void onTierStatusChange(
    const combat_robot_msgs::msg::TierStatusChange::SharedPtr msg);

  void onTick();   // 100 ms

  /// PATCH 2026-05-13: explicit raw vs PNG dispatch with no dead-code
  /// branches. PNG decode uses stb_image (header-only). Returns false
  /// if the payload cannot be interpreted; caller WARN-throttles.
  bool decodeCostGrid(
    const combat_robot_msgs::msg::CostMapUpdate & msg,
    CostMapView & out) const;

  // Config
  uint32_t robot_id_{3};
  uint32_t tick_period_ms_{100};
  float evasion_linear_speed_{1.0f};
  float evasion_angular_max_{1.0f};
  bool raw_grid_mode_{false};           // ★ PATCH 2026-05-13
  bool heading_aware_evasion_{true};     // ★ PATCH 2026-05-13
  bool png_decode_warned_{false};

  // Latest snapshot — protected by state_mu_
  std::mutex state_mu_;
  CostMapView cost_map_;
  uint64_t cost_map_received_us_{0};
  float current_x_{0.0f}, current_y_{0.0f};
  float current_yaw_{0.0f};             // ★ PATCH 2026-05-13 (M10)
  float target_x_{0.0f}, target_y_{0.0f};
  bool target_valid_{false};
  uint8_t tier_state_{1};

  // ROS interfaces
  rclcpp::Subscription<combat_robot_msgs::msg::CostMapUpdate>::SharedPtr cost_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::FollowerTargetMessage>::SharedPtr target_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr status_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::TierStatusChange>::SharedPtr tier_sub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr obstacle_pub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
  rclcpp::TimerBase::SharedPtr tick_timer_;
};

}  // namespace san_reroute_planner

#endif  // SAN_REROUTE_PLANNER__REROUTE_NODE_HPP_
