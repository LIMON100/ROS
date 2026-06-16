// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — SurveillanceNode (Hub-side).
//
// PATCH 2026-05-13 (deep-dive review):
//   * Tracks each robot's world yaw + velocity so the allocator can
//     emit world-frame sectors while driving (사업 §3 운용 개념).
//   * DriveClassifier hysteresis chooses Heading vs World frame.
//   * Threat alert path no longer deadlocks (recursive_mutex + lock-
//     guard scope split).
//   * ThreatAlert bearing actually extracted (from msg.position).
//   * PanTiltCommand always emitted in HEADING frame (body-mounted
//     pan-tilt hardware) — node transforms world→heading using the
//     latest known yaw per-robot.
//
// 권원:
//   * SDD-SWARM v1.5 §8 (360° 감시 영역 분배)
//   * SDD-SUR v1.5 §3 (감시 영역 분배 정책)
//   * IDS-CMD v1.5 §5.10, §5.11
//   * 사업수요신청서 §3 운용 개념 나 (방어모드 360° + 공격모드 주행 중)

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
#include "san_surveillance/sector_frame.hpp"

namespace san_surveillance
{

class SurveillanceNode : public rclcpp::Node
{
public:
  explicit SurveillanceNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

  // ─── Test accessors ─────────────────────────────────────
  SectorFrame      currentFrameForTest() const;
  std::size_t      robotCountForTest()  const;
  DriveClassifier::State driveStateForTest() const;

private:
  void declareParameters();
  void loadParameters();

  void onRobotStatus(
    const combat_robot_msgs::msg::RobotStatus::SharedPtr msg);
  void onThreatAlert(
    const combat_robot_msgs::msg::ThreatAlert::SharedPtr msg);

  void onReallocateTick();         // periodic
  void reallocateNow();

  // Build/publish snapshot helpers — see implementation for the
  // "build under lock, publish after release" pattern.
  struct PublishSnapshot
  {
    std::vector<SectorAssignment> assignments;
    // Map robot_id → yaw_world_deg (for World→Heading transform).
    std::map<uint32_t, float> yaw_by_id;
    uint64_t now_ms = 0;
    SectorFrame frame = SectorFrame::Heading;
    // Latest threat elevation (horizon-frame degrees) if fresh, else
    // nullopt — used to set PanTiltCommand.target_tilt_deg in Track mode.
    std::optional<float> threat_elevation_deg;
  };
  void publishAssignments(const PublishSnapshot & snap);

  // ─── State ───────────────────────────────────────────────
  // Per-robot tracking — pose, velocity, role, alive flag.
  struct RobotState
  {
    RobotRole role = RobotRole::Follower;
    bool alive = true;
    uint64_t last_seen_ms = 0;
    float yaw_world_deg = 0.0f;
    float linear_speed_mps = 0.0f;
    float angular_speed_dps = 0.0f;
  };

  // PATCH 2026-05-13: recursive_mutex eliminates the deadlock that
  // existed when onThreatAlert (held lock) called reallocateNow
  // (also took the same lock).
  mutable std::recursive_mutex state_mu_;
  std::map<uint32_t, RobotState> robots_;

  // Latest threat (decays after threat_validity_sec_)
  // std::optional<float> threat_bearing_world_deg_;             // PATCH: now WORLD frame
  // std::optional<float> threat_elevation_deg_;                 // horizon frame (msg.elevation_deg)
  // uint64_t threat_timestamp_ms_{0};

  struct ActiveThreat { float bearing_world_deg; uint64_t ts_ms; };
  std::vector<ActiveThreat> threats_;
  std::optional<float> threat_elevation_deg_;

  // Drive vs Patrol classifier — drives the choice of sector frame.
  DriveClassifier drive_classifier_;
  SectorFrame current_frame_{SectorFrame::Heading};

  // Active surveillance mode (mapped from MissionStateCommand).
  SurveillanceMode current_mode_{SurveillanceMode::Recon};

  // Publishers / Subscribers / Timers
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr robot_status_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::ThreatAlert>::SharedPtr threat_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::SurveillanceSectorAssignment>::SharedPtr sector_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::PanTiltCommand>::SharedPtr pantilt_pub_;
  rclcpp::TimerBase::SharedPtr realloc_timer_;

  // Params
  int realloc_period_sec_ = 10;
  int robot_timeout_sec_ = 3;
  int threat_validity_sec_ = 10;
  uint32_t leader_robot_id_ = 1;        // anchors world-frame sectors
  uint32_t sequence_counter_ = 0;
};

}  // namespace san_surveillance

#endif  // SAN_SURVEILLANCE__SURVEILLANCE_NODE_HPP_
