// SAN v1.4 PHASE 8 - BatteryMonitor unit test.
//
// Verifies the 3rd-tier (battery-max follower) selection logic in
// isolation: ranking, tie-breaking by lower robot_id, exclusion of
// Leader/Hub/Deputy, and the failure detection helpers.

#include <gtest/gtest.h>

#include "san_role_management/battery_monitor.hpp"
#include "san_role_management/role_types.hpp"

using namespace san_role_management;

namespace {

BatterySnapshot makeSnap(uint32_t id, float pct, bool sbc1 = true,
                         bool sbc2 = true)
{
    BatterySnapshot s;
    s.robot_id = id;
    s.battery_percent = pct;
    s.sbc1_healthy = sbc1;
    s.sbc2_healthy = sbc2;
    return s;
}

}  // namespace

TEST(BatteryMonitor, EmptyReturnsZero) {
    BatteryMonitor mon;
    EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, 10.0f), 0u);
}

TEST(BatteryMonitor, PicksHighestBatteryFollower) {
    BatteryMonitor mon;
    mon.update(makeSnap(4, 60.f));
    mon.update(makeSnap(5, 90.f));
    mon.update(makeSnap(6, 45.f));
    EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);
}

TEST(BatteryMonitor, ExcludesLeaderHubDeputy) {
    BatteryMonitor mon;
    mon.update(makeSnap(1, 99.f));    // Leader - excluded
    mon.update(makeSnap(2, 95.f));    // Hub    - excluded
    mon.update(makeSnap(3, 92.f));    // Deputy - excluded
    mon.update(makeSnap(4, 40.f));
    mon.update(makeSnap(5, 80.f));
    EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);
}

TEST(BatteryMonitor, TieResolvedByLowerRobotId) {
    BatteryMonitor mon;
    mon.update(makeSnap(7, 75.f));
    mon.update(makeSnap(4, 75.f));      // tie with 7, lower id wins
    mon.update(makeSnap(8, 75.f));
    EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 4u);
}

TEST(BatteryMonitor, BelowMinimumBatteryIsSkipped) {
    BatteryMonitor mon;
    mon.update(makeSnap(4, 5.f));       // below 10% floor
    mon.update(makeSnap(5, 8.f));       // below 10% floor
    EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 0u);
}

TEST(BatteryMonitor, UnhealthySbcSkipped) {
    BatteryMonitor mon;
    mon.update(makeSnap(4, 90.f, /*sbc1=*/false, /*sbc2=*/true));
    mon.update(makeSnap(5, 50.f, /*sbc1=*/true,  /*sbc2=*/true));
    EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);
}

TEST(BatteryMonitor, HubFailedDetection) {
    BatteryMonitor mon;
    EXPECT_TRUE(mon.isHubFailed(2));    // unseen = failed
    mon.update(makeSnap(2, 50.f, true, true));
    EXPECT_FALSE(mon.isHubFailed(2));
    mon.update(makeSnap(2, 50.f, false, false));
    EXPECT_TRUE(mon.isHubFailed(2));
    mon.update(makeSnap(2, 50.f, true, false));   // one SBC OK
    EXPECT_FALSE(mon.isHubFailed(2));
}

TEST(BatteryMonitor, DeputyFailedDetection) {
    BatteryMonitor mon;
    EXPECT_TRUE(mon.isDeputyFailed(3));
    mon.update(makeSnap(3, 50.f, true, true));
    EXPECT_FALSE(mon.isDeputyFailed(3));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
