// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "san_rtk_gnss/rtk_quality_manager_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<san_rtk_gnss::RtkQualityManagerNode>());
  rclcpp::shutdown();
  return 0;
}
