// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 0 — swarm topology + ID-mapping constants.
//
// References:
//   * SAN-SDD-SWARM-001 v1.5 §3 + §5.6 (4-tier Leader succession)
//   * SAN-IDS-CMD-001 v1.5 §5.15 / §5.16 (Leader/Hub role announcements)
//   * DCN-2026-001 D-001 (8-robot max / 4-robot min squadron)
//
// v1.4 added DEPUTY_ROBOT_ID (S3) ahead of HUB_ROBOT_ID in the
// Leader-succession chain. v1.5 retains the v1.4 topology unchanged but
// re-points the spec references to v1.5 and expands the LTE backup chain
// (handled in san_lte_redundancy package).

#pragma once

#include <cstdint>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>

#include "swarm_coordinator/hub_health_monitor.hpp"

namespace swarm_coordinator
{

// ----- v1.3 / v1.4 robot ID constants -----
constexpr uint32_t LEADER_ROBOT_ID = 1;    // S1 = Unitree Go2 Leader
constexpr uint32_t HUB_ROBOT_ID = 2;       // S2 = Hub UGV
constexpr uint32_t DEPUTY_ROBOT_ID = 3;    // S3 = Deputy UGV (v1.4)
constexpr uint32_t MAX_ROBOTS = 8;         // S1..S8
constexpr uint32_t MIN_ROBOTS = 4;         // Leader + Hub + Deputy + 1 follower (v1.4)

// ----- v1.3 legacy deputy chain (kept for backward compatibility) -----
// Used by v1.3-era code that only knew about Hub-based LTE backup.
// v1.4 callers should use DEFAULT_LEADER_SUCCESSION_CHAIN instead.
inline const std::vector<int32_t> DEFAULT_DEPUTY_CHAIN = {
  2, 3, 4, 5, 6, 7, 8,
};

// ----- v1.4 4-tier Leader succession chain -----
//   [0] = Deputy UGV (1st priority, is_deputy_ugv=true)
//   [1] = Hub UGV (2nd priority)
//   [2] = -1 sentinel — runtime picks battery-max follower
//   [3] = -2 sentinel — Limp Mode entry (no eligible candidate)
inline const std::vector<int32_t> DEFAULT_LEADER_SUCCESSION_CHAIN = {
  static_cast<int32_t>(DEPUTY_ROBOT_ID),     // 1st
  static_cast<int32_t>(HUB_ROBOT_ID),        // 2nd
  -1,                                         // 3rd: battery-max follower
  -2,                                         // 4th: Limp Mode entry
};

class SwarmCoordinator : public rclcpp::Node
{
public:
  SwarmCoordinator();
  explicit SwarmCoordinator(const rclcpp::NodeOptions & options);

  // Test accessors.
  uint32_t maxRobots() const {return MAX_ROBOTS;}
  uint32_t minRobots() const {return MIN_ROBOTS;}
  uint32_t leaderRobotId() const {return LEADER_ROBOT_ID;}
  uint32_t hubRobotId() const {return HUB_ROBOT_ID;}
  uint32_t deputyRobotId() const {return DEPUTY_ROBOT_ID;}
  const std::vector<int32_t> & successionChain() const
  {
    return DEFAULT_LEADER_SUCCESSION_CHAIN;
  }

  // v1.3 PHASE 4: expose Hub UGV dual-SBC health for downstream
  // decision (deputy chain inclusion, operator banner).
  HubHealthMonitor & hubHealth() {return hub_health_;}
  const HubHealthMonitor & hubHealth() const {return hub_health_;}

private:
  using RobotStatus = combat_robot_msgs::msg::RobotStatus;
  rclcpp::Subscription<RobotStatus>::SharedPtr status_sub_;
  HubHealthMonitor hub_health_{HUB_ROBOT_ID};

  void onRobotStatus(RobotStatus::SharedPtr msg);
  uint64_t nowMs() const;
};

}  // namespace swarm_coordinator
