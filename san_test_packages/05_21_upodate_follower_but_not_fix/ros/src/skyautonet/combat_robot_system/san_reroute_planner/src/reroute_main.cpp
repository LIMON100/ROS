// SAN v1.5 — RerouteNode main.
#include <rclcpp/rclcpp.hpp>
#include "san_reroute_planner/reroute_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_reroute_planner::RerouteNode>();
    rclcpp::spin(node);
  } catch (const std::exception& e) {
    RCLCPP_FATAL(rclcpp::get_logger("reroute_main"),
                 "RerouteNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
