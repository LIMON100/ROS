// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 PHASE 8 - main entry for the role-management process.
//
// Hosts HubRoleManager, LeaderRoleManager, and LimpModeManager in a
// single MultiThreadedExecutor so all three watchdogs run on dedicated
// callback threads.

#include <rclcpp/rclcpp.hpp>

#include "san_role_management/hub_role_manager.hpp"
#include "san_role_management/leader_role_manager.hpp"
#include "san_role_management/limp_mode_manager.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto hub_mgr = std::make_shared<san_role_management::HubRoleManager>();
  auto leader_mgr = std::make_shared<san_role_management::LeaderRoleManager>();
  auto limp_mgr = std::make_shared<san_role_management::LimpModeManager>();

  rclcpp::executors::MultiThreadedExecutor exec;
  exec.add_node(hub_mgr);
  exec.add_node(leader_mgr);
  exec.add_node(limp_mgr);
  exec.spin();

  rclcpp::shutdown();
  return 0;
}
