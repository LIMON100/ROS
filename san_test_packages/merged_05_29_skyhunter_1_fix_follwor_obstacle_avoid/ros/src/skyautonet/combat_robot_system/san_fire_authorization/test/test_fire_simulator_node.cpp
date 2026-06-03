// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-018] FireSimulatorNode tests — 5 cases.
//
// Pure-logic via the FireSimulatorNode::evaluateForTest seam — no
// live ROS graph spin required. Each case constructs the node with
// minimal NodeOptions, drives the test seam, and asserts the
// resulting combat_robot_msgs/FireResult.

#include <cmath>
#include <memory>

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/fire_authorization_response.hpp>
#include <combat_robot_msgs/msg/fire_result.hpp>

#include "san_fire_authorization/fire_simulator_node.hpp"

namespace san_fire_authorization
{
namespace
{

using FireResp = combat_robot_msgs::msg::FireAuthorizationResponse;
using FireRes = combat_robot_msgs::msg::FireResult;

class FireSimFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
    rclcpp::NodeOptions opts;
    opts.parameter_overrides(
        {
          rclcpp::Parameter("robot_id", 1),
          rclcpp::Parameter("alignment_tolerance_deg", 2.0),
          rclcpp::Parameter("target_pan_offset_rad", 0.0),
          rclcpp::Parameter("target_tilt_offset_rad", 0.0),
        });
    node_ = std::make_shared<FireSimulatorNode>(opts);
  }

  FireResp grantedResponse(uint32_t request_id)
  {
    FireResp r;
    r.request_id = request_id;
    r.sequence = 1;
    r.response_timestamp_ms = 1'700'000'000'000ULL;
    r.granted = true;
    r.reason = FireResp::REASON_GRANTED;
    r.reason_detail = "OK";
    r.audit_log_uuid = "uuid-test-1234";
    return r;
  }

  FireResp deniedResponse(uint32_t request_id)
  {
    FireResp r = grantedResponse(request_id);
    r.granted = false;
    r.reason = FireResp::REASON_DENIED_HMAC_FAIL;
    r.reason_detail = "HMAC fail";
    return r;
  }

  std::shared_ptr<FireSimulatorNode> node_;
};

// ─── T1: granted + aligned (gimbal at target) → RESULT_SUCCESS ───────────
TEST_F(FireSimFixture, T1_GrantedAndAlignedPublishesHit) {
  node_->setGimbalForTest(0.0, 0.0);          // gimbal at origin
  node_->setTargetForTest(0.0, 0.0, 42);      // target at origin → 0° err

  auto fr = node_->evaluateForTest(grantedResponse(100));
  EXPECT_EQ(fr.result, FireRes::RESULT_SUCCESS);
  EXPECT_EQ(fr.rounds_fired, 1u);
  EXPECT_EQ(fr.target_id, 42u);
  EXPECT_EQ(fr.command_id, 100u);
  EXPECT_EQ(fr.authorization_chain, "uuid-test-1234");
  EXPECT_FLOAT_EQ(fr.confidence, 1.0f);
}

// ─── T2: not granted → RESULT_NO_AUTHORIZATION, no rounds fired ──────────
TEST_F(FireSimFixture, T2_NotGrantedYieldsNoAuthorization) {
  node_->setGimbalForTest(0.0, 0.0);
  node_->setTargetForTest(0.0, 0.0, 7);

  auto fr = node_->evaluateForTest(deniedResponse(101));
  EXPECT_EQ(fr.result, FireRes::RESULT_NO_AUTHORIZATION);
  EXPECT_EQ(fr.rounds_fired, 0u);
  EXPECT_EQ(fr.command_id, 101u);
}

// ─── T3: granted + 5° misalignment → RESULT_MISS ─────────────────────────
TEST_F(FireSimFixture, T3_MisalignedFiveDegreesYieldsMiss) {
  const double five_deg = 5.0 * M_PI / 180.0;
  node_->setGimbalForTest(0.0, 0.0);
  node_->setTargetForTest(five_deg, 0.0, 9);     // target 5° off → MISS

  auto fr = node_->evaluateForTest(grantedResponse(102));
  EXPECT_EQ(fr.result, FireRes::RESULT_MISS);
  EXPECT_EQ(fr.rounds_fired, 1u);
  EXPECT_FLOAT_EQ(fr.confidence, 0.0f);
  EXPECT_NE(fr.notes.find("Outside tolerance"), std::string::npos);
}

// ─── T4: gimbal state cached → next evaluation uses latest gimbal ────────
TEST_F(FireSimFixture, T4_GimbalCacheIsApplied) {
  node_->setTargetForTest(0.0, 0.0, 1);

  // Initial gimbal at origin → HIT
  node_->setGimbalForTest(0.0, 0.0);
  auto fr1 = node_->evaluateForTest(grantedResponse(200));
  EXPECT_EQ(fr1.result, FireRes::RESULT_SUCCESS);

  // Gimbal slewed away → MISS without changing the target
  node_->setGimbalForTest(0.1, 0.0);     // 5.7° off
  auto fr2 = node_->evaluateForTest(grantedResponse(201));
  EXPECT_EQ(fr2.result, FireRes::RESULT_MISS);
}

// ─── T5: FireResult carries gimbal readings + auth chain ─────────────────
TEST_F(FireSimFixture, T5_ResultIncludesGimbalAndAuthChain) {
  node_->setGimbalForTest(0.123, 0.456);
  node_->setTargetForTest(0.123, 0.456, 77);

  auto resp = grantedResponse(300);
  resp.audit_log_uuid = "AUDIT-CHAIN-300";
  auto fr = node_->evaluateForTest(resp);

  EXPECT_FLOAT_EQ(fr.impact_point_x_m, 0.123f);     // pan
  EXPECT_FLOAT_EQ(fr.impact_point_y_m, 0.456f);     // tilt
  EXPECT_EQ(fr.authorization_chain, "AUDIT-CHAIN-300");
  EXPECT_EQ(fr.robot_id, 1u);
  EXPECT_GT(fr.timestamp_report_ms, 0u);
  EXPECT_EQ(fr.timestamp_fire_ms, resp.response_timestamp_ms);
}

}  // namespace
}  // namespace san_fire_authorization

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  const int rc = RUN_ALL_TESTS();
  if (rclcpp::ok()) {rclcpp::shutdown();}
  return rc;
}
