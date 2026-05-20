// SAN v1.5 — RerouteNode (per-follower).
//
// Implements the BT side of SDD-SWARM §6.4 Tier 1.5 AUTO_REROUTE.
//
// Subscriptions:
//   /swarm/formation/follower_target          — goal pose (1s predicted)
//   /robot_X/robot_status                     — current pose
//   /robot_X/cost_map_node/cost_map_update    — local cost map (PNG)
//   /robot_X/tier_node/tier_status_change     — current FSM tier (informational)
//
// Publications:
//   /robot_X/obstacle_on_path  (std_msgs/Bool)  — fed to tier_node
//   /robot_X/cmd_vel           (geometry_msgs/Twist) — when T1.5 active
//
// 100 ms tick:
//   1. Decode cost grid (PNG → raw uint8 — see decodeCostGrid below)
//   2. Run cost_path_checker on (current → target)
//   3. Publish obstacle_on_path (true/false)
//   4. If obstacle: run lateral_evasion → publish /cmd_vel
//
// KPP-2 measurement: log microsecond timing from cost map update to
// /cmd_vel publication. Aggregate to operator UI for PDR evidence.
//
// 권원:
//   * SDD-SWARM v1.5 §6.4 (Cost Map T1.5)
//   * KPP-2 (회피 반응 ≤ 300ms)

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

#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <combat_robot_msgs/msg/leader_state.hpp>

namespace san_reroute_planner {

class RerouteNode : public rclcpp::Node {
public:
  explicit RerouteNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());

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

  /// Decode the cost grid out of CostMapUpdate. The wire format is a
  /// PNG-encoded uint8 image; for PDR-5 we accept a fallback path of
  /// "raw uint8 payload" (when an upstream san_costmap option is set
  /// to skip PNG encoding for integration testing). PNG decode itself
  /// requires libpng / stb_image — left as TODO in production
  /// (san_costmap can be configured to publish raw via parameter).
  bool decodeCostGrid(
      const combat_robot_msgs::msg::CostMapUpdate& msg,
      CostMapView& out) const;

  // Config
  uint32_t robot_id_{3};
  uint32_t tick_period_ms_{100};
  float    evasion_linear_speed_{1.0f};
  float    evasion_angular_max_{1.0f};
  bool     png_decode_warned_{false};

  // Latest snapshot — protected by state_mu_
  std::mutex      state_mu_;
  CostMapView     cost_map_;
  uint64_t        cost_map_received_us_{0};   // for KPP-2 measurement
  float           current_x_{0.0f}, current_y_{0.0f};
  float           target_x_{0.0f},  target_y_{0.0f};
  bool            target_valid_{false};
  uint8_t         tier_state_{1};              // start in T1


  float current_yaw_{0.0f};
  float target_vx_{0.0f};
  
  rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr swarm_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::LeaderState>::SharedPtr combat_sub_;

  sensor_msgs::msg::NavSatFix latest_gps_;
  bool has_gps_ = false;
  geometry_msgs::msg::PoseArray swarm_poses_;
  bool has_swarm_ = false;
  combat_robot_msgs::msg::LeaderState combat_state_;
  bool is_combat_ = false;

  // Anti-stuck variables
  double last_lat_ = 0.0;
  double last_lon_ = 0.0;
  rclcpp::Time last_stuck_check_time_;
  int high_cmd_ticks_ = 0;
  int total_ticks_ = 0;
  bool is_reversing_ = false;
  rclcpp::Time reverse_start_time_;
  float last_cmd_vel_x_ = 0.0;

  // ROS interfaces
  rclcpp::Subscription<combat_robot_msgs::msg::CostMapUpdate>::SharedPtr cost_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::FollowerTargetMessage>::SharedPtr target_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr  status_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::TierStatusChange>::SharedPtr tier_sub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr                     obstacle_pub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr               cmd_vel_pub_;
  rclcpp::TimerBase::SharedPtr                                          tick_timer_;
};

}  // namespace san_reroute_planner

#endif  // SAN_REROUTE_PLANNER__REROUTE_NODE_HPP_
