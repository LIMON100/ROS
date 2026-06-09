// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 1 - san_lidar main entry.

#include <rclcpp/rclcpp.hpp>
#include "san_lidar/robosense_e1_driver.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<san_lidar::RobosenseE1Driver>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
