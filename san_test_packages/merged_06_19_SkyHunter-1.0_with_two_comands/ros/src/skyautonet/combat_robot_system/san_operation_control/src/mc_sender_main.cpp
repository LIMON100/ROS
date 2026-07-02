// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-019] mc_sender_node executable main.

#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "san_operation_control/mc_sender_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(
    std::make_shared<san_operation_control::McSenderNode>());
  rclcpp::shutdown();
  return 0;
}
