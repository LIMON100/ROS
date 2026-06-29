// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Drone trajectory engine implementation.

#include "san_sim_gazebo_helpers/drone_trajectory.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace san_sim_gazebo_helpers
{

namespace
{
constexpr double kTwoPi = 6.283185307179586;
}

const char * toString(TrajectoryKind k)
{
  switch (k) {
    case TrajectoryKind::Loiter:       return "Loiter";
    case TrajectoryKind::AttackRun:    return "AttackRun";
    case TrajectoryKind::Patrol:       return "Patrol";
    case TrajectoryKind::SwarmEvasion: return "SwarmEvasion";
  }
  return "?";
}

// ─── LoiterTrajectory ───────────────────────────────────────────────────

LoiterTrajectory::LoiterTrajectory(LoiterConfig cfg, std::string name)
: cfg_(std::move(cfg)), name_(std::move(name))
{
  if (cfg_.radius_m <= 0.0) {
    throw std::invalid_argument("LoiterTrajectory: radius_m must be > 0");
  }
  if (cfg_.tangential_speed_mps <= 0.0) {
    throw std::invalid_argument(
            "LoiterTrajectory: tangential_speed_mps must be > 0");
  }
}

KinState LoiterTrajectory::sample(double t) const
{
  // Angular velocity ω = v / r
  const double omega = cfg_.tangential_speed_mps / cfg_.radius_m;
  const double phi = cfg_.initial_phase_rad + omega * t;

  KinState s;
  s.t = t;
  s.pose.x = cfg_.center.x + cfg_.radius_m * std::cos(phi);
  s.pose.y = cfg_.center.y + cfg_.radius_m * std::sin(phi);
  s.pose.z = cfg_.center.z;
  // Heading is tangential — 90° ahead of radial.
  s.pose.yaw = phi + kTwoPi / 4.0;
  // Velocity is tangential.
  s.twist.vx = -cfg_.tangential_speed_mps * std::sin(phi);
  s.twist.vy = cfg_.tangential_speed_mps * std::cos(phi);
  s.twist.vz = 0.0;
  s.twist.wz = omega;
  return s;
}

// ─── AttackRunTrajectory ────────────────────────────────────────────────

AttackRunTrajectory::AttackRunTrajectory(
  AttackRunConfig cfg,
  std::string name)
: cfg_(std::move(cfg)), name_(std::move(name))
{
  if (cfg_.speed_mps <= 0.0) {
    throw std::invalid_argument("AttackRun: speed_mps must be > 0");
  }
  const double dx = cfg_.target.x - cfg_.start.x;
  const double dy = cfg_.target.y - cfg_.start.y;
  const double dz = cfg_.target.z - cfg_.start.z;
  const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (dist < 1e-6) {
    throw std::invalid_argument("AttackRun: start == target");
  }
  unit_dx_ = dx / dist;
  unit_dy_ = dy / dist;
  unit_dz_ = dz / dist;
  duration_s_ = dist / cfg_.speed_mps;
}

double AttackRunTrajectory::durationSeconds() const {return duration_s_;}

KinState AttackRunTrajectory::sample(double t) const
{
  KinState s;
  s.t = t;
  if (t >= duration_s_) {
    // Impact — sit at target.
    s.pose.x = cfg_.target.x;
    s.pose.y = cfg_.target.y;
    s.pose.z = cfg_.target.z;
    s.pose.yaw = std::atan2(unit_dy_, unit_dx_);
    // Stationary post-impact.
    return s;
  }
  const double traveled = cfg_.speed_mps * std::max(0.0, t);
  s.pose.x = cfg_.start.x + unit_dx_ * traveled;
  s.pose.y = cfg_.start.y + unit_dy_ * traveled;
  s.pose.z = cfg_.start.z + unit_dz_ * traveled;
  s.pose.yaw = std::atan2(unit_dy_, unit_dx_);
  s.twist.vx = cfg_.speed_mps * unit_dx_;
  s.twist.vy = cfg_.speed_mps * unit_dy_;
  s.twist.vz = cfg_.speed_mps * unit_dz_;
  return s;
}

// ─── PatrolTrajectory ───────────────────────────────────────────────────

PatrolTrajectory::PatrolTrajectory(PatrolConfig cfg, std::string name)
: cfg_(std::move(cfg)), name_(std::move(name))
{
  if (cfg_.waypoints.size() < 2) {
    throw std::invalid_argument(
            "PatrolTrajectory: need at least 2 waypoints");
  }
  if (cfg_.speed_mps <= 0.0) {
    throw std::invalid_argument("PatrolTrajectory: speed_mps must be > 0");
  }
  // Pre-compute cumulative leg durations.
  leg_durations_s_.reserve(cfg_.waypoints.size());
  double cum = 0.0;
  for (std::size_t i = 0; i + 1 < cfg_.waypoints.size(); ++i) {
    const double dx = cfg_.waypoints[i + 1].x - cfg_.waypoints[i].x;
    const double dy = cfg_.waypoints[i + 1].y - cfg_.waypoints[i].y;
    const double dz = cfg_.waypoints[i + 1].z - cfg_.waypoints[i].z;
    cum += std::sqrt(dx * dx + dy * dy + dz * dz) / cfg_.speed_mps;
    leg_durations_s_.push_back(cum);
  }
  total_loop_s_ = cum;
}

KinState PatrolTrajectory::sample(double t) const
{
  KinState s;
  s.t = t;
  double tt = t;
  if (cfg_.loop) {
    tt = std::fmod(t, total_loop_s_);
    if (tt < 0) {tt += total_loop_s_;}
  } else if (tt >= total_loop_s_) {
    // Arrived at final waypoint — sit there.
    s.pose = cfg_.waypoints.back();
    return s;
  }
  // Find current leg.
  std::size_t leg = 0;
  for (; leg < leg_durations_s_.size() && tt > leg_durations_s_[leg]; ++leg) {
  }
  if (leg >= leg_durations_s_.size()) {leg = leg_durations_s_.size() - 1;}
  const double leg_start = (leg == 0) ? 0.0 : leg_durations_s_[leg - 1];
  const double leg_dur = leg_durations_s_[leg] - leg_start;
  const double frac = (leg_dur > 1e-9) ? (tt - leg_start) / leg_dur : 0.0;
  const auto & a = cfg_.waypoints[leg];
  const auto & b = cfg_.waypoints[leg + 1];
  s.pose.x = a.x + (b.x - a.x) * frac;
  s.pose.y = a.y + (b.y - a.y) * frac;
  s.pose.z = a.z + (b.z - a.z) * frac;
  s.pose.yaw = std::atan2(b.y - a.y, b.x - a.x);
  const double dist = std::sqrt(
    (b.x - a.x) * (b.x - a.x) +
    (b.y - a.y) * (b.y - a.y) +
    (b.z - a.z) * (b.z - a.z));
  if (dist > 1e-9) {
    s.twist.vx = (b.x - a.x) / dist * cfg_.speed_mps;
    s.twist.vy = (b.y - a.y) / dist * cfg_.speed_mps;
    s.twist.vz = (b.z - a.z) / dist * cfg_.speed_mps;
  }
  return s;
}

// ─── SwarmEvasionTrajectory ─────────────────────────────────────────────

SwarmEvasionTrajectory::SwarmEvasionTrajectory(
  SwarmEvasionConfig cfg,
  std::string name)
: cfg_(std::move(cfg)), name_(std::move(name))
{
  if (cfg_.direction_change_period_s <= 0.0) {
    throw std::invalid_argument(
            "SwarmEvasion: direction_change_period_s must be > 0");
  }
  if (cfg_.speed_mps <= 0.0) {
    throw std::invalid_argument("SwarmEvasion: speed_mps must be > 0");
  }
  const auto horizon =
    static_cast<std::size_t>(
    schedule_horizon_s_ / cfg_.direction_change_period_s) + 1;
  buildSchedule(horizon);
}

void SwarmEvasionTrajectory::buildSchedule(std::size_t horizon_steps)
{
  velocity_schedule_.reserve(horizon_steps);
  std::mt19937 rng(cfg_.rng_seed);
  // Uniform unit-3D direction (rejection sampled).
  std::uniform_real_distribution<double> uniform(-1.0, 1.0);

  for (std::size_t i = 0; i < horizon_steps; ++i) {
    double dx, dy, dz, mag;
    do {
      dx = uniform(rng); dy = uniform(rng); dz = uniform(rng);
      mag = std::sqrt(dx * dx + dy * dy + dz * dz);
    } while (mag < 0.1 || mag > 1.0);
    dx /= mag; dy /= mag; dz /= mag;
    Twist3d v;
    v.vx = dx * cfg_.speed_mps;
    v.vy = dy * cfg_.speed_mps;
    v.vz = dz * cfg_.speed_mps * 0.3;   // mostly horizontal motion
    velocity_schedule_.push_back(v);
  }
}

KinState SwarmEvasionTrajectory::sample(double t) const
{
  KinState s;
  s.t = t;
  // Integrate the velocity schedule from 0 to t.
  // Each segment lasts cfg_.direction_change_period_s.
  const double seg_dur = cfg_.direction_change_period_s;
  const std::size_t full_segs = static_cast<std::size_t>(t / seg_dur);
  const double partial = t - full_segs * seg_dur;
  double x = cfg_.center.x, y = cfg_.center.y, z = cfg_.center.z;

  for (std::size_t i = 0; i < full_segs && i < velocity_schedule_.size(); ++i) {
    x += velocity_schedule_[i].vx * seg_dur;
    y += velocity_schedule_[i].vy * seg_dur;
    z += velocity_schedule_[i].vz * seg_dur;
  }
  if (full_segs < velocity_schedule_.size()) {
    x += velocity_schedule_[full_segs].vx * partial;
    y += velocity_schedule_[full_segs].vy * partial;
    z += velocity_schedule_[full_segs].vz * partial;
  }
  // Clamp to bounding box around center (reflective).
  auto clamp = [](double v, double c, double half) {
      if (v > c + half) {return c + half - (v - (c + half));}
      if (v < c - half) {return c - half + ((c - half) - v);}
      return v;
    };
  s.pose.x = clamp(x, cfg_.center.x, cfg_.bbox_x);
  s.pose.y = clamp(y, cfg_.center.y, cfg_.bbox_y);
  s.pose.z = clamp(z, cfg_.center.z, cfg_.bbox_z);

  if (full_segs < velocity_schedule_.size()) {
    s.twist = velocity_schedule_[full_segs];
    s.pose.yaw = std::atan2(s.twist.vy, s.twist.vx);
  }
  return s;
}

// ─── Factory ────────────────────────────────────────────────────────────

std::unique_ptr<TrajectoryInterface> makeLoiter(LoiterConfig cfg)
{
  return std::make_unique<LoiterTrajectory>(std::move(cfg));
}
std::unique_ptr<TrajectoryInterface> makeAttackRun(AttackRunConfig cfg)
{
  return std::make_unique<AttackRunTrajectory>(std::move(cfg));
}
std::unique_ptr<TrajectoryInterface> makePatrol(PatrolConfig cfg)
{
  return std::make_unique<PatrolTrajectory>(std::move(cfg));
}
std::unique_ptr<TrajectoryInterface> makeSwarmEvasion(SwarmEvasionConfig cfg)
{
  return std::make_unique<SwarmEvasionTrajectory>(std::move(cfg));
}

}  // namespace san_sim_gazebo_helpers
