// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Pan-Tilt controller implementation (patched 2026-05-13).

#include "san_surveillance/pan_tilt_controller.hpp"

#include <algorithm>
#include <cmath>

#include "san_surveillance/sector_frame.hpp"

namespace san_surveillance
{

float computeSweepDps(
  const PanTiltConfig & cfg,
  float sector_width_deg,
  bool night_mode)
{
  // SDD §8.4: ω_sweep = (sector_width - HFOV) × 2 / period
  const float arc = sector_width_deg - cfg.hfov_deg;
  if (arc <= 0.0f) {return 0.0f;}
  if (cfg.sweep_period_s <= 0.0f) {
    return 0.0f;                                      // PATCH: guard
  }
  const float omega = arc * 2.0f / cfg.sweep_period_s;
  const float max_dps = night_mode ? cfg.sweep_night_dps : cfg.sweep_max_dps;
  return std::min(omega, max_dps);
}

namespace
{

float clamp(float v, float lo, float hi)
{
  return std::max(lo, std::min(hi, v));
}

/// Step Sweep mode — bounce inside [centre - W/2, centre + W/2].
///
/// PATCH 2026-05-13: when the pan crosses the boundary mid-tick, the
/// residual motion is now reflected (preserved on the other side of
/// the bounce). Previously the residual was dropped → 6 s sweep
/// period would drift by ~1-2% over long runs.
PanTiltOutput stepSweep(
  const PanTiltConfig & cfg,
  PanTiltState & state,
  float dt_s,
  const SweepParams & p)
{
  const float w_half = p.sector_width_deg * 0.5f;
  const float lo = p.centre_deg - w_half;
  const float hi = p.centre_deg + w_half;

  // M7 PATCH: latch night_mode on Sweep entry — see PanTiltState.
  if (!state.sweep_in_progress) {
    state.sweep_in_progress = true;
    state.sweep_night_mode_latched = p.night_mode;
  }
  const float omega = computeSweepDps(
    cfg, p.sector_width_deg, state.sweep_night_mode_latched);
  float remaining_step = omega * dt_s;

  // Loop in case dt is large enough to cross MULTIPLE boundaries
  // (rare but possible at low tick rates or huge dt spikes).
  while (remaining_step > 0.0f) {
    if (state.sweep_going_right) {
      const float room = hi - state.pan_deg;
      if (remaining_step <= room) {
        state.pan_deg += remaining_step;
        remaining_step = 0.0f;
      } else {
        // Hit right boundary; reflect the residual.
        state.pan_deg = hi;
        state.sweep_going_right = false;
        remaining_step -= room;
      }
    } else {
      const float room = state.pan_deg - lo;
      if (remaining_step <= room) {
        state.pan_deg -= remaining_step;
        remaining_step = 0.0f;
      } else {
        state.pan_deg = lo;
        state.sweep_going_right = true;
        remaining_step -= room;
      }
    }
  }

  // Hold tilt within configured envelope.
  state.tilt_deg = clamp(state.tilt_deg, cfg.tilt_min_deg, cfg.tilt_max_deg);
  return {state.pan_deg, state.tilt_deg, omega};
}

/// Step Track mode — proper proportional control (dt-INDEPENDENT).
///
/// PATCH 2026-05-13: the previous code computed
///   cmd_dps = kp * err / dt_s
/// which made the effective gain depend on tick rate. At 100Hz the
/// loop was 10× too aggressive vs 10Hz → tuning impossible, gains
/// hand-picked for one tick rate would oscillate at another.
///
/// New formulation:
///   cmd_dps = kp * err               (°/s = (°/s per °) * °)
///   delta   = cmd_dps * dt_s         (degrees consumed this tick)
///
/// This is the textbook proportional controller and behaves
/// identically across tick rates (assuming dt is small enough for
/// stability — gain kp ≤ 1/dt is the stability bound).
PanTiltOutput stepTrack(
  const PanTiltConfig & cfg,
  PanTiltState & state,
  float dt_s,
  const TrackTarget & target)
{
  const float pan_err = normalizeAngle(target.bearing_deg - state.pan_deg);
  const float tilt_err = target.elevation_deg - state.tilt_deg;

  // Total error magnitude (Euclidean angular)
  state.last_track_error_deg = std::sqrt(
    pan_err * pan_err +
    tilt_err * tilt_err);

  // Dead-zone: at very small errors, hold position to avoid jitter
  // (helps achieve KPP-quality steady-state ≤ 0.05°)
  if (std::fabs(pan_err) < cfg.track_dead_zone &&
    std::fabs(tilt_err) < cfg.track_dead_zone)
  {
    return {state.pan_deg, state.tilt_deg, 0.0f};
  }

  // Proportional control (dt-independent rate).
  float pan_cmd_dps = cfg.track_kp_pan * pan_err;
  float tilt_cmd_dps = cfg.track_kp_tilt * tilt_err;
  pan_cmd_dps = clamp(pan_cmd_dps, -cfg.track_max_dps, +cfg.track_max_dps);
  tilt_cmd_dps = clamp(tilt_cmd_dps, -cfg.track_max_dps, +cfg.track_max_dps);

  state.pan_deg += pan_cmd_dps * dt_s;
  state.tilt_deg += tilt_cmd_dps * dt_s;
  state.pan_deg = normalizeAngle(state.pan_deg);
  // Phase 0 PR-C + main: clamp Track-mode tilt to the INTERSECTION
  // of the mechanical/safety envelope (cfg.tilt_min/max_deg, per
  // SDD §8.3) and the Track-specific operational envelope
  // (cfg.track_tilt_min/max_deg). The outer cfg.tilt_min/max is the
  // hard limit — track_tilt_* may NARROW but MUST NOT widen beyond
  // it. Without this intersection, an operator who configures a
  // tight tilt_max (e.g. +10° for a friendly-fire-safe arc) would
  // see Track silently swing the muzzle to track_tilt_max_deg
  // (default +60°) — the very PR-C safety bug, just relocated.
  const float tilt_lo = std::max(cfg.tilt_min_deg, cfg.track_tilt_min_deg);
  const float tilt_hi = std::min(cfg.tilt_max_deg, cfg.track_tilt_max_deg);
  state.tilt_deg = clamp(state.tilt_deg, tilt_lo, tilt_hi);

  const float spd = std::sqrt(
    pan_cmd_dps * pan_cmd_dps +
    tilt_cmd_dps * tilt_cmd_dps);
  return {state.pan_deg, state.tilt_deg, spd};
}

/// Step Engage mode — Track + lookahead lead.
PanTiltOutput stepEngage(
  const PanTiltConfig & cfg,
  PanTiltState & state,
  float dt_s,
  const EngageTarget & e)
{
  // Use Track logic but on lookahead-compensated target.
  TrackTarget t;
  t.bearing_deg = e.bearing_deg +
    e.bearing_rate_dps * cfg.engage_lookahead_s;
  t.elevation_deg = e.elevation_deg +
    e.elevation_rate_dps * cfg.engage_lookahead_s;
  return stepTrack(cfg, state, dt_s, t);
}

}  // namespace

PanTiltOutput stepController(
  const PanTiltConfig & cfg,
  PanTiltState & state,
  float dt_s,
  std::optional<SweepParams> sweep,
  std::optional<TrackTarget> track_target,
  std::optional<EngageTarget> engage_target)
{
  if (dt_s <= 0.0f) {
    return {state.pan_deg, state.tilt_deg, 0.0f};
  }
  // M7 PATCH: clear the sweep-night-mode latch whenever we leave Sweep
  // so the next Sweep entry re-captures the live night_mode value.
  if (state.mode != PanTiltMode::Sweep) {
    state.sweep_in_progress = false;
  }
  switch (state.mode) {
    case PanTiltMode::Sweep:
      if (sweep) {return stepSweep(cfg, state, dt_s, *sweep);}
      break;
    case PanTiltMode::Fixed:
      return {state.pan_deg, state.tilt_deg, 0.0f};
    case PanTiltMode::Track:
      if (track_target) {return stepTrack(cfg, state, dt_s, *track_target);}
      break;
    case PanTiltMode::Engage:
      if (engage_target) {return stepEngage(cfg, state, dt_s, *engage_target);}
      break;
  }
  // Mode active but no input provided — hold position
  return {state.pan_deg, state.tilt_deg, 0.0f};
}

}  // namespace san_surveillance
