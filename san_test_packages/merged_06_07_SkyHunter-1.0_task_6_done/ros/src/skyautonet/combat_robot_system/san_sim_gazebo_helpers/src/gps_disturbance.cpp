// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — GPS disturbance injector implementation.

#include "san_sim_gazebo_helpers/gps_disturbance.hpp"

#include <algorithm>
#include <cmath>

namespace san_sim_gazebo_helpers
{

namespace
{
constexpr double kDegLatPerMeter = 1.0 / 111132.92;
constexpr double kDegLonPerMeterEquat = 1.0 / 111412.84;
}

const char * toString(DisturbanceKind k)
{
  switch (k) {
    case DisturbanceKind::None:    return "None";
    case DisturbanceKind::Jump:    return "Jump";
    case DisturbanceKind::Drift:   return "Drift";
    case DisturbanceKind::Noise:   return "Noise";
    case DisturbanceKind::Dropout: return "Dropout";
  }
  return "?";
}

// ─── Converter ──────────────────────────────────────────────────────────

double MetersToDegConverter::metersToDegLat(double m_north) const
{
  return m_north * kDegLatPerMeter;
}

double MetersToDegConverter::metersToDegLon(double m_east) const
{
  const double rad = ref_lat_deg * M_PI / 180.0;
  const double cos_lat = std::cos(rad);
  if (std::abs(cos_lat) < 1e-9) {
    return 0.0;                                // polar — degenerate
  }
  return m_east * kDegLonPerMeterEquat / cos_lat;
}

// ─── GpsDisturbance ─────────────────────────────────────────────────────

GpsDisturbance & GpsDisturbance::withJump(JumpConfig cfg)
{
  jump_ = cfg;
  return *this;
}

GpsDisturbance & GpsDisturbance::withDrift(DriftConfig cfg)
{
  drift_ = cfg;
  return *this;
}

GpsDisturbance & GpsDisturbance::withNoise(NoiseConfig cfg)
{
  noise_ = cfg;
  return *this;
}

GpsDisturbance & GpsDisturbance::withDropout(DropoutConfig cfg)
{
  dropout_ = cfg;
  return *this;
}

std::optional<GpsFix> GpsDisturbance::apply(const GpsFix & in, double t)
{
  // 1. Check dropout — fastest path, may short-circuit.
  if (dropout_) {
    const double start = dropout_->start_at_s;
    const double end = start + dropout_->duration_s;
    if (t >= start && t < end) {
      ++dropouts_applied_;
      return std::nullopt;
    }
  }

  GpsFix out = in;
  MetersToDegConverter conv;
  conv.ref_lat_deg = in.latitude_deg;

  double east_offset_m = 0.0;
  double north_offset_m = 0.0;
  double alt_offset_m = 0.0;

  // 2. Jump (step disturbance, optionally decaying).
  if (jump_) {
    const double dt_since_jump = t - jump_->at_sim_time_s;
    if (dt_since_jump >= 0.0) {
      // Decay over recovery_time_s. After recovery, no contribution.
      double decay = 1.0;
      if (jump_->recovery_time_s > 1e-9) {
        decay = std::max(
          0.0,
          1.0 - dt_since_jump / jump_->recovery_time_s);
      }
      if (decay > 0.0) {
        east_offset_m += jump_->east_offset_m * decay;
        north_offset_m += jump_->north_offset_m * decay;
        alt_offset_m += jump_->altitude_offset_m * decay;
        if (dt_since_jump < 0.01) {
          ++jumps_applied_;                            // tally onset
        }
      }
    }
  }

  // 3. Drift (linear ramp during the window).
  if (drift_) {
    const double clamped = std::clamp(t, drift_->start_at_s, drift_->end_at_s);
    const double dt = clamped - drift_->start_at_s;
    east_offset_m += drift_->east_rate_m_per_s * dt;
    north_offset_m += drift_->north_rate_m_per_s * dt;
  }

  // 4. Noise (gaussian per axis).
  if (noise_) {
    if (!rng_seeded_) {
      rng_.seed(noise_->rng_seed);
      rng_seeded_ = true;
    }
    std::normal_distribution<double> nx(0.0, noise_->east_std_m);
    std::normal_distribution<double> ny(0.0, noise_->north_std_m);
    std::normal_distribution<double> nz(0.0, noise_->altitude_std_m);
    east_offset_m += nx(rng_);
    north_offset_m += ny(rng_);
    alt_offset_m += nz(rng_);
    ++noise_samples_;
  }

  // 5. Apply combined offsets.
  out.latitude_deg += conv.metersToDegLat(north_offset_m);
  out.longitude_deg += conv.metersToDegLon(east_offset_m);
  out.altitude_m += alt_offset_m;

  // Update covariance — inflate diagonal proportionally to applied
  // disturbances so downstream EKF/Nav2 can react (this models
  // receiver-reported uncertainty during multipath/RTK transitions).
  if (jump_ || drift_) {
    out.position_covariance[0] += east_offset_m * east_offset_m;    // East
    out.position_covariance[4] += north_offset_m * north_offset_m;  // North
    out.position_covariance[8] += alt_offset_m * alt_offset_m;      // Up
  }
  if (noise_) {
    out.position_covariance[0] += noise_->east_std_m * noise_->east_std_m;
    out.position_covariance[4] += noise_->north_std_m * noise_->north_std_m;
    out.position_covariance[8] += noise_->altitude_std_m * noise_->altitude_std_m;
  }
  return out;
}

}  // namespace san_sim_gazebo_helpers
