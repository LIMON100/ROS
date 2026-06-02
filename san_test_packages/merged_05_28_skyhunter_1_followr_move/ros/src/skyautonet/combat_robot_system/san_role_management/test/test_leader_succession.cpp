// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 PHASE 8 - 4-tier Leader succession test.
//
// Covers S18-1 (Leader -> Deputy), S18-2 (Leader+Deputy -> Hub),
// S18-4 (3-fail -> battery-max follower). Drives the LeaderRoleManager
// through its public test entry points (no actual leader heartbeats
// required - we inject status messages directly).

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <thread>

#include "san_role_management/leader_role_manager.hpp"

using namespace san_role_management;
using Status = combat_robot_msgs::msg::RobotStatus;

namespace
{

rclcpp::NodeOptions makeOpts(int robot_id)
{
  rclcpp::NodeOptions opts;
  opts.parameter_overrides(
    {
      {"robot_id", robot_id},
      {"leader_robot_id", 1},
      {"hub_robot_id", 2},
      {"deputy_robot_id", 3},
      {"leader_heartbeat_timeout_ms", 1400},
      {"watchdog_period_ms", 100},
      {"grace_step_ms", 20},          // fast-path for tests
      {"min_battery_for_leader", 20.0},
      {"min_battery_follower", 10.0},
    });
  return opts;
}

Status makeStatus(
  uint32_t id, float battery, bool sbc1 = true,
  bool sbc2 = true, bool is_deputy = false)
{
  Status s;
  s.robot_id = id;
  s.battery_percent = battery;
  s.sbc1_healthy = sbc1;
  s.sbc2_healthy = sbc2;
  s.is_deputy_ugv = is_deputy;
  return s;
}

}  // namespace

class LeaderSuccessionTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
  }
};

TEST_F(LeaderSuccessionTest, S18_1_DeputyIsFirstPriority) {
  auto deputy = std::make_shared<LeaderRoleManager>(makeOpts(3));

  // Snapshot of full swarm; everyone healthy.
  deputy->injectStatusForTest(makeStatus(1, 95.f));
  deputy->injectStatusForTest(makeStatus(2, 80.f));     // Hub
  deputy->injectStatusForTest(makeStatus(3, 80.f, true, true, true));     // Deputy = self
  deputy->injectStatusForTest(makeStatus(4, 60.f));
  deputy->injectStatusForTest(makeStatus(5, 90.f));

  auto priority = deputy->evaluateSuccessionForTest();
  EXPECT_EQ(priority, SuccessionPriority::DEPUTY);
  EXPECT_EQ(deputy->getRole(), LeaderRole::NORMAL);

  deputy->promoteForTest(SuccessionPriority::DEPUTY);
  EXPECT_EQ(deputy->getRole(), LeaderRole::PROMOTED);
  EXPECT_EQ(deputy->getSuccessionPriority(), SuccessionPriority::DEPUTY);
  EXPECT_GT(deputy->getLeaderTerm(), 0u);
}

TEST_F(LeaderSuccessionTest, S18_2_HubFallbackWhenDeputyFailed) {
  auto hub = std::make_shared<LeaderRoleManager>(makeOpts(2));

  hub->injectStatusForTest(makeStatus(1, 95.f));
  hub->injectStatusForTest(makeStatus(2, 80.f));        // Hub = self
  // Deputy with both SBCs down -> failed
  hub->injectStatusForTest(makeStatus(3, 0.f, false, false, true));
  hub->injectStatusForTest(makeStatus(4, 60.f));
  hub->injectStatusForTest(makeStatus(5, 90.f));

  auto priority = hub->evaluateSuccessionForTest();
  EXPECT_EQ(priority, SuccessionPriority::HUB);

  hub->promoteForTest(SuccessionPriority::HUB);
  EXPECT_EQ(hub->getRole(), LeaderRole::PROMOTED);
  EXPECT_EQ(hub->getSuccessionPriority(), SuccessionPriority::HUB);
}

