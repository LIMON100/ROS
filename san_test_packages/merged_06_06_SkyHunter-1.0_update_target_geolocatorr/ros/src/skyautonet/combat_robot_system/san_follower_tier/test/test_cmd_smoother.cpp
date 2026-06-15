// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.
//
// Pure-logic tests for CmdSmoother — no rclcpp, runs in the fast standalone
// gtest runner and in full colcon test.

#include <gtest/gtest.h>

#include "san_follower_tier/cmd_smoother.hpp"

using san_follower_tier::CmdSmoother;

// The very first command after construction passes straight through (no state
// to ramp from yet), regardless of the configured limits.
TEST(CmdSmoother, FirstCallPassesThrough) {
  CmdSmoother s;
  s.max_lin_accel = 0.1;
  s.max_ang_accel = 0.1;
  const auto r = s.apply(0.7, -0.4, 0.05);
  EXPECT_NEAR(r.first, 0.7, 1e-9);
  EXPECT_NEAR(r.second, -0.4, 1e-9);
}

// Linear acceleration is bounded to max_lin_accel * dt per tick.
TEST(CmdSmoother, LinearAccelLimited) {
  CmdSmoother s;
  s.max_lin_accel = 2.0;
  s.lp_alpha_v = 1.0;
  s.reset();  // state = 0, initialised
  const auto r = s.apply(1.0, 0.0, 0.05);
  EXPECT_NEAR(r.first, 0.10, 1e-9);  // 2.0 * 0.05
}

// Braking (toward 0) may use the larger decel budget — faster than accel — so
// obstacle/anti-collision stops are not delayed by smoothing.
TEST(CmdSmoother, DecelFasterThanAccel) {
  CmdSmoother s;
  s.max_lin_accel = 2.0;
  s.max_lin_decel = 8.0;
  s.lp_alpha_v = 1.0;
  s.reset();
  s.apply(1.0, 0.0, 0.05);                  // -> 0.10
  const auto up = s.apply(1.0, 0.0, 0.05);  // -> 0.20 (accel +0.10)
  EXPECT_NEAR(up.first, 0.20, 1e-9);
  const auto down = s.apply(0.0, 0.0, 0.05);  // decel budget 0.40 > 0.20 -> stop
  EXPECT_NEAR(down.first, 0.0, 1e-9);
}

// Angular rate change is bounded to max_ang_accel * dt (chatter damping).
TEST(CmdSmoother, AngularSlewLimited) {
  CmdSmoother s;
  s.max_ang_accel = 4.0;
  s.lp_alpha_w = 1.0;
  s.reset();
  const auto r = s.apply(0.0, 1.5, 0.05);
  EXPECT_NEAR(r.second, 0.20, 1e-9);  // 4.0 * 0.05
}

// A commanded sign flip cannot jump instantly — it slews through, which is
// exactly what damps the follower "shaking".
TEST(CmdSmoother, AngularSignFlipIsRateLimited) {
  CmdSmoother s;
  s.max_ang_accel = 4.0;
  s.lp_alpha_w = 1.0;
  s.reset();
  for (int i = 0; i < 100; ++i) {
    s.apply(0.0, 1.0, 0.05);
  }                                         // settle at +1.0
  const auto r = s.apply(0.0, -1.0, 0.05);  // flip target
  EXPECT_NEAR(r.second, 0.80, 1e-9);        // 1.0 - 4.0*0.05, not -1.0
}

// Low-pass (EMA) on the angular target moves a fraction toward it each tick.
TEST(CmdSmoother, AngularLowPass) {
  CmdSmoother s;
  s.max_ang_accel = 1.0e9;  // effectively no slew limit
  s.lp_alpha_w = 0.5;
  s.reset();
  const auto r1 = s.apply(0.0, 1.0, 0.05);
  EXPECT_NEAR(r1.second, 0.5, 1e-9);
  const auto r2 = s.apply(0.0, 1.0, 0.05);
  EXPECT_NEAR(r2.second, 0.75, 1e-9);
}

// With huge limits and no low-pass the smoother is a pass-through (disabled).
TEST(CmdSmoother, DisabledIsPassThrough) {
  CmdSmoother s;
  s.max_lin_accel = 1.0e9;
  s.max_lin_decel = 1.0e9;
  s.max_ang_accel = 1.0e9;
  s.lp_alpha_v = 1.0;
  s.lp_alpha_w = 1.0;
  s.reset();
  const auto r = s.apply(0.9, 1.3, 0.05);
  EXPECT_NEAR(r.first, 0.9, 1e-9);
  EXPECT_NEAR(r.second, 1.3, 1e-9);
}

// Repeated ticks converge to the target.
TEST(CmdSmoother, ConvergesToTarget) {
  CmdSmoother s;
  s.reset();
  std::pair<double, double> r{0.0, 0.0};
  for (int i = 0; i < 400; ++i) {
    r = s.apply(0.6, -0.3, 0.05);
  }
  EXPECT_NEAR(r.first, 0.6, 1e-6);
  EXPECT_NEAR(r.second, -0.3, 1e-6);
}

// reset() forces a full stop so the next motion ramps from rest.
TEST(CmdSmoother, ResetForcesStop) {
  CmdSmoother s;
  s.max_lin_accel = 2.0;
  s.lp_alpha_v = 1.0;
  s.reset();
  for (int i = 0; i < 50; ++i) {
    s.apply(1.0, 0.5, 0.05);
  }
  s.reset();
  const auto r = s.apply(1.0, 0.0, 0.05);
  EXPECT_NEAR(r.first, 0.10, 1e-9);  // ramps from 0 again, not from cruise
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
