// SAN v1.3 PHASE 2 v2 - split-brain prevention test.
//
// Two peers each believe they should be LTE_PROMOTED. We verify:
//   1. The peer that sees a higher term defers.
//   2. A stale announcement (term lower than local) is ignored.

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
    auto logger = rclcpp::get_logger("test_split_brain");
    auto uci = std::make_unique<san_lte_redundancy::Mwan3UciController>(logger);
    auto ubus = std::make_unique<san_lte_redundancy::Mwan3UbusMonitor>(logger);
    ubus->injectStatusEventForTest("wan_lte", true);
    return std::make_shared<san_lte_redundancy::LTERoleManager>(
        opts, std::move(uci), std::move(ubus));
}

void driveToActive(
    std::shared_ptr<san_lte_redundancy::LTERoleManager>& node,
    uint32_t hub_term)
{
    Ann hub_down;
    hub_down.header.stamp = node->now();
    hub_down.robot_id = 2;
    hub_down.lte_term = hub_term;
    hub_down.role = Ann::LTE_DEMOTED;
    hub_down.reason = "hub_lte_down";
    node->injectAnnouncementForTest(hub_down);

    auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline
           && !node->isLteActive()) {
        rclcpp::spin_some(node);
        std::this_thread::sleep_for(20ms);
    }
}

}  // namespace

class SplitBrainTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(SplitBrainTest, HigherTermWins) {
    auto s3 = makeFollower(3, {3, 5});

    // S3 becomes active with term=2.
    driveToActive(s3, 1);
    ASSERT_TRUE(s3->isLteActive());
    const uint32_t s3_term = s3->getLteTerm();

    // A peer claims PROMOTED with a higher term.
    Ann higher;
    higher.header.stamp = s3->now();
    higher.robot_id = 5;            // any peer
    higher.lte_term = s3_term + 5;
    higher.role = Ann::LTE_PROMOTED;
    higher.reason = "force_takeover";
    s3->injectAnnouncementForTest(higher);

    rclcpp::spin_some(s3);
    EXPECT_FALSE(s3->isLteActive())
        << "S3 must defer to a higher-term peer";
    EXPECT_GE(s3->getLteTerm(), higher.lte_term);
}

TEST_F(SplitBrainTest, StaleAnnouncementIgnored) {
    auto s3 = makeFollower(3, {3, 5});
    driveToActive(s3, 1);
    const uint32_t s3_term = s3->getLteTerm();
    ASSERT_GT(s3_term, 0u);

    // Stale message - term below current.
    Ann stale;
    stale.header.stamp = s3->now();
    stale.robot_id = 7;
    stale.lte_term = 0;
    stale.role = Ann::LTE_PROMOTED;
    stale.reason = "stale_replay";
    s3->injectAnnouncementForTest(stale);

    rclcpp::spin_some(s3);
    EXPECT_TRUE(s3->isLteActive())
        << "S3 must stay ACTIVE despite stale announcement";
    EXPECT_EQ(s3->getLteTerm(), s3_term);
}

TEST_F(SplitBrainTest, TermMonotonicallyIncreases) {
    auto s3 = makeFollower(3, {3, 5});
    const uint32_t initial = s3->getLteTerm();

    driveToActive(s3, 7);
    EXPECT_GE(s3->getLteTerm(), 7u)
        << "term must clamp to max(local, received) before bumping";
    EXPECT_GT(s3->getLteTerm(), initial);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
