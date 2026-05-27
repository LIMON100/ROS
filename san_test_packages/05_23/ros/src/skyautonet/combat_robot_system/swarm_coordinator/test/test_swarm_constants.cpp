// SAN v1.3 PHASE 0 - swarm topology constant test.
//
// The succession chain ordering is load-bearing for the v1.4 4-tier
// Leader policy: Deputy must come before Hub, and the sentinels
// (-1, -2) must be present at positions 2 and 3.

#include <gtest/gtest.h>

#include "swarm_coordinator/swarm_coordinator.hpp"

using namespace swarm_coordinator;

TEST(SwarmConstants, RobotIdsAreCanonical) {
    EXPECT_EQ(LEADER_ROBOT_ID, 1u);
    EXPECT_EQ(HUB_ROBOT_ID,    2u);
    EXPECT_EQ(DEPUTY_ROBOT_ID, 3u);
    EXPECT_EQ(MAX_ROBOTS,      8u);
    EXPECT_EQ(MIN_ROBOTS,      4u);
}

TEST(SwarmConstants, V13LegacyDeputyChainPreserved) {
    const std::vector<int32_t> expected = {2, 3, 4, 5, 6, 7, 8};
    EXPECT_EQ(DEFAULT_DEPUTY_CHAIN, expected);
}

TEST(SwarmConstants, V14LeaderSuccessionChainOrder) {
    ASSERT_EQ(DEFAULT_LEADER_SUCCESSION_CHAIN.size(), 4u);
    EXPECT_EQ(DEFAULT_LEADER_SUCCESSION_CHAIN[0],
              static_cast<int32_t>(DEPUTY_ROBOT_ID))
        << "1st priority must be Deputy UGV (v1.4)";
    EXPECT_EQ(DEFAULT_LEADER_SUCCESSION_CHAIN[1],
              static_cast<int32_t>(HUB_ROBOT_ID));
    EXPECT_EQ(DEFAULT_LEADER_SUCCESSION_CHAIN[2], -1)
        << "3rd slot is the runtime-picked battery-max sentinel";
    EXPECT_EQ(DEFAULT_LEADER_SUCCESSION_CHAIN[3], -2)
        << "4th slot is the Limp Mode entry sentinel";
}

TEST(SwarmConstants, MinRobotsCoversThreeFixedRoles) {
    // Leader + Hub + Deputy + at least 1 follower = 4
    EXPECT_GE(MIN_ROBOTS, 4u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
