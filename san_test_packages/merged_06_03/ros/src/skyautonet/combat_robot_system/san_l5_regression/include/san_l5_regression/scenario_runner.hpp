// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 L5 regression - S18 scenario orchestrator.
//
// One ScenarioRunner runs an entire S18 pass (1..6). Each scenario:
//   1. resets the synthetic baseline (8 robots healthy, S3 = Deputy)
//   2. injects the failure(s) appropriate for the scenario
//   3. waits on the matching role announcement with a deadline
//   4. records pass/timeout + elapsed ms into the ScenarioReportWriter
//   5. restores the baseline for the next run
//
// The runner is a regular rclcpp::Node so it can co-exist with the
// real san_role_management nodes on the live DDS bus.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/leader_role_announcement.hpp>
#include <combat_robot_msgs/msg/hub_role_announcement.hpp>
#include <std_msgs/msg/string.hpp>

#include <memory>
#include <string>

#include "san_l5_regression/failure_injector.hpp"
#include "san_l5_regression/scenario_report.hpp"
#include "san_l5_regression/topic_watcher.hpp"

namespace san_l5_regression
{

struct RunnerConfig
{
  uint32_t hub_robot_id = 2;
  uint32_t deputy_robot_id = 3;
  int heartbeat_period_ms = 200;            // synthetic publish cadence

  // Per-scenario deadlines. Defaults match SAN-TST-INT-001 v1.4 §S18.
  int s18_1_deadline_ms = 5000;
  int s18_2_deadline_ms = 8000;
  int s18_3_deadline_ms = 7000;
  int s18_4_deadline_ms = 10000;
  int s18_5_deadline_ms = 8000;
  int s18_6_deadline_ms = 5000;
};

class ScenarioRunner : public rclcpp::Node
{
public:
  using LeaderMsg = combat_robot_msgs::msg::LeaderRoleAnnouncement;
  using HubMsg = combat_robot_msgs::msg::HubRoleAnnouncement;

  explicit ScenarioRunner(const RunnerConfig & cfg = {});

  // Run S18-1..S18-6 in order and return the writer.
  ScenarioReportWriter runAll();

  // Individual scenarios (also called by runAll). Each returns the
  // ScenarioReport for telemetry / test verification.
  ScenarioReport runS18_1_LeaderToDeputy();
  ScenarioReport runS18_2_LeaderDeputyToHub();
  ScenarioReport runS18_3_HubToDeputyTakeover();
  ScenarioReport runS18_4_ThreeFailedBatteryFollower();
  ScenarioReport runS18_5_HubDeputyBothDownLimpEnter();
  ScenarioReport runS18_6_DeputyRecoversLimpExit();

  FailureInjector & injector() {return *injector_;}

  // Reset every robot to the healthy baseline and republish.
  void resetBaseline();

  // Drive 1 publish tick. Tests can call this manually instead of
  // running the wall-clock heartbeat timer.
  void heartbeatTick();

private:
  RunnerConfig cfg_;
  std::unique_ptr<FailureInjector> injector_;
  std::unique_ptr<TopicWatcher<LeaderMsg>> leader_watcher_;
  std::unique_ptr<TopicWatcher<HubMsg>> hub_watcher_;
  std::unique_ptr<TopicWatcher<std_msgs::msg::String>> limp_alert_watcher_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // Predicate helpers - centralized so tests can reuse them.
  static bool isLeaderPromotedBy(
    const LeaderMsg & m, uint32_t robot_id,
    uint8_t priority);
  static bool isLeaderPromotedAnyPriority(
    const LeaderMsg & m,
    uint8_t priority);
  static bool isHubPromotedWithFullTakeover(
    const HubMsg & m,
    uint32_t robot_id);
  static bool isLimpModeAlert(
    const std_msgs::msg::String & m,
    const std::string & needle);

  // Common scenario plumbing.
  void seedEightRobotBaseline();
};

}  // namespace san_l5_regression
