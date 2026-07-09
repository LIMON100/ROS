// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-017 RTH helpers.
//
// Pure-logic helpers extracted from rth_action_node so the gtest can
// exercise the accuracy check + home-pose YAML round-trip + RTK-loss
// classifier without spinning rclcpp / DDS / action server runtime.

#ifndef SAN_RTH__RTH_HELPERS_HPP_
#define SAN_RTH__RTH_HELPERS_HPP_

#include <cmath>
#include <fstream>
#include <optional>
#include <string>

#include <geometry_msgs/msg/pose.hpp>
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <yaml-cpp/yaml.h>

namespace san_rth
{

struct AccuracyThresholds
{
  double max_distance_m;   // e.g. 2.0
  double max_yaw_rad;      // e.g. 10° in rad
};

struct AccuracyResult
{
  bool passed;
  double distance_m;
  double yaw_error_rad;
};

// Compute Euclidean distance + absolute yaw error between two poses.
// |yaw_error| is wrapped into [0, π] so a 359° / 1° pair reports 2°
// (the short way), not 358°.
//
// Boundary semantics: the spec ±2 m / ±10° is INCLUSIVE — a pose that
// is exactly on the bound passes. A small tolerance (kBoundaryEps) is
// added to absorb floating-point roundoff from the Pose → Quaternion →
// getYaw roundtrip. 1e-9 rad is ~6e-8° — well below RTK / IMU noise so
// safety margin is unaffected.
inline AccuracyResult evaluateAccuracy(
  const geometry_msgs::msg::Pose & current,
  const geometry_msgs::msg::Pose & home,
  const AccuracyThresholds & thr)
{
  constexpr double kBoundaryEps = 1e-9;

  const double dx = current.position.x - home.position.x;
  const double dy = current.position.y - home.position.y;
  const double dist = std::hypot(dx, dy);

  const double yaw_cur = tf2::getYaw(current.orientation);
  const double yaw_home = tf2::getYaw(home.orientation);
  double dyaw = yaw_cur - yaw_home;
  // Wrap to [-π, π] then take absolute.
  while (dyaw > M_PI) {dyaw -= 2.0 * M_PI;}
  while (dyaw < -M_PI) {dyaw += 2.0 * M_PI;}
  const double yaw_err = std::abs(dyaw);

  return AccuracyResult{
    (dist <= thr.max_distance_m + kBoundaryEps) &&
    (yaw_err <= thr.max_yaw_rad + kBoundaryEps),
    dist,
    yaw_err,
  };
}

// Persist a Pose as a 3-field YAML doc. Returns true on success.
// /run/skyautonet/ is the conventional location (tmpfs — survives
// rclcpp restart but not power cycle, intentional).
inline bool writeHomePoseYaml(
  const std::string & path,
  const geometry_msgs::msg::Pose & pose)
{
  std::ofstream out(path);
  if (!out) {return false;}
  YAML::Node n;
  n["x"] = pose.position.x;
  n["y"] = pose.position.y;
  n["yaw"] = tf2::getYaw(pose.orientation);
  out << n;
  return static_cast<bool>(out);
}

// Inverse of writeHomePoseYaml — load the 3-field doc back. Returns
// std::nullopt if the file is missing or malformed.
inline std::optional<geometry_msgs::msg::Pose> readHomePoseYaml(
  const std::string & path)
{
  try {
    const YAML::Node n = YAML::LoadFile(path);
    if (!n["x"] || !n["y"] || !n["yaw"]) {return std::nullopt;}
    geometry_msgs::msg::Pose p;
    p.position.x = n["x"].as<double>();
    p.position.y = n["y"].as<double>();
    p.position.z = 0.0;
    tf2::Quaternion q;
    q.setRPY(0.0, 0.0, n["yaw"].as<double>());
    p.orientation = tf2::toMsg(q);
    return p;
  } catch (const YAML::Exception &) {
    return std::nullopt;
  }
}

// Decide whether the RTK heading source is currently in a "lost"
// state. Caller tracks the timestamp of the LAST observation that
// crossed the covariance threshold; this helper only checks duration.
//
// loss_start  : when covariance first exceeded the threshold; nullopt
//               means "currently below threshold, no loss in flight".
// now_sec     : current monotonic time in seconds.
// hold_sec    : how long the cov must stay high to declare loss
//               (default 5.0 per spec D-083).
inline bool rtkLossActive(
  std::optional<double> loss_start_sec,
  double now_sec,
  double hold_sec = 5.0)
{
  if (!loss_start_sec.has_value()) {return false;}
  return (now_sec - *loss_start_sec) > hold_sec;
}

}  // namespace san_rth

#endif  // SAN_RTH__RTH_HELPERS_HPP_
