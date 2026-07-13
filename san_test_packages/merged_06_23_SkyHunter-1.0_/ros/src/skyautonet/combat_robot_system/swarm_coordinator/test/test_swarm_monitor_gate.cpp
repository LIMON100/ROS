// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-013 swarm_monitor_node gate gtest.
//
// 4 cases mapped to T1..T4 acceptance (spec):
//   T1  Hub      → publisher CREATED
//   T2  Leader   → publisher CREATED
//   T3  Follower → publisher SUPPRESSED
//   T4  Deputy   → publisher SUPPRESSED
//
// Uses publisherExistsForTest() so the assertion is observable WITHOUT
// spinning the executor (the constructor decides the gate before any
// timer fires) and WITHOUT a live TF graph (TF buffer is unconditionally
// created — only the PUBLISHER is gated).

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "swarm_coordinator/swarm_monitor_node.hpp"

using swarm_coordinator::SwarmMonitorNode;

namespace
{

// Per-test rclpy lifecycle: each TEST_F gets a clean context. The
// monitor node touches no global state outside its own publishers /
// timers / TF buffer, so init/shutdown bracketing is enough.
class SwarmMonitorGateTest : public ::testing::Test
{
protected:
  void SetUp() override {rclcpp::init(0, nullptr);}
  void TearDown() override {rclcpp::shutdown();}

  std::shared_ptr<SwarmMonitorNode> makeNode(const std::string & role)
  {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({rclcpp::Parameter("robot_role", role)});
    return std::make_shared<SwarmMonitorNode>(opts);
  }
};

}  // namespace

// ----------------------------------------------------------------- T1
// Hub role → publisher is created.
TEST_F(SwarmMonitorGateTest, T1_HubRoleCreatesPublisher) {
  auto node = makeNode("hub");
  EXPECT_TRUE(node->isPublisherEnabled());
  EXPECT_TRUE(node->publisherExistsForTest())
    << "Hub role must create /swarm/poses publisher";
}

// ----------------------------------------------------------------- T2
// Leader role → publisher is created.
TEST_F(SwarmMonitorGateTest, T2_LeaderRoleCreatesPublisher) {
  auto node = makeNode("leader");
  EXPECT_TRUE(node->isPublisherEnabled());
  EXPECT_TRUE(node->publisherExistsForTest())
    << "Leader role must create /swarm/poses publisher";
}

// ----------------------------------------------------------------- T3
// Follower role → publisher is suppressed (subscribe-only mode).
TEST_F(SwarmMonitorGateTest, T3_FollowerRoleSuppressesPublisher) {
  auto node = makeNode("follower");
  EXPECT_FALSE(node->isPublisherEnabled());
  EXPECT_FALSE(node->publisherExistsForTest())
    << "Follower role must NOT create /swarm/poses publisher "
    "(prevents N-way redundant publish + race condition).";
}

// ----------------------------------------------------------------- T4
// Deputy role → publisher is suppressed (until promotion to Hub).
TEST_F(SwarmMonitorGateTest, T4_DeputyRoleSuppressesPublisher) {
  auto node = makeNode("deputy");
  EXPECT_FALSE(node->isPublisherEnabled());
  EXPECT_FALSE(node->publisherExistsForTest())
    << "Deputy role must NOT create /swarm/poses publisher until "
    "promoted to Hub via Hub-Deputy redundancy (DCN-2026-001 D-005)";
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
