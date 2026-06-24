// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-019] McProtocolNode implementation.

#include "san_operation_control/mc_protocol_node.hpp"

#include <boost/crc.hpp>

namespace san_operation_control
{

McProtocolNode::McProtocolNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("mc_protocol_node", opts)
{
  cmd_sub_ = create_subscription<combat_robot_msgs::msg::MCMessage>(
    "/mc/command", rclcpp::QoS(100).reliable(),
    std::bind(
      &McProtocolNode::onCommand, this,
      std::placeholders::_1));

  ack_pub_ = create_publisher<combat_robot_msgs::msg::MCAck>(
    "/mc/ack", rclcpp::QoS(100).reliable());

  validated_pub_ = create_publisher<combat_robot_msgs::msg::MCMessage>(
    "/mc/command_validated", rclcpp::QoS(10).reliable());

  RCLCPP_INFO(
    get_logger(),
    "[DCN-2026-019] McProtocolNode ready "
    "(WINDOW=%zu, /mc/command → /mc/ack + /mc/command_validated)",
    WINDOW_SIZE);
}

std::string McProtocolNode::classifyOutcomeLocked(
  const combat_robot_msgs::msg::MCMessage & msg,
  uint32_t * expected_crc_out) const
{
  const uint32_t expected = computeCrc32(msg.command + msg.payload);
  // Audit B5 (P2) — return expected crc so the caller (onCommand)
  // doesn't recompute it for the RCLCPP_WARN log line.
  if (expected_crc_out) {*expected_crc_out = expected;}
  if (expected != msg.crc32_checksum) {
    return "INVALID_CHECKSUM";
  }
  // ─── Audit finding B1 (P1) fix ──────────────────────────────────
  // Reject ANY seq that lies at or behind the window's low edge, NOT
  // just seqs outside the window. Without this, a seq that was once
  // accepted then evicted from received_seqs_ would silently re-pass
  // (count() == 0) and bypass dedup — a replay-attack window of
  // size WINDOW_SIZE.
  //
  // New rule (per highest_acked anchor):
  //   * seq > highest_acked            → always new → OK (record)
  //   * seq == highest_acked           → DUPLICATE
  //   * highest_acked - seq ≤ WINDOW   → DUPLICATE (within window;
  //                                       must have been seen or be
  //                                       a same-seq retry — sender
  //                                       only ever steps forward)
  //   * highest_acked - seq >  WINDOW  → OUT_OF_ORDER (too old)
  if (highest_acked_ > 0 && msg.seq <= highest_acked_) {
    if ((highest_acked_ - msg.seq) > WINDOW_SIZE) {
      return "OUT_OF_ORDER";
    }
    return "DUPLICATE";
  }
  return "OK";
}

void McProtocolNode::recordAcceptedLocked(uint32_t seq)
{
  received_seqs_.insert(seq);
  while (received_seqs_.size() > WINDOW_SIZE) {
    received_seqs_.erase(received_seqs_.begin());
  }
  if (seq > highest_acked_) {highest_acked_ = seq;}
}

void McProtocolNode::onCommand(
  combat_robot_msgs::msg::MCMessage::SharedPtr msg)
{
  if (msg == nullptr) {return;}

  combat_robot_msgs::msg::MCAck ack;
  ack.stamp = now();
  ack.seq = msg->seq;
  // Audit B2 — classify + record under one lock (atomic decision).
  // Audit B5 — capture expected crc from the lock-side classify so
  // the INVALID_CHECKSUM log line doesn't recompute it.
  bool publish_validated = false;
  uint32_t expected_crc = 0;
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    ack.outcome = classifyOutcomeLocked(*msg, &expected_crc);
    if (ack.outcome == "OK") {
      recordAcceptedLocked(msg->seq);
      publish_validated = true;
    }
  }

  // publish + log outside the lock (rmw publish shouldn't serialize
  // with concurrent onCommand calls under MTE).
  if (publish_validated) {
    validated_pub_->publish(*msg);
  } else if (ack.outcome == "INVALID_CHECKSUM") {
    RCLCPP_WARN(
      get_logger(),
      "Bad checksum seq=%u (got 0x%08x, expected 0x%08x)",
      msg->seq, msg->crc32_checksum, expected_crc);
  }
  ack_pub_->publish(ack);
}

combat_robot_msgs::msg::MCAck McProtocolNode::evaluateForTest(
  const combat_robot_msgs::msg::MCMessage & msg) const
{
  combat_robot_msgs::msg::MCAck ack;
  ack.seq = msg.seq;
  std::lock_guard<std::mutex> lock(state_mu_);
  ack.outcome = classifyOutcomeLocked(msg);
  return ack;
}

combat_robot_msgs::msg::MCAck McProtocolNode::evaluateAndUpdateForTest(
  const combat_robot_msgs::msg::MCMessage & msg)
{
  combat_robot_msgs::msg::MCAck ack;
  ack.seq = msg.seq;
  std::lock_guard<std::mutex> lock(state_mu_);
  ack.outcome = classifyOutcomeLocked(msg);
  if (ack.outcome == "OK") {
    recordAcceptedLocked(msg.seq);
  }
  return ack;
}

uint32_t McProtocolNode::computeCrc32(const std::string & s)
{
  boost::crc_32_type crc;
  crc.process_bytes(s.data(), s.size());
  return crc.checksum();
}

}  // namespace san_operation_control
