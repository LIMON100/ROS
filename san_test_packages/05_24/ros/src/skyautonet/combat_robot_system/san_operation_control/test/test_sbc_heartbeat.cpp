// SAN v1.5.2 — DCN-2026-011 D-033 dual-SBC heartbeat tests.
//
// Coverage:
//   T1 NonHubRobotPublishesEmptySbcFields
//   T2 Sbc1ReportsOwnHealthyPeerUnknownInitially
//   T3 Sbc1ReportsBothHealthyWithFreshPeerHeartbeat
//   T4 Sbc2ReportsPeerStaleAfterTimeout

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "san_operation_control/operation_control_node.hpp"

namespace san_operation_control {
namespace {

rclcpp::NodeOptions makeOpts(int sbc_id) {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"robot_id", 2},
        {"deployment_mode", std::string("bench")},
        {"sbc_id", sbc_id},
        // Bench mode keeps the watchdog quiet and disables the demo
        // sequencer so the test stays focused on the heartbeat path.
    });
    return opts;
}

}  // namespace

class SbcHeartbeatTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

// ─── T1: non-Hub robot publishes sentinel-zero SBC fields ────────────
TEST_F(SbcHeartbeatTest, NonHubRobotPublishesEmptySbcFields) {
    auto node = std::make_shared<OperationControlNode>(makeOpts(0));
    auto s = node->publishStatusForTest();
    EXPECT_FALSE(s.sbc1_healthy);
    EXPECT_FALSE(s.sbc2_healthy)
        << "sbc_id=0 (non-Hub) must publish both fields false — "
        << "HubHealthMonitor ignores non-Hub entries, so the sentinel "
        << "is unambiguous";
}

// ─── T2: SBC1 reports own healthy + peer unknown when no heartbeat ──
TEST_F(SbcHeartbeatTest, Sbc1ReportsOwnHealthyPeerUnknownInitially) {
    auto node = std::make_shared<OperationControlNode>(makeOpts(1));
    auto s = node->publishStatusForTest();
    EXPECT_TRUE(s.sbc1_healthy);
    EXPECT_FALSE(s.sbc2_healthy)
        << "no peer heartbeat received yet → peer reported unhealthy";
}

// ─── T3: SBC1 reports both healthy with a fresh peer heartbeat ──────
TEST_F(SbcHeartbeatTest, Sbc1ReportsBothHealthyWithFreshPeerHeartbeat) {
    auto node = std::make_shared<OperationControlNode>(makeOpts(1));
    node->injectPeerHeartbeatForTest(node->now());
    auto s = node->publishStatusForTest();
    EXPECT_TRUE(s.sbc1_healthy);
    EXPECT_TRUE(s.sbc2_healthy)
        << "fresh peer Header → peer reported healthy";
}

// ─── T4: SBC2 reports peer stale after timeout ───────────────────────
TEST_F(SbcHeartbeatTest, Sbc2ReportsPeerStaleAfterTimeout) {
    auto node = std::make_shared<OperationControlNode>(makeOpts(2));
    // Inject a Header stamped 5 seconds ago — exceeds the 3 s threshold.
    const auto stale_stamp =
        node->now() - rclcpp::Duration(std::chrono::seconds(5));
    node->injectPeerHeartbeatForTest(stale_stamp);
    auto s = node->publishStatusForTest();
    EXPECT_TRUE(s.sbc2_healthy);
    EXPECT_FALSE(s.sbc1_healthy)
        << "peer heartbeat older than kPeerStaleSec (3 s) → peer unhealthy";
}

}  // namespace san_operation_control
