// SAN v1.5 Phase 2-E Turn 8 — ThreatAggregatorNode (hub_only).
//
// Subscribes to per-robot threat alerts on /robot_*/threat_alert and
// publishes the hub-level aggregated stream on /hub/threat_alert
// after dedup + severity escalation. Also publishes immediately on
// FATAL/CRITICAL for fast operator notification.

#ifndef SAN_HUB_ORCHESTRATOR__THREAT_AGGREGATOR_NODE_HPP_
#define SAN_HUB_ORCHESTRATOR__THREAT_AGGREGATOR_NODE_HPP_

#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>

#include "san_hub_orchestrator/threat_aggregator.hpp"

namespace san_hub_orchestrator {

class ThreatAggregatorNode : public rclcpp::Node {
public:
  explicit ThreatAggregatorNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());
  ~ThreatAggregatorNode() override = default;

private:
  using ThreatMsg = combat_robot_msgs::msg::ThreatAlert;

  void declareParameters();
  void loadParameters();
  void onIncomingThreat(const ThreatMsg::SharedPtr msg);
  void onPollTick();
  void publish(const ThreatOutput& t);

  // Params
  double      dedup_window_s_;
  double      poll_period_s_;
  std::string input_topic_;
  std::string output_topic_;

  ThreatAggregator                                aggregator_;
  rclcpp::Subscription<ThreatMsg>::SharedPtr      threat_sub_;
  rclcpp::Publisher<ThreatMsg>::SharedPtr          threat_pub_;
  rclcpp::TimerBase::SharedPtr                     poll_timer_;

  std::atomic<uint32_t> ingest_count_{0};
  std::atomic<uint32_t> publish_count_{0};
};

}  // namespace san_hub_orchestrator

#endif  // SAN_HUB_ORCHESTRATOR__THREAT_AGGREGATOR_NODE_HPP_
