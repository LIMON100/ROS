// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-017 RTH helpers gtest.
//
// Covers the pure-logic helpers in include/san_rth/rth_helpers.hpp.
// Spec calls for 6 cases mapped to T1..T6 acceptance; integration
// behaviour of the rclcpp_action server itself is exercised by the
// san_integration_tests bench (out of scope here — those need a live
// Nav2 + odom + RTK graph).

#include <gtest/gtest.h>

#include <unistd.h>          // POSIX getpid()

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <geometry_msgs/msg/pose.hpp>
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include "san_rth/rth_helpers.hpp"

using san_rth::AccuracyThresholds;
using san_rth::evaluateAccuracy;
using san_rth::readHomePoseYaml;
using san_rth::rtkLossActive;
using san_rth::writeHomePoseYaml;

namespace
{

geometry_msgs::msg::Pose makePose(double x, double y, double yaw_rad)
{
  geometry_msgs::msg::Pose p;
  p.position.x = x;
  p.position.y = y;
  p.position.z = 0.0;
  tf2::Quaternion q;
  q.setRPY(0.0, 0.0, yaw_rad);
  p.orientation = tf2::toMsg(q);
  return p;
}

std::string tempPath(const char * suffix)
{
  static int seq = 0;
  return std::string("/tmp/rth_test_") +
         std::to_string(::getpid()) + "_" +
         std::to_string(++seq) + suffix;
}

}  // namespace

// ------------------------------------------------------- T1
// Home pose auto-recording → roundtrip via YAML file
TEST(RthHelpers, T1_HomePoseYamlRoundtrip) {
  const auto path = tempPath(".yaml");
  const auto orig = makePose(12.34, -5.67, M_PI / 3.0);
  ASSERT_TRUE(writeHomePoseYaml(path, orig)) << "write failed";
  ASSERT_TRUE(std::filesystem::exists(path));

  const auto loaded = readHomePoseYaml(path);
  ASSERT_TRUE(loaded.has_value()) << "read returned nullopt";
  EXPECT_NEAR(loaded->position.x, orig.position.x, 1e-9);
  EXPECT_NEAR(loaded->position.y, orig.position.y, 1e-9);
  EXPECT_NEAR(
    tf2::getYaw(loaded->orientation),
    tf2::getYaw(orig.orientation), 1e-9);
  std::remove(path.c_str());
}

// ------------------------------------------------------- T2
// RTH completes within ±2m accuracy → passed = true
TEST(RthHelpers, T2_AccuracyPassWithinBounds) {
  const auto home = makePose(0.0, 0.0, 0.0);
  const auto cur = makePose(1.5, 1.0, 0.05);   // dist=√3.25≈1.80, yaw≈3°
  const AccuracyThresholds thr{2.0, 10.0 * M_PI / 180.0};
  const auto r = evaluateAccuracy(cur, home, thr);
  EXPECT_TRUE(r.passed);
  EXPECT_NEAR(r.distance_m, std::hypot(1.5, 1.0), 1e-9);
  EXPECT_LT(r.yaw_error_rad, 10.0 * M_PI / 180.0);
}

// ------------------------------------------------------- T3
// No home pose YAML → readHomePoseYaml returns nullopt
TEST(RthHelpers, T3_RejectsIfHomePoseMissing) {
  const auto missing = tempPath("_missing.yaml");
  ASSERT_FALSE(std::filesystem::exists(missing));
  EXPECT_EQ(readHomePoseYaml(missing), std::nullopt);
}

// ------------------------------------------------------- T4
// RTK loss classifier — only triggers AFTER hold_sec sustained
TEST(RthHelpers, T4_RtkLossOnlyAfterHoldDuration) {
  const double t0 = 1000.0;
  // No loss in flight → false regardless of now.
  EXPECT_FALSE(rtkLossActive(std::nullopt, t0, 5.0));
  EXPECT_FALSE(rtkLossActive(std::nullopt, t0 + 100.0, 5.0));

  // Loss started 3s ago, hold=5s → still false.
  EXPECT_FALSE(rtkLossActive(std::optional<double>{t0 - 3.0}, t0, 5.0));

  // Loss started 6s ago, hold=5s → true.
  EXPECT_TRUE(rtkLossActive(std::optional<double>{t0 - 6.0}, t0, 5.0));

  // Boundary — exactly hold_sec elapsed → NOT yet (strict >).
  EXPECT_FALSE(rtkLossActive(std::optional<double>{t0 - 5.0}, t0, 5.0));
}

// ------------------------------------------------------- T5
// reset_home_pose flag → writing a fresh pose overwrites the file
TEST(RthHelpers, T5_ResetHomeOverwritesFile) {
  const auto path = tempPath("_reset.yaml");
  const auto first = makePose(1.0, 1.0, 0.0);
  const auto second = makePose(7.0, 8.0, M_PI / 2.0);

  ASSERT_TRUE(writeHomePoseYaml(path, first));
  ASSERT_TRUE(writeHomePoseYaml(path, second));   // simulates RESET
  const auto loaded = readHomePoseYaml(path);
  ASSERT_TRUE(loaded.has_value());
  EXPECT_NEAR(loaded->position.x, 7.0, 1e-9);
  EXPECT_NEAR(loaded->position.y, 8.0, 1e-9);
  EXPECT_NEAR(tf2::getYaw(loaded->orientation), M_PI / 2.0, 1e-9);
  std::remove(path.c_str());
}

// ------------------------------------------------------- T6
// Yaw alignment mismatch > threshold → passed = false (even if
// position is exactly on home)
TEST(RthHelpers, T6_YawMismatchFailsAccuracy) {
  const auto home = makePose(0.0, 0.0, 0.0);
  const auto cur = makePose(0.0, 0.0, 15.0 * M_PI / 180.0);
  const AccuracyThresholds thr{2.0, 10.0 * M_PI / 180.0};
  const auto r = evaluateAccuracy(cur, home, thr);
  EXPECT_FALSE(r.passed) << "15° yaw with 10° bound must fail";
  EXPECT_LT(r.distance_m, 1e-9);
  EXPECT_NEAR(r.yaw_error_rad, 15.0 * M_PI / 180.0, 1e-9);

  // Wrap test — 350° vs 0° home should report 10° (short way) and
  // exactly hit the bound → passed (≤ threshold).
  const auto cur2 = makePose(0.0, 0.0, 350.0 * M_PI / 180.0);
  const auto r2 = evaluateAccuracy(cur2, home, thr);
  EXPECT_TRUE(r2.passed) << "350° vs 0° should wrap to 10° = bound";
  EXPECT_NEAR(r2.yaw_error_rad, 10.0 * M_PI / 180.0, 1e-9);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
