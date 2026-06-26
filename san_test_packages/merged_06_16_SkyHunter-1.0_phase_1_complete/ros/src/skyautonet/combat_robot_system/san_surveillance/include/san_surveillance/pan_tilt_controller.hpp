// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Pan-Tilt controller per SDD-SWARM §8.3 + §8.4.
//
// 4 modes:
//   Sweep    — 할당 sector 내 좌우 왕복 (default 30°/s, 야간 15°/s)
//   Fixed    — 지정 방향 정지
//   Track    — 표적 위치 자동 추종 (KPP ≤ 0.05° 정확도)
//   Engage   — 「Confirm Fire」 직전 lock-on + 예측 보정
//
// PATCH 2026-05-13 (deep-dive review):
//   * Track loop is now dt-INDEPENDENT (was dividing by dt → effective
//     gain scaled with tick rate, making tuning impossible).
//   * Track tilt clamp uses cfg envelope (was extending ±20°/+50°
//     beyond cfg → could exceed mechanical limits silently).
//   * Sweep correctly carries over residual velocity when it hits a
//     boundary mid-tick (was losing fractional motion → 6 s period
//     was slightly off over long runs).
//
// Pure C++17, no rclcpp. Standalone testable.

#ifndef SAN_SURVEILLANCE__PAN_TILT_CONTROLLER_HPP_
#define SAN_SURVEILLANCE__PAN_TILT_CONTROLLER_HPP_

#include <cstdint>
#include <optional>

namespace san_surveillance
{

enum class PanTiltMode : uint8_t
{
  Sweep  = 0,
  Fixed  = 1,
  Track  = 2,
  Engage = 3,
};

/// Configuration constants per SDD §8.4.
struct PanTiltConfig
{
  float hfov_deg = 60.0f;            // camera HFOV (default 60°)
  float sweep_period_s = 6.0f;       // default 6s per half-cycle
  float sweep_max_dps = 30.0f;       // saturation
  float sweep_night_dps = 15.0f;     // thermal mode (HFOV 좁음)

  // Tilt envelope per SDD §8.3 (Sweep mode: -5° ~ +10° horizon ref)
  float tilt_min_deg = -5.0f;
  float tilt_max_deg = +10.0f;

  // Track mode tilt envelope — wider than sweep, but still mechanical.
  // PATCH 2026-05-13: previous code used cfg.tilt_min - 20° / cfg.tilt_max + 50°
  // which silently extended beyond mechanical limits. Now explicit.
  float track_tilt_min_deg = -25.0f;
  float track_tilt_max_deg = +60.0f;

  // Track mode tuning — keep error well below KPP 0.05°
  //
  // PATCH 2026-05-13: gains are now PROPER proportional (°/s per °).
  // Previously the loop computed cmd_dps = kp * err / dt_s, which made
  // the effective gain depend on tick rate (100Hz → 10x stronger than
  // 10Hz). With dt removed, kp=4 gives a comfortable settling time
  // and stays below KPP across tick rates 10..200 Hz.
  float track_kp_pan = 4.0f;         // °/s per ° error
  float track_kp_tilt = 4.0f;
  float track_max_dps = 40.0f;       // saturated angular velocity
  float track_dead_zone = 0.02f;     // ignore errors < this (deg)

  // Engage lookahead — time forward to predict target position
  // (typ. ammunition flight-time + sensor latency budget).
  float engage_lookahead_s = 0.3f;
};

/// Live pan-tilt state.
struct PanTiltState
{
  float pan_deg = 0.0f;       // current pan, heading-frame
  float tilt_deg = 0.0f;      // current tilt, horizon-frame
  PanTiltMode mode = PanTiltMode::Sweep;

  // Sweep state
  bool sweep_going_right = true;

  // M7 PATCH: night_mode is latched on Sweep entry so the half-cycle
  // period stays deterministic even if the caller flips night_mode
  // mid-sweep. `sweep_in_progress` resets when stepController dispatches
  // to a non-Sweep mode.
  bool sweep_in_progress = false;
  bool sweep_night_mode_latched = false;

  // Track state — last error magnitude (for KPP measurement)
  float last_track_error_deg = 0.0f;
};

/// Sweep params for a given sector.
struct SweepParams
{
  float centre_deg;       // sector centre, heading-frame (after frame transform!)
  float sector_width_deg;  // full width of sweep arc
  bool night_mode;        // use sweep_night_dps if true
};

/// Track target — a single point in robot heading frame.
struct TrackTarget
{
  float bearing_deg;   // pan angle to target (heading-frame)
  float elevation_deg;  // tilt angle to target
};

/// Engage target — track + velocity prediction.
struct EngageTarget
{
  float bearing_deg;
  float elevation_deg;
  float bearing_rate_dps;     // ° per second (predicted)
  float elevation_rate_dps;
};

/// Compute the recommended sweep velocity for given sector + HFOV
/// per SDD §8.4:
///   ω_sweep = (sector_width - HFOV) × 2 / period
/// Clamped to [0, sweep_max_dps] (or night).
float computeSweepDps(
  const PanTiltConfig & cfg,
  float sector_width_deg,
  bool night_mode = false);

struct PanTiltOutput
{
  float target_pan_deg;
  float target_tilt_deg;
  float speed_dps;
};

PanTiltOutput stepController(
  const PanTiltConfig & cfg,
  PanTiltState & state,
  float dt_s,
  std::optional<SweepParams> sweep = std::nullopt,
  std::optional<TrackTarget> track_target = std::nullopt,
  std::optional<EngageTarget> engage_target = std::nullopt);

}  // namespace san_surveillance

#endif  // SAN_SURVEILLANCE__PAN_TILT_CONTROLLER_HPP_
