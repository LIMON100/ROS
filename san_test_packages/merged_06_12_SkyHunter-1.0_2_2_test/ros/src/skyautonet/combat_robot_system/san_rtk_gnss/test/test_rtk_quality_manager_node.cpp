// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — RTK Fix→Float mitigation Layer 3: rtk_quality_manager node.
//
// Drives the node through its deterministic test hooks (inject a fix
// status + tick at an injected time) — no executor, no real clock — to
// verify it maps FSM state onto NavConstraints and emits a ThreatAlert
// exactly once on the LOST transition.

#include "san_rtk_gnss/rtk_quality_manager_node.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/rtk_fix_status.hpp>
#include <combat_robot_msgs/msg/nav_constraints.hpp>

using san_rtk_gnss::RtkQualityManagerNode;
using san_rtk_gnss::RtkQualityState;
using RtkStatus = combat_robot_msgs::msg::RtkFixStatus;
using NavConstraints = combat_robot_msgs::msg::NavConstraints;

class RtkQualityManagerTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
  }
  std::shared_ptr<RtkQualityManagerNode> makeNode()
  {
    rclcpp::NodeOptions o;
    o.parameter_overrides(
    {
      rclcpp::Parameter("robot_id", "3"),
      rclcpp::Parameter("grace_sec", 5.0),
      rclcpp::Parameter("active_lost_sec", 30.0),
      rclcpp::Parameter("recover_sec", 2.0),
      rclcpp::Parameter("sensor_timeout_sec", 2.0),
    });
    return std::make_shared<RtkQualityManagerNode>(o);
  }
};

TEST_F(RtkQualityManagerTest, FixKeepsOkWithNominalSpeed) {
  auto node = makeNode();
  node->injectStatusForTest(RtkStatus::FIX_RTK_FIX, 0.0);
  const NavConstraints c = node->tickForTest(0.0);
  EXPECT_EQ(c.rtk_state, NavConstraints::RTK_OK);
  EXPECT_FLOAT_EQ(c.max_speed_mps, 1.5f);
  EXPECT_FALSE(c.formation_loose);
  EXPECT_EQ(c.source_robot_id, "3");
}

TEST_F(RtkQualityManagerTest, FloatEscalatesToLostAndAlertsOnce) {
  auto node = makeNode();
  node->injectStatusForTest(RtkStatus::FIX_RTK_FIX, 0.0);
  node->tickForTest(0.0);

  // RTK_FLOAT (fresh but not Fixed) → grace, then active, then lost.
  node->injectStatusForTest(RtkStatus::FIX_RTK_FLOAT, 1.0);
  EXPECT_EQ(node->tickForTest(1.0).rtk_state, NavConstraints::RTK_DEGRADED_GRACE);

  node->injectStatusForTest(RtkStatus::FIX_RTK_FLOAT, 7.0);
  EXPECT_EQ(
    node->tickForTest(7.0).rtk_state, NavConstraints::RTK_DEGRADED_ACTIVE);

  node->injectStatusForTest(RtkStatus::FIX_RTK_FLOAT, 32.0);
  const NavConstraints lost = node->tickForTest(32.0);
  EXPECT_EQ(lost.rtk_state, NavConstraints::RTK_LOST);
  EXPECT_FLOAT_EQ(lost.max_speed_mps, 0.5f);     // defensive
  EXPECT_TRUE(lost.formation_loose);
  EXPECT_EQ(node->threatAlertsEmitted(), 1u);

  // Staying lost must not re-emit the alert.
  node->injectStatusForTest(RtkStatus::FIX_RTK_FLOAT, 33.0);
  node->tickForTest(33.0);
  EXPECT_EQ(node->threatAlertsEmitted(), 1u);
}

TEST_F(RtkQualityManagerTest, SilentReceiverEscalatesViaTimeout) {
  auto node = makeNode();
  node->injectStatusForTest(RtkStatus::FIX_RTK_FIX, 0.0);
  node->tickForTest(0.0);
  // No further status injected: the fix goes stale (> sensor_timeout) so
  // ticks alone drive escalation even though the last fix WAS Fixed.
  EXPECT_EQ(node->tickForTest(3.0).rtk_state, NavConstraints::RTK_DEGRADED_GRACE);
  EXPECT_EQ(node->tickForTest(40.0).rtk_state, NavConstraints::RTK_LOST);
}

TEST_F(RtkQualityManagerTest, SustainedFixRecoversToOk) {
  auto node = makeNode();
  node->injectStatusForTest(RtkStatus::FIX_RTK_FLOAT, 0.0);
  node->tickForTest(0.0);
  node->injectStatusForTest(RtkStatus::FIX_RTK_FLOAT, 8.0);
  node->tickForTest(8.0);
  ASSERT_EQ(node->fsmState(), RtkQualityState::DegradedActive);

  node->injectStatusForTest(RtkStatus::FIX_RTK_FIX, 10.0);
  EXPECT_NE(node->tickForTest(10.0).rtk_state, NavConstraints::RTK_OK);
  node->injectStatusForTest(RtkStatus::FIX_RTK_FIX, 12.5);
  EXPECT_EQ(node->tickForTest(12.5).rtk_state, NavConstraints::RTK_OK);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  const int rc = RUN_ALL_TESTS();
  if (rclcpp::ok()) {rclcpp::shutdown();}
  return rc;
}
