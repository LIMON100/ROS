// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Drone target simulator rclcpp Node.
//
// Drives N target drones in Gazebo by publishing pose commands on
// /target_drone_{i}/cmd_pose at 50 Hz. The actual drone entities
// are spawned by a launch wrapper (drone_target.launch.py) that
// instantiates simple kinematic SDF models and bridges cmd_pose
// to Gazebo via ros_gz_bridge.
//
// Also publishes a ground-truth combat_robot_msgs/DetectionArray on
// /sim_truth/detections so test assertions can verify the perception
// pipeline picked up every drone (KPP-2 sanity check).
//
// PATCH 2026-05-13 (Gazebo deep-dive review):
//   Adds the first end-to-end anti-drone test capability to the sim.
//   Existing sim had no drone targets, making KPP-2 (avoidance ≤ 300ms)
//   validation impossible.

#ifndef SAN_SIM_GAZEBO_HELPERS__DRONE_TARGET_SIMULATOR_NODE_HPP_
#define SAN_SIM_GAZEBO_HELPERS__DRONE_TARGET_SIMULATOR_NODE_HPP_

#include <memory>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/pose_array.hpp>

#include "san_sim_gazebo_helpers/drone_trajectory.hpp"

namespace san_sim_gazebo_helpers
{

class DroneTargetSimulatorNode : public rclcpp::Node
{
public:
  explicit DroneTargetSimulatorNode(
    const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  // ─── Test accessors ────────────────────────────────────
  std::size_t numTargets() const {return targets_.size();}
  KinState    lastSampleForTest(std::size_t i) const;

private:
  struct Target
  {
    std::string name;
    std::unique_ptr<TrajectoryInterface> traj;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr cmd_pub;
    KinState last_sample;
  };

  void declareParameters();
  void loadTargets();
  void onTick();
  Target makeTargetFromYaml(const std::string & target_yaml_string);

  std::vector<Target> targets_;

  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr truth_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  double sim_start_time_s_ = 0.0;
  double tick_rate_hz_ = 50.0;
};

}  // namespace san_sim_gazebo_helpers

#endif  // SAN_SIM_GAZEBO_HELPERS__DRONE_TARGET_SIMULATOR_NODE_HPP_
