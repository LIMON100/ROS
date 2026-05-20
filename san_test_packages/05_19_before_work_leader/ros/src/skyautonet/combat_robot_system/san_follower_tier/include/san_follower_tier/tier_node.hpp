// SAN v1.5 — TierNode (Follower-side).
//
// Wraps TierFsm in a rclcpp::Node:
//   * Subscribe FollowerTargetMessage (P0 from formation_node)
//   * Subscribe RobotStatus (own pose)
//   * Subscribe SlotAssignment (target slot)
//   * Subscribe ~/obstacle_on_path (Bool, populated by cost-map check
//                                    — see PDR-5 for full integration)
//   * 100ms tick: compute TierInput → step FSM → publish event
//
// On each transition, publish TierStatusChange to ~/tier_status_change
// for operator UI + post-mission analysis (KPP-2 evidence).
//
// 권원:
//   * SDD-SWARM v1.5 §6.2 (5-Tier FSM)
//   * IDS-CMD v1.5 §4.5

#ifndef SAN_FOLLOWER_TIER__TIER_NODE_HPP_
#define SAN_FOLLOWER_TIER__TIER_NODE_HPP_

#include <memory>
#include <mutex>
#include <optional>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>

#include <combat_robot_msgs/msg/follower_target_message.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/slot_assignment.hpp>
#include <combat_robot_msgs/msg/tier_status_change.hpp>

#include "san_follower_tier/tier_fsm.hpp"

namespace san_follower_tier {

class TierNode : public rclcpp::Node {
public:
  explicit TierNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());

private:
  void declareParameters();
  void loadParameters();

  // Subscriptions
  void onFollowerTarget(
      const combat_robot_msgs::msg::FollowerTargetMessage::SharedPtr msg);
  void onRobotStatus(
      const combat_robot_msgs::msg::RobotStatus::SharedPtr msg);
  void onSlotAssignment(
      const combat_robot_msgs::msg::SlotAssignment::SharedPtr msg);
  void onObstacleOnPath(const std_msgs::msg::Bool::SharedPtr msg);

  // 100ms tick
  void onTick();
  void publishTransition(Tier prev, Tier curr,
                          const std::string& reason,
                          const TierInput&   in);

  // Config
  uint32_t  robot_id_{1};
  uint32_t  tick_period_ms_{100};
  TierConfig fsm_cfg_;

  // Internal state
  std::mutex                       state_mu_;
  std::unique_ptr<TierFsm>         fsm_;
  uint64_t                         last_target_ms_{0};
  bool                             obstacle_on_path_{false};
  float                            current_x_{0.0f};
  float                            current_y_{0.0f};
  float                            target_x_{0.0f};
  float                            target_y_{0.0f};
  float                            base_distance_d0_m_{5.0f};

  // ROS interfaces
  rclcpp::Subscription<combat_robot_msgs::msg::FollowerTargetMessage>::SharedPtr target_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr           status_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::SlotAssignment>::SharedPtr        slot_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr                           obstacle_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::TierStatusChange>::SharedPtr         tier_pub_;
  rclcpp::TimerBase::SharedPtr                                                   tick_timer_;
};

}  // namespace san_follower_tier

#endif  // SAN_FOLLOWER_TIER__TIER_NODE_HPP_
