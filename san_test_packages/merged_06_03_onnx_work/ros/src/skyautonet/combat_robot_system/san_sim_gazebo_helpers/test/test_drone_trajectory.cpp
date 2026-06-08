// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Drone trajectory unit tests.

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "san_sim_gazebo_helpers/drone_trajectory.hpp"

using namespace san_sim_gazebo_helpers;

constexpr double kEps = 1e-6;

// ─── Loiter ─────────────────────────────────────────────────────────────

TEST(Loiter, T1_RadiusInvariant) {
  LoiterConfig cfg;
  cfg.center = {10.0, -5.0, 30.0, 0.0};
  cfg.radius_m = 25.0;
  cfg.tangential_speed_mps = 10.0;
  LoiterTrajectory traj(cfg);
  for (double t = 0.0; t < 60.0; t += 0.5) {
    const auto s = traj.sample(t);
    const double dx = s.pose.x - cfg.center.x;
    const double dy = s.pose.y - cfg.center.y;
    const double r = std::sqrt(dx * dx + dy * dy);
    EXPECT_NEAR(r, cfg.radius_m, 1e-4) << "t=" << t;
    EXPECT_NEAR(s.pose.z, cfg.center.z, kEps);
  }
}

TEST(Loiter, T2_SpeedMatches) {
  LoiterConfig cfg;
  cfg.radius_m = 50.0;
  cfg.tangential_speed_mps = 8.0;
  LoiterTrajectory traj(cfg);
  const auto s = traj.sample(1.234);
  const double v = std::sqrt(s.twist.vx * s.twist.vx + s.twist.vy * s.twist.vy);
  EXPECT_NEAR(v, cfg.tangential_speed_mps, 1e-3);
}

TEST(Loiter, T3_InvalidRadiusThrows) {
  LoiterConfig cfg;
  cfg.radius_m = 0.0;
  // brace-init avoids "most vexing parse" — `LoiterTrajectory(cfg)`
  // as a statement gets parsed as a function declaration in
  // EXPECT_THROW's macro expansion.
  EXPECT_THROW(LoiterTrajectory{cfg}, std::invalid_argument);
}

// ─── AttackRun ──────────────────────────────────────────────────────────

TEST(AttackRun, T4_StartAndTarget) {
  AttackRunConfig cfg;
  cfg.start = {100.0, 0.0, 40.0, 0.0};
  cfg.target = {0.0, 0.0, 1.5, 0.0};
  cfg.speed_mps = 15.0;
  AttackRunTrajectory traj(cfg);
  const auto s0 = traj.sample(0.0);
  EXPECT_NEAR(s0.pose.x, cfg.start.x, 1e-3);
  EXPECT_NEAR(s0.pose.y, cfg.start.y, 1e-3);

  // After duration → arrived at target
  const auto s_end = traj.sample(traj.durationSeconds() + 1.0);
  EXPECT_NEAR(s_end.pose.x, cfg.target.x, 1e-3);
  EXPECT_NEAR(s_end.pose.y, cfg.target.y, 1e-3);
  EXPECT_NEAR(s_end.pose.z, cfg.target.z, 1e-3);
}

TEST(AttackRun, T5_StraightLineMidpoint) {
  AttackRunConfig cfg;
  cfg.start = {100.0, 0.0, 40.0, 0.0};
  cfg.target = {0.0, 0.0, 1.5, 0.0};
  cfg.speed_mps = 15.0;
  AttackRunTrajectory traj(cfg);
  const double mid_t = traj.durationSeconds() / 2.0;
  const auto sm = traj.sample(mid_t);
  // Midpoint should be on the line, halfway.
  EXPECT_NEAR(sm.pose.x, 50.0, 0.5);
  EXPECT_NEAR(sm.pose.z, (cfg.start.z + cfg.target.z) / 2.0, 0.5);
}

// ─── Patrol ─────────────────────────────────────────────────────────────

TEST(Patrol, T6_WaypointTraversal) {
  PatrolConfig cfg;
  cfg.waypoints = {
    {0.0, 0.0, 20.0, 0.0},
    {50.0, 0.0, 20.0, 0.0},
    {50.0, 50.0, 20.0, 0.0},
    {0.0, 50.0, 20.0, 0.0},
  };
  cfg.speed_mps = 10.0;
  cfg.loop = true;
  PatrolTrajectory traj(cfg);
  // At t=0 should be at first waypoint.
  const auto s0 = traj.sample(0.0);
  EXPECT_NEAR(s0.pose.x, 0.0, 1e-3);
  EXPECT_NEAR(s0.pose.y, 0.0, 1e-3);
  // After 5s @ 10m/s = 50m → at second waypoint.
  const auto s5 = traj.sample(5.0);
  EXPECT_NEAR(s5.pose.x, 50.0, 0.05);
  EXPECT_NEAR(s5.pose.y, 0.0, 0.05);
}

// ─── SwarmEvasion (deterministic seed) ──────────────────────────────────

TEST(SwarmEvasion, T7_DeterministicGivenSeed) {
  SwarmEvasionConfig cfg;
  cfg.rng_seed = 7;
  SwarmEvasionTrajectory a(cfg);
  SwarmEvasionTrajectory b(cfg);
  for (double t = 0.0; t < 15.0; t += 0.3) {
    const auto sa = a.sample(t);
    const auto sb = b.sample(t);
    EXPECT_NEAR(sa.pose.x, sb.pose.x, 1e-9) << "t=" << t;
    EXPECT_NEAR(sa.pose.y, sb.pose.y, 1e-9) << "t=" << t;
    EXPECT_NEAR(sa.pose.z, sb.pose.z, 1e-9) << "t=" << t;
  }
}

TEST(SwarmEvasion, T8_StaysInBoundingBox) {
  SwarmEvasionConfig cfg;
  cfg.center = {0.0, 0.0, 25.0, 0.0};
  cfg.bbox_x = 50.0;
  cfg.bbox_y = 50.0;
  cfg.bbox_z = 10.0;
  cfg.rng_seed = 13;
  SwarmEvasionTrajectory traj(cfg);
  for (double t = 0.0; t < 30.0; t += 0.1) {
    const auto s = traj.sample(t);
    EXPECT_GE(s.pose.x, cfg.center.x - cfg.bbox_x - 1e-6) << "t=" << t;
    EXPECT_LE(s.pose.x, cfg.center.x + cfg.bbox_x + 1e-6) << "t=" << t;
    EXPECT_GE(s.pose.y, cfg.center.y - cfg.bbox_y - 1e-6) << "t=" << t;
    EXPECT_LE(s.pose.y, cfg.center.y + cfg.bbox_y + 1e-6) << "t=" << t;
    EXPECT_GE(s.pose.z, cfg.center.z - cfg.bbox_z - 1e-6) << "t=" << t;
    EXPECT_LE(s.pose.z, cfg.center.z + cfg.bbox_z + 1e-6) << "t=" << t;
  }
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
