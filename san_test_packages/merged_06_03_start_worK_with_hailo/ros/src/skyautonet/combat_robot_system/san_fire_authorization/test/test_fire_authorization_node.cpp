// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — FireAuthorizationNode ROS-level integration test.
//
// This complements test_integration_scenarios.cpp (which tests the
// gate logic standalone) by exercising the actual rclcpp plumbing:
//   * Topic publish/subscribe matching
//   * QoS handshake (RELIABLE / depth=10 for fire topics)
//   * 10 Hz tick timer (Two-key timeout detection)
//   * OperationState heartbeat consumption (limp_mode flag)
//
// Runs only under `colcon test` (requires rclcpp + combat_robot_msgs
// generated headers at compile time). The CMakeLists guards with
// find_package(rclcpp QUIET) so missing rclcpp doesn't break the
// build elsewhere.

#include <chrono>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/fire_authorization_request.hpp>
#include <combat_robot_msgs/msg/fire_authorization_response.hpp>
#include <combat_robot_msgs/msg/operation_state.hpp>

#include "san_fire_authorization/fire_authorization_node.hpp"
#include "san_fire_authorization/hmac_authenticator.hpp"

namespace san_fire_authorization
{
namespace
{

using FireReq = combat_robot_msgs::msg::FireAuthorizationRequest;
using FireResp = combat_robot_msgs::msg::FireAuthorizationResponse;
using OpState = combat_robot_msgs::msg::OperationState;
using namespace std::chrono_literals;

class NodeFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // Init rclcpp once per test process.
    if (!rclcpp::ok()) {
      rclcpp::init(0, nullptr);
    }
    // Write a test secret to a temp path with mode 0400.
    secret_path_ = "/tmp/san_test_mesh_secret.bin";
    audit_path_ = "/tmp/san_test_fire_audit.log";

    {
      std::ofstream f(secret_path_, std::ios::binary);
      for (int i = 0; i < 32; ++i) {f.put(0x55);}
    }
    ::chmod(secret_path_.c_str(), 0400);
    // Pre-load the secret for sign() side.
    std::array<uint8_t, kHmacSha256Bytes> s{};
    s.fill(0x55);
    signer_ = std::make_unique<HmacAuthenticator>(s);

    // Construct the node with our test parameters.
    rclcpp::NodeOptions opts;
    opts.parameter_overrides(
        {
          rclcpp::Parameter("secret_path", secret_path_),
          rclcpp::Parameter("audit_log_path", audit_path_),
          rclcpp::Parameter("rotation_bytes", 10 * 1024 * 1024),
        });
    node_ = std::make_shared<FireAuthorizationNode>(opts);

    // Helper test publisher + subscriber on a sibling node.
    helper_ = std::make_shared<rclcpp::Node>("test_helper");
    req_pub_ = helper_->create_publisher<FireReq>(
      "/swarm/fire/authorization_request", rclcpp::QoS(10).reliable());
    op_pub_ = helper_->create_publisher<OpState>(
      "/swarm/operation_state", rclcpp::QoS(1).best_effort());
    resp_sub_ = helper_->create_subscription<FireResp>(
      "/swarm/fire/authorization_response", rclcpp::QoS(10).reliable(),
      [this](FireResp::SharedPtr m) {responses_.push_back(*m);});

    executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    executor_->add_node(node_);
    executor_->add_node(helper_);
  }

  void TearDown() override
  {
    executor_.reset();
    node_.reset();
    helper_.reset();
    ::unlink(secret_path_.c_str());
    ::unlink(audit_path_.c_str());
  }

  void spinFor(std::chrono::milliseconds dur)
  {
    const auto deadline = std::chrono::steady_clock::now() + dur;
    while (std::chrono::steady_clock::now() < deadline && rclcpp::ok()) {
      executor_->spin_some(50ms);
    }
  }

  uint64_t nowMs()
  {
    return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
  }

