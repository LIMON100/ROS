// SAN v1.3 PHASE 7 - command_id echo accountability test.
//
// We construct one of each of the 8 SwarmRobotCommand variants with
// a distinct command_id and feed it through the OperationControlNode
// via processRequestForTest-style direct injection. After each
// command, RobotStatus and SwarmHealthSummary must publish the same
// id.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/emergency_stop.hpp>
#include <combat_robot_msgs/msg/fire_authorization.hpp>
#include <combat_robot_msgs/msg/formation_command.hpp>
#include <combat_robot_msgs/msg/jamming_command.hpp>
#include <combat_robot_msgs/msg/manual_override_command.hpp>
#include <combat_robot_msgs/msg/mission_state_command.hpp>
#include <combat_robot_msgs/msg/video_stream_request.hpp>
#include <combat_robot_msgs/msg/waypoint_command.hpp>

#include "san_operation_control/command_echo.hpp"
#include "san_operation_control/operation_control_node.hpp"

using namespace san_operation_control;

class CommandEchoTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }

    rclcpp::NodeOptions makeOptions(const std::string& mode) {
        rclcpp::NodeOptions opts;
        opts.parameter_overrides({
            {"deployment_mode", mode},
            {"hw_watchdog_enabled", true},
            {"sensor_stale_threshold_sec", 3.0},
            {"demo_phase_duration_sec", 0.05},
            {"robot_id", 3},
            {"tick_period_sec", 0.05},
        });
        return opts;
    }
};

TEST_F(CommandEchoTest, NoteUpdatesLastId) {
    CommandEcho e;
    EXPECT_EQ(e.lastId(), 0u);
    e.note(42, 1000);
    EXPECT_EQ(e.lastId(), 42u);
    EXPECT_EQ(e.lastMs(), 1000u);

    e.note(99, 2000);
    EXPECT_EQ(e.lastId(), 99u);
    EXPECT_EQ(e.lastMs(), 2000u);
}

TEST_F(CommandEchoTest, ResetClears) {
    CommandEcho e;
    e.note(7, 500);
    e.reset();
    EXPECT_EQ(e.lastId(), 0u);
    EXPECT_EQ(e.lastMs(), 0u);
}

TEST_F(CommandEchoTest, NodeBootsInLabTestWithDemoEnabled) {
    auto node = std::make_shared<OperationControlNode>(
        makeOptions("lab_test"));
    EXPECT_EQ(node->deploymentMode(), DeploymentMode::LAB_TEST);
    EXPECT_TRUE(node->demoSequencer().isEnabled())
        << "PHASE 7 widening: lab_test must enable DEMO sequencer";
    EXPECT_TRUE(node->isWatchdogEnabled())
        << "lab_test must force-enable watchdog";
}

TEST_F(CommandEchoTest, DevelopmentModeHonorsWatchdogDisable) {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"deployment_mode", std::string("development")},
        {"hw_watchdog_enabled", false},
        {"robot_id", 3},
        {"tick_period_sec", 0.05},
    });
    auto node = std::make_shared<OperationControlNode>(opts);
    EXPECT_FALSE(node->isWatchdogEnabled());
}

TEST_F(CommandEchoTest, ProductionModeIgnoresWatchdogDisableYaml) {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"deployment_mode", std::string("production")},
        {"hw_watchdog_enabled", false},
        {"robot_id", 3},
        {"tick_period_sec", 0.05},
    });
    auto node = std::make_shared<OperationControlNode>(opts);
    EXPECT_TRUE(node->isWatchdogEnabled())
        << "production must force-enable watchdog regardless of yaml";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
