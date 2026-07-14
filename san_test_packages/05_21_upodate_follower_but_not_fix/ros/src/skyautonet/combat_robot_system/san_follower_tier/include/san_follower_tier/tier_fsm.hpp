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
// Hysteresis: catchup transitions use small margins to prevent
// oscillation when δ is at the boundary.
//
// Pure C++17 — no rclcpp, no ROS messages. Standalone testable.

#ifndef SAN_FOLLOWER_TIER__TIER_FSM_HPP_
#define SAN_FOLLOWER_TIER__TIER_FSM_HPP_

#include <cstdint>
#include <optional>
#include <string>

namespace san_follower_tier {

/// 6 tier states. Numeric codes match TierStatusChange.msg.
/// T1.5 sits at position 2 between T1 and T2 (inserted in v1.5).
enum class Tier : uint8_t {
  T0   = 0,     // PREDICTIVE_TRACK
  T1   = 1,     // NORMAL
  T1_5 = 2,     // AUTO_REROUTE  ★ KPP-2
  T2   = 3,     // CATCH_UP
  T3   = 4,     // HARD_CATCH_UP
  T4   = 5,     // BREADCRUMB_RECOVERY
};

/// Live inputs evaluated at each FSM step (typically 100ms).
struct TierInput {
  bool     prediction_received{false};
  uint32_t prediction_loss_ms{0};      // accumulated since last prediction
  bool     comm_link_alive{true};       // /comm_link/status
  bool     obstacle_on_path{false};     // T1.5 trigger (cost map)
  float    delta_m{0.0f};               // |pose - target_slot|
  float    base_distance_d0_m{5.0f};    // preset spacing
  bool     breadcrumb_available{true};  // T4 fallback
};

/// Configuration (defaults match SDD §6.2 thresholds).
struct TierConfig {
  // δ-ratio thresholds (relative to base_distance_d0_m).
  float    catch_up_ratio       = 1.5f;   // T0/T1 → T2
  float    hard_catch_up_ratio  = 2.0f;   // T2 → T3
  float    breadcrumb_ratio     = 4.0f;   // T3 → T4

  // Hysteresis (prevent oscillation at boundaries) — ratio drop from
  // active threshold before lower tier is selected.
  float    hysteresis_ratio     = 0.1f;   // 10% margin

  // Communication timeout for T4.
  uint32_t comm_timeout_ms      = 60000;  // 60s per SDD §6.2

  // Minimum dwell time in T1.5 before downgrading (anti-flap).
  uint32_t auto_reroute_min_dwell_ms = 100;
};

/// FSM. Construct with config, then step() repeatedly with input.
/// Thread-safety: NOT thread-safe; caller must serialise.
class TierFsm {
public:
  explicit TierFsm(const TierConfig& cfg = {});

  /// Current tier (initial = T1, defensive default until first step).
  Tier currentTier() const { return tier_; }

  /// Evaluate input and (possibly) transition.
  /// Returns the NEW tier if a transition occurred; std::nullopt if
  /// no change. Always updates lastReason().
  ///
  /// `dt_ms = 0` is treated as "use default 100ms" (back-compat).
  /// Phase 7 fix: pre-patch hardcoded 100ms inside step() ignored
  /// the caller's `tick_period_ms` parameter — anti-flap dwell
  /// measured against wall time would be wrong by a factor of
  /// (real_dt / 100). Pass the actual tick interval to fix.
  std::optional<Tier> step(const TierInput& in, uint32_t dt_ms = 0);

  /// Human-readable trigger for the most recent transition.
  /// Empty string if no transition has occurred yet.
  const std::string& lastReason() const { return reason_; }

  /// Milliseconds the FSM has been in its current tier.
  uint32_t dwellMs() const { return dwell_ms_; }

  /// Internal step counter (each step() advances by 100ms by default;
  /// caller may pass dt via stepWithDt to drive accurately).
  void stepWithDt(uint32_t dt_ms);

private:
  /// Compute the tier that input would select, ignoring dwell.
  Tier evaluate(const TierInput& in) const;

  TierConfig cfg_;
  Tier       tier_{Tier::T1};   // safe default: NORMAL
  std::string reason_;
  uint32_t   dwell_ms_{0};
};

/// String name for a tier (for logs / messages).
const char* tierName(Tier t);

}  // namespace san_follower_tier

#endif  // SAN_FOLLOWER_TIER__TIER_FSM_HPP_
