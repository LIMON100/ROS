// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — TierFsm implementation per SDD-SWARM §6.2 (patched 2026-05-13).

#include "san_follower_tier/tier_fsm.hpp"

namespace san_follower_tier
{

const char * tierName(Tier t)
{
  switch (t) {
    case Tier::T0:   return "T0_PREDICTIVE_TRACK";
    case Tier::T1:   return "T1_NORMAL";
    case Tier::T1_5: return "T1.5_AUTO_REROUTE";
    case Tier::T2:   return "T2_CATCH_UP";
    case Tier::T3:   return "T3_HARD_CATCH_UP";
    case Tier::T4:   return "T4_BREADCRUMB_RECOVERY";
  }
  return "UNKNOWN";
}

TierFsm::TierFsm(const TierConfig & cfg)
: cfg_(cfg) {}

Tier TierFsm::evaluate(const TierInput & in) const
{
  // ─── Priority 1: Safety override (T1.5 AUTO_REROUTE) ─────────────
  // KPP-2: must enter T1.5 in the same step obstacle becomes true.
  if (in.obstacle_on_path) {return Tier::T1_5;}

  const float d0 = in.base_distance_d0_m;
  const float h = cfg_.hysteresis_ratio;
  // PATCH 2026-05-13: upgrade hysteresis is OPT-IN; downgrade is always on.
  const float h_up = cfg_.upgrade_hysteresis_enabled ? h : 0.0f;

  // The "effective" threshold depends on the CURRENT tier — if we are
  // already in a tier, we use the downgrade threshold; if we are
  // below, we use the upgrade threshold.
  //
  // Example for T1↔T2 boundary at 1.5*d0 with h=0.1 (both enabled):
  //   in T1 (or T0)   → upgrade  to T2 only if δ > 1.6 d₀
  //   in T2 (or T3/4) → downgrade to ≤T1 only if δ < 1.4 d₀
  //   → 0.2 d₀ deadband prevents oscillation
  const bool in_t2_or_higher = (tier_ == Tier::T2 || tier_ == Tier::T3 ||
    tier_ == Tier::T4);
  const bool in_t3_or_higher = (tier_ == Tier::T3 || tier_ == Tier::T4);
  const bool in_t4 = (tier_ == Tier::T4);

  const float thr_t2 = d0 * (cfg_.catch_up_ratio +
    (in_t2_or_higher ? -h : +h_up));
  const float thr_t3 = d0 * (cfg_.hard_catch_up_ratio +
    (in_t3_or_higher ? -h : +h_up));
  const float thr_t4 = d0 * (cfg_.breadcrumb_ratio +
    (in_t4 ? -h : +h_up));

  // ─── Priority 2: T4 BREADCRUMB_RECOVERY ──────────────────────────
  if (in.delta_m > thr_t4 ||
    in.prediction_loss_ms > cfg_.comm_timeout_ms ||
    (!in.comm_link_alive &&
    in.prediction_loss_ms > cfg_.comm_timeout_ms))
  {
    return Tier::T4;
  }

  // ─── Priority 3: T3 HARD_CATCH_UP ────────────────────────────────
  if (in.delta_m > thr_t3) {return Tier::T3;}

  // ─── Priority 4: T2 CATCH_UP ─────────────────────────────────────
  if (in.delta_m > thr_t2) {return Tier::T2;}

  // ─── Priority 5/6: T1 NORMAL vs T0 PREDICTIVE_TRACK ──────────────
  if (in.prediction_received) {return Tier::T0;}
  if (in.comm_link_alive) {return Tier::T1;}

  // Comm down but δ small — prefer T4 (breadcrumb fallback) over T1.
  return Tier::T4;
}

bool TierFsm::canLeaveTier(Tier from, Tier to) const
{
  // Allow upgrade (to higher tier) without dwell check — safety first.
  if (static_cast<int>(to) > static_cast<int>(from)) {return true;}

  // Downgrade — enforce per-tier dwell.
  // PATCH 2026-05-13: strict greater-than preserves the original
  // semantics where a single nominal tick at exactly the min_dwell
  // value is NOT enough to leave (one full extra tick required).
  uint32_t min_dwell = 0;
  switch (from) {
    case Tier::T1_5: min_dwell = cfg_.auto_reroute_min_dwell_ms;   break;
    case Tier::T2:   min_dwell = cfg_.catch_up_min_dwell_ms;       break;
    case Tier::T3:   min_dwell = cfg_.hard_catch_up_min_dwell_ms;  break;
    case Tier::T4:   min_dwell = cfg_.breadcrumb_min_dwell_ms;     break;
    default:         min_dwell = 0;                                break;
  }
  return dwell_ms_ > min_dwell;
}

std::optional<Tier> TierFsm::step(const TierInput & in, uint32_t dt_ms)
{
  // ─── R-6 deep-dive: KPP-2 latency probe ───────────────────────
  // Stamp the moment obstacle_on_path goes false→true. The publish
  // path reads this to compute trigger-to-action latency.
  if (in.obstacle_on_path && !prev_obstacle_on_path_) {
    obstacle_trigger_stamp_ = std::chrono::steady_clock::now();
  }
  prev_obstacle_on_path_ = in.obstacle_on_path;

  // R-6 deep-dive: accumulate dwell FIRST so canLeaveTier checks the
  // total time-in-tier including this tick's elapsed dt. This matches
  // the natural semantics "I have been in tier X for at least N ms"
  // at the moment the decision is made. (Subsumes main's Phase 7 fix:
  // the explicit overload above handles the legacy 100ms default.)
  dwell_ms_ += dt_ms;

  const Tier desired = evaluate(in);

  if (desired == tier_) {
    return std::nullopt;
  }

  // PATCH 2026-05-13: anti-flap dwell check applied to all downgrades.
  if (!canLeaveTier(tier_, desired)) {
    return std::nullopt;
  }

  // Transition.
  const Tier prev = tier_;
  tier_ = desired;
  dwell_ms_ = 0;

  if (desired == Tier::T1_5) {
    reason_ = "obstacle_on_path";
  } else if (prev == Tier::T1_5 && !in.obstacle_on_path) {
    reason_ = "obstacle_cleared";
  } else if (desired == Tier::T4) {
    if (in.prediction_loss_ms > cfg_.comm_timeout_ms) {
      reason_ = "comm_timeout_60s";
    } else if (!in.comm_link_alive) {
      reason_ = "comm_link_down";
    } else {
      reason_ = "delta_gt_4.0_d0";
    }
  } else if (desired == Tier::T3) {
    reason_ = "delta_gt_2.0_d0";
  } else if (desired == Tier::T2) {
    reason_ = "delta_gt_1.5_d0";
  } else if (desired == Tier::T1) {
    reason_ = "prediction_lost";
  } else if (desired == Tier::T0) {
    if (prev == Tier::T4) {
      reason_ = "comm_restored";
    } else {
      reason_ = "prediction_received";
    }
  } else {
    reason_ = "transition";
  }
  return desired;
}

}  // namespace san_follower_tier
