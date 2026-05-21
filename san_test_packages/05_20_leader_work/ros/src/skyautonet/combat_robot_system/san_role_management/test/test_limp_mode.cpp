// SAN v1.4 PHASE 8 - Limp Mode test (S18-5, S18-6).
//
// Revised v1.4 definition: Limp Mode keeps fire + video operational
// via Android mesh-direct. We verify:
//   1. Entry on Hub+Deputy both dead past the 7 s guard.
//   2. Fire auth flag flips to mesh_direct.
//   3. Video sender mode redirects to android_direct + Android IP.
//   4. Complex missions paused; simple missions unaffected.
//   5. Exit when either Hub or Deputy recovers.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "san_role_management/limp_mode_manager.hpp"

using namespace san_role_management;
using HubAnn = combat_robot_msgs::msg::HubRoleAnnouncement;
using Status = combat_robot_msgs::msg::RobotStatus;

namespace {

rclcpp::NodeOptions makeOpts() {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"hub_robot_id", 2},
        {"deputy_robot_id", 3},
        {"hub_timeout_ms", 200},     // tight for unit test
        {"deputy_timeout_ms", 200},
        {"limp_guard_ms", 200},      // 200 ms guard
        {"watchdog_period_ms", 50},
    });
    return opts;
}

}  // namespace

class LimpModeTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(LimpModeTest, InitialStateNotInLimp) {
    auto mgr = std::make_shared<LimpModeManager>(makeOpts());
    EXPECT_FALSE(mgr->isInLimpMode());
    EXPECT_FALSE(mgr->fireAuthMeshDirect());
}

TEST_F(LimpModeTest, S18_5_DualLossEntersLimpWithMeshFire) {
    auto mgr = std::make_shared<LimpModeManager>(makeOpts());
    mgr->setAndroidEndpointForTest("10.0.0.250");

    // Simulate both Hub and Deputy gone past the guard window.
    mgr->simulateHubLossForTest();
    mgr->simulateDeputyLossForTest();
    mgr->tickForTest();

    EXPECT_TRUE(mgr->isInLimpMode());
    EXPECT_TRUE(mgr->fireAuthMeshDirect())
        << "Limp Mode must enable mesh-direct fire authorization";

    const auto vmode = mgr->videoSenderMode();
    EXPECT_EQ(vmode.stream_target, "android_direct");
    EXPECT_EQ(vmode.android_app_ip, "10.0.0.250");
    EXPECT_EQ(vmode.transport_mode, "srt_direct_with_udp_fallback");

    EXPECT_TRUE(mgr->isComplexMissionPaused());
    EXPECT_FALSE(mgr->isSimpleMissionPaused())
        << "v1.4 revised: simple missions (recon/movement/fire) keep "
        << "operating in Limp Mode";
}

TEST_F(LimpModeTest, NoEntryWhenOnePartyAlive) {
    auto mgr = std::make_shared<LimpModeManager>(makeOpts());
    mgr->setAndroidEndpointForTest("10.0.0.250");

    mgr->simulateHubLossForTest();
    mgr->simulateDeputyRecoveryForTest();   // Deputy still alive
    mgr->tickForTest();

    EXPECT_FALSE(mgr->isInLimpMode());
}

TEST_F(LimpModeTest, S18_6_RecoveryExitsLimp) {
    auto mgr = std::make_shared<LimpModeManager>(makeOpts());
    mgr->setAndroidEndpointForTest("10.0.0.250");

    mgr->simulateHubLossForTest();
    mgr->simulateDeputyLossForTest();
    mgr->tickForTest();
    ASSERT_TRUE(mgr->isInLimpMode());

    // Deputy comes back.
    mgr->simulateDeputyRecoveryForTest();
    mgr->tickForTest();

    EXPECT_FALSE(mgr->isInLimpMode());
    EXPECT_FALSE(mgr->fireAuthMeshDirect())
        << "Exiting Limp Mode must reset fire auth source";
    EXPECT_FALSE(mgr->isComplexMissionPaused());
}

TEST_F(LimpModeTest, AndroidEndpointMissingKeepsLocalFlagsButNoVideoRedirect) {
    // Without an Android IP discovery, redirectVideoToAndroidDirect()
    // is a no-op so videoSenderMode stays at the defaults. The fire
    // auth flag still flips so the operator can shoot via mesh once
    // they re-establish the endpoint.
    auto mgr = std::make_shared<LimpModeManager>(makeOpts());
    mgr->simulateHubLossForTest();
    mgr->simulateDeputyLossForTest();
    mgr->tickForTest();

    EXPECT_TRUE(mgr->isInLimpMode());
    EXPECT_TRUE(mgr->fireAuthMeshDirect());
    const auto vmode = mgr->videoSenderMode();
    EXPECT_NE(vmode.stream_target, "android_direct");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
