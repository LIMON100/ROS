// SAN v1.5 — TierNode main.
#include <rclcpp/rclcpp.hpp>
#include "san_follower_tier/tier_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_follower_tier::TierNode>();
    rclcpp::spin(node);
  } catch (const std::exception& e) {
    RCLCPP_FATAL(rclcpp::get_logger("tier_main"),
                 "TierNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
