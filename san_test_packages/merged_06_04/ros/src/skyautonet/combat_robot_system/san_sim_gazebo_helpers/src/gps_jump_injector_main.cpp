// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — GPS jump injector main entry.

#include <memory>
#include <rclcpp/rclcpp.hpp>

#include "san_sim_gazebo_helpers/gps_jump_injector_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<
    san_sim_gazebo_helpers::GpsJumpInjectorNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
