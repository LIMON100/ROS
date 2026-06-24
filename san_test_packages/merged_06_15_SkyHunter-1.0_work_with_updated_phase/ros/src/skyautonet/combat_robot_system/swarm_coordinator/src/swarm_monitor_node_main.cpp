// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-013 swarm_monitor_node executable entry.

#include <rclcpp/rclcpp.hpp>
#include "swarm_coordinator/swarm_monitor_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<swarm_coordinator::SwarmMonitorNode>());
  rclcpp::shutdown();
  return 0;
}
