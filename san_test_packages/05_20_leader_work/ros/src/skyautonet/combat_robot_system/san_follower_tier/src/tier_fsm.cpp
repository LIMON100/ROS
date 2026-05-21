// SAN v1.5 — TierFsm implementation per SDD-SWARM §6.2.

#include "san_follower_tier/tier_fsm.hpp"

namespace san_follower_tier {

const char* tierName(Tier t) {
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

TierFsm::TierFsm(const TierConfig& cfg) : cfg_(cfg) {}

Tier TierFsm::evaluate(const TierInput& in) const {
  // ─── Priority 1: Safety override (T1.5 AUTO_REROUTE) ─────────────
  // Obstacle on predicted path takes precedence over everything. This
  // is the KPP-2 path: the FSM must be in T1.5 within one step of
  // obstacle detection.
  if (in.obstacle_on_path) return Tier::T1_5;

  // Compute thresholds in absolute meters with hysteresis. When we
  // are ALREADY in a higher tier we lower the threshold slightly so
  // we don't bounce back-and-forth at the boundary.
  const float d0 = in.base_distance_d0_m;
  const float h  = cfg_.hysteresis_ratio;

  // Effective thresholds per current tier
  const bool in_t2 = tier_ == Tier::T2;
  const bool in_t3 = tier_ == Tier::T3;
  const bool in_t4 = tier_ == Tier::T4;

  const float thr_t2 = d0 * (cfg_.catch_up_ratio       - (in_t2 ? h : 0.0f));
  const float thr_t3 = d0 * (cfg_.hard_catch_up_ratio  - (in_t3 ? h : 0.0f));
  const float thr_t4 = d0 * (cfg_.breadcrumb_ratio     - (in_t4 ? h : 0.0f));

  // ─── Priority 2: T4 BREADCRUMB_RECOVERY ──────────────────────────
  // Either far gone (δ > 4 d₀) OR 60s without comms.
  if (in.delta_m > thr_t4
      || in.prediction_loss_ms > cfg_.comm_timeout_ms) {
    return Tier::T4;
  }

  // ─── Priority 3: T3 HARD_CATCH_UP ────────────────────────────────
  if (in.delta_m > thr_t3) return Tier::T3;

  // ─── Priority 4: T2 CATCH_UP ─────────────────────────────────────
  if (in.delta_m > thr_t2) return Tier::T2;

  // ─── Priority 5/6: T1 NORMAL vs T0 PREDICTIVE_TRACK ──────────────
  if (in.prediction_received) return Tier::T0;
  if (in.comm_link_alive)     return Tier::T1;

  // Comm down but δ small — still try predictive recovery via
  // breadcrumb fallback (T4 path). Prefer T4 over T1 in this case.
  return Tier::T4;
}

void TierFsm::stepWithDt(uint32_t dt_ms) {
  dwell_ms_ += dt_ms;
}

std::optional<Tier> TierFsm::step(const TierInput& in, uint32_t dt_ms) {
  // Phase 7 fix: dt_ms == 0 sentinel means "use legacy 100ms default"
  // for back-compat with callers that don't pass dt. New callers
  // should always pass their tick interval so anti-flap dwell is
  // measured in real wall-clock ms.
  const uint32_t advance_ms = (dt_ms == 0) ? 100u : dt_ms;
  const Tier desired = evaluate(in);

  // Anti-flap: do not leave T1.5 too quickly.
  if (tier_ == Tier::T1_5 && desired != Tier::T1_5) {
    if (dwell_ms_ < cfg_.auto_reroute_min_dwell_ms) {
      stepWithDt(advance_ms);
      return std::nullopt;
    }
  }

  if (desired == tier_) {
    stepWithDt(advance_ms);
    return std::nullopt;
  }

  // Transition — compute human-readable reason for telemetry.
  const Tier prev = tier_;
  tier_ = desired;
  dwell_ms_ = 0;

  if (desired == Tier::T1_5) {
    reason_ = "obstacle_on_path";
  } else if (prev == Tier::T1_5 && !in.obstacle_on_path) {
    reason_ = "obstacle_cleared";
  } else if (desired == Tier::T4) {
    reason_ = (in.prediction_loss_ms > cfg_.comm_timeout_ms)
              ? "comm_timeout_60s"
              : "delta_gt_4.0_d0";
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
