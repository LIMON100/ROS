// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — GPS disturbance injector (pure C++17, no ROS).
//
// Tests the Limon dual-EKF patches (ekf_local.yaml + ekf_global.yaml).
// The whole point of dual-EKF is to keep base_link velocity stable
// when GPS misbehaves; without a disturbance source in the sim, that
// behavior is untestable.
//
// Disturbances:
//   * JUMP    — large step in lat/lon (RTK fix transition, satellite swap)
//   * DRIFT   — slow walk in position bias (multipath, ionospheric)
//   * NOISE   — gaussian additive (baseline receiver noise)
//   * DROPOUT — no fix for N seconds (tree cover, building shadow)
//
// All disturbances are deterministic given the rng_seed.

#ifndef SAN_SIM_GAZEBO_HELPERS__GPS_DISTURBANCE_HPP_
#define SAN_SIM_GAZEBO_HELPERS__GPS_DISTURBANCE_HPP_

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace san_sim_gazebo_helpers
{

// ─── Types ──────────────────────────────────────────────────────────────

struct GpsFix
{
  double latitude_deg;
  double longitude_deg;
  double altitude_m;
  double position_covariance[9] = {0};    // east, north, up (m²)
  uint8_t status = 0;     // -1=NO_FIX, 0=FIX, 1=SBAS, 2=GBAS
  bool valid = true;
};

enum class DisturbanceKind : uint8_t
{
  None    = 0,
  Jump    = 1,
  Drift   = 2,
  Noise   = 3,
  Dropout = 4,
};

const char * toString(DisturbanceKind k);

// ─── Disturbance config ─────────────────────────────────────────────────

struct JumpConfig
{
  double at_sim_time_s = 5.0;
  double east_offset_m = 2.0;           // east step (meters)
  double north_offset_m = 0.0;
  double altitude_offset_m = 0.0;
  double recovery_time_s = 2.0;         // step decays to 0 over this period
};

struct DriftConfig
{
  double start_at_s = 0.0;
  double end_at_s = 60.0;
  double east_rate_m_per_s = 0.05;      // 5cm/s
  double north_rate_m_per_s = 0.0;
};

struct NoiseConfig
{
  double east_std_m = 0.3;              // 30cm 1-σ
  double north_std_m = 0.3;
  double altitude_std_m = 0.6;
  uint32_t rng_seed = 42;
};

struct DropoutConfig
{
  double start_at_s = 10.0;
  double duration_s = 3.0;
  uint8_t status_during = 0xFF;         // -1 → NO_FIX
};

// ─── Pure-logic disturbance engine ──────────────────────────────────────

class GpsDisturbance
{
public:
  GpsDisturbance() = default;

  // Builder-style configuration (any subset can be enabled).
  GpsDisturbance & withJump(JumpConfig cfg);
  GpsDisturbance & withDrift(DriftConfig cfg);
  GpsDisturbance & withNoise(NoiseConfig cfg);
  GpsDisturbance & withDropout(DropoutConfig cfg);

  /// Apply all enabled disturbances to `in` at sim time `t`. Returns
  /// nullopt during a configured dropout window (caller should not
  /// publish the message). Otherwise returns the disturbed fix.
  std::optional<GpsFix> apply(const GpsFix & in, double t);

  // Introspection (for stats / debug).
  uint64_t jumpsApplied()   const {return jumps_applied_;}
  uint64_t dropoutsApplied() const {return dropouts_applied_;}
  uint64_t noiseSamples()   const {return noise_samples_;}

private:
  std::optional<JumpConfig> jump_;
  std::optional<DriftConfig> drift_;
  std::optional<NoiseConfig> noise_;
  std::optional<DropoutConfig> dropout_;

  // Mutable RNG (so apply() can be effectively const-correct from outside).
  mutable std::mt19937 rng_{0};
  bool rng_seeded_ = false;
  uint64_t jumps_applied_ = 0;
  uint64_t dropouts_applied_ = 0;
  uint64_t noise_samples_ = 0;
};

// ─── Approx meters → degrees converter (around a reference latitude) ────

// At latitude ϕ:
//   1 deg lat ≈ 111,132.92 - 559.82 cos(2ϕ) + ...      ≈ 111,000 m
//   1 deg lon ≈ 111,412.84 cos(ϕ) - ...                 ≈ 111,000 cos(ϕ) m
struct MetersToDegConverter
{
  double ref_lat_deg = 37.0;       // default ≈ Korean peninsula
  double metersToDegLat(double m_north) const;
  double metersToDegLon(double m_east)  const;
};

}  // namespace san_sim_gazebo_helpers

#endif  // SAN_SIM_GAZEBO_HELPERS__GPS_DISTURBANCE_HPP_
