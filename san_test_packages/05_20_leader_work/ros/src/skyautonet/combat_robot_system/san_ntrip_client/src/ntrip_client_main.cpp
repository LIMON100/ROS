// SAN v1.5 Phase 2-E Turn 4 — NtripClientNode main.
#include <rclcpp/rclcpp.hpp>
#include "san_ntrip_client/ntrip_client_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_ntrip_client::NtripClientNode>();
    rclcpp::spin(node);
  } catch (const std::exception& e) {
    RCLCPP_FATAL(rclcpp::get_logger("ntrip_client_main"),
                  "NtripClientNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
