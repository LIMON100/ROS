// SAN v1.5 Phase 2-E Turn 4 — RtkGnssNode main.
#include <rclcpp/rclcpp.hpp>
#include "san_rtk_gnss/rtk_gnss_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_rtk_gnss::RtkGnssNode>();
    rclcpp::spin(node);
  } catch (const std::exception& e) {
    RCLCPP_FATAL(rclcpp::get_logger("rtk_gnss_main"),
                  "RtkGnssNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
