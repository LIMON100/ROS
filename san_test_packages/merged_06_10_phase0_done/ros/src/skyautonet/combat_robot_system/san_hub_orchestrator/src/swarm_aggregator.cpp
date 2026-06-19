// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.1 (DCN-2026-004 D-006) — Swarm aggregator implementation.
// Thread-safety: every public method acquires `mutex_` (see header for
// rationale). Overhead is negligible at the 1 Hz publish rate.

#include "san_hub_orchestrator/swarm_aggregator.hpp"

#include <algorithm>

namespace san_hub_orchestrator
{

void SwarmAggregator::update(const RobotSnapshot & s)
{
  std::lock_guard<std::mutex> lock(mutex_);
  robots_[s.robot_id] = s;
}

FleetSnapshot SwarmAggregator::aggregate(uint64_t now_ms) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  FleetSnapshot out;
  out.total_robots = static_cast<uint8_t>(
    std::min<size_t>(robots_.size(), 255));

  if (robots_.empty()) {
    out.min_battery_percent = 0.0f;
    return out;
  }

  float sum_battery = 0.0f;
  float sum_mission_prog = 0.0f;
  uint16_t sum_threats = 0;
  uint8_t connected = 0;

  for (const auto & [id, r] : robots_) {
    // Disconnect check first
    const bool disconnected =
      (now_ms > r.last_heartbeat_ms) &&
      (now_ms - r.last_heartbeat_ms > disconnect_threshold_ms_);
    if (disconnected) {
      if (out.disconnected_robots < 255) {++out.disconnected_robots;}
      continue;
    }
    ++connected;

    if (r.in_limp_mode) {
      if (out.limp_mode_robots < 255) {++out.limp_mode_robots;}
    } else {
      if (out.healthy_robots < 255) {++out.healthy_robots;}
    }

    sum_battery += r.battery_percent;
    out.min_battery_percent =
      std::min(out.min_battery_percent, r.battery_percent);
    sum_mission_prog += r.mission_progress_percent;
    sum_threats += r.active_threats;

    switch (r.rtk_fix_grade) {
      case 3: if (out.robots_with_rtk_fix < 255) {++out.robots_with_rtk_fix;} break;
      case 2: if (out.robots_with_rtk_float < 255) {++out.robots_with_rtk_float;} break;
      default: if (out.robots_with_no_fix < 255) {++out.robots_with_no_fix;}
    }
  }

  if (connected > 0) {
    out.mean_battery_percent = sum_battery / connected;
    out.mission_progress_percent = sum_mission_prog / connected;
  } else {
    // No connected robots — min_battery_percent stays at 100 default;
    // reset to 0 to signal "no info"
    out.min_battery_percent = 0.0f;
  }
  out.active_threats_count = sum_threats;
  return out;
}

}  // namespace san_hub_orchestrator
