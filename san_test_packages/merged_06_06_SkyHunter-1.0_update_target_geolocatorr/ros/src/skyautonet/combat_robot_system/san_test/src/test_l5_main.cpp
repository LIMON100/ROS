// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-022 test_l5_main.
//
// gtest main for the build-time unit-test invocation (colcon test).
// Unlike gate1_regression_runner.cpp, this DOES NOT set SAN_L5_LIVE,
// so scenarios that depend on a live ROS graph will skip cleanly —
// the build-time gate is structural, not behavioural.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();
  if (rclcpp::ok()) {rclcpp::shutdown();}
  return result;
}
