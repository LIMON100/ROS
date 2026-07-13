// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-022 gate1_regression_runner.
//
// Standalone executable that runs the L5_* gtest suite and emits
// JUnit XML to /tmp/gate1_regression_results.xml so external CI
// (GitLab/Jenkins/etc.) can consume the result.
//
// Distinct from the ament_add_gtest target (test_l5_scenarios):
//   * test_l5_scenarios is colcon-test driven — meant for build-time
//     unit-test gate
//   * gate1_regression_runner is launch/ros2-run driven — meant for
//     post-bring-up integration validation inside a live ROS graph
//
// The scenarios themselves come from san_test_scenarios (shared lib);
// this main just configures gtest + spins rclcpp once for the suite
// lifetime, then exits with the gtest result code.

#include <rclcpp/rclcpp.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char ** argv)
{
  // Initialise ROS first so scenarios can use the global context
  // (their test fixtures detect this and reuse it rather than
  // double-initialising).
  rclcpp::init(argc, argv);

  // Tests that REQUIRE live dependencies check the SAN_L5_LIVE env
  // var; the launch file sets it before invoking this runner. We
  // also set it here so direct `ros2 run` invocation behaves the
  // same way (assumes the caller HAS brought up the dependency
  // graph — otherwise live scenarios will fail, which is the
  // correct signal for "you're using the runner wrong").
  setenv("SAN_L5_LIVE", "1", 1);

  // Inject default --gtest_output + --gtest_filter into argv BEFORE
  // InitGoogleTest so they take effect even if the user didn't
  // supply them. Setting ::testing::GTEST_FLAG(...) AFTER
  // InitGoogleTest is too late for output path resolution — gtest
  // reads it once during init. CLI override still works (gtest
  // de-duplicates by last-wins).
  std::vector<char *> argv_extended(argv, argv + argc);
  static char default_output[] =
    "--gtest_output=xml:/tmp/gate1_regression_results.xml";
  static char default_filter[] = "--gtest_filter=L5Scenario.L5_*";
  argv_extended.push_back(default_output);
  argv_extended.push_back(default_filter);
  int argc_extended = static_cast<int>(argv_extended.size());

  ::testing::InitGoogleTest(&argc_extended, argv_extended.data());

  const auto start = std::chrono::steady_clock::now();
  const int result = RUN_ALL_TESTS();
  const auto elapsed = std::chrono::steady_clock::now() - start;

  const auto sec =
    std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

  std::cout << "\n"
            << "════════════════════════════════════════════════════════\n"
            << "Gate-1 Regression Suite Complete\n"
            << "  Duration: " << sec << " seconds\n"
            << "  Result:   " << (result == 0 ? "PASS" : "FAIL") << "\n"
            << "  JUnit XML: /tmp/gate1_regression_results.xml\n"
            << "════════════════════════════════════════════════════════\n";

  if (rclcpp::ok()) {rclcpp::shutdown();}
  return result;
}
