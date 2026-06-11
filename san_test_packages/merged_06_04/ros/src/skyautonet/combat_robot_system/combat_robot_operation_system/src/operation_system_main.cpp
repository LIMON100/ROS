// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 0 - main entry for the operation_system node.

#include <rclcpp/rclcpp.hpp>
#include "combat_robot_operation_system/combat_robot_operation_system.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<
    combat_robot_operation_system::CombatRobotOperationSystem>();
  try {
    node->initialize();
  } catch (const std::exception & e) {
    RCLCPP_FATAL(
      node->get_logger(),
      "operation_system init failed: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
