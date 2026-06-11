// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 4 — Hub UGV dual-SBC health monitor.
//
// Subscribes to /swarm/robot_status, tracks the most recent
// sbc1_healthy / sbc2_healthy flags from the Hub UGV (robot_id == 2),
// and exposes:
//   * hubExcludedFromLeaderChain() — true when BOTH SBCs are down
//   * isHubSlamSbcAvailable() / isHubCommSbcAvailable() — per-SBC flags
//
// Test seam: injectStatusForTest() so unit tests can drive the three
// failure scenarios from SAN-TST-INT-001 v1.3 §S15-3:
//   Case A — SBC #1 down only (SLAM 정전, comm OK)
//   Case B — SBC #2 down only (LTE backup 활성, SLAM OK)
//   Case C — 양쪽 SBC down (Hub UGV 전체 손상; deputy_chain 에서 제외)
//
// Phase 5 (medium tier) fixes:
//   - isFresh() treated future timestamps as fresh — clock-skewed or
//     bag-replayed heartbeat could keep Hub "fresh" forever and
//     prevent Deputy promotion. Now rejected.
//   - SBC flags were updated directly from each RobotStatus, so a
//     producer flapping at 10 Hz would oscillate the monitor's
//     classification 10 times per second. Now we count consecutive
//     bad/good samples; only N-in-a-row flips the public bool.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>

namespace swarm_coordinator
{

enum class HubHealthCase : uint8_t
{
  NORMAL     = 0,     // both SBCs healthy
  CASE_A     = 1,     // SBC #1 down (SLAM 정전)
  CASE_B     = 2,     // SBC #2 down (LTE backup needed)
  CASE_C     = 3,     // 양쪽 down (excluded from leader chain)
  UNKNOWN    = 4,     // no recent heartbeat
};

/// Phase 5: hysteresis policy. Public flag flips after `bad_threshold`
/// consecutive bad samples (good→bad), and only flips back after
/// `good_threshold` consecutive good samples (bad→good). For
/// production, use FlappingPolicy{3, 5} — bias toward conservative
/// (assume unhealthy unless clearly recovered, 3-tick latency on
/// failure detection, 5-tick latency on recovery).
///
/// Default values {1, 1} preserve pre-patch immediate-response
/// behavior so existing tests + bringup paths are unchanged. Tune
/// `bad_threshold` / `good_threshold` at construction.
struct FlappingPolicy
{
  uint32_t bad_threshold = 1;
  uint32_t good_threshold = 1;
};

/// Maximum tolerated clock skew (ms). Heartbeats stamped more than
/// this far in the future are rejected as stale (clock-skew / bag
/// replay / spoofing defense).
inline constexpr int64_t kHubMaxSkewMs = 500;

class HubHealthMonitor
{
public:
  explicit HubHealthMonitor(
    uint32_t hub_robot_id = 2,
    int stale_threshold_ms = 3000,
    FlappingPolicy policy = FlappingPolicy{});

  // Update from a RobotStatus broadcast.
  void update(
    const combat_robot_msgs::msg::RobotStatus & status,
    uint64_t now_ms);

  // Accessors.
  bool isHubSlamSbcAvailable(uint64_t now_ms) const;
  bool isHubCommSbcAvailable(uint64_t now_ms) const;
  bool hubExcludedFromLeaderChain(uint64_t now_ms) const;
  HubHealthCase classify(uint64_t now_ms) const;

  /// Returns true if any heartbeat has ever been observed.
  /// Distinguishes UNKNOWN-because-never-seen from
  /// UNKNOWN-because-stale.
  bool hasEverSeenHub() const;

  // Test entry point - inject without a real ROS subscription.
  void injectStatusForTest(bool sbc1, bool sbc2, uint64_t now_ms);

private:
  uint32_t hub_robot_id_;
  int stale_threshold_ms_;
  FlappingPolicy policy_;
  mutable std::mutex mutex_;
  bool sbc1_healthy_ = false;
  bool sbc2_healthy_ = false;
  // Phase 5: hysteresis counters — public flag flips only after
  // N consecutive same-direction samples.
  uint32_t sbc1_bad_count_ = 0;
  uint32_t sbc1_good_count_ = 0;
  uint32_t sbc2_bad_count_ = 0;
  uint32_t sbc2_good_count_ = 0;
  uint64_t last_heartbeat_ms_ = 0;
  bool ever_seen_ = false;

  bool isFresh(uint64_t now_ms) const;
  /// Apply hysteresis: returns the new public flag value after
  /// processing an incoming raw sample.
  bool applyHysteresis(
    bool current, bool incoming,
    uint32_t & bad_count, uint32_t & good_count) const;
};

}  // namespace swarm_coordinator
