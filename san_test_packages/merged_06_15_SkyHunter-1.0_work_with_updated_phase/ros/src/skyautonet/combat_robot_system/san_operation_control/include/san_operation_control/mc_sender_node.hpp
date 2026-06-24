// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-019] McSenderNode — sender side of MC wire-level protocol.
// Accepts raw operator commands on /mc/raw_command, stamps each with
// seq + crc32 + publishes on /mc/command. Tracks pending entries +
// retransmits if /mc/ack OK doesn't arrive within ack_timeout_ms.
// After max_retries, publishes the unacked seq on /mc/timeout so
// upstream mission layer can decide escalation.

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/mc_ack.hpp>
#include <combat_robot_msgs/msg/mc_message.hpp>
#include <std_msgs/msg/u_int32.hpp>

namespace san_operation_control
{

struct PendingCommand
{
  combat_robot_msgs::msg::MCMessage msg;
  rclcpp::Time first_sent;
  rclcpp::Time last_sent;
  int retries = 0;
};

class McSenderNode : public rclcpp::Node
{
public:
  explicit McSenderNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

  // ─── Test seams ──────────────────────────────────────────────────
  /// Stamp + checksum a raw command + record as pending. Returns the
  /// stamped message (seq populated, ready to publish).
  combat_robot_msgs::msg::MCMessage stampForTest(
    combat_robot_msgs::msg::MCMessage raw);

  /// Manually mark a seq as acked (drives onAck path without ROS).
  void ackForTest(uint32_t seq, const std::string & outcome);

  /// Pending count (test asserts on retransmit/erase).
  std::size_t pendingCount() const
  {
    std::lock_guard<std::mutex> lock(pending_mu_);
    return pending_.size();
  }

  /// Public crc32 helper — symmetric with McProtocolNode::computeCrc32.
  static uint32_t computeCrc32(const std::string & s);

  int ackTimeoutMs() const {return ack_timeout_ms_;}
  int maxRetries()   const {return max_retries_;}

private:
  void onRawCommand(combat_robot_msgs::msg::MCMessage::SharedPtr msg);
  void onAck(combat_robot_msgs::msg::MCAck::SharedPtr msg);
  void retransmitTick();

  rclcpp::Subscription<combat_robot_msgs::msg::MCMessage>::SharedPtr raw_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::MCAck>::SharedPtr ack_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::MCMessage>::SharedPtr cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::UInt32>::SharedPtr timeout_pub_;
  rclcpp::TimerBase::SharedPtr retransmit_timer_;

  // ─── Audit finding B10 (P1) fix ─────────────────────────────────
  // Guards `pending_` + `next_seq_` against concurrent access from
  // (a) `onRawCommand`  — subscription callback,
  // (b) `onAck`         — subscription callback,
  // (c) `retransmitTick` — wall-timer callback.
  // Under MultiThreadedExecutor these three callbacks may execute on
  // different threads, where the for-loop in retransmitTick was
  // vulnerable to iterator invalidation when onAck called erase()
  // mid-iteration. SingleThreadedExecutor was already safe; the
  // mutex makes MTE safe too with negligible single-thread overhead.
  mutable std::mutex pending_mu_;
  std::map<uint32_t, PendingCommand> pending_;
  uint32_t next_seq_ = 1;
  int ack_timeout_ms_ = 500;
  int max_retries_ = 3;
};

}  // namespace san_operation_control
