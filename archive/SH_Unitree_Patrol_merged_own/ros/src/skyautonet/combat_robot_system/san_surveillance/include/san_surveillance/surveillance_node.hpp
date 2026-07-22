// SAN v1.5 — SurveillanceNode (Hub-side).
//
// Responsibilities (SDD-SWARM §8 + SDD-SUR §3):
//   1. Subscribe RobotStatus (all robots) — alive tracking
//   2. Subscribe ThreatAlert — threat bearing for focus mode
//   3. Subscribe MissionStateCommand — mode change (Recon/Defence/Assault)
//   4. Compute sector assignments via sector_allocator
//   5. Publish per-robot SurveillanceSectorAssignment (10s + event)
//   6. Optionally publish initial PanTiltCommand to enter Sweep mode
//
// 권원:
//   * SDD-SWARM v1.5 §8 (360° 감시 영역 분배)
//   * SDD-SUR v1.5 §3 (감시 영역 분배 정책)
//   * IDS-CMD v1.5 §5.10, §5.11

#ifndef SAN_SURVEILLANCE__SURVEILLANCE_NODE_HPP_
#define SAN_SURVEILLANCE__SURVEILLANCE_NODE_HPP_

#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/pan_tilt_command.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/surveillance_sector_assignment.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>

#include "san_surveillance/sector_allocator.hpp"

namespace san_surveillance {

class SurveillanceNode : public rclcpp::Node {
public:
  explicit SurveillanceNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());

private:
  void declareParameters();
  void loadParameters();

  void onRobotStatus(
      const combat_robot_msgs::msg::RobotStatus::SharedPtr msg);
  void onThreatAlert(
      const combat_robot_msgs::msg::ThreatAlert::SharedPtr msg);

  void onReallocateTick();         // 10 s periodic
  void reallocateNow();
  void publishAssignments(
      const std::vector<SectorAssignment>& assignments);

  // Map of robot_id → (RobotRole, alive flag, last_seen)
  struct RobotState {
    RobotRole role     = RobotRole::Follower;
    bool      alive    = true;
    uint64_t  last_seen_ms = 0;
  };
  std::mutex                       state_mu_;
  std::map<uint32_t, RobotState>   robots_;

  // Latest threat (decays after 10s)
  std::optional<float>            threat_bearing_deg_;
  uint64_t                        threat_timestamp_ms_{0};

  // Active surveillance mode (mapped from MissionStateCommand)
  SurveillanceMode current_mode_{SurveillanceMode::Recon};

  // Publishers / Subscribers / Timers
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr robot_status_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::ThreatAlert>::SharedPtr threat_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::SurveillanceSectorAssignment>::SharedPtr sector_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::PanTiltCommand>::SharedPtr pantilt_pub_;
  rclcpp::TimerBase::SharedPtr     realloc_timer_;

  // Params
  int   realloc_period_sec_  = 10;
  int   robot_timeout_sec_   = 3;
  int   threat_validity_sec_ = 10;
  uint32_t sequence_counter_ = 0;
};

}  // namespace san_surveillance

#endif  // SAN_SURVEILLANCE__SURVEILLANCE_NODE_HPP_
