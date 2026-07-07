// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-022] Gate-1 acceptance scenarios L5_26 ~ L5_33.
//
// 8 standalone scenarios, each producing a ScenarioReport. Each scenario
// either:
//   * connects to a live service / action / topic (production smoke), or
//   * times out gracefully with recordFail("…not available") so CI
//     without the full bring-up still produces a usable JUnit XML.
//
// regression_main --scenario L5_NN runs a single scenario;
// regression_main --scenario gate1_suite runs all 8 + emits JUnit XML.

#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include "san_l5_regression/scenario_report.hpp"

namespace san_l5_regression
{

// ─── Per-scenario base wrapper (uniform run() return + timeouts) ─────────

struct Gate1Defaults
{
  std::chrono::seconds boot_deadline       {90};
  std::chrono::seconds rtk_window          {5};
  std::chrono::seconds costmap_window      {10};
  std::chrono::seconds nav2_deadline       {30};
  std::chrono::seconds rth_deadline        {60};
  std::chrono::milliseconds estop_deadline {200};
  std::chrono::seconds mission_loop_window {60};
  std::chrono::seconds gate1_e2e_window    {300};
};

#define GATE1_SCENARIO(CLS) \
  class CLS { \
public: \
    explicit CLS(rclcpp::Node & host, const Gate1Defaults & d = {}) \
      : host_(host), d_(d) {} \
    ScenarioReport run(); \
private: \
    rclcpp::Node & host_; \
    Gate1Defaults d_; \
  };

// L5_26 — Deputy boot: 5 critical nodes ACTIVE within 90 s
GATE1_SCENARIO(L5_26_DeputyBoot)

// L5_27 — RTK lock: dual-antenna heading covariance < 0.03 for 5 s
GATE1_SCENARIO(L5_27_RtkLock)

// L5_28 — Costmap rate: >= 10 Hz over 10 s window
GATE1_SCENARIO(L5_28_CostmapRate)

// L5_29 — Nav2 waypoint: goal → within 1 m at completion
GATE1_SCENARIO(L5_29_Nav2WaypointAccuracy)

// L5_30 — RTH accuracy: /rth action final_distance_m < 2.0
GATE1_SCENARIO(L5_30_RthAccuracy)

// L5_31 — E-Stop response: /emergency_stop → /rth dispatch within 200 ms
GATE1_SCENARIO(L5_31_EmergencyStopResponse)

// L5_32 — Mission BT loop: IDLE → ACTIVE → IDLE
GATE1_SCENARIO(L5_32_MissionBtLoop)

// L5_33 — Full Gate-1 demo E2E: /gate1/start_demo + 6 phase transitions
GATE1_SCENARIO(L5_33_Gate1DemoE2E)

#undef GATE1_SCENARIO

// ─── Suite runner + JUnit emitter ────────────────────────────────────────

/// Run all 8 L5_26~33 scenarios on `host` (sequential). Returns the
/// reports in order; suite outcome = AND of all PASSes.
std::vector<ScenarioReport> runGate1Suite(
  rclcpp::Node & host, const Gate1Defaults & d = {});

/// JUnit XML emitter (testsuites > testsuite > testcase). Pure-logic —
/// no ROS calls. Accepts a vector of ScenarioReport and serializes to
/// the simple jest-junit-compatible schema used by the CI reporter.
std::string renderJunitXml(
  const std::string & suite_name,
  const std::vector<ScenarioReport> & reports);

}  // namespace san_l5_regression
