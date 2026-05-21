// SAN v1.4 PHASE 8 - 4-tier Leader succession test.
//
// Covers S18-1 (Leader -> Deputy), S18-2 (Leader+Deputy -> Hub),
// S18-4 (3-fail -> battery-max follower). Drives the LeaderRoleManager
// through its public test entry points (no actual leader heartbeats
// required - we inject status messages directly).

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "san_role_management/leader_role_manager.hpp"

using namespace san_role_management;
using Status = combat_robot_msgs::msg::RobotStatus;

namespace {

rclcpp::NodeOptions makeOpts(int robot_id) {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"robot_id", robot_id},
        {"leader_robot_id", 1},
        {"hub_robot_id", 2},
        {"deputy_robot_id", 3},
        {"leader_heartbeat_timeout_ms", 1400},
        {"watchdog_period_ms", 100},
        {"grace_step_ms", 20},        // fast-path for tests
        {"min_battery_for_leader", 20.0},
        {"min_battery_follower", 10.0},
    });
    return opts;
}

Status makeStatus(uint32_t id, float battery, bool sbc1 = true,
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

class LeaderSuccessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(LeaderSuccessionTest, S18_1_DeputyIsFirstPriority) {
    auto deputy = std::make_shared<LeaderRoleManager>(makeOpts(3));

    // Snapshot of full swarm; everyone healthy.
    deputy->injectStatusForTest(makeStatus(1, 95.f));
    deputy->injectStatusForTest(makeStatus(2, 80.f));   // Hub
    deputy->injectStatusForTest(makeStatus(3, 80.f, true, true, true));   // Deputy = self
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
    hub->injectStatusForTest(makeStatus(2, 80.f));      // Hub = self
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
    f5->injectStatusForTest(makeStatus(5, 90.f));       // highest
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
    f4->injectStatusForTest(makeStatus(4, 60.f));     // f4 = self
    f4->injectStatusForTest(makeStatus(5, 90.f));     // higher than us
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