  FireReq buildSignedRequest(uint32_t req_id, uint8_t cmd, uint64_t nonce)
  {
    FireReq m;
    m.header.stamp = helper_->now();
    m.request_id = req_id;
    m.sequence = 1;
    m.operator_id = "op_test";
    m.nonce = nonce;
    m.request_timestamp_ms = nowMs();
    m.command_type = cmd;
    m.target_lat_e7 = 374200000;
    m.target_lon_e7 = 1270000000;
    m.target_alt_mm = 250;

    AuthMessage am;
    am.request_id = m.request_id;
    am.sequence = m.sequence;
    am.operator_id = m.operator_id;
    am.nonce = m.nonce;
    am.request_timestamp_ms = m.request_timestamp_ms;
    am.command_type = m.command_type;
    am.target_lat_e7 = m.target_lat_e7;
    am.target_lon_e7 = m.target_lon_e7;
    am.target_alt_mm = m.target_alt_mm;
    m.hmac_signature = signer_->sign(am);
    return m;
  }

  std::string secret_path_;
  std::string audit_path_;
  std::unique_ptr<HmacAuthenticator> signer_;
  std::shared_ptr<FireAuthorizationNode> node_;
  std::shared_ptr<rclcpp::Node> helper_;
  rclcpp::Publisher<FireReq>::SharedPtr req_pub_;
  rclcpp::Publisher<OpState>::SharedPtr op_pub_;
  rclcpp::Subscription<FireResp>::SharedPtr resp_sub_;
  std::shared_ptr<rclcpp::executors::SingleThreadedExecutor> executor_;
  std::vector<FireResp> responses_;
};

// ─── Node-level scenarios ──────────────────────────────────────────────

TEST_F(NodeFixture, NormalKey1Key2RoundTripGrant) {
  // KEY1
  req_pub_->publish(buildSignedRequest(/*req=*/ 300, /*cmd=*/ 1, /*nonce=*/ 0xAA));
  spinFor(200ms);
  ASSERT_EQ(responses_.size(), 1u);
  EXPECT_FALSE(responses_[0].granted);  // ARMED → granted=false
  EXPECT_EQ(
    responses_[0].reason,
    FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE);

  // KEY2
  req_pub_->publish(buildSignedRequest(/*req=*/ 300, /*cmd=*/ 2, /*nonce=*/ 0xAB));
  spinFor(200ms);
  ASSERT_EQ(responses_.size(), 2u);
  EXPECT_TRUE(responses_[1].granted);
  EXPECT_EQ(responses_[1].reason, FireResp::REASON_GRANTED);
  EXPECT_FALSE(responses_[1].audit_log_uuid.empty());
}

TEST_F(NodeFixture, LimpModeGrantTagsLimpModeFireTrue) {
  // Publish operation_state with in_limp_mode=true; wait for node to receive.
  OpState op;
  op.in_limp_mode = true;
  op.n_alive_robots = 4;   // minimum squadron
  op.timestamp_ms = nowMs();
  op_pub_->publish(op);
  spinFor(200ms);

  // KEY1 + KEY2 (within timeout)
  req_pub_->publish(buildSignedRequest(400, 1, 0xCC));
  spinFor(150ms);
  req_pub_->publish(buildSignedRequest(400, 2, 0xCD));
  spinFor(150ms);

  ASSERT_GE(responses_.size(), 2u);
  const auto & last = responses_.back();
  EXPECT_TRUE(last.granted);
  EXPECT_EQ(last.reason, FireResp::REASON_GRANTED);
  EXPECT_TRUE(last.limp_mode_fire)
    << "DCN-2026-001 D-004: limp_mode_fire must be true when granting in Limp Mode";
}

TEST_F(NodeFixture, TickTimerCatchesKey1Timeout) {
  // KEY1 only — no KEY2.
  req_pub_->publish(buildSignedRequest(500, 1, 0xDD));
  spinFor(200ms);
  ASSERT_EQ(responses_.size(), 1u);

  // Wait longer than 5s so the 10 Hz tick fires the timeout. To keep
  // test runtime reasonable we use 6 seconds.
  spinFor(6000ms);

  // No new response is published on tick-detected timeout (it's an
  // internal reset). But a subsequent KEY2 should now fail as
  // DENIED_INCOMPLETE because the state was reset.
  req_pub_->publish(buildSignedRequest(500, 2, 0xDE));
  spinFor(200ms);
  ASSERT_GE(responses_.size(), 2u);
  EXPECT_FALSE(responses_.back().granted);
  EXPECT_EQ(
    responses_.back().reason,
    FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE);
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
