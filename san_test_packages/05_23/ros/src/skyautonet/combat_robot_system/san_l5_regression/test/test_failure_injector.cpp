// SAN v1.4 L5 regression - FailureInjector unit test.
//
// Verifies the synthetic injector tracks per-robot state and publishes
// RobotStatus / LteLinkQuality messages with the fields scenarios rely on.

#include <gtest/gtest.h>

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/lte_link_quality.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#include "san_l5_regression/failure_injector.hpp"

using namespace san_l5_regression;
using Status = combat_robot_msgs::msg::RobotStatus;
using Lq     = combat_robot_msgs::msg::LteLinkQuality;

class FailureInjectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
        node_ = std::make_shared<rclcpp::Node>("fi_test");
        inj_ = std::make_unique<FailureInjector>(node_.get());
    }
    std::shared_ptr<rclcpp::Node> node_;
    std::unique_ptr<FailureInjector> inj_;
};

TEST_F(FailureInjectorTest, SetHealthThenReadBack) {
    RobotHealth h;
    h.sbc1_healthy = false;
    h.battery_percent = 42.5f;
    h.is_deputy_ugv = true;
    inj_->setHealth(3, h);

    auto out = inj_->health(3);
    EXPECT_FALSE(out.sbc1_healthy);
    EXPECT_FLOAT_EQ(out.battery_percent, 42.5f);
    EXPECT_TRUE(out.is_deputy_ugv);
}

TEST_F(FailureInjectorTest, KillSbcPreservesOtherFields) {
    RobotHealth h;
    h.battery_percent = 73.0f;
    h.is_deputy_ugv = true;
    inj_->setHealth(3, h);

    inj_->killSbc(3, /*sbc1=*/false, /*sbc2=*/false);

    auto out = inj_->health(3);
    EXPECT_FALSE(out.sbc1_healthy);
    EXPECT_FALSE(out.sbc2_healthy);
    EXPECT_FLOAT_EQ(out.battery_percent, 73.0f)
        << "killSbc must not clobber battery";
    EXPECT_TRUE(out.is_deputy_ugv)
        << "killSbc must not clobber deputy flag";
}

TEST_F(FailureInjectorTest, ResetClearsAllRobots) {
    inj_->setHealth(1, {});
    inj_->setHealth(2, {});
    EXPECT_EQ(inj_->trackedRobotCount(), 2u);
    inj_->reset();
    EXPECT_EQ(inj_->trackedRobotCount(), 0u);
}

TEST_F(FailureInjectorTest, PublishAllEmitsOneMessagePerRobot) {
    std::atomic<int> recv_count{0};
    auto sub_node = std::make_shared<rclcpp::Node>("fi_test_sub");
    auto sub = sub_node->create_subscription<Status>(
        FailureInjector::kRobotStatusTopic, rclcpp::QoS(20).reliable(),
        [&recv_count](Status::SharedPtr) { ++recv_count; });

    inj_->setHealth(1, {});
    inj_->setHealth(2, {});
    inj_->setHealth(3, {});

    auto sent = inj_->publishAll();
    EXPECT_EQ(sent, 3u);

    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(sub_node);
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < deadline && recv_count.load() < 3) {
        exec.spin_some();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_GE(recv_count.load(), 3);
}

TEST_F(FailureInjectorTest, BuildMessageRoundTripFieldsForDeputy) {
    std::vector<Status> received;
    auto sub_node = std::make_shared<rclcpp::Node>("fi_test_sub2");
    auto sub = sub_node->create_subscription<Status>(
        FailureInjector::kRobotStatusTopic, rclcpp::QoS(20).reliable(),
        [&received](Status::SharedPtr m) { received.push_back(*m); });

    RobotHealth h;
    h.sbc1_healthy = true;
    h.sbc2_healthy = true;
    h.battery_percent = 88.0f;
    h.is_deputy_ugv = true;
    inj_->setHealth(3, h);
    inj_->publishOne(3);

    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(sub_node);
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < deadline && received.empty()) {
        exec.spin_some();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_FALSE(received.empty());
    const auto& m = received.back();
    EXPECT_EQ(m.robot_id, 3u);
    EXPECT_TRUE(m.is_deputy_ugv);
    EXPECT_TRUE(m.sbc1_healthy);
    EXPECT_TRUE(m.sbc2_healthy);
    EXPECT_FLOAT_EQ(m.battery_percent, 88.0f);
    EXPECT_TRUE(m.is_lte_backup_designated)
        << "robot_id=3 must be flagged as LTE backup candidate";
}

TEST_F(FailureInjectorTest, RemoveRobotStopsPublication) {
    inj_->setHealth(2, {});
    inj_->setHealth(3, {});
    EXPECT_TRUE(inj_->isPublished(2));
    EXPECT_TRUE(inj_->isPublished(3));

    inj_->removeRobot(2);
    EXPECT_FALSE(inj_->isPublished(2));
    EXPECT_TRUE(inj_->isPublished(3));

    auto sent = inj_->publishAll();
    EXPECT_EQ(sent, 1u) << "removed robots must be skipped";
}

TEST_F(FailureInjectorTest, RestoreRobotBringsBackPublication) {
    inj_->setHealth(3, {});
    inj_->removeRobot(3);
    ASSERT_FALSE(inj_->isPublished(3));

    RobotHealth h;
    h.battery_percent = 75.0f;
    h.is_deputy_ugv = true;
    inj_->restoreRobot(3, h);

    EXPECT_TRUE(inj_->isPublished(3));
    auto out = inj_->health(3);
    EXPECT_FLOAT_EQ(out.battery_percent, 75.0f);
    EXPECT_TRUE(out.is_deputy_ugv);
}

TEST_F(FailureInjectorTest, PublishLteGradePropagatesFields) {
    std::vector<Lq> received;
    auto sub_node = std::make_shared<rclcpp::Node>("fi_test_lq");
    auto sub = sub_node->create_subscription<Lq>(
        FailureInjector::kLinkQualityTopic, rclcpp::QoS(10).best_effort(),
        [&received](Lq::SharedPtr m) { received.push_back(*m); });

    inj_->publishLteGrade(Lq::LTE_GRADE_POOR, /*rsrp=*/-120, /*iface=*/"lte0");

    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(sub_node);
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < deadline && received.empty()) {
        exec.spin_some();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_FALSE(received.empty());
    EXPECT_EQ(received.back().grade, Lq::LTE_GRADE_POOR);
    EXPECT_EQ(received.back().rsrp_dbm, -120);
    EXPECT_EQ(received.back().source_iface, "lte0");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    if (rclcpp::ok()) rclcpp::shutdown();
    return rc;
}
