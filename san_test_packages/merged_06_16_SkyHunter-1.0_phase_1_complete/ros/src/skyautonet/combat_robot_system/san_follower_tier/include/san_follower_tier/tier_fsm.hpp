// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Follower Tier FSM per SDD-SWARM §6.2.
//
// 6-state FSM that determines a follower's tracking behaviour based on
// quality of received prediction, communication state, and distance
// error δ relative to formation spacing d₀.
//
// Priority of state selection (highest first):
//   1. obstacle_on_path        → T1.5 AUTO_REROUTE (★ KPP-2)
//   2. δ > 4 d₀ OR 60s comm    → T4  BREADCRUMB_RECOVERY
//   3. δ > 2 d₀                → T3  HARD_CATCH_UP
//   4. δ > 1.5 d₀              → T2  CATCH_UP
//   5. !prediction && comm OK  → T1  NORMAL
//   6. prediction received     → T0  PREDICTIVE_TRACK
//
// PATCH 2026-05-13 (deep-dive review):
//   * step() now takes an explicit dt_ms (C1) — caller passes real
//     elapsed time so dwell timers reflect reality, not nominal tick.
//   * Bidirectional hysteresis (M8) — upgrade thresholds use +h
//     margin, downgrade uses -h. Previously only downgrade was
//     protected.
//   * Anti-flap dwell extended to T2/T3/T4 (M6), not just T1.5.
//   * KPP-2 latency tracking — record steady_clock timestamp at the
//     moment obstacle_on_path becomes true so the node can measure
//     T0→T1.5 latency from external trigger to transition.
//
// Pure C++17 — no rclcpp, no ROS messages. Standalone testable.

#ifndef SAN_FOLLOWER_TIER__TIER_FSM_HPP_
#define SAN_FOLLOWER_TIER__TIER_FSM_HPP_

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace san_follower_tier
{

/// 6 tier states. Numeric codes match TierStatusChange.msg.
enum class Tier : uint8_t
{
  T0   = 0,     // PREDICTIVE_TRACK
  T1   = 1,     // NORMAL
  T1_5 = 2,     // AUTO_REROUTE  ★ KPP-2
  T2   = 3,     // CATCH_UP
  T3   = 4,     // HARD_CATCH_UP
  T4   = 5,     // BREADCRUMB_RECOVERY
};

/// Live inputs evaluated at each FSM step.
struct TierInput
{
  bool prediction_received{false};
  uint32_t prediction_loss_ms{0};      // since last prediction
  bool comm_link_alive{true};
  bool obstacle_on_path{false};        // T1.5 trigger
  float delta_m{0.0f};                  // |pose - target_slot|
  float base_distance_d0_m{5.0f};
  bool breadcrumb_available{true};
};

/// Configuration (defaults match SDD §6.2 thresholds).
struct TierConfig
{
  // δ-ratio thresholds (relative to base_distance_d0_m).
  float catch_up_ratio = 1.5f;
  float hard_catch_up_ratio = 2.0f;
  float breadcrumb_ratio = 4.0f;

  // Hysteresis — bidirectional support (★ PATCH 2026-05-13).
  //   Downgrade always uses -h margin (was the only mode before patch).
  //   Upgrade uses +h margin ONLY when upgrade_hysteresis_enabled.
  //   Default off so existing thresholds behave like v1.5.0.
  float hysteresis_ratio = 0.1f;
  bool upgrade_hysteresis_enabled = false;

  // Communication timeout for T4.
  uint32_t comm_timeout_ms = 60000;

  // Minimum dwell time in each upgraded tier before downgrading.
  // PATCH 2026-05-13: T2/T3/T4 dwells default to 0 (opt-in).
  // Set to non-zero in config to enable per-tier anti-flap.
  uint32_t auto_reroute_min_dwell_ms = 100;
  uint32_t catch_up_min_dwell_ms = 0;
  uint32_t hard_catch_up_min_dwell_ms = 0;
  uint32_t breadcrumb_min_dwell_ms = 0;
};

/// FSM. Thread-safety: NOT thread-safe; caller must serialise.
class TierFsm
{
public:
  explicit TierFsm(const TierConfig & cfg = TierConfig{});

  Tier currentTier() const {return tier_;}

  /// Evaluate input and (possibly) transition. Returns the NEW tier
  /// if a transition occurred; std::nullopt if no change.
  ///
  /// R-6 deep-dive + Phase 7 fix: dt_ms is now EXPLICIT (was
  /// hardcoded 100ms). Caller passes the real elapsed time since
  /// the previous step() so dwell counters track wall time
  /// accurately — KPP-2 latency measurement depends on this.
  std::optional<Tier> step(const TierInput & in, uint32_t dt_ms);

  /// Backward-compatible single-arg overload (assumes 100ms tick).
  std::optional<Tier> step(const TierInput & in)
  {
    return step(in, 100u);
  }

  /// Human-readable trigger for the most recent transition.
  const std::string & lastReason() const {return reason_;}

  /// Milliseconds the FSM has been in its current tier.
  uint32_t dwellMs() const {return dwell_ms_;}

  /// ★ KPP-2 latency probe.
  /// When obstacle_on_path goes from false→true the FSM stamps a
  /// steady_clock timepoint. When the FSM subsequently enters T1.5
  /// the node can read this back and compute the trigger-to-action
  /// latency for KPP-2 evidence.
  /// Returns nullopt if no obstacle trigger is pending or the
  /// transition has already been accounted for.
  std::optional<std::chrono::steady_clock::time_point>
  pendingObstacleTriggerStamp() const {return obstacle_trigger_stamp_;}

  /// Caller clears the trigger stamp after recording the latency
  /// (in publishTransition).
  void clearObstacleTriggerStamp() {obstacle_trigger_stamp_.reset();}

private:
  /// Compute the tier that input would select, honoring hysteresis.
  Tier evaluate(const TierInput & in) const;

  /// Returns true if dwell in current tier is sufficient to downgrade.
  bool canLeaveTier(Tier from, Tier to) const;

  TierConfig cfg_;
  Tier tier_{Tier::T1};          // safe default
  std::string reason_;
  uint32_t dwell_ms_{0};

  // Obstacle latency tracking (PATCH 2026-05-13).
  bool prev_obstacle_on_path_{false};
  std::optional<std::chrono::steady_clock::time_point>
  obstacle_trigger_stamp_;
};

/// String name for a tier (for logs / messages).
const char * tierName(Tier t);

}  // namespace san_follower_tier

#endif  // SAN_FOLLOWER_TIER__TIER_FSM_HPP_