TEST_F(LeaderSuccessionTest, S18_4_BatteryMaxFollowerWhenHubDeputyDown) {
  auto f5 = std::make_shared<LeaderRoleManager>(makeOpts(5));

  f5->injectStatusForTest(makeStatus(1, 95.f));
  // Hub and Deputy both unhealthy
  f5->injectStatusForTest(makeStatus(2, 0.f, false, false));
  f5->injectStatusForTest(makeStatus(3, 0.f, false, false, true));
  f5->injectStatusForTest(makeStatus(4, 60.f));
  f5->injectStatusForTest(makeStatus(5, 90.f));         // highest
  f5->injectStatusForTest(makeStatus(6, 45.f));

  auto priority = f5->evaluateSuccessionForTest();
  EXPECT_EQ(priority, SuccessionPriority::BATTERY_MAX);

  f5->promoteForTest(SuccessionPriority::BATTERY_MAX);
  EXPECT_EQ(f5->getRole(), LeaderRole::PROMOTED);
}

TEST_F(LeaderSuccessionTest, NonWinnerFollowerStaysNormal) {
  auto f4 = std::make_shared<LeaderRoleManager>(makeOpts(4));

  f4->injectStatusForTest(makeStatus(1, 95.f));
  f4->injectStatusForTest(makeStatus(2, 0.f, false, false));
  f4->injectStatusForTest(makeStatus(3, 0.f, false, false, true));
  f4->injectStatusForTest(makeStatus(4, 60.f));       // f4 = self
  f4->injectStatusForTest(makeStatus(5, 90.f));       // higher than us
  f4->injectStatusForTest(makeStatus(6, 45.f));

  auto priority = f4->evaluateSuccessionForTest();
  EXPECT_EQ(priority, SuccessionPriority::LIMP_MODE)
    << "battery_percent=60 < max=90 so f4 should not self-promote";
  EXPECT_EQ(f4->getRole(), LeaderRole::NORMAL);
}

TEST_F(LeaderSuccessionTest, BelowBatteryFloorYieldsLimp) {
  auto deputy = std::make_shared<LeaderRoleManager>(makeOpts(3));
  // Deputy battery below 20 % minimum.
  deputy->injectStatusForTest(makeStatus(1, 95.f));
  deputy->injectStatusForTest(makeStatus(2, 0.f, false, false));
  deputy->injectStatusForTest(makeStatus(3, 15.f, true, true, true));

  auto priority = deputy->evaluateSuccessionForTest();
  EXPECT_EQ(priority, SuccessionPriority::LIMP_MODE)
    << "Deputy below 20% cannot take Leader role";
}

TEST_F(LeaderSuccessionTest, HigherTermPeerCausesDemotion) {
  auto deputy = std::make_shared<LeaderRoleManager>(makeOpts(3));
  deputy->injectStatusForTest(makeStatus(3, 80.f, true, true, true));
  deputy->promoteForTest(SuccessionPriority::DEPUTY);
  ASSERT_EQ(deputy->getRole(), LeaderRole::PROMOTED);
  const uint32_t our_term = deputy->getLeaderTerm();

  combat_robot_msgs::msg::LeaderRoleAnnouncement winner;
  winner.robot_id = 5;
  winner.leader_term = our_term + 1;
  winner.role = winner.LEADER_PROMOTED;
  winner.succession_priority =
    static_cast<uint8_t>(SuccessionPriority::BATTERY_MAX);
  deputy->injectAnnouncementForTest(winner);

  EXPECT_EQ(deputy->getRole(), LeaderRole::DEMOTED)
    << "higher-term peer wins; we must yield";
}

// ═══════════════════════════════════════════════════════════════════════
// PATCH 2026-05-13 — new tests covering deep-dive fixes
// ═══════════════════════════════════════════════════════════════════════

