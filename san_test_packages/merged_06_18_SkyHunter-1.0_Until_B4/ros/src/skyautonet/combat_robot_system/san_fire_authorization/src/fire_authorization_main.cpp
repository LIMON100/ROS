// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — FireAuthorizationNode main entry.

#include <rclcpp/rclcpp.hpp>

#include "san_fire_authorization/fire_authorization_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<
      san_fire_authorization::FireAuthorizationNode>();
    rclcpp::spin(node);
  } catch (const std::exception & e) {
    // Fail-closed: any module ctor / HMAC secret / audit log failure
    // takes the node down rather than silently authorizing fire
    // without the requisite safeguards. systemd Restart=on-failure
    // will retry; if it keeps failing, the system stays safe.
    RCLCPP_FATAL(
      rclcpp::get_logger("fire_authorization_main"),
      "FireAuthorizationNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
