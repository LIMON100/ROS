// SAN v1.3 PHASE 2 v2 - chain promotion test.
//
// Verifies that S5 (chain position 2) only promotes itself when it is
// re-listed as the first chain element - i.e. after S3 has failed and
// the operator updates the chain to [5]. This is the same correctness
// rule the runtime enforces: amFirstInBackupChain() is the gate.

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

std::shared_ptr<san_lte_redundancy::LTERoleManager>
makeFollower(int robot_id, const std::vector<int64_t>& chain)
{
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"robot_id", robot_id},
        {"hub_robot_id", 2},
        {"lte_backup_chain", chain},
        {"hub_lte_down_timeout_s", 8.0},
        {"ppp_activation_timeout_s", 0.2},
        {"watchdog_period_s", 0.1},
    });
    auto logger = rclcpp::get_logger("test_chain");
    auto uci = std::make_unique<san_lte_redundancy::Mwan3UciController>(logger);
    auto ubus = std::make_unique<san_lte_redundancy::Mwan3UbusMonitor>(logger);
    ubus->injectStatusEventForTest("wan_lte", true);
    return std::make_shared<san_lte_redundancy::LTERoleManager>(
        opts, std::move(uci), std::move(ubus));
}

void waitUntilActive(
    std::shared_ptr<san_lte_redundancy::LTERoleManager>& node)
{
    auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline
           && !node->isLteActive()) {
        rclcpp::spin_some(node);
        std::this_thread::sleep_for(20ms);
    }
}

}  // namespace

class ChainPromotionTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(ChainPromotionTest, ChainHeadPromotesOthersStandby) {
    auto s3 = makeFollower(3, {3, 5});   // S3 is head
    auto s5 = makeFollower(5, {3, 5});   // S5 is tail

    Ann hub_down;
    hub_down.header.stamp = s3->now();
    hub_down.robot_id = 2;
    hub_down.lte_term = 1;
    hub_down.role = Ann::LTE_DEMOTED;
    hub_down.reason = "hub_lte_down";

    s3->injectAnnouncementForTest(hub_down);
    s5->injectAnnouncementForTest(hub_down);

    waitUntilActive(s3);
    EXPECT_TRUE(s3->isLteActive());

    rclcpp::spin_some(s5);
    EXPECT_FALSE(s5->isLteActive())
        << "S5 must remain STANDBY while S3 is head of the chain";
    EXPECT_EQ(s5->getRole(), san_lte_redundancy::LTERole::BACKUP_STANDBY);
}

TEST_F(ChainPromotionTest, S5PromotesWhenS5IsChainHead) {
    // Simulate the operator reconfiguring chain to [5] after S3 has died.
    auto s5 = makeFollower(5, {5});

    Ann hub_down;
    hub_down.header.stamp = s5->now();
    hub_down.robot_id = 2;
    hub_down.lte_term = 3;
    hub_down.role = Ann::LTE_DEMOTED;
    hub_down.reason = "hub_lte_down";
    s5->injectAnnouncementForTest(hub_down);

    waitUntilActive(s5);
    EXPECT_TRUE(s5->isLteActive());
    EXPECT_EQ(s5->getRole(), san_lte_redundancy::LTERole::LTE_ACTIVE);
    EXPECT_GT(s5->getLteTerm(), 3u);
}

TEST_F(ChainPromotionTest, NonChainRobotIgnoresHubDown) {
    auto s7 = makeFollower(7, {3, 5});   // S7 is not in chain at all

    Ann hub_down;
    hub_down.header.stamp = s7->now();
    hub_down.robot_id = 2;
    hub_down.lte_term = 1;
    hub_down.role = Ann::LTE_DEMOTED;
    s7->injectAnnouncementForTest(hub_down);

    rclcpp::spin_some(s7);
    EXPECT_FALSE(s7->isLteActive());
    EXPECT_EQ(s7->getRole(), san_lte_redundancy::LTERole::NONE);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
