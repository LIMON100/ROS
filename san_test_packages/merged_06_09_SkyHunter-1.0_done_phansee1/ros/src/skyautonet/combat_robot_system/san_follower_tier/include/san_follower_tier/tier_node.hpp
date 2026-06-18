// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — TierNode (Follower-side).
//
// Wraps TierFsm in a rclcpp::Node:
//   * Subscribe FollowerTargetMessage  (from formation_node)
//   * Subscribe RobotStatus           (own pose)
//   * Subscribe SlotAssignment        (target slot)
//   * Subscribe ~/obstacle_on_path    (Bool from costmap)
//   * Subscribe ~/comm_link_status    (Bool, ★ PATCH 2026-05-13)
//   * Subscribe ~/breadcrumb          (Bool, ★ PATCH 2026-05-13)
//   * 100ms tick: compute TierInput → step FSM → publish event
//
// PATCH 2026-05-13 (deep-dive review):
//   * comm_link_alive + breadcrumb_available now wired to REAL topics
//     via CommHealth tracker (was hardcoded true).
//   * Tick handler measures actual elapsed dt (was using nominal 100ms).
//   * FSM call moved INSIDE state_mu_ scope so the node is safe under
//     MultiThreadedExecutor (was outside → race).
//   * KPP-2 latency probe: time from obstacle msg arrival to T1.5
//     transition publish is recorded in TierStatusChange.transition_latency_ms.
//   * prediction_received threshold is now derived from tick_period_ms
//     (was hardcoded 500ms regardless of tick).
//
// 권원:
//   * SDD-SWARM v1.5 §6.2 (6-Tier FSM)
//   * IDS-CMD v1.5 §4.5

#ifndef SAN_FOLLOWER_TIER__TIER_NODE_HPP_
#define SAN_FOLLOWER_TIER__TIER_NODE_HPP_

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>

#include <combat_robot_msgs/msg/follower_target_message.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/slot_assignment.hpp>
#include <combat_robot_msgs/msg/tier_status_change.hpp>

#include "san_follower_tier/comm_health.hpp"
#include "san_follower_tier/tier_fsm.hpp"

namespace san_follower_tier
{

class TierNode : public rclcpp::Node
{
public:
  explicit TierNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

  // ─── Test accessors ────────────────────────────────────
  Tier currentTierForTest() const;

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
  void onCommLinkStatus(const std_msgs::msg::Bool::SharedPtr msg);
  void onBreadcrumb(const std_msgs::msg::Bool::SharedPtr msg);

  // 100ms tick (actually re-measured per call)
  void onTick();

  /// Snapshot of work to publish — produced under lock, published after release.
  struct PublishSnapshot
  {
    bool emit = false;
    Tier previous;
    Tier current;
    std::string reason;
    TierInput input;
    std::optional<uint32_t> obstacle_trigger_latency_ms;
  };
  void publishTransition(const PublishSnapshot & snap);

  // Config
  uint32_t robot_id_{1};
  uint32_t tick_period_ms_{100};
  TierConfig fsm_cfg_;

  // Internal state — state_mu_ protects ALL of the following.
  mutable std::mutex state_mu_;
  std::unique_ptr<TierFsm> fsm_;
  uint64_t last_target_ms_{0};
  bool obstacle_on_path_{false};
  float current_x_{0.0f};
  float current_y_{0.0f};
  float target_x_{0.0f};
  float target_y_{0.0f};
  float base_distance_d0_m_{5.0f};

  // PATCH 2026-05-13: real elapsed dt tracking.
  std::optional<std::chrono::steady_clock::time_point> last_tick_;

  // PATCH 2026-05-13: comm + breadcrumb wired to real topics.
  CommHealth comm_health_;

  // ROS interfaces
  rclcpp::Subscription<combat_robot_msgs::msg::FollowerTargetMessage>::SharedPtr target_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr status_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::SlotAssignment>::SharedPtr slot_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr obstacle_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr comm_link_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr breadcrumb_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::TierStatusChange>::SharedPtr tier_pub_;
  rclcpp::TimerBase::SharedPtr tick_timer_;
};

}  // namespace san_follower_tier

#endif  // SAN_FOLLOWER_TIER__TIER_NODE_HPP_
