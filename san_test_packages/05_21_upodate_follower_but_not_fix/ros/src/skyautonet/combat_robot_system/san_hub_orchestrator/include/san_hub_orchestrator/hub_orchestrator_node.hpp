// SAN v1.5 Phase 2-E Turn 8 — HubOrchestratorNode.
//
// Hub-only node. Subscribes to per-robot status feeds, aggregates with
// SwarmAggregator, publishes FleetStatus at ~1 Hz.
//
// Replaces (parts of) adapters/hub_ugv.py::HubUgvAdapter per
// DCN-2026-002 D-008. The GStreamer relay portion stays in
// san_hub_comm (already in Phase 1).

#ifndef SAN_HUB_ORCHESTRATOR__HUB_ORCHESTRATOR_NODE_HPP_
#define SAN_HUB_ORCHESTRATOR__HUB_ORCHESTRATOR_NODE_HPP_

#include <memory>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/fleet_status.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>

#include "san_hub_orchestrator/swarm_aggregator.hpp"

namespace san_hub_orchestrator {

class HubOrchestratorNode : public rclcpp::Node {
public:
  explicit HubOrchestratorNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());

private:
  void declareParameters();
  void loadParameters();
  void onRobotStatus(
      const combat_robot_msgs::msg::RobotStatus::SharedPtr msg);
  void onTick();

  SwarmAggregator aggregator_;
  rclcpp::Subscription<combat_robot_msgs::msg::RobotStatus>::SharedPtr status_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FleetStatus>::SharedPtr fleet_pub_;
  rclcpp::TimerBase::SharedPtr tick_timer_;

  // Params
  int disconnect_threshold_ms_;
  double aggregation_hz_;
};

}  // namespace san_hub_orchestrator

#endif  // SAN_HUB_ORCHESTRATOR__HUB_ORCHESTRATOR_NODE_HPP_
