// SAN v1.3 PHASE 0 - deployment_mode parsing + auth-token enforcement.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <cstdlib>
#include <string>

#include "combat_robot_operation_system/combat_robot_operation_system.hpp"

using combat_robot_operation_system::CombatRobotOperationSystem;
using combat_robot_operation_system::DeploymentMode;

class DeploymentModeTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
        // Wipe the token so each test starts from a known state.
#ifdef _WIN32
        _putenv_s("DEVELOPER_AUTH_TOKEN", "");
#else
        unsetenv("DEVELOPER_AUTH_TOKEN");
#endif
    }

    rclcpp::NodeOptions makeOpts(const std::string& mode,
                                  bool auth_required = false)
    {
        rclcpp::NodeOptions opts;
        opts.parameter_overrides({
            {"deployment_mode", mode},
            {"safety.developer_auth_required", auth_required},
            {"safety.hw_watchdog_enabled", true},
            {"heartbeat_period_sec", 1.0},
        });
        return opts;
    }

    void setToken(const std::string& v) {
#ifdef _WIN32
        _putenv_s("DEVELOPER_AUTH_TOKEN", v.c_str());
#else
        setenv("DEVELOPER_AUTH_TOKEN", v.c_str(), 1);
#endif
    }
};

TEST_F(DeploymentModeTest, AllFiveModesParse) {
    for (const auto& mode :
         {"production", "demo", "lab_test", "bench", "development"})
    {
        // Development needs the env token when developer_auth_required.
        const bool is_dev = std::string(mode) == "development";
        if (is_dev) setToken("test-token-abc123");
        auto node = std::make_shared<CombatRobotOperationSystem>(
            makeOpts(mode, /*auth_required=*/is_dev));
        EXPECT_NO_THROW(node->initialize());
    }
}

TEST_F(DeploymentModeTest, InvalidModeThrows) {
    auto node = std::make_shared<CombatRobotOperationSystem>(
        makeOpts("invalid_mode"));
    EXPECT_THROW(node->initialize(), std::runtime_error);
}

TEST_F(DeploymentModeTest, DevelopmentRequiresAuth) {
    auto node = std::make_shared<CombatRobotOperationSystem>(
        makeOpts("development", /*auth_required=*/true));
    EXPECT_THROW(node->initialize(), std::runtime_error);
}

TEST_F(DeploymentModeTest, DevelopmentAuthFlagDisabledAllowsBoot) {
    auto node = std::make_shared<CombatRobotOperationSystem>(
        makeOpts("development", /*auth_required=*/false));
    EXPECT_NO_THROW(node->initialize());
}

TEST_F(DeploymentModeTest, WeaponsAllowedOnlyInProduction) {
    setToken("ok");
    auto check = [&](const std::string& mode, bool expected) {
        const bool is_dev = (mode == "development");
        auto node = std::make_shared<CombatRobotOperationSystem>(
            makeOpts(mode, /*auth_required=*/is_dev));
        ASSERT_NO_THROW(node->initialize()) << "mode=" << mode;
        EXPECT_EQ(node->weaponsAllowed(), expected) << "mode=" << mode;
    };
    check("production",  true);
    check("demo",        false);
    check("lab_test",    false);
    check("bench",       false);
    check("development", false);
}

TEST_F(DeploymentModeTest, WatchdogForcedExceptInDevelopment) {
    setToken("ok");
    for (const auto& mode :
         {"production", "demo", "lab_test", "bench"})
    {
        rclcpp::NodeOptions opts;
        opts.parameter_overrides({
            {"deployment_mode", std::string(mode)},
            {"safety.developer_auth_required", false},
            {"safety.hw_watchdog_enabled", false},     // try to disable
        });
        auto node = std::make_shared<CombatRobotOperationSystem>(opts);
        ASSERT_NO_THROW(node->initialize());
        EXPECT_TRUE(node->watchdogEnabled())
            << "watchdog must be force-on in mode=" << mode;
    }
    // Only development honors the yaml flag.
    {
        rclcpp::NodeOptions opts;
        opts.parameter_overrides({
            {"deployment_mode", std::string("development")},
            {"safety.developer_auth_required", false},
            {"safety.hw_watchdog_enabled", false},
        });
        auto node = std::make_shared<CombatRobotOperationSystem>(opts);
        ASSERT_NO_THROW(node->initialize());
        EXPECT_FALSE(node->watchdogEnabled())
            << "development must honor yaml hw_watchdog_enabled=false";
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
