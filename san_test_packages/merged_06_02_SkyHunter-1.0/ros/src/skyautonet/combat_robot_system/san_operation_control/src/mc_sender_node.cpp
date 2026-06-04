// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-019] McSenderNode implementation.

#include "san_operation_control/mc_sender_node.hpp"

#include <vector>

#include <boost/crc.hpp>

namespace san_operation_control
{

McSenderNode::McSenderNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("mc_sender_node", opts)
{
  declare_parameter<int>("ack_timeout_ms", 500);
  declare_parameter<int>("max_retries", 3);
  ack_timeout_ms_ = get_parameter("ack_timeout_ms").as_int();
  max_retries_ = get_parameter("max_retries").as_int();

  raw_sub_ = create_subscription<combat_robot_msgs::msg::MCMessage>(
    "/mc/raw_command", rclcpp::QoS(100).reliable(),
    std::bind(
      &McSenderNode::onRawCommand, this,
      std::placeholders::_1));
  ack_sub_ = create_subscription<combat_robot_msgs::msg::MCAck>(
    "/mc/ack", rclcpp::QoS(100).reliable(),
    std::bind(
      &McSenderNode::onAck, this,
      std::placeholders::_1));

  cmd_pub_ = create_publisher<combat_robot_msgs::msg::MCMessage>(
    "/mc/command", rclcpp::QoS(100).reliable());
  timeout_pub_ = create_publisher<std_msgs::msg::UInt32>(
    "/mc/timeout", rclcpp::QoS(10).reliable());

  using namespace std::chrono_literals;
  retransmit_timer_ = create_wall_timer(
    100ms, std::bind(&McSenderNode::retransmitTick, this));

  RCLCPP_INFO(
    get_logger(),
    "[DCN-2026-019] McSenderNode ready "
    "(ack_timeout=%dms, max_retries=%d)",
    ack_timeout_ms_, max_retries_);
}

combat_robot_msgs::msg::MCMessage McSenderNode::stampForTest(
  combat_robot_msgs::msg::MCMessage raw)
{
  // Audit B10: lock-protected stamp + pending insert. now() is ROS-side
  // and safe outside the lock, but the seq increment + map insert must
  // be atomic vs onAck/retransmitTick.
  std::lock_guard<std::mutex> lock(pending_mu_);
  raw.seq = next_seq_++;
  raw.stamp = now();
  raw.crc32_checksum = computeCrc32(raw.command + raw.payload);

  PendingCommand p;
  p.msg = raw;
  p.first_sent = now();
  p.last_sent = now();
  p.retries = 0;
  pending_[raw.seq] = p;
  return raw;
}

void McSenderNode::ackForTest(uint32_t seq, const std::string & outcome)
{
  if (outcome == "OK" || outcome == "DUPLICATE") {
    std::lock_guard<std::mutex> lock(pending_mu_);
    const auto removed = pending_.erase(seq);
    // Audit B9 (P3): unexpected ack (no matching pending entry)
    // — could indicate a late ack after timeout, a stale receiver,
    // or a wire-level duplicate. RCLCPP_DEBUG keeps it observable
    // without flooding INFO/WARN on every benign late-ack.
    if (removed == 0) {
      RCLCPP_DEBUG(
        get_logger(),
        "Late/orphan ack outcome=%s seq=%u (no matching pending)",
        outcome.c_str(), seq);
    }
    return;
  }
  if (outcome == "INVALID_CHECKSUM") {
    // ─── Audit B7 (P2) fix ──────────────────────────────────────
    // Pre-fix: leave pending → retransmit on every tick → 3 retries
    // burned on the same wrong crc. The sender computed the crc
    // itself from (command + payload), so a mismatch means in-
    // transit corruption (DDS layer is reliable but middleware /
    // user-overridden serialization could still drop bits). A
    // retransmit sends the same bytes with the same crc — and
    // very likely takes the same corruption path. Skip retries +
    // declare TIMEOUT immediately so upstream mission layer can
    // escalate (operator notification / RTH / etc.).
    std::lock_guard<std::mutex> lock(pending_mu_);
    auto it = pending_.find(seq);
    if (it == pending_.end()) {return;}
    pending_.erase(it);
    std_msgs::msg::UInt32 timeout_msg;
    timeout_msg.data = seq;
    timeout_pub_->publish(timeout_msg);
    RCLCPP_ERROR(
      get_logger(),
      "Receiver reported INVALID_CHECKSUM seq=%u — declared "
      "TIMEOUT immediately (retransmit of identical bytes is "
      "wasted; upstream must escalate)", seq);
    return;
  }
  // OUT_OF_ORDER → leave pending; receiver's window may catch a
  // retransmit if the protocol head jumps forward.
}

void McSenderNode::onRawCommand(
  combat_robot_msgs::msg::MCMessage::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  auto stamped = stampForTest(*msg);
  cmd_pub_->publish(stamped);
}

void McSenderNode::onAck(
  combat_robot_msgs::msg::MCAck::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  ackForTest(msg->seq, msg->outcome);
}

void McSenderNode::retransmitTick()
{
  const auto now_t = now();
  // Audit B10: snapshot pending under lock, iterate the snapshot
  // outside the lock so publish() doesn't hold the mutex (publish
  // can call into rmw which we don't want to serialize with onAck).
  // We separately collect (seq, retried_msg) to publish + (seq) to
  // erase, then apply mutations under a second short lock.
  struct Decision { uint32_t seq; bool timeout; combat_robot_msgs::msg::MCMessage msg;
    int new_retries; };
  std::vector<Decision> decisions;
  {
    std::lock_guard<std::mutex> lock(pending_mu_);
    for (auto & [seq, p] : pending_) {
      const double age_ms =
        (now_t - p.last_sent).nanoseconds() / 1.0e6;
      if (age_ms < ack_timeout_ms_) {continue;}
      if (p.retries >= max_retries_) {
        decisions.push_back({seq, true, combat_robot_msgs::msg::MCMessage(), p.retries});
      } else {
        p.retries++;
        p.last_sent = now_t;
        decisions.push_back({seq, false, p.msg, p.retries});
      }
    }
  }

  // Publish + log + erase outside the lock.
  for (const auto & d : decisions) {
    if (d.timeout) {
      std_msgs::msg::UInt32 timeout_msg;
      timeout_msg.data = d.seq;
      timeout_pub_->publish(timeout_msg);
      RCLCPP_WARN(
        get_logger(),
        "Max retries reached for seq=%u — declared TIMEOUT",
        d.seq);
      std::lock_guard<std::mutex> lock(pending_mu_);
      pending_.erase(d.seq);
    } else {
      cmd_pub_->publish(d.msg);
      RCLCPP_DEBUG(
        get_logger(),
        "Retransmit seq=%u (retry %d/%d)",
        d.seq, d.new_retries, max_retries_);
    }
  }
}

uint32_t McSenderNode::computeCrc32(const std::string & s)
{
  boost::crc_32_type crc;
  crc.process_bytes(s.data(), s.size());
  return crc.checksum();
}

}  // namespace san_operation_control
