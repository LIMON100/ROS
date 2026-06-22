// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 4 - HubHealthMonitor unit test.
//
// Verifies the three failure scenarios from SAN-TST-INT-001 v1.3
// §S15-3 (Hub UGV SBC partial-fail handling):
//   Case A: SBC #1 down only (SLAM 정전, comm OK)
//   Case B: SBC #2 down only (LTE backup activates, SLAM OK)
//   Case C: 양쪽 SBC down (Hub excluded from deputy chain)

#include <gtest/gtest.h>

#include "swarm_coordinator/hub_health_monitor.hpp"

using namespace swarm_coordinator;

namespace
{

combat_robot_msgs::msg::RobotStatus
makeHubStatus(bool sbc1, bool sbc2)
{
  combat_robot_msgs::msg::RobotStatus s;
  s.robot_id = 2;
  s.sbc1_healthy = sbc1;
  s.sbc2_healthy = sbc2;
  return s;
}

}  // namespace

TEST(HubHealthMonitor, InitialStateUnknown) {
  HubHealthMonitor mon(/*hub_robot_id=*/ 2);
  EXPECT_EQ(mon.classify(1000), HubHealthCase::UNKNOWN);
  EXPECT_TRUE(mon.hubExcludedFromLeaderChain(1000));
  EXPECT_FALSE(mon.isHubSlamSbcAvailable(1000));
  EXPECT_FALSE(mon.isHubCommSbcAvailable(1000));
}

TEST(HubHealthMonitor, NormalCaseBothHealthy) {
  HubHealthMonitor mon(2);
  mon.injectStatusForTest(true, true, 1000);
  EXPECT_EQ(mon.classify(1100), HubHealthCase::NORMAL);
  EXPECT_TRUE(mon.isHubSlamSbcAvailable(1100));
  EXPECT_TRUE(mon.isHubCommSbcAvailable(1100));
  EXPECT_FALSE(mon.hubExcludedFromLeaderChain(1100));
}

TEST(HubHealthMonitor, CaseA_Sbc1FailedOnly) {
  HubHealthMonitor mon(2);
  mon.injectStatusForTest(/*sbc1=*/ false, /*sbc2=*/ true, 1000);
  EXPECT_EQ(mon.classify(1100), HubHealthCase::CASE_A);
  EXPECT_FALSE(mon.isHubSlamSbcAvailable(1100));
  EXPECT_TRUE(mon.isHubCommSbcAvailable(1100));
  EXPECT_FALSE(mon.hubExcludedFromLeaderChain(1100))
    << "Hub stays in chain as long as at least one SBC is alive";
}

TEST(HubHealthMonitor, CaseB_Sbc2FailedOnly) {
  HubHealthMonitor mon(2);
  mon.injectStatusForTest(/*sbc1=*/ true, /*sbc2=*/ false, 1000);
  EXPECT_EQ(mon.classify(1100), HubHealthCase::CASE_B);
  EXPECT_TRUE(mon.isHubSlamSbcAvailable(1100));
  EXPECT_FALSE(mon.isHubCommSbcAvailable(1100));
  EXPECT_FALSE(mon.hubExcludedFromLeaderChain(1100));
}

TEST(HubHealthMonitor, CaseC_BothFailedExcludedFromChain) {
  HubHealthMonitor mon(2);
  mon.injectStatusForTest(false, false, 1000);
  EXPECT_EQ(mon.classify(1100), HubHealthCase::CASE_C);
  EXPECT_FALSE(mon.isHubSlamSbcAvailable(1100));
  EXPECT_FALSE(mon.isHubCommSbcAvailable(1100));
  EXPECT_TRUE(mon.hubExcludedFromLeaderChain(1100))
    << "Both SBCs dead - Hub must drop out of the deputy chain";
}

