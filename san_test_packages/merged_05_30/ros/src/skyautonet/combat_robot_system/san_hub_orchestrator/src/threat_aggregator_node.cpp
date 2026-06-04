// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — ThreatAggregatorNode implementation.

#include "san_hub_orchestrator/threat_aggregator_node.hpp"

#include <chrono>
#include <stdexcept>

namespace san_hub_orchestrator
{

using namespace std::chrono_literals;

ThreatAggregatorNode::ThreatAggregatorNode(
  const rclcpp::NodeOptions & opts)
: rclcpp::Node("threat_aggregator_node", opts),
  aggregator_(5.0 /* placeholder; reset after loadParameters */)
{
  declareParameters();
  loadParameters();

  // Rebuild aggregator with the configured dedup window
  aggregator_ = ThreatAggregator(dedup_window_s_);

  threat_pub_ = create_publisher<ThreatMsg>(
    output_topic_, rclcpp::QoS(50).reliable());

  // Subscribe with absolute topic so all robots' threat_alert
  // streams arrive here (relies on a "+" wildcard ROS doesn't have,
  // so we use a pattern of remappings from launch instead. The input
  // topic itself is a single unified channel — robots publish there).
  threat_sub_ = create_subscription<ThreatMsg>(
    input_topic_, rclcpp::QoS(50).reliable(),
    std::bind(
      &ThreatAggregatorNode::onIncomingThreat,
      this, std::placeholders::_1));

  const auto period_ms = std::chrono::milliseconds(
    static_cast<int64_t>(poll_period_s_ * 1000.0));
  poll_timer_ = create_wall_timer(
    period_ms, std::bind(&ThreatAggregatorNode::onPollTick, this));

  RCLCPP_INFO(
    get_logger(),
    "ThreatAggregatorNode UP: in=%s out=%s window=%.1fs poll=%.1fs",
    input_topic_.c_str(), output_topic_.c_str(),
    dedup_window_s_, poll_period_s_);
}

void ThreatAggregatorNode::declareParameters()
{
  declare_parameter<double>("dedup_window_s", 5.0);
  declare_parameter<double>("poll_period_s", 1.0);
  declare_parameter<std::string>("input_topic", "/swarm/threat_alert_raw");
  declare_parameter<std::string>("output_topic", "/hub/threat_alert");
}

void ThreatAggregatorNode::loadParameters()
{
  dedup_window_s_ = get_parameter("dedup_window_s").as_double();
  poll_period_s_ = get_parameter("poll_period_s").as_double();
  input_topic_ = get_parameter("input_topic").as_string();
  output_topic_ = get_parameter("output_topic").as_string();
  if (dedup_window_s_ <= 0.0 || dedup_window_s_ > 3600.0) {
    throw std::runtime_error(
            "ThreatAggregatorNode: dedup_window_s out of range");
  }
  if (poll_period_s_ <= 0.0 || poll_period_s_ > 60.0) {
    throw std::runtime_error(
            "ThreatAggregatorNode: poll_period_s out of range");
  }
}

void ThreatAggregatorNode::onIncomingThreat(
  const ThreatMsg::SharedPtr msg)
{
  ++ingest_count_;
  ThreatInput in;
  in.severity = msg->severity;
  in.threat_type = msg->threat_type;
  in.source_robot_id = msg->source_robot_id;
  in.peer_id = msg->peer_id;
  in.message_ko = msg->message_ko;
  in.detail = msg->detail;
  // Phase 7 fix: anchor `in.timestamp_ms` to the HUB's local clock,
  // not the publisher's. Pre-patch a robot with future-skewed clock
  // could set window_start_ms far in the future → pollReady never
  // fires; lag-skewed → fold immediately + erase.
  // Preserve publisher's original ts on the ThreatOutput.detail
  // payload would be ideal but for now we discard.
  in.timestamp_ms = static_cast<uint64_t>(
    now().nanoseconds() / 1'000'000);
  const bool fresh_or_promoted = aggregator_.ingest(in);

  // Fast-path: for CRITICAL/FATAL on fresh-or-promoted, publish
  // immediately without waiting for poll window. Phase 6 fix: use
  // pop() instead of peek() so the slot is removed — otherwise
  // pollReady would re-publish the same alert after the dedup window
  // elapses (operator would see CRITICAL/FATAL twice).
  if (fresh_or_promoted &&
    (msg->severity == ThreatMsg::SEVERITY_CRITICAL ||
    msg->severity == ThreatMsg::SEVERITY_FATAL))
  {
    auto p = aggregator_.pop(in.source_robot_id, in.threat_type);
    if (p) {publish(*p);}
  }
}

void ThreatAggregatorNode::onPollTick()
{
  const auto now_ms =
    static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
  auto ready = aggregator_.pollReady(now_ms);
  for (const auto & t : ready) {
    publish(t);
  }

  RCLCPP_DEBUG(
    get_logger(),
    "threats ingest=%u publish=%u active=%zu",
    ingest_count_.load(), publish_count_.load(),
    aggregator_.activeCount());
}

void ThreatAggregatorNode::publish(const ThreatOutput & t)
{
  ThreatMsg msg;
  msg.header.stamp = now();
  msg.header.frame_id = "hub";
  msg.severity = t.severity;
  msg.threat_type = t.threat_type;
  msg.source_robot_id = t.source_robot_id;
  msg.peer_id = t.peer_id;
  msg.message_ko = t.message_ko;
  msg.detail = t.detail;
  msg.timestamp_ms = t.timestamp_ms;
  msg.instance_count = t.instance_count;
  threat_pub_->publish(msg);
  ++publish_count_;
}

}  // namespace san_hub_orchestrator
