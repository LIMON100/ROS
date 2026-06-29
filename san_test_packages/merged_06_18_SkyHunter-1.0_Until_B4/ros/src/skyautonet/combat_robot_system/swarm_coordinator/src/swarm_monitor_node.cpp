// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-013 swarm_monitor_node implementation.

#include "swarm_coordinator/swarm_monitor_node.hpp"

#include <chrono>
#include <string>

#include <tf2/exceptions.h>
#include <tf2/time.h>

using namespace std::chrono_literals;

namespace swarm_coordinator
{

SwarmMonitorNode::SwarmMonitorNode()
: SwarmMonitorNode(rclcpp::NodeOptions())
{}

SwarmMonitorNode::SwarmMonitorNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("swarm_monitor_node", options)
{
  // DCN-2026-013: declare robot_role so the gate can read it during
  // constructor. Default "follower" — the safe choice (no publisher
  // is created unless an operator/launch file overrides).
  declare_parameter<std::string>("robot_role", "follower");

  // TF infrastructure — always created. Non-hub roles still use the
  // local TF graph (formation tracking, T0 PREDICTIVE_TRACK on
  // followers, etc.); only the AGGREGATE PUBLISHER is gated.
  tf_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

  // DCN-2026-013: Hub/Leader-only gate. Mirrors DCN-2026-006 EXT
  // D-024 — only the active aggregator publishes, others stay
  // subscribe-only.
  if (isPublisherEnabled()) {
    poses_pub_ = create_publisher<geometry_msgs::msg::PoseArray>(
      "/swarm/poses",
      rclcpp::QoS(rclcpp::KeepLast(5)).reliable());

    update_timer_ = create_wall_timer(
      100ms,                                                // 10 Hz
      std::bind(&SwarmMonitorNode::updateAndPublish, this));

    RCLCPP_INFO(
      get_logger(),
      "swarm_monitor: PUBLISHING /swarm/poses (role=%s, 10 Hz)",
      get_parameter("robot_role").as_string().c_str());
  } else {
    RCLCPP_INFO(
      get_logger(),
      "swarm_monitor: standby (role=%s) — publisher suppressed, "
      "subscribe-only mode (DCN-2026-013 Hub-only gate)",
      get_parameter("robot_role").as_string().c_str());
  }
}

bool SwarmMonitorNode::isPublisherEnabled() const
{
  const auto role = get_parameter("robot_role").as_string();
  return role == "hub" || role == "leader";
}

void SwarmMonitorNode::updateAndPublish()
{
  // DCN-2026-013: defense in depth. Even though the constructor gates
  // publisher creation, a future refactor or a manual unset_parameter
  // could create a window where the timer fires while poses_pub_ is
  // nullptr; this guard makes that crash-free.
  if (!poses_pub_) {return;}

  geometry_msgs::msg::PoseArray msg;
  msg.header.stamp = now();
  msg.header.frame_id = "map";

  // Probe TF for each robot_id in 1..MAX_ROBOTS. Robots whose
  // base_link frame isn't in the local TF tree (offline, network
  // partition) are silently skipped — the PoseArray length is the
  // CURRENTLY-OBSERVABLE swarm size, not the configured maximum.
  for (uint32_t robot_id = 1; robot_id <= MAX_ROBOTS; ++robot_id) {
    const std::string child = "robot_" + std::to_string(robot_id) +
      "/base_link";
    try {
      const auto t = tf_buffer_->lookupTransform(
        "map", child, tf2::TimePointZero,
        tf2::durationFromSec(0.0));           // non-blocking — cached only
      geometry_msgs::msg::Pose pose;
      pose.position.x = t.transform.translation.x;
      pose.position.y = t.transform.translation.y;
      pose.position.z = t.transform.translation.z;
      pose.orientation = t.transform.rotation;
      msg.poses.push_back(pose);
    } catch (const tf2::TransformException &) {
      // Robot not in TF (yet) — skip silently. Next tick will
      // retry; rclcpp logger throttling would clutter at 10 Hz.
    }
  }

  poses_pub_->publish(msg);
}

}  // namespace swarm_coordinator
