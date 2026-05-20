// SAN v1.5 — FormationNode (Hub-side or Leader-side).
//
// Responsibilities:
//   1. Subscribe FormationCommand (operator tablet) — formation change request
//   2. Subscribe RobotStatus (all robots) — current poses
//   3. Compute slot positions via Formations module
//   4. Assign robots to slots via Hungarian
//   5. Publish:
//        ~/slot_assignment (P1 reliable) — assignment epoch + slots
//        ~/follower_target (P0 reliable, 10 Hz) — per-robot target
//        ~/formation_status (P1 reliable, 1 Hz) — telemetry
//
// Re-plans on formation change OR when avg alignment error > threshold.
//
// 권원:
//   * SDD-SWARM v1.5 §7 (9 Formations + Hungarian)
//   * IDS-CMD v1.5 §4.2, §5.1, §5.9

#ifndef SAN_FORMATION__FORMATION_NODE_HPP_
#define SAN_FORMATION__FORMATION_NODE_HPP_

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/follower_target_message.hpp>
#include <combat_robot_msgs/msg/formation_command.hpp>
#include <combat_robot_msgs/msg/formation_status.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/slot_assignment.hpp>

#include "san_formation/formations.hpp"

namespace san_formation {

class FormationNode : public rclcpp::Node {
public:
  explicit FormationNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());

private:
  void declareParameters();
  void loadParameters();

  // Subscriptions
  void onFormationCommand(
      const combat_robot_msgs::msg::FormationCommand::SharedPtr msg);
  void onRobotStatus(
      const combat_robot_msgs::msg::RobotStatus::SharedPtr msg);

  // Periodic
  void onAssignTick();        // re-plan if needed
  void onFollowerTargetTick();// 10 Hz target broadcast
  void onStatusTick();        // 1 Hz formation status

  // Internal
  void recomputeAssignment();
  combat_robot_msgs::msg::SlotAssignment buildAssignmentMsg() const;
  void publishFollowerTargets();
  combat_robot_msgs::msg::FormationStatus buildStatusMsg() const;
  float computeAlignmentError(uint32_t robot_id) const;

  // ─── State ─────────────────────────────────────────────────────
  // Current desired formation
  Formation current_formation_{Formation::VShape};
  uint8_t   current_preset_{2};            // recon_defence default
  float     spacing_d_m_{5.0f};
  float     spread_theta_deg_{90.0f};
  uint8_t   phase_{0};                     // assembly default

  // Robot snapshots: robot_id → (pose, velocity, timestamp).
  // Velocity estimated by finite-difference from successive pose updates.
  // PDR-3: enables 1-second prediction for FollowerTargetMessage
  // (SDD §6.3 — T0 PREDICTIVE_TRACK input).
  struct PoseSnap {
    float    x{0.0f};
    float    y{0.0f};
    float    vx{0.0f};    // estimated linear vel (m/s)
    float    vy{0.0f};
    uint64_t timestamp_ms{0};
  };
  mutable std::mutex             state_mu_;
  std::map<uint32_t, PoseSnap>   robot_poses_;

  // Leader velocity tracking — used to extrapolate slot positions
  // 1 second forward (SDD §6.3).
  uint32_t leader_robot_id_{1};

  // Latest assignment
  struct AssignedSlot {
    uint32_t robot_id;
    uint8_t  slot_index;
    float    target_x;
    float    target_y;
  };
  std::vector<AssignedSlot>      current_assignment_;
  uint32_t                       current_epoch_{0};
  float                          last_total_cost_{0.0f};
  uint64_t                       last_reassignment_ms_{0};

  // Publishers / Subscribers / Timers
  rclcpp::Subscription<combat_robot_msgs::msg::FormationCommand>::SharedPtr formation_cmd_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr      robot_status_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::SlotAssignment>::SharedPtr      slot_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FollowerTargetMessage>::SharedPtr target_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FormationStatus>::SharedPtr     status_pub_;
  rclcpp::TimerBase::SharedPtr                                               assign_timer_;
  rclcpp::TimerBase::SharedPtr                                               target_timer_;
  rclcpp::TimerBase::SharedPtr                                               status_timer_;

  // Params
  float realign_threshold_m_{2.0f};         // KPP-1: re-plan if avg err > 2m
  float max_speed_recon_{1.3f};
  float lead_bias_s_{0.1f};
};

}  // namespace san_formation

#endif  // SAN_FORMATION__FORMATION_NODE_HPP_
