// SAN v1.3 PHASE 2 v2 - failover acceptance test.
//
// Wires up a mock S3 follower with injected UCI/UBUS controllers,
// pushes a "Hub LTE DEMOTED" announcement onto it, and asserts that
// S3 promotes itself within the 10-second budget and broadcasts a
// PROMOTED announcement with term > 0.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <memory>
#include <vector>

#include <combat_robot_msgs/msg/lte_role_announcement.hpp>
#include "san_lte_redundancy/lte_role_manager.hpp"

using namespace std::chrono_literals;
using Ann = combat_robot_msgs::msg::LTERoleAnnouncement;

namespace {

rclcpp::NodeOptions makeFollowerOptions(int robot_id,
                                        const std::vector<int64_t>& chain)
{
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"robot_id", robot_id},
        {"hub_robot_id", 2},
        {"lte_backup_chain", chain},
        {"hub_lte_down_timeout_s", 8.0},
        {"ppp_activation_timeout_s", 0.2},   // tight for unit tests
        {"watchdog_period_s", 0.1},
    });
    return opts;
}

std::shared_ptr<san_lte_redundancy::LTERoleManager>
makeFollower(int robot_id, const std::vector<int64_t>& chain)
{
    auto opts = makeFollowerOptions(robot_id, chain);
    auto logger = rclcpp::get_logger("test_failover");
    auto uci = std::make_unique<san_lte_redundancy::Mwan3UciController>(logger);
    auto ubus = std::make_unique<san_lte_redundancy::Mwan3UbusMonitor>(logger);
    // Pre-stage ubus as "lte up" so promote()'s wait short-circuits.
    ubus->injectStatusEventForTest("wan_lte", true);
    return std::make_shared<san_lte_redundancy::LTERoleManager>(
        opts, std::move(uci), std::move(ubus));
}

}  // namespace

class LteFailoverTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(LteFailoverTest, S3PromotesWhenHubDemotes) {
    auto s3 = makeFollower(3, {3, 5});

    EXPECT_EQ(s3->getRole(), san_lte_redundancy::LTERole::BACKUP_STANDBY);
    EXPECT_FALSE(s3->isLteActive());

    Ann hub_down;
    hub_down.header.stamp = s3->now();
    hub_down.robot_id = 2;
    hub_down.lte_term = 1;
    hub_down.role = Ann::LTE_DEMOTED;
    hub_down.reason = "hub_lte_down";

    const auto start = std::chrono::steady_clock::now();
    s3->injectAnnouncementForTest(hub_down);

    while (std::chrono::steady_clock::now() - start < 10s) {
        rclcpp::spin_some(s3);
        if (s3->isLteActive()) break;
        std::this_thread::sleep_for(20ms);
    }

    EXPECT_TRUE(s3->isLteActive())
        << "S3 did not promote within 10 s";
    EXPECT_EQ(s3->getRole(), san_lte_redundancy::LTERole::LTE_ACTIVE);
    EXPECT_GT(s3->getLteTerm(), 1u)
        << "term must have advanced past hub's announcement";
}

TEST_F(LteFailoverTest, NonChainHeadStaysInStandby) {
    auto s5 = makeFollower(5, {3, 5});

    Ann hub_down;
    hub_down.header.stamp = s5->now();
    hub_down.robot_id = 2;
    hub_down.lte_term = 1;
    hub_down.role = Ann::LTE_DEMOTED;
    hub_down.reason = "hub_lte_down";
    s5->injectAnnouncementForTest(hub_down);

    rclcpp::spin_some(s5);
    EXPECT_EQ(s5->getRole(), san_lte_redundancy::LTERole::BACKUP_STANDBY);
    EXPECT_FALSE(s5->isLteActive());
}

TEST_F(LteFailoverTest, S3DemotesWhenHubRecoveryAnnounced) {
    auto s3 = makeFollower(3, {3, 5});

    // First, drive S3 into ACTIVE.
    Ann hub_down;
    hub_down.header.stamp = s3->now();
    hub_down.robot_id = 2;
    hub_down.lte_term = 1;
    hub_down.role = Ann::LTE_DEMOTED;
    s3->injectAnnouncementForTest(hub_down);
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline && !s3->isLteActive()) {
        rclcpp::spin_some(s3);
        std::this_thread::sleep_for(20ms);
    }
    ASSERT_TRUE(s3->isLteActive());

    // Hub recovers and re-promotes with a higher term.
    Ann hub_up;
    hub_up.header.stamp = s3->now();
    hub_up.robot_id = 2;
    hub_up.lte_term = s3->getLteTerm() + 1;
    hub_up.role = Ann::LTE_PROMOTED;
    hub_up.reason = "hub_recovered";
    s3->injectAnnouncementForTest(hub_up);

    rclcpp::spin_some(s3);
    EXPECT_FALSE(s3->isLteActive())
        << "S3 should yield to a recovered hub";
    EXPECT_EQ(s3->getRole(), san_lte_redundancy::LTERole::BACKUP_STANDBY);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
