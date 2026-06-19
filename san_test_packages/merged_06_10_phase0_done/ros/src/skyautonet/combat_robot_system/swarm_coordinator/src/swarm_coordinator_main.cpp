// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 0 - main entry for swarm_coordinator.

#include <rclcpp/rclcpp.hpp>
#include "swarm_coordinator/swarm_coordinator.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<swarm_coordinator::SwarmCoordinator>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
