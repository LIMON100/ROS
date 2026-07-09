// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 2 v2 - one-shot mwan3 interface initializer.
//
// Configures the mwan3.wan_lte UCI section on boot using libuci
// (Mwan3UciController). Replaces the v1 openwrt_mwan3_{primary,backup}.sh
// bash scripts entirely - no shell exec, no /etc/init.d invocation.
//
// Runs once and asks rclcpp to shut down. Launch via
// `launch/lte_init.launch.xml` from the robot's systemd service.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <memory>
#include <string>

#include "san_lte_redundancy/mwan3_uci_controller.hpp"

namespace san_lte_redundancy
{

class Mwan3InitNode : public rclcpp::Node
{
public:
  Mwan3InitNode();
  explicit Mwan3InitNode(const rclcpp::NodeOptions & options);

  // Test hook - inject a custom UCI controller.
  Mwan3InitNode(
    const rclcpp::NodeOptions & options,
    std::unique_ptr<Mwan3UciController> uci);

  // Returns true if configureMwan3() completed successfully.
  bool isConfigured() const {return configured_;}

private:
  int robot_id_ = 0;
  std::string role_ = "backup";         // "primary" | "backup"
  std::string track_ip_ = "8.8.8.8";
  int initial_weight_ = 0;
  bool configured_ = false;

  std::unique_ptr<Mwan3UciController> uci_;

  void readParameters();
  void configureMwan3();
};

}  // namespace san_lte_redundancy
