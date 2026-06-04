// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5b - main entry for the LTE link-quality publisher.

#include <rclcpp/rclcpp.hpp>

#include "san_lte_redundancy/lte_link_quality_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<san_lte_redundancy::LteLinkQualityNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
