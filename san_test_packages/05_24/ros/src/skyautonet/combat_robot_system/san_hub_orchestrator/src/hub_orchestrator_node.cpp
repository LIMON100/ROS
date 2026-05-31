// SAN v1.5 Phase 2-E Turn 8 — HubOrchestratorNode implementation.

#include "san_hub_orchestrator/hub_orchestrator_node.hpp"

#include <chrono>

namespace san_hub_orchestrator {

using namespace std::chrono_literals;
using RobotStatusMsg = combat_robot_msgs::msg::RobotStatus;
using FleetStatusMsg = combat_robot_msgs::msg::FleetStatus;

HubOrchestratorNode::HubOrchestratorNode(const rclcpp::NodeOptions& opts)
    : rclcpp::Node("hub_orchestrator_node", opts),
      aggregator_(5000) {
  declareParameters();
  loadParameters();
  aggregator_.setDisconnectThreshold(disconnect_threshold_ms_);

  fleet_pub_ = create_publisher<FleetStatusMsg>(
      "~/fleet_status", rclcpp::QoS(1).reliable());

  // Subscribe to all robot status broadcasts on the global topic
  // /robot_status (each robot publishes here at 5 Hz to the swarm
  // bridge per IDS v1.5 §5).
  status_sub_ = create_subscription<RobotStatusMsg>(
      "/robot_status",
      rclcpp::QoS(50).best_effort(),
      std::bind(&HubOrchestratorNode::onRobotStatus, this,
                std::placeholders::_1));

  const auto period = std::chrono::milliseconds(
      static_cast<int64_t>(1000.0 / aggregation_hz_));
  tick_timer_ = create_wall_timer(
      period, std::bind(&HubOrchestratorNode::onTick, this));

  RCLCPP_INFO(get_logger(),
      "HubOrchestratorNode UP @ %.1f Hz, disconnect_threshold=%dms",
      aggregation_hz_, disconnect_threshold_ms_);
}

void HubOrchestratorNode::declareParameters() {
  declare_parameter<int>("disconnect_threshold_ms",   5000);
  declare_parameter<double>("aggregation_hz",         1.0);
}

void HubOrchestratorNode::loadParameters() {
  disconnect_threshold_ms_ =
      static_cast<int>(get_parameter("disconnect_threshold_ms").as_int());
  aggregation_hz_ = get_parameter("aggregation_hz").as_double();
  if (aggregation_hz_ <= 0.0 || aggregation_hz_ > 50.0) {
    throw std::runtime_error("aggregation_hz out of range");
  }
}

void HubOrchestratorNode::onRobotStatus(
    const RobotStatusMsg::SharedPtr msg) {
  RobotSnapshot s;
  s.robot_id              = "robot_" + std::to_string(msg->robot_id);
  // Phase 7 fix: anchor `last_heartbeat_ms` to the hub's local wall
  // clock, not the publisher's `timestamp_ms`. Pre-patch a robot
  // with skewed clock leading the hub by N seconds would never be
  // marked disconnected (now_ms - last > 0 was false); one with
  // lagging clock would be marked disconnected immediately.
  s.last_heartbeat_ms     = static_cast<uint64_t>(
                                now().nanoseconds() / 1'000'000);
  s.in_limp_mode          = msg->in_limp_mode;
  s.battery_percent       = msg->battery_percent;
  // mission_progress / RTK info not present in RobotStatus today;
  // TODO Turn 14-15: subscribe per-robot rtk_status to refine.
  s.mission_progress_percent = 0.0f;
  s.rtk_fix_grade            = 0;
  // active_threats not in RobotStatus either — placeholder 0.
  s.active_threats        = 0;
  aggregator_.update(s);
}

void HubOrchestratorNode::onTick() {
  const uint64_t now_ms =
      static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
  auto f = aggregator_.aggregate(now_ms);

  FleetStatusMsg msg;
  msg.header.stamp                    = now();
  msg.header.frame_id                 = "hub";
  msg.total_robots                    = f.total_robots;
  msg.healthy_robots                  = f.healthy_robots;
  msg.limp_mode_robots                = f.limp_mode_robots;
  msg.disconnected_robots             = f.disconnected_robots;
  msg.min_battery_percent             = f.min_battery_percent;
  msg.mean_battery_percent            = f.mean_battery_percent;
  msg.mission_progress_percent        = f.mission_progress_percent;
  msg.robots_with_rtk_fix             = f.robots_with_rtk_fix;
  msg.robots_with_rtk_float           = f.robots_with_rtk_float;
  msg.robots_with_no_fix              = f.robots_with_no_fix;
  msg.active_threats_count            = f.active_threats_count;
  msg.last_aggregation_timestamp_ms   = now_ms;
  msg.follower_map_tiles_received     = 0;   // TODO Turn 14-15

  fleet_pub_->publish(msg);

  RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
      "fleet: total=%u healthy=%u limp=%u disc=%u min_bat=%.1f",
      msg.total_robots, msg.healthy_robots, msg.limp_mode_robots,
      msg.disconnected_robots, msg.min_battery_percent);
}

}  // namespace san_hub_orchestrator
