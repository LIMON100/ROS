// SAN v1.5 — Pan-Tilt controller implementation.

#include "san_surveillance/pan_tilt_controller.hpp"

#include <algorithm>
#include <cmath>

namespace san_surveillance {

float computeSweepDps(const PanTiltConfig& cfg,
                      float sector_width_deg,
                      bool  night_mode) {
  // SDD §8.4: ω_sweep = (sector_width - HFOV) × 2 / period
  float arc = sector_width_deg - cfg.hfov_deg;
  if (arc <= 0.0f) return 0.0f;
  float omega = arc * 2.0f / cfg.sweep_period_s;
  const float max_dps = night_mode ? cfg.sweep_night_dps : cfg.sweep_max_dps;
  return std::min(omega, max_dps);
}

namespace {

float clamp(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

/// Step Sweep mode — bounce inside [centre - W/2, centre + W/2].
PanTiltOutput stepSweep(const PanTiltConfig& cfg,
                         PanTiltState&        state,
                         float                dt_s,
                         const SweepParams&   p) {
  const float w_half = p.sector_width_deg * 0.5f;
  const float lo = p.centre_deg - w_half;
  const float hi = p.centre_deg + w_half;
  const float omega = computeSweepDps(cfg, p.sector_width_deg, p.night_mode);

  // Move pan in current direction; flip at boundary
  if (state.sweep_going_right) {
    state.pan_deg += omega * dt_s;
    if (state.pan_deg >= hi) {
      state.pan_deg = hi;
      state.sweep_going_right = false;
    }
  } else {
    state.pan_deg -= omega * dt_s;
    if (state.pan_deg <= lo) {
      state.pan_deg = lo;
      state.sweep_going_right = true;
    }
  }

  // Hold tilt mid-range (0° horizontal)
  state.tilt_deg = clamp(state.tilt_deg, cfg.tilt_min_deg, cfg.tilt_max_deg);

  return {state.pan_deg, state.tilt_deg, omega};
}

/// Step Track mode — proportional toward target with dead-zone for KPP.
PanTiltOutput stepTrack(const PanTiltConfig& cfg,
                         PanTiltState&        state,
                         float                dt_s,
                         const TrackTarget&   target) {
  // Compute angular errors (normalized to ±180)
  auto wrap = [](float a) {
    while (a >  180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
  };
  const float pan_err  = wrap(target.bearing_deg - state.pan_deg);
  const float tilt_err = target.elevation_deg - state.tilt_deg;

  // Total error magnitude (Euclidean angular)
  state.last_track_error_deg = std::sqrt(pan_err * pan_err
                                          + tilt_err * tilt_err);

  // Dead-zone: at very small errors, hold position to avoid jitter
  // (helps achieve KPP-quality steady-state ≤ 0.05°)
  if (std::fabs(pan_err)  < cfg.track_dead_zone &&
      std::fabs(tilt_err) < cfg.track_dead_zone) {
    return {state.pan_deg, state.tilt_deg, 0.0f};
  }

  // Proportional control with rate limit
  float pan_cmd_dps  = cfg.track_kp_pan  * pan_err  / dt_s;
  float tilt_cmd_dps = cfg.track_kp_tilt * tilt_err / dt_s;
  pan_cmd_dps  = clamp(pan_cmd_dps,  -cfg.track_max_dps, +cfg.track_max_dps);
  tilt_cmd_dps = clamp(tilt_cmd_dps, -cfg.track_max_dps, +cfg.track_max_dps);

  state.pan_deg  += pan_cmd_dps  * dt_s;
  state.tilt_deg += tilt_cmd_dps * dt_s;
  state.pan_deg  = wrap(state.pan_deg);
  state.tilt_deg = clamp(state.tilt_deg, cfg.tilt_min_deg - 20.0f,
                                          cfg.tilt_max_deg + 50.0f);

  const float spd = std::sqrt(pan_cmd_dps*pan_cmd_dps +
                              tilt_cmd_dps*tilt_cmd_dps);
  return {state.pan_deg, state.tilt_deg, spd};
}

/// Step Engage mode — Track + lookahead lead.
PanTiltOutput stepEngage(const PanTiltConfig& cfg,
                          PanTiltState&        state,
                          float                dt_s,
                          const EngageTarget&  e) {
  // Use Track logic but on lookahead-compensated target.
  // Lookahead is measured from current time (not dt_s), so the lead
  // is the predicted target position at t = now + engage_lookahead_s.
  TrackTarget t;
  t.bearing_deg   = e.bearing_deg
                   + e.bearing_rate_dps   * cfg.engage_lookahead_s;
  t.elevation_deg = e.elevation_deg
                   + e.elevation_rate_dps * cfg.engage_lookahead_s;
  return stepTrack(cfg, state, dt_s, t);
}

}  // namespace

PanTiltOutput stepController(
    const PanTiltConfig& cfg,
    PanTiltState&        state,
    float                dt_s,
    std::optional<SweepParams>   sweep,
    std::optional<TrackTarget>   track_target,
    std::optional<EngageTarget>  engage_target) {
  if (dt_s <= 0.0f) {
    return {state.pan_deg, state.tilt_deg, 0.0f};
  }
  switch (state.mode) {
    case PanTiltMode::Sweep:
      if (sweep) return stepSweep(cfg, state, dt_s, *sweep);
      break;
    case PanTiltMode::Fixed:
      return {state.pan_deg, state.tilt_deg, 0.0f};
    case PanTiltMode::Track:
      if (track_target) return stepTrack(cfg, state, dt_s, *track_target);
      break;
    case PanTiltMode::Engage:
      if (engage_target) return stepEngage(cfg, state, dt_s, *engage_target);
      break;
  }
  // Mode active but no input provided — hold position
  return {state.pan_deg, state.tilt_deg, 0.0f};
}

}  // namespace san_surveillance
