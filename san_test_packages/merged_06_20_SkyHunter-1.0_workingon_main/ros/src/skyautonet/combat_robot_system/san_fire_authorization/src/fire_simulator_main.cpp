// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-018] fire_simulator_node executable main.

#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "san_fire_authorization/fire_simulator_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(
    std::make_shared<san_fire_authorization::FireSimulatorNode>());
  rclcpp::shutdown();
  return 0;
}
