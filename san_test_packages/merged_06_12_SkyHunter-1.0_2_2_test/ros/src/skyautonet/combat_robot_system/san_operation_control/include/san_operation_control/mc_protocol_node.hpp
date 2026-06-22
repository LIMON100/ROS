// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-019] McProtocolNode — receiver side of MC wire-level
// protocol. Validates seq + crc32, dedups duplicates, drops
// out-of-order beyond the 16-deep sliding window, and republishes
// accepted commands on /mc/command_validated for the mission layer.

#pragma once

#include <cstdint>
#include <mutex>
#include <set>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/mc_ack.hpp>
#include <combat_robot_msgs/msg/mc_message.hpp>

namespace san_operation_control
{

class McProtocolNode : public rclcpp::Node
{
public:
  static constexpr std::size_t WINDOW_SIZE = 16;

  explicit McProtocolNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

  // ─── Test seam — exposes the validation pipeline without spawning
  // a publisher/subscriber pair. Returns the MCAck the receiver
  // would publish for `msg`; side-effects (sliding-window update +
  // /mc/command_validated publish) are skipped so unit tests can
  // assert outcomes independently. evaluateAndUpdateForTest()
  // applies the side-effects to mirror the production path.
  combat_robot_msgs::msg::MCAck evaluateForTest(
    const combat_robot_msgs::msg::MCMessage & msg) const;

  combat_robot_msgs::msg::MCAck evaluateAndUpdateForTest(
    const combat_robot_msgs::msg::MCMessage & msg);

  /// Public crc32 helper — symmetric with McSenderNode::computeCrc32.
  static uint32_t computeCrc32(const std::string & s);

  std::size_t windowSizeForTest() const
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    return received_seqs_.size();
  }
  uint32_t highestAckedForTest() const
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    return highest_acked_;
  }

private:
  void onCommand(combat_robot_msgs::msg::MCMessage::SharedPtr msg);

  /// Compute outcome without mutating state.
  /// Caller is responsible for holding state_mu_.
  /// `expected_crc_out`, if non-null, receives the crc32(command +
  /// payload) computed once during classification — avoids the
  /// audit-B5 double-compute in onCommand's INVALID_CHECKSUM log.
  std::string classifyOutcomeLocked(
    const combat_robot_msgs::msg::MCMessage & msg,
    uint32_t * expected_crc_out = nullptr) const;

  /// Insert seq + maintain sliding-window invariant.
  /// Caller is responsible for holding state_mu_.
  void recordAcceptedLocked(uint32_t seq);

  rclcpp::Subscription<combat_robot_msgs::msg::MCMessage>::SharedPtr cmd_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::MCAck>::SharedPtr ack_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::MCMessage>::SharedPtr validated_pub_;

  // ─── Audit B2 (P2) fix ──────────────────────────────────────────
  // Guards received_seqs_ + highest_acked_ against concurrent
  // mutation from onCommand under MTE (single subscription callback
  // is safe alone, but test seams + accessor calls from other
  // threads weren't). All access through `*Locked` helpers.
  mutable std::mutex state_mu_;
  std::set<uint32_t> received_seqs_;
  // Audit B3 (P3) note: uint32 wraparound at 4 billion msgs. At 1 kHz
  // sustained that's ~46 days continuous — implausible for an
  // operator session. If sustained-multi-week operation becomes a
  // requirement, promote to uint64 + adjust MCMessage.msg schema
  // (separate DCN — schema change).
  uint32_t highest_acked_ = 0;
};

}  // namespace san_operation_control
