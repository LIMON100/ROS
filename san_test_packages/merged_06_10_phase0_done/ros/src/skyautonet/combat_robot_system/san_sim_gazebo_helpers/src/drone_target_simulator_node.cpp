// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Drone target simulator rclcpp Node implementation.

#include "san_sim_gazebo_helpers/drone_target_simulator_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#include <rcl_interfaces/msg/parameter_descriptor.hpp>

namespace san_sim_gazebo_helpers
{

namespace
{

// Convert yaw to quaternion (z-axis only).
void yawToQuat(double yaw, double & qx, double & qy, double & qz, double & qw)
{
  const double half = 0.5 * yaw;
  qx = 0.0;
  qy = 0.0;
  qz = std::sin(half);
  qw = std::cos(half);
}

}  // namespace

DroneTargetSimulatorNode::DroneTargetSimulatorNode(
  const rclcpp::NodeOptions & options)
: Node("drone_target_simulator", options)
{
  declareParameters();
  loadTargets();

  if (targets_.empty()) {
    RCLCPP_WARN(
      get_logger(),
      "No drone targets configured — node will idle. Set the "
      "'targets' YAML parameter to enable target spawning.");
  }

  const rclcpp::QoS reliable_qos =
    rclcpp::QoS(rclcpp::KeepLast(10)).reliable();

  truth_pub_ = create_publisher<geometry_msgs::msg::PoseArray>(
    "/sim_truth/drone_poses", reliable_qos);
  // Note: world-frame ground-truth detections are NOT published here.
  // combat_robot_msgs/Detection.msg uses an image-bbox + depth schema
  // (Phase 2-E Turn 11-12) which can't represent 3D world poses.
  // Downstream tests can derive perception ground truth from the
  // /sim_truth/drone_poses PoseArray instead.

  sim_start_time_s_ = now().seconds();
  const auto period = std::chrono::milliseconds(
    static_cast<int>(1000.0 / tick_rate_hz_));
  timer_ = create_wall_timer(
    period,
    std::bind(&DroneTargetSimulatorNode::onTick, this));

  RCLCPP_INFO(
    get_logger(),
    "DroneTargetSimulator UP: %zu target(s), tick=%.1f Hz",
    targets_.size(), tick_rate_hz_);
}

// ─── Parameter declaration ──────────────────────────────────────────────

void DroneTargetSimulatorNode::declareParameters()
{
  declare_parameter<double>("tick_rate_hz", 50.0);
  // Built-in scenario presets, in lieu of full YAML parsing here.
  // For richer configs, use a launch wrapper that constructs the
  // YAML and re-instantiates the node with parameters.
  declare_parameter<std::vector<std::string>>(
    "scenarios", {
      "loiter:30,0,30,50,8.0",       // center xyz, radius, speed
      "attack_run:80,30,40,0,0,1.5,15.0",  // start xyz, target xyz, speed
      "swarm_evasion:0,0,25,60,60,15,12.0,1.5,42",
    });
  tick_rate_hz_ = get_parameter("tick_rate_hz").as_double();
}

void DroneTargetSimulatorNode::loadTargets()
{
  const auto scenarios =
    get_parameter("scenarios").as_string_array();
  for (std::size_t i = 0; i < scenarios.size(); ++i) {
    try {
      auto t = makeTargetFromYaml(scenarios[i]);
      const rclcpp::QoS qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
      t.cmd_pub = create_publisher<geometry_msgs::msg::PoseStamped>(
        "/target_drone_" + std::to_string(i + 1) + "/cmd_pose", qos);
      RCLCPP_INFO(
        get_logger(),
        "Loaded target %zu: name=%s kind=%s",
        i + 1, t.name.c_str(), toString(t.traj->kind()));
      targets_.push_back(std::move(t));
    } catch (const std::exception & e) {
      RCLCPP_ERROR(
        get_logger(),
        "Failed to load scenario '%s': %s",
        scenarios[i].c_str(), e.what());
    }
  }
}

// Tiny CSV-ish parser — extends the YAML approach without full yaml-cpp.
DroneTargetSimulatorNode::Target
DroneTargetSimulatorNode::makeTargetFromYaml(
  const std::string & target_string)
{
  // Format examples:
  //   "loiter:cx,cy,cz,r,speed"
  //   "attack_run:sx,sy,sz,tx,ty,tz,speed"
  //   "swarm_evasion:cx,cy,cz,bx,by,bz,speed,period,seed"
  const auto colon = target_string.find(':');
  if (colon == std::string::npos) {
    throw std::invalid_argument("missing ':' in scenario spec");
  }
  const std::string kind = target_string.substr(0, colon);
  const std::string args = target_string.substr(colon + 1);

  std::vector<double> nums;
  std::size_t pos = 0;
  while (pos < args.size()) {
    const auto comma = args.find(',', pos);
    const std::string tok =
      args.substr(
      pos, comma == std::string::npos ?
      std::string::npos : (comma - pos));
    nums.push_back(std::stod(tok));
    if (comma == std::string::npos) {break;}
    pos = comma + 1;
  }

  Target t;
  if (kind == "loiter") {
    if (nums.size() < 5) {
      throw std::invalid_argument("loiter needs 5 args");
    }
    LoiterConfig cfg;
    cfg.center = {nums[0], nums[1], nums[2], 0.0};
    cfg.radius_m = nums[3];
    cfg.tangential_speed_mps = nums[4];
    t.traj = makeLoiter(cfg);
    t.name = "loiter";
  } else if (kind == "attack_run") {
    if (nums.size() < 7) {
      throw std::invalid_argument("attack_run needs 7 args");
    }
    AttackRunConfig cfg;
    cfg.start = {nums[0], nums[1], nums[2], 0.0};
    cfg.target = {nums[3], nums[4], nums[5], 0.0};
    cfg.speed_mps = nums[6];
    t.traj = makeAttackRun(cfg);
    t.name = "attack_run";
  } else if (kind == "swarm_evasion") {
    if (nums.size() < 9) {
      throw std::invalid_argument("swarm_evasion needs 9 args");
    }
    SwarmEvasionConfig cfg;
    cfg.center = {nums[0], nums[1], nums[2], 0.0};
    cfg.bbox_x = nums[3];
    cfg.bbox_y = nums[4];
    cfg.bbox_z = nums[5];
    cfg.speed_mps = nums[6];
    cfg.direction_change_period_s = nums[7];
    cfg.rng_seed = static_cast<uint32_t>(nums[8]);
    t.traj = makeSwarmEvasion(cfg);
    t.name = "swarm_evasion";
  } else {
    throw std::invalid_argument("unknown kind: " + kind);
  }
  return t;
}

// ─── Main tick ──────────────────────────────────────────────────────────

void DroneTargetSimulatorNode::onTick()
{
  const double t_now = now().seconds() - sim_start_time_s_;

  geometry_msgs::msg::PoseArray truth;
  truth.header.stamp = now();
  truth.header.frame_id = "map";

  for (std::size_t i = 0; i < targets_.size(); ++i) {
    auto & t = targets_[i];
    const auto s = t.traj->sample(t_now);
    t.last_sample = s;

    geometry_msgs::msg::PoseStamped ps;
    ps.header.stamp = now();
    ps.header.frame_id = "map";
    ps.pose.position.x = s.pose.x;
    ps.pose.position.y = s.pose.y;
    ps.pose.position.z = s.pose.z;
    yawToQuat(
      s.pose.yaw,
      ps.pose.orientation.x, ps.pose.orientation.y,
      ps.pose.orientation.z, ps.pose.orientation.w);
    t.cmd_pub->publish(ps);
    truth.poses.push_back(ps.pose);
  }
  truth_pub_->publish(truth);
}

KinState DroneTargetSimulatorNode::lastSampleForTest(std::size_t i) const
{
  if (i >= targets_.size()) {return {};}
  return targets_[i].last_sample;
}

}  // namespace san_sim_gazebo_helpers
