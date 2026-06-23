// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.1 (DCN-2026-004 D-006) — Swarm aggregator (pure logic).
//
// Computes fleet-level statistics from per-robot status updates.
// Used by HubOrchestratorNode to publish FleetStatus at ~1 Hz.
//
// Pure C++17, no ROS — fully standalone testable.
//
// ─── Thread-safety policy (v1.5.1 / DCN-2026-004 C-6 fix) ────────────
//
// Originally (v1.5 PDR-prep) this class was non-thread-safe and relied
// on the assumption that HubOrchestratorNode runs under a single-
// threaded executor (rclcpp::spin). That assumption is fragile:
//
//   * Phase 7 (#129) introduced lifecycle clients that may later need
//     their own callback group to avoid the deadlock pattern resolved
//     in DCN-2026-003 D-005.
//   * Future Limp-mode rework may migrate HubOrchestratorNode onto a
//     MultiThreadedExecutor for parallel telemetry handling.
//
// To prevent latent bugs (race on robots_ unordered_map between
// update() inserting/erasing buckets and aggregate() iterating), this
// class now guards every member function with an internal mutex.
// Overhead is negligible at the 1 Hz publish rate.

#ifndef SAN_HUB_ORCHESTRATOR__SWARM_AGGREGATOR_HPP_
#define SAN_HUB_ORCHESTRATOR__SWARM_AGGREGATOR_HPP_

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace san_hub_orchestrator
{

/// Per-robot status snapshot fed by HubOrchestratorNode subscriber.
struct RobotSnapshot
{
  std::string robot_id;            // e.g. "robot_3"
  uint64_t last_heartbeat_ms;      // wall-time at last RobotStatus
  bool in_limp_mode;
  float battery_percent;
  float mission_progress_percent;
  // RTK fix: 0=none, 1=auto2d/dgps, 2=rtk_float, 3=rtk_fix
  uint8_t rtk_fix_grade;
  uint16_t active_threats;
};

struct FleetSnapshot
{
  uint8_t total_robots = 0;
  uint8_t healthy_robots = 0;
  uint8_t limp_mode_robots = 0;
  uint8_t disconnected_robots = 0;
  float min_battery_percent = 100.0f;
  float mean_battery_percent = 0.0f;
  float mission_progress_percent = 0.0f;
  uint8_t robots_with_rtk_fix = 0;
  uint8_t robots_with_rtk_float = 0;
  uint8_t robots_with_no_fix = 0;
  uint16_t active_threats_count = 0;
};

class SwarmAggregator
{
public:
  /// Threshold past which a robot is considered "disconnected".
  /// Default 5 s — RobotStatus is published at 1 Hz.
  explicit SwarmAggregator(uint64_t disconnect_threshold_ms = 5000)
  : disconnect_threshold_ms_(disconnect_threshold_ms) {}

  /// Insert or update a robot's snapshot.
  void update(const RobotSnapshot & s);

  /// Compute fleet-level aggregation given a current wall-time `now_ms`.
  /// Robots whose `last_heartbeat_ms` is older than `now_ms -
  /// disconnect_threshold_ms_` are counted as disconnected (excluded
  /// from healthy / battery aggregation).
  FleetSnapshot aggregate(uint64_t now_ms) const;

  /// Remove all tracked robots.
  void clear()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    robots_.clear();
  }

  /// Update the disconnect threshold after construction. Needed because
  /// HubOrchestratorNode constructs the aggregator with a default, then
  /// learns the real threshold from rclcpp parameters in its body — the
  /// internal std::mutex (C-6 fix) made reassigning the whole object
  /// impossible.
  void setDisconnectThreshold(uint64_t ms)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    disconnect_threshold_ms_ = ms;
  }

  /// Introspection
  size_t trackedRobotCount() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return robots_.size();
  }

private:
  uint64_t disconnect_threshold_ms_;
  // [v1.5.1 C-6 fix] mutable so const members (aggregate, trackedRobotCount)
  // can lock. All public methods serialize on this mutex.
  mutable std::mutex mutex_;
  std::unordered_map<std::string, RobotSnapshot> robots_;
};

}  // namespace san_hub_orchestrator

#endif  // SAN_HUB_ORCHESTRATOR__SWARM_AGGREGATOR_HPP_