// ─── PL1 (★ C1 fix): announcement during grace cancels promotion ────
TEST_F(LeaderSuccessionTest, PL1_NonBlockingGraceAllowsYield) {
  auto deputy = std::make_shared<LeaderRoleManager>(makeOpts(3));
  deputy->injectStatusForTest(makeStatus(3, 80.f, true, true, true));
  deputy->simulateLeaderHeartbeatLossForTest();

  // Step into grace WITHOUT scheduling the rclcpp timer (test mode).
  deputy->watchdogTickForTest();
  EXPECT_TRUE(deputy->isGraceInProgress());
  EXPECT_EQ(deputy->getRole(), LeaderRole::CANDIDATE);

  // ★ C1: while we're in grace, a higher-priority peer (here a
  // peer with robot_id < our 3, so tiebreak too) announces it
  // promoted. In v1.5.0 the executor would be sleeping inside
  // watchdogTick and this announcement could not be processed.
  // Now the announce_sub_ callback CAN fire mid-grace.
  combat_robot_msgs::msg::LeaderRoleAnnouncement peer_win;
  peer_win.robot_id = 2;                               // peer with lower id
  peer_win.leader_term = deputy->getLeaderTerm() + 1;
  peer_win.role = peer_win.LEADER_PROMOTED;
  peer_win.succession_priority =
    static_cast<uint8_t>(SuccessionPriority::HUB);
  deputy->injectAnnouncementForTest(peer_win);

  // We must have yielded — DEMOTED, not CANDIDATE or PROMOTED.
  EXPECT_EQ(deputy->getRole(), LeaderRole::DEMOTED);

  // Now if the grace timer fires (we simulate that here), onGraceComplete
  // must see role_ != CANDIDATE and abort the promotion.
  deputy->finishGraceForTest();
  EXPECT_EQ(deputy->getRole(), LeaderRole::DEMOTED)
    << "post-yield grace completion must NOT re-promote";
}

// ─── PL2 (★ C5 fix): DEMOTED auto re-arms after cooldown ─────────────
TEST_F(LeaderSuccessionTest, PL2_RearmsFromDemotedAfterCooldown) {
  auto opts = makeOpts(3);
  opts.parameter_overrides(
  {
    {"robot_id", 3}, {"leader_robot_id", 1},
    {"hub_robot_id", 2}, {"deputy_robot_id", 3},
    {"leader_heartbeat_timeout_ms", 1400},
    {"watchdog_period_ms", 100},
    {"grace_step_ms", 20},
    {"min_battery_for_leader", 20.0},
    {"min_battery_follower", 10.0},
    {"demote_cooldown_ms", 50},         // ★ short cooldown for test
    {"status_max_age_ms", 0},           // disable freshness check
  });
  auto deputy = std::make_shared<LeaderRoleManager>(opts);
  deputy->injectStatusForTest(makeStatus(3, 80.f, true, true, true));
  deputy->promoteForTest(SuccessionPriority::DEPUTY);
  ASSERT_EQ(deputy->getRole(), LeaderRole::PROMOTED);
  const uint32_t our_term = deputy->getLeaderTerm();

  // Force a demote.
  combat_robot_msgs::msg::LeaderRoleAnnouncement winner;
  winner.robot_id = 5;
  winner.leader_term = our_term + 1;
  winner.role = winner.LEADER_PROMOTED;
  deputy->injectAnnouncementForTest(winner);
  ASSERT_EQ(deputy->getRole(), LeaderRole::DEMOTED);

  // Wait past the cooldown window.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Simulate the next heartbeat loss + watchdog tick.
  deputy->simulateLeaderHeartbeatLossForTest();
  deputy->watchdogTickForTest();

  // We should have re-armed: DEMOTED → NORMAL → CANDIDATE in this tick.
  EXPECT_NE(deputy->getRole(), LeaderRole::DEMOTED)
    << "must re-arm after cooldown";
}