TEST(HubHealthMonitor, StaleHeartbeatTreatedAsUnknown) {
  HubHealthMonitor mon(/*hub_robot_id=*/ 2,
    /*stale_threshold_ms=*/ 3000);
  mon.injectStatusForTest(true, true, 1000);
  // 3.1 s later - over the 3 s threshold.
  EXPECT_EQ(mon.classify(4100), HubHealthCase::UNKNOWN);
  EXPECT_TRUE(mon.hubExcludedFromLeaderChain(4100));
}

TEST(HubHealthMonitor, NonHubStatusIgnored) {
  HubHealthMonitor mon(2);
  auto s = makeHubStatus(true, true);
  s.robot_id = 5;       // a follower
  mon.update(s, 1000);
  EXPECT_EQ(mon.classify(1100), HubHealthCase::UNKNOWN);
}

TEST(HubHealthMonitor, RecoveryFromCaseC) {
  HubHealthMonitor mon(2);
  mon.injectStatusForTest(false, false, 1000);
  ASSERT_EQ(mon.classify(1100), HubHealthCase::CASE_C);

  mon.injectStatusForTest(true, true, 2000);
  EXPECT_EQ(mon.classify(2100), HubHealthCase::NORMAL);
  EXPECT_FALSE(mon.hubExcludedFromLeaderChain(2100));
}

// ─── Phase 5: hysteresis policy ─────────────────────────────────────────

TEST(HubHealthMonitor, FutureHeartbeatBeyondSkewIsStale) {
  HubHealthMonitor mon(2);
  // Insert a heartbeat stamped 1 second in the future relative to
  // the clock we'll query with. Pre-patch this was treated as
  // fresh; post-patch any skew > kHubMaxSkewMs (500ms) → stale.
  mon.injectStatusForTest(true, true, /*hb_now_ms=*/ 5000);
  EXPECT_EQ(
    mon.classify(/*query_now_ms=*/ 4000),
    HubHealthCase::UNKNOWN);
  EXPECT_TRUE(mon.hubExcludedFromLeaderChain(4000));
}

TEST(HubHealthMonitor, FutureHeartbeatWithinSkewToleranceFresh) {
  HubHealthMonitor mon(2);
  // 100ms future is within kHubMaxSkewMs (500ms) — accepted.
  mon.injectStatusForTest(true, true, /*hb_now_ms=*/ 5100);
  EXPECT_EQ(mon.classify(5000), HubHealthCase::NORMAL);
}

TEST(HubHealthMonitor, HysteresisIgnoresSingleSpike) {
  // Production policy: 3 consecutive bad → flip to unhealthy,
  // 5 consecutive good → flip back to healthy.
  HubHealthMonitor mon(/*hub_robot_id=*/ 2,
    /*stale_threshold_ms=*/ 3000,
    FlappingPolicy{3, 5});
  // Warm start: both healthy.
  mon.injectStatusForTest(true, true, 1000);
  ASSERT_EQ(mon.classify(1100), HubHealthCase::NORMAL);

  // Single spike of SBC1 down — must NOT flip (hysteresis ignores).
  mon.injectStatusForTest(false, true, 1200);
  EXPECT_EQ(mon.classify(1300), HubHealthCase::NORMAL);
  mon.injectStatusForTest(true, true, 1400);     // recovers
  EXPECT_EQ(mon.classify(1500), HubHealthCase::NORMAL);
}

TEST(HubHealthMonitor, HysteresisFlipsAfterNConsecutiveBad) {
  HubHealthMonitor mon(2, 3000, FlappingPolicy{3, 5});
  mon.injectStatusForTest(true, true, 1000);
  ASSERT_EQ(mon.classify(1050), HubHealthCase::NORMAL);

  // Three consecutive bad samples → flip.
  mon.injectStatusForTest(false, true, 1100);
  mon.injectStatusForTest(false, true, 1200);
  EXPECT_EQ(mon.classify(1250), HubHealthCase::NORMAL)
    << "Still NORMAL after 2 bad samples (threshold = 3)";
  mon.injectStatusForTest(false, true, 1300);
  EXPECT_EQ(mon.classify(1350), HubHealthCase::CASE_A)
    << "Should flip to CASE_A after the 3rd consecutive bad sample";
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
