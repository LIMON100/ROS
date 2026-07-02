// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 DCN-2026-010 D-028 — DetectionToThreatNode main.
#include <rclcpp/rclcpp.hpp>
#include "san_hub_orchestrator/detection_to_threat_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<
      san_hub_orchestrator::DetectionToThreatNode>();
    rclcpp::spin(node);
  } catch (const std::exception & e) {
    RCLCPP_FATAL(
      rclcpp::get_logger("det_to_threat_main"),
      "DetectionToThreatNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
