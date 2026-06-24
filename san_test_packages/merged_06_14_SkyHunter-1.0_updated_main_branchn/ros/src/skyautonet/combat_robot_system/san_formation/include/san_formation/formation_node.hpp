// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — FormationNode (Hub-side or Leader-side).
//
// Responsibilities:
//   1. Subscribe FormationCommand (operator tablet) — formation change request
//   2. Subscribe RobotStatus (all robots) — current poses
//   3. Compute slot positions via Formations module
//   4. Assign robots to slots via Hungarian (with FRAME-CORRECT cost matrix)
//   5. Publish:
//        ~/slot_assignment  (P1 reliable) — assignment epoch + slots
//        ~/follower_target  (P0 reliable, 10 Hz) — per-robot target
//        ~/formation_status (P1 reliable, 1 Hz) — telemetry
//
// Re-plans on formation change OR when avg alignment error > threshold.
//
// PATCH 2026-05-13 (deep-dive fixes):
//   * leader_robot_id_ is now respected — leader anchors the slot frame
//   * Per-robot VelocityEstimator runs continuously → leader velocity
//     populated and used by 1-second slot prediction (SDD §6.3, PDR-3)
//   * Cost matrix uses proper frame transform (leader-local → world)
//   * v1.5 9-formation IDs supported directly (was clamped to v1.3's 0..4)
//   * Publishers invoked OUTSIDE the FSM lock (snapshot under lock,
//     publish after release)
//   * Hungarian failure surfaces in FormationStatus.replan_failed
//
// 권원:
//   * SDD-SWARM v1.5 §7  (9 Formations + Hungarian)
//   * SDD-SWARM v1.5 §6.3 (T0 PREDICTIVE_TRACK, 1-second prediction)
//   * IDS-CMD v1.5 §4.2, §5.1, §5.9

#ifndef SAN_FORMATION__FORMATION_NODE_HPP_
#define SAN_FORMATION__FORMATION_NODE_HPP_

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include <std_msgs/msg/bool.hpp>
#include <combat_robot_msgs/msg/follower_target_message.hpp>
#include <combat_robot_msgs/msg/formation_command.hpp>
#include <combat_robot_msgs/msg/formation_status.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/slot_assignment.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>

#include "san_formation/encircle_combat.hpp"
#include "san_formation/formations.hpp"
#include "san_formation/formation_planner.hpp"

namespace san_formation
{

class FormationNode : public rclcpp::Node
{
public:
  explicit FormationNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

  // ─── Test accessors ────────────────────────────────────
  uint32_t currentEpochForTest() const;
  std::size_t robotCountForTest() const;

private:
  void declareParameters();
  void loadParameters();

  // Subscriptions
  void onFormationCommand(
    const combat_robot_msgs::msg::FormationCommand::SharedPtr msg);
  void onRobotStatus(
    const combat_robot_msgs::msg::RobotStatus::SharedPtr msg);
  // DCN-2026-026 C-2 — Encircle combat trigger inputs.
  void onThreatAlert(
    const combat_robot_msgs::msg::ThreatAlert::SharedPtr msg);
  void onEncircleConfirm(const std_msgs::msg::Bool::SharedPtr msg);

  // Periodic
  void onAssignTick();         // re-plan if needed
  void onFollowerTargetTick();  // 10 Hz target broadcast
  void onStatusTick();         // 1 Hz formation status

  // Internal — return decoupled outputs so we can release the lock
  // BEFORE publishing (PATCH 2026-05-13: listener-outside-lock pattern).
  struct PlanResult
  {
    bool success = false;
    combat_robot_msgs::msg::SlotAssignment assignment_msg;
    std::string error_reason;
  };
  PlanResult recomputeAssignmentLocked();   // caller holds lock; returns snapshot
  // DCN-2026-026 C-2 — slot-frame anchor: the encircle threat anchor
  // while combat is Active, the leader pose otherwise. Unifies the
  // KPP-1 error check, assignment msg, targets and status on ONE
  // anchor so combat metrics aren't computed against the leader.
  PoseXY slotAnchorLocked() const;
  combat_robot_msgs::msg::SlotAssignment buildAssignmentMsgLocked() const;
  std::vector<combat_robot_msgs::msg::FollowerTargetMessage>
  buildFollowerTargetsLocked();
  combat_robot_msgs::msg::FormationStatus buildStatusMsgLocked() const;

  // ─── State ─────────────────────────────────────────────────────
  // Current desired formation
  Formation current_formation_{Formation::VShape};
  uint8_t current_preset_{2};              // recon_defence default
  float spacing_d_m_{5.0f};
  float spread_theta_deg_{90.0f};
  uint8_t phase_{0};                       // assembly default

  // Robot snapshots — pose + filtered velocity (PATCH 2026-05-13).
  struct PoseSnap
  {
    PoseXY pose;
    VelocityEstimator vel_est{0.3f};
    uint64_t timestamp_ms{0};
  };
  mutable std::mutex state_mu_;
  std::map<uint32_t, PoseSnap> robot_poses_;

  // Leader is the robot that anchors the slot frame.
  uint32_t leader_robot_id_{1};

  // Latest assignment
  struct AssignedSlot
  {
    uint32_t robot_id;
    uint8_t slot_index;
    float slot_local_x;        // ★ PATCH: keep in LEADER LOCAL frame
    float slot_local_y;        //         so we can transform per-tick
  };
  std::vector<AssignedSlot> current_assignment_;
  uint32_t current_epoch_{0};
  float last_total_cost_{0.0f};
  uint64_t last_reassignment_ms_{0};
  bool last_plan_failed_{false};                             // ★ PATCH

  // DCN-2026-026 C-2 — Encircle combat state (pure logic).
  EncircleCombat encircle_;
  float encircle_radius_m_{7.0f};

  // Publishers / Subscribers / Timers
  rclcpp::Subscription<combat_robot_msgs::msg::FormationCommand>::SharedPtr formation_cmd_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::ThreatAlert>::SharedPtr threat_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr encircle_confirm_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr robot_status_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::SlotAssignment>::SharedPtr slot_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FollowerTargetMessage>::SharedPtr target_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FormationStatus>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr assign_timer_;
  rclcpp::TimerBase::SharedPtr target_timer_;
  rclcpp::TimerBase::SharedPtr status_timer_;

  // Params
  float realign_threshold_m_{2.0f};         // KPP-1: re-plan if avg err > 2m
  float max_speed_recon_{1.3f};
  float lead_bias_s_{0.1f};
  float prediction_horizon_s_{1.0f};        // ★ PATCH: SDD §6.3
};

}  // namespace san_formation

#endif  // SAN_FORMATION__FORMATION_NODE_HPP_
