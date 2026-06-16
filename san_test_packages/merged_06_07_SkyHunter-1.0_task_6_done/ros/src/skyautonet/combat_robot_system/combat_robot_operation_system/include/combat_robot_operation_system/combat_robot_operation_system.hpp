// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 0 — combat robot operation system controller.
//
// Owns the 5-tier deployment_mode resolution + DEVELOPER_AUTH_TOKEN
// enforcement, and publishes the OperationState heartbeat at 1 Hz so
// downstream consumers (operator UI, audit log, swarm coordinator)
// observe the resolved posture without re-reading the yaml overlay
// tree.
//
// References:
//   * SAN-SDD-SWARM-001 v1.3 §11 (deployment_mode + safety)
//   * SAN-OPS-SOP-001 v1.3 §1

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/operation_state.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace combat_robot_operation_system
{

enum class DeploymentMode
{
  PRODUCTION,
  DEMO,
  LAB_TEST,
  BENCH,
  DEVELOPMENT,
};

const char * toString(DeploymentMode m);
DeploymentMode fromString(const std::string & s);   // throws on unknown

class CombatRobotOperationSystem : public rclcpp::Node
{
public:
  CombatRobotOperationSystem();
  explicit CombatRobotOperationSystem(const rclcpp::NodeOptions & options);

  // Two-phase init: constructor wires parameters + interfaces;
  // initialize() validates deployment_mode (may throw). Splitting
  // out the throwing logic keeps the constructor exception-free
  // for shared_ptr factories.
  void initialize();

  // Accessors.
  DeploymentMode deploymentMode() const {return m_deployment_mode_;}
  bool isLiveOps() const
  {
    return m_deployment_mode_ == DeploymentMode::PRODUCTION;
  }
  bool weaponsAllowed() const
  {
    return m_deployment_mode_ == DeploymentMode::PRODUCTION;
  }
  bool watchdogEnabled() const;

private:
  DeploymentMode m_deployment_mode_ = DeploymentMode::PRODUCTION;
  bool m_initialized_ = false;
  uint32_t m_robot_id_ = 0;
  uint32_t m_sequence_ = 0;

  rclcpp::Publisher<combat_robot_msgs::msg::OperationState>::SharedPtr
    m_op_state_pub_;
  rclcpp::TimerBase::SharedPtr m_heartbeat_timer_;

  void declareAllParameters();
  void initParameters();
  void validateDeploymentMode(const std::string & mode_str);
  void enforceDeveloperAuth();
  void publishDeploymentModeBanner();
  void wirePublishers();
  void onHeartbeat();
  std::string defaultBanner(DeploymentMode m) const;

  uint64_t nowMs() const;
};

}  // namespace combat_robot_operation_system
