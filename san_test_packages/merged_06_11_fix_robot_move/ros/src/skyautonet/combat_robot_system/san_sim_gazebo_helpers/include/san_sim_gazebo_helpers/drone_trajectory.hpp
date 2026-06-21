// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Drone target trajectory engine (pure C++17, no ROS/Gazebo).
//
// Generates flight paths for FPV/quad-copter target drones used to
// stress-test the SkyHunter anti-drone pipeline in Gazebo simulation.
//
// Why this matters: the existing Gazebo sim has tactical obstacles
// (poles, bricks) but NO drone targets. Without simulated FPV drones,
// KPP-2 (회피 ≤ 300ms) and the entire perception → fire_authorization
// → effector pipeline cannot be validated end-to-end.
//
// Trajectory types (cover the threats per 사업수요신청서 §4):
//   * Loiter      — orbits at constant radius, simulates surveillance UAV
//   * AttackRun   — straight-line dive toward a target (FPV kamikaze)
//   * Patrol      — zig-zag waypoint patrol
//   * SwarmEvasion — pseudo-random altitude jitter (defeats simple trackers)
//
// Pure C++17, deterministic given a seed → repeatable for KPP tests.

#ifndef SAN_SIM_GAZEBO_HELPERS__DRONE_TRAJECTORY_HPP_
#define SAN_SIM_GAZEBO_HELPERS__DRONE_TRAJECTORY_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace san_sim_gazebo_helpers
{

// ─── Geometry ───────────────────────────────────────────────────────────

struct Pose3d
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  double yaw = 0.0;       // heading (rad)
};

struct Twist3d
{
  double vx = 0.0;
  double vy = 0.0;
  double vz = 0.0;
  double wz = 0.0;        // yaw rate
};

// Full kinematic snapshot at a given sim time.
struct KinState
{
  Pose3d pose;
  Twist3d twist;
  double t = 0.0;         // sim seconds since trajectory start
};

// ─── Trajectory configurations ──────────────────────────────────────────

enum class TrajectoryKind : uint8_t
{
  Loiter        = 0,
  AttackRun     = 1,
  Patrol        = 2,
  SwarmEvasion  = 3,
};

const char * toString(TrajectoryKind k);

struct LoiterConfig
{
  Pose3d center = {0.0, 0.0, 30.0, 0.0};            // 30m altitude
  double radius_m = 50.0;
  double tangential_speed_mps = 8.0;                // ~ 28.8 km/h
  double initial_phase_rad = 0.0;
};

struct AttackRunConfig
{
  Pose3d start = {100.0, 0.0, 40.0, 0.0};
  Pose3d target = {0.0, 0.0, 1.5, 0.0};             // 1.5m above ground
  double speed_mps = 15.0;                          // FPV diving speed
};

struct PatrolConfig
{
  std::vector<Pose3d> waypoints;
  double speed_mps = 6.0;
  bool loop = true;
};

struct SwarmEvasionConfig
{
  Pose3d center = {0.0, 0.0, 25.0, 0.0};
  double bbox_x = 60.0;            // bounding box +- half-extents
  double bbox_y = 60.0;
  double bbox_z = 15.0;
  double speed_mps = 12.0;
  double direction_change_period_s = 1.5;
  uint32_t rng_seed = 42;          // deterministic
};

// ─── Trajectory interface ───────────────────────────────────────────────

class TrajectoryInterface
{
public:
  virtual ~TrajectoryInterface() = default;

  /// Sample the trajectory at sim time t (seconds since start).
  /// MUST be deterministic — same t always produces same KinState
  /// (modulo RNG-seeded trajectories, which are still repeatable
  /// given the seed).
  virtual KinState sample(double t) const = 0;

  virtual TrajectoryKind kind() const = 0;
  virtual std::string    name() const = 0;
};

// ─── Concrete trajectories ──────────────────────────────────────────────

class LoiterTrajectory : public TrajectoryInterface
{
public:
  explicit LoiterTrajectory(LoiterConfig cfg, std::string name = "loiter");
  KinState sample(double t) const override;
  TrajectoryKind kind() const override {return TrajectoryKind::Loiter;}
  std::string    name() const override {return name_;}

private:
  LoiterConfig cfg_;
  std::string name_;
};

class AttackRunTrajectory : public TrajectoryInterface
{
public:
  explicit AttackRunTrajectory(
    AttackRunConfig cfg,
    std::string name = "attack_run");
  KinState sample(double t) const override;
  TrajectoryKind kind() const override {return TrajectoryKind::AttackRun;}
  std::string    name() const override {return name_;}

  /// Total time to reach the target (after which sample() returns
  /// the target pose stationary).
  double durationSeconds() const;

private:
  AttackRunConfig cfg_;
  std::string name_;
  double duration_s_;
  double unit_dx_, unit_dy_, unit_dz_;            // unit vector
};

class PatrolTrajectory : public TrajectoryInterface
{
public:
  explicit PatrolTrajectory(PatrolConfig cfg, std::string name = "patrol");
  KinState sample(double t) const override;
  TrajectoryKind kind() const override {return TrajectoryKind::Patrol;}
  std::string    name() const override {return name_;}

private:
  PatrolConfig cfg_;
  std::string name_;
  std::vector<double> leg_durations_s_;       // cumulative
  double total_loop_s_ = 0.0;
};

/// Pseudo-random swarm-style evasion. Bounded RW with smoothed
/// direction changes — designed to be challenging for naive trackers
/// while remaining deterministic given the seed.
class SwarmEvasionTrajectory : public TrajectoryInterface
{
public:
  explicit SwarmEvasionTrajectory(
    SwarmEvasionConfig cfg,
    std::string name = "evasion");
  KinState sample(double t) const override;
  TrajectoryKind kind() const override
  {
    return TrajectoryKind::SwarmEvasion;
  }
  std::string    name() const override {return name_;}

private:
  // Direction sample at integer multiples of cfg_.direction_change_period_s.
  // We use a cached schedule rather than per-sample RNG so the
  // trajectory is stateless wrt sample(t).
  void buildSchedule(std::size_t horizon_steps);

  SwarmEvasionConfig cfg_;
  std::string name_;
  std::vector<Twist3d> velocity_schedule_;    // pre-computed
  double schedule_horizon_s_ = 60.0;
};

// ─── Factory ────────────────────────────────────────────────────────────

std::unique_ptr<TrajectoryInterface> makeLoiter(LoiterConfig cfg);
std::unique_ptr<TrajectoryInterface> makeAttackRun(AttackRunConfig cfg);
std::unique_ptr<TrajectoryInterface> makePatrol(PatrolConfig cfg);
std::unique_ptr<TrajectoryInterface> makeSwarmEvasion(SwarmEvasionConfig cfg);

}  // namespace san_sim_gazebo_helpers

#endif  // SAN_SIM_GAZEBO_HELPERS__DRONE_TRAJECTORY_HPP_
