// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-013 swarm_monitor_node.
//
// Aggregates per-robot poses (queried over TF: map → robot_<id>/base_link)
// and republishes them as a single geometry_msgs/PoseArray on /swarm/poses
// at 10 Hz, for downstream consumers (operator overlay, formation node,
// Reynolds Boids on followers, audit aggregation on Hub).
//
// Hub-only PUBLISHER gate (DCN-2026-013 + ADR-008 Tier 1)
// -------------------------------------------------------
// The monitor node is instantiated on EVERY robot role so that the role
// can be promoted later (Deputy → Hub on takeover, Follower → Limp Mode
// supervisor) without re-spawning processes. However, the PUBLISHER is
// only created when robot_role ∈ {"hub", "leader"} — Followers / Deputy
// remain subscribe-only so that the squadron's /swarm/poses topic has
// exactly ONE active publisher at any moment. Two consequences:
//
//   1. Race-condition avoidance — concurrent publishers on the same topic
//      from multiple robots produce interleaved PoseArrays, breaking any
//      consumer that assumes monotonic ordering. DCN-2026-006 EXT D-024
//      P0 hotfix applied the same gate to san_role_management's audit
//      publisher; this DCN extends it to swarm_coordinator.
//
//   2. DDS bandwidth saving — 4 redundant publishers × 10 Hz × ~250 B
//      per PoseArray ≈ 10 KB/s saved per follower per second on a
//      mesh-constrained EasyMesh transport.
//
// References:
//   * DCN-2026-013 (this DCN) — Hub-only /swarm/poses publisher gate.
//   * DCN-2026-006 EXT D-024 — precedent (san_role_management audit gate).
//   * ADR-008 — Tier 1 (realtime swarm coordination, C++ mandatory).
//   * SDD-SWARM v1.5 §6.3 (formation slot assignment consumers).
//
// ─── Dual-channel design (Phase-7 audit F3 clarification) ───────────
// The squadron exposes TWO orthogonal swarm-pose distribution paths:
//
//   1. /swarm/robot_status (combat_robot_msgs/RobotStatus)
//      Per-robot publish (5 Hz best-effort), N publishers / N
//      subscribers. Carries pose + battery + role + health in one
//      compact message. Consumed by:
//        * san_formation::formation_node (slot assignment input)
//        * san_role_management::leader_role_manager (succession)
//        * san_role_management::hub_role_manager (audit aggregation)
//        * san_role_management::limp_mode_manager (degradation)
//        * san_hub_orchestrator::hub_orchestrator_node (aggregator)
//
//   2. /swarm/poses (geometry_msgs/PoseArray) — THIS NODE
//      Hub/Leader-aggregated publish (10 Hz reliable), 1 publisher /
//      N subscribers. Carries ONLY positions (no role/battery/health).
//      Currently no PRODUCTION consumer — provisioned as forward-
//      compatible infrastructure for:
//        * Operator UI map overlay (Aban Android — see DCN-2026-021
//          Aban_Android_rosbridge_schema_v2.md §2.1)
//        * Future Reynolds Boids / flocking layer that wants poses
//          without RobotStatus overhead.
//
// The two channels are deliberately independent: removing /swarm/poses
// would NOT break formation/role-management. Removing /swarm/robot_status
// WOULD (it is the operational pose channel). The Hub-only gate on
// /swarm/poses is PREVENTIVE — it prevents N-way redundant publish
// the day consumers DO subscribe. Without consumers today the gate
// has no observable runtime effect, but the design is mandatory for
// race-free composition (DCN-2026-006 EXT D-024 pattern).

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include "swarm_coordinator/swarm_coordinator.hpp"   // MAX_ROBOTS

namespace swarm_coordinator
{

class SwarmMonitorNode : public rclcpp::Node
{
public:
  SwarmMonitorNode();
  explicit SwarmMonitorNode(const rclcpp::NodeOptions & options);

  // DCN-2026-013: Hub/Leader-only publisher gate.
  // Pattern matches DCN-2026-006 EXT D-024 P0 hotfix.
  // The node is instantiated on every robot to allow promotion (Limp
  // Mode → Deputy → Hub) without re-spawning processes, but PUBLISHER
  // creation is restricted to active Hub or Leader.
  bool isPublisherEnabled() const;

  // For testing visibility (gtest accessor) — lets the gate test
  // verify outcome without spinning the executor or constructing a
  // mock TF graph.
  bool publisherExistsForTest() const {return poses_pub_ != nullptr;}

private:
  void updateAndPublish();

  // Optional — nullptr when robot_role is follower/deputy (Hub-only
  // gate, DCN-2026-013).
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr poses_pub_;
  rclcpp::TimerBase::SharedPtr update_timer_;

  // TF infrastructure (always created — non-hub roles still need
  // local pose awareness for downstream consumers).
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

}  // namespace swarm_coordinator
