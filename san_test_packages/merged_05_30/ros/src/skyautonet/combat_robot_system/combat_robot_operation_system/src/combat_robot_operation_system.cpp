// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "combat_robot_operation_system/combat_robot_operation_system.hpp"

#include <chrono>
#include <cstdlib>
#include <stdexcept>

namespace combat_robot_operation_system
{

namespace
{

const std::unordered_map<std::string, DeploymentMode> & modeMap()
{
  // Vocabulary + aliases must stay in lock-step with
  // san_operation_control::fromString (deployment_mode.cpp). Both nodes
  // are configured from the same squadron deployment_mode argument, so a
  // value one accepts the other must accept too — otherwise a launch
  // that satisfies one node fatals the other (e.g. 'dev' booted
  // san_operation_control but crashed this node pre-fix).
  static const std::unordered_map<std::string, DeploymentMode> m = {
    {"production", DeploymentMode::PRODUCTION},
    {"prod", DeploymentMode::PRODUCTION},
    {"demo", DeploymentMode::DEMO},
    {"lab_test", DeploymentMode::LAB_TEST},
    {"lab", DeploymentMode::LAB_TEST},
    {"bench", DeploymentMode::BENCH},
    {"development", DeploymentMode::DEVELOPMENT},
    {"dev", DeploymentMode::DEVELOPMENT},
  };
  return m;
}

}  // namespace

const char * toString(DeploymentMode m)
{
  switch (m) {
    case DeploymentMode::PRODUCTION:  return "production";
    case DeploymentMode::DEMO:        return "demo";
    case DeploymentMode::LAB_TEST:    return "lab_test";
    case DeploymentMode::BENCH:       return "bench";
    case DeploymentMode::DEVELOPMENT: return "development";
  }
  return "production";
}

DeploymentMode fromString(const std::string & s)
{
  auto it = modeMap().find(s);
  if (it == modeMap().end()) {
    throw std::runtime_error("unknown deployment_mode: " + s);
  }
  return it->second;
}

CombatRobotOperationSystem::CombatRobotOperationSystem()
: CombatRobotOperationSystem(rclcpp::NodeOptions())
{}

CombatRobotOperationSystem::CombatRobotOperationSystem(
  const rclcpp::NodeOptions & options)
: rclcpp::Node("combat_robot_operation_system", options)
{
  declareAllParameters();
}

void CombatRobotOperationSystem::declareAllParameters()
{
  declare_parameter<std::string>("deployment_mode", "production");
  declare_parameter<int>("robot_id", 0);
  declare_parameter<double>("heartbeat_period_sec", 1.0);

  // safety.*
  declare_parameter<std::string>(
    "safety.gun_trigger_permission",
    "DENIED");
  declare_parameter<bool>("safety.hw_watchdog_enabled", true);
  declare_parameter<bool>("safety.developer_auth_required", false);
  declare_parameter<std::string>("safety.operator_screen_banner", "");
  declare_parameter<std::string>("safety.operator_screen_color", "");
}

void CombatRobotOperationSystem::initialize()
{
  initParameters();
  wirePublishers();
  publishDeploymentModeBanner();
  m_initialized_ = true;
}

void CombatRobotOperationSystem::initParameters()
{
  const std::string mode_str =
    get_parameter("deployment_mode").as_string();
  validateDeploymentMode(mode_str);

  m_robot_id_ = static_cast<uint32_t>(
    get_parameter("robot_id").as_int());

  if (m_deployment_mode_ == DeploymentMode::DEVELOPMENT) {
    enforceDeveloperAuth();
    RCLCPP_WARN(
      get_logger(),
      "================================================================\n"
      "  *** DEVELOPMENT MODE ACTIVE - DO NOT USE IN LIVE OPS ***\n"
      "  All audit logs forwarded to /var/log/san_audit.log\n"
      "================================================================");
  }

  RCLCPP_INFO(
    get_logger(),
    "Operating in deployment_mode: %s (robot_id=%u)",
    mode_str.c_str(), m_robot_id_);
}

void CombatRobotOperationSystem::validateDeploymentMode(
  const std::string & mode_str)
{
  auto it = modeMap().find(mode_str);
  if (it == modeMap().end()) {
    RCLCPP_FATAL(
      get_logger(),
      "Invalid deployment_mode: '%s'. "
      "Allowed: production(prod)|demo|lab_test(lab)|bench|development(dev)",
      mode_str.c_str());
    throw std::runtime_error(
            "Invalid deployment_mode: " + mode_str);
  }
  m_deployment_mode_ = it->second;
}

void CombatRobotOperationSystem::enforceDeveloperAuth()
{
  const bool required =
    get_parameter("safety.developer_auth_required").as_bool();
  if (!required) {return;}
  const char * token = std::getenv("DEVELOPER_AUTH_TOKEN");
  if (token == nullptr || std::string(token).empty()) {
    RCLCPP_FATAL(
      get_logger(),
      "deployment_mode=development requires DEVELOPER_AUTH_TOKEN "
      "env var. Set via: export DEVELOPER_AUTH_TOKEN=<your-token>");
    throw std::runtime_error("Missing DEVELOPER_AUTH_TOKEN");
  }
}

bool CombatRobotOperationSystem::watchdogEnabled() const
{
  // production / demo / lab_test / bench force watchdog on
  // regardless of yaml (mirrors the san_operation_control policy).
  // development honors the yaml flag.
  const bool yaml =
    get_parameter("safety.hw_watchdog_enabled").as_bool();
  if (m_deployment_mode_ == DeploymentMode::DEVELOPMENT) {
    return yaml;
  }
  return true;
}

std::string CombatRobotOperationSystem::defaultBanner(DeploymentMode m) const
{
  switch (m) {
    case DeploymentMode::PRODUCTION:  return "ARMED";
    case DeploymentMode::DEMO:        return "DEMO";
    case DeploymentMode::LAB_TEST:    return "LAB";
    case DeploymentMode::BENCH:       return "BENCH";
    case DeploymentMode::DEVELOPMENT: return "DEV";
  }
  return "ARMED";
}

void CombatRobotOperationSystem::wirePublishers()
{
  m_op_state_pub_ = create_publisher<
    combat_robot_msgs::msg::OperationState>(
    "/operation_state", rclcpp::QoS(5).reliable());

  const auto period_s = get_parameter("heartbeat_period_sec").as_double();
  const auto period_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::duration<double>(period_s));
  m_heartbeat_timer_ = create_wall_timer(
    period_ns, std::bind(
      &CombatRobotOperationSystem::onHeartbeat,
      this));
}

void CombatRobotOperationSystem::publishDeploymentModeBanner()
{
  onHeartbeat();     // immediate first publish on init
}

void CombatRobotOperationSystem::onHeartbeat()
{
  if (!m_op_state_pub_) {return;}
  combat_robot_msgs::msg::OperationState s;
  s.header.stamp = now();
  s.header.frame_id = "swarm";
  s.sequence = ++m_sequence_;
  s.deployment_mode = toString(m_deployment_mode_);

  // Yaml override > default banner.
  const std::string yaml_banner =
    get_parameter("safety.operator_screen_banner").as_string();
  s.operator_banner = yaml_banner.empty() ?
    defaultBanner(m_deployment_mode_) : yaml_banner;

  s.robot_id = m_robot_id_;
  s.leader_robot_id = 1;
  s.hub_robot_id = 2;
  s.deputy_robot_id = 3;
  s.n_alive_robots = 0;      // filled in by swarm coordinator later
  s.timestamp_ms = nowMs();
  m_op_state_pub_->publish(s);
}

uint64_t CombatRobotOperationSystem::nowMs() const
{
  return static_cast<uint64_t>(now().nanoseconds() / 1'000'000ll);
}

}  // namespace combat_robot_operation_system
