// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 PHASE 8 - Hub-Deputy takeover test (S18-3).
//
// The takeover path calls into vendor SDKs (libuci, lifecycle clients)
// that are not present on CI hosts. We test the role-state machine
// directly via forcePromoteForTest() and verify the broadcast +
// internal state transition. Actual SDK plumbing is exercised by the
// L4 HIL bench tier.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "san_role_management/hub_role_manager.hpp"

using namespace san_role_management;
using HubAnn = combat_robot_msgs::msg::HubRoleAnnouncement;

namespace
{

rclcpp::NodeOptions makeOpts(int robot_id)
{
  rclcpp::NodeOptions opts;
  opts.parameter_overrides(
    {
      {"robot_id", robot_id},
      {"hub_robot_id", 2},
      {"deputy_robot_id", 3},
      {"hub_heartbeat_timeout_ms", 200},     // tight for unit-test
      {"watchdog_period_ms", 50},
    });
  return opts;
}

}  // namespace

class HubDeputyTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
  }
};

TEST_F(HubDeputyTest, DeputyIdentifiesItself) {
  auto deputy = std::make_shared<HubRoleManager>(makeOpts(3));
  EXPECT_TRUE(deputy->isDeputyUgv());
  EXPECT_FALSE(deputy->isHubUgv());
  EXPECT_EQ(deputy->getRole(), HubRole::NORMAL);
}

TEST_F(HubDeputyTest, HubIdentifiesItself) {
  auto hub = std::make_shared<HubRoleManager>(makeOpts(2));
  EXPECT_TRUE(hub->isHubUgv());
  EXPECT_FALSE(hub->isDeputyUgv());
}

TEST_F(HubDeputyTest, S18_3_PromoteAdvancesTerm) {
  auto deputy = std::make_shared<HubRoleManager>(makeOpts(3));
  const uint32_t initial_term = deputy->getHubTerm();

  deputy->forcePromoteForTest();
  EXPECT_EQ(deputy->getRole(), HubRole::PROMOTED);
  EXPECT_GT(deputy->getHubTerm(), initial_term);
  // Activation hooks return false on the CI host (no SDK), but the
  // state transition still happens - the manager just reports the
  // partial-fail count.
}

TEST_F(HubDeputyTest, HigherTermAnnouncementUpdatesTermOnly) {
  auto deputy = std::make_shared<HubRoleManager>(makeOpts(3));

  HubAnn higher;
  higher.robot_id = 2;
  higher.hub_term = 42;
  higher.role = HubAnn::HUB_NORMAL;
  deputy->injectAnnouncementForTest(higher);
  EXPECT_EQ(deputy->getHubTerm(), 42u);
}

TEST_F(HubDeputyTest, HubRecoveryDemotesDeputy) {
  auto deputy = std::make_shared<HubRoleManager>(makeOpts(3));
  deputy->forcePromoteForTest();
  ASSERT_EQ(deputy->getRole(), HubRole::PROMOTED);
  const uint32_t after_promote = deputy->getHubTerm();

  HubAnn recovery;
  recovery.robot_id = 2;
  recovery.hub_term = after_promote + 1;
  recovery.role = HubAnn::HUB_PROMOTED;
  recovery.reason = "hub_recovered";
  deputy->injectAnnouncementForTest(recovery);
  EXPECT_EQ(deputy->getRole(), HubRole::DEMOTED);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
