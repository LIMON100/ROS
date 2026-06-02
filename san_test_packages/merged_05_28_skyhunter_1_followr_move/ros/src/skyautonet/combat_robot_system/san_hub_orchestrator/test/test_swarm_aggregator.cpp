// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — Swarm aggregator tests (standalone).
//
// Coverage:
//   A1  Empty squadron → all-zero snapshot, total=0
//   A2  Single healthy robot
//   A3  Three robots — 2 healthy + 1 limp
//   A4  Disconnected robot excluded from healthy count
//   A5  min/mean battery calculation
//   A6  RTK fix grade tally (fix/float/no_fix)
//   A7  active_threats sums across robots
//   A8  update() replaces existing snapshot (same robot_id)
//   A9  clear() removes all
//   A10 All robots disconnected → no aggregation done

#include "san_hub_orchestrator/swarm_aggregator.hpp"

#include <gtest/gtest.h>

namespace san_hub_orchestrator
{
namespace
{

RobotSnapshot make(
  const std::string & id, uint64_t last_hb_ms,
  bool limp, float bat, float prog,
  uint8_t rtk, uint16_t threats = 0)
{
  RobotSnapshot s;
  s.robot_id = id;
  s.last_heartbeat_ms = last_hb_ms;
  s.in_limp_mode = limp;
  s.battery_percent = bat;
  s.mission_progress_percent = prog;
  s.rtk_fix_grade = rtk;
  s.active_threats = threats;
  return s;
}

TEST(SwarmAggregator, A1_EmptySquadron) {
  SwarmAggregator a;
  auto f = a.aggregate(10'000);
  EXPECT_EQ(f.total_robots, 0);
  EXPECT_EQ(f.healthy_robots, 0);
  EXPECT_EQ(f.limp_mode_robots, 0);
  EXPECT_EQ(f.disconnected_robots, 0);
  EXPECT_FLOAT_EQ(f.min_battery_percent, 0.0f);
}

TEST(SwarmAggregator, A2_SingleHealthy) {
  SwarmAggregator a;
  a.update(make("robot_1", 10'000, false, 85.0f, 30.0f, 3));
  auto f = a.aggregate(10'500);   // 0.5 s old → connected
  EXPECT_EQ(f.total_robots, 1);
  EXPECT_EQ(f.healthy_robots, 1);
  EXPECT_EQ(f.limp_mode_robots, 0);
  EXPECT_EQ(f.disconnected_robots, 0);
  EXPECT_FLOAT_EQ(f.min_battery_percent, 85.0f);
  EXPECT_FLOAT_EQ(f.mean_battery_percent, 85.0f);
  EXPECT_EQ(f.robots_with_rtk_fix, 1);
}

TEST(SwarmAggregator, A3_ThreeRobotsMixedLimp) {
  SwarmAggregator a;
  a.update(make("robot_1", 10'000, false, 80.0f, 50.0f, 3));
  a.update(make("robot_2", 10'000, false, 60.0f, 50.0f, 3));
  a.update(make("robot_3", 10'000, true, 40.0f, 25.0f, 2));
  auto f = a.aggregate(10'500);
  EXPECT_EQ(f.total_robots, 3);
  EXPECT_EQ(f.healthy_robots, 2);
  EXPECT_EQ(f.limp_mode_robots, 1);
  EXPECT_FLOAT_EQ(f.min_battery_percent, 40.0f);
  EXPECT_NEAR(f.mean_battery_percent, (80.0f + 60.0f + 40.0f) / 3, 1e-3);
}

TEST(SwarmAggregator, A4_DisconnectedExcluded) {
  SwarmAggregator a(5000);
  a.update(make("robot_1", 10'000, false, 80.0f, 50.0f, 3));
  a.update(make("robot_2", 3'000, false, 60.0f, 50.0f, 3));    // 7 s old
  auto f = a.aggregate(10'500);
  EXPECT_EQ(f.total_robots, 2);
  EXPECT_EQ(f.healthy_robots, 1);
  EXPECT_EQ(f.disconnected_robots, 1);
  // Battery only includes connected robot
  EXPECT_FLOAT_EQ(f.mean_battery_percent, 80.0f);
}

TEST(SwarmAggregator, A5_MinBattery) {
  SwarmAggregator a;
  a.update(make("a", 100, false, 95.0f, 0, 0));
  a.update(make("b", 100, false, 30.0f, 0, 0));   // min
  a.update(make("c", 100, false, 70.0f, 0, 0));
  auto f = a.aggregate(101);
  EXPECT_FLOAT_EQ(f.min_battery_percent, 30.0f);
}

TEST(SwarmAggregator, A6_RtkFixGradeTally) {
  SwarmAggregator a;
  a.update(make("a", 100, false, 90, 0, 3));   // RTK_FIX
  a.update(make("b", 100, false, 90, 0, 3));
  a.update(make("c", 100, false, 90, 0, 2));   // RTK_FLOAT
  a.update(make("d", 100, false, 90, 0, 0));   // NO_FIX
  auto f = a.aggregate(101);
  EXPECT_EQ(f.robots_with_rtk_fix, 2);
  EXPECT_EQ(f.robots_with_rtk_float, 1);
  EXPECT_EQ(f.robots_with_no_fix, 1);
}

TEST(SwarmAggregator, A7_ActiveThreatsSum) {
  SwarmAggregator a;
  a.update(make("a", 100, false, 90, 0, 3, /*threats=*/ 2));
  a.update(make("b", 100, false, 90, 0, 3, /*threats=*/ 5));
  a.update(make("c", 100, false, 90, 0, 3, /*threats=*/ 1));
  auto f = a.aggregate(101);
  EXPECT_EQ(f.active_threats_count, 8u);
}

TEST(SwarmAggregator, A8_UpdateReplacesSnapshot) {
  SwarmAggregator a;
  a.update(make("robot_1", 100, false, 80, 50, 3));
  a.update(make("robot_1", 200, true, 20, 60, 0));   // same id, new state
  EXPECT_EQ(a.trackedRobotCount(), 1u);
  auto f = a.aggregate(300);
  EXPECT_EQ(f.total_robots, 1);
  EXPECT_EQ(f.limp_mode_robots, 1);
  EXPECT_FLOAT_EQ(f.min_battery_percent, 20.0f);
}

TEST(SwarmAggregator, A9_ClearRemovesAll) {
  SwarmAggregator a;
  a.update(make("a", 100, false, 90, 0, 3));
  a.update(make("b", 100, false, 90, 0, 3));
  EXPECT_EQ(a.trackedRobotCount(), 2u);
  a.clear();
  EXPECT_EQ(a.trackedRobotCount(), 0u);
  auto f = a.aggregate(101);
  EXPECT_EQ(f.total_robots, 0);
}

TEST(SwarmAggregator, A10_AllDisconnectedNoAggregation) {
  SwarmAggregator a(5000);
  a.update(make("a", 1000, false, 90, 0, 3));
  a.update(make("b", 1000, false, 90, 0, 3));
  // now_ms = 10000 → all stale by 9 s
  auto f = a.aggregate(10'000);
  EXPECT_EQ(f.total_robots, 2);
  EXPECT_EQ(f.disconnected_robots, 2);
  EXPECT_EQ(f.healthy_robots, 0);
  EXPECT_FLOAT_EQ(f.mean_battery_percent, 0.0f);
}

}  // namespace
}  // namespace san_hub_orchestrator