// ─── PL3 (★ M6/M7 fix): equal-term different-robot_id tiebreak ───────
TEST_F(LeaderSuccessionTest, PL3_EqualTermTiebreakLowerRobotIdWins) {
  auto us = std::make_shared<LeaderRoleManager>(makeOpts(5));
  us->injectStatusForTest(makeStatus(2, 0.f, false, false));     // Hub down
  us->injectStatusForTest(makeStatus(3, 0.f, false, false, true));   // Deputy down
  us->injectStatusForTest(makeStatus(5, 90.f, true, true));
  us->promoteForTest(SuccessionPriority::BATTERY_MAX);
  ASSERT_EQ(us->getRole(), LeaderRole::PROMOTED);
  const uint32_t our_term = us->getLeaderTerm();

  // Peer with EQUAL term but HIGHER robot_id — they must NOT win.
  combat_robot_msgs::msg::LeaderRoleAnnouncement peer_loss;
  peer_loss.robot_id = 6;                         // > 5 → tiebreak loses
  peer_loss.leader_term = our_term;                // equal term
  peer_loss.role = peer_loss.LEADER_PROMOTED;
  us->injectAnnouncementForTest(peer_loss);
  EXPECT_EQ(us->getRole(), LeaderRole::PROMOTED)
    << "equal term + higher robot_id must NOT yield";

  // Peer with EQUAL term but LOWER robot_id — they win.
  combat_robot_msgs::msg::LeaderRoleAnnouncement peer_win;
  peer_win.robot_id = 4;                          // < 5 → wins tiebreak
  peer_win.leader_term = our_term;
  peer_win.role = peer_win.LEADER_PROMOTED;
  us->injectAnnouncementForTest(peer_win);
  EXPECT_EQ(us->getRole(), LeaderRole::DEMOTED)
    << "equal term + lower robot_id must take over";
}

// ─── PL4 (★ M9 fix): stale RobotStatus is rejected ──────────────────
TEST_F(LeaderSuccessionTest, PL4_StaleRobotStatusRejected) {
  auto opts = makeOpts(3);
  opts.parameter_overrides(
  {
    {"robot_id", 3}, {"leader_robot_id", 1},
    {"hub_robot_id", 2}, {"deputy_robot_id", 3},
    {"leader_heartbeat_timeout_ms", 1400},
    {"watchdog_period_ms", 100},
    {"grace_step_ms", 20},
    {"min_battery_for_leader", 20.0},
    {"min_battery_follower", 10.0},
    {"demote_cooldown_ms", 2000},
    {"status_max_age_ms", 100},         // ★ 100ms freshness window
  });
  auto deputy = std::make_shared<LeaderRoleManager>(opts);
  // Inject a status with timestamp far in the past.
  Status old_status = makeStatus(3, 80.f, true, true, true);
  old_status.timestamp_ms = 1ULL;       // ancient
  deputy->injectStatusForTest(old_status);
  // The stale snapshot must NOT be in the battery monitor.
  EXPECT_FALSE(deputy->batteryMonitor().has(3))
    << "Stale RobotStatus must be rejected; freshness=100ms";
}

// ─── PL5 (★ M10 fix): self-loopback impersonation is rejected ───────
TEST_F(LeaderSuccessionTest, PL5_SelfLoopbackImpersonationRejected) {
  auto deputy = std::make_shared<LeaderRoleManager>(makeOpts(3));
  deputy->injectStatusForTest(makeStatus(3, 80.f, true, true, true));
  const uint32_t initial_term = deputy->getLeaderTerm();

  // Fake announcement claiming to be us, with a HIGHER term.
  combat_robot_msgs::msg::LeaderRoleAnnouncement fake;
  fake.robot_id = 3;                                // claims to be us
  fake.leader_term = initial_term + 100;            // huge term jump
  fake.role = fake.LEADER_PROMOTED;
  deputy->injectAnnouncementForTest(fake);

  // Term must NOT have been blindly accepted.
  EXPECT_EQ(deputy->getLeaderTerm(), initial_term)
    << "impersonation of self with inflated term must be rejected";
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
