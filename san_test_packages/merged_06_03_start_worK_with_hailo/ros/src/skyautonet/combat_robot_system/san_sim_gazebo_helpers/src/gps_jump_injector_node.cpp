// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — GPS jump injector rclcpp Node implementation.

#include "san_sim_gazebo_helpers/gps_jump_injector_node.hpp"

#include <algorithm>
#include <chrono>
#include <string>

namespace san_sim_gazebo_helpers
{

GpsJumpInjectorNode::GpsJumpInjectorNode(const rclcpp::NodeOptions & options)
: Node("gps_jump_injector", options)
{
  declareParameters();
  wireUp();
  sim_start_s_ = now().seconds();
  RCLCPP_INFO(
    get_logger(), "GpsJumpInjector UP for %zu robot(s)",
    robots_.size());
}

void GpsJumpInjectorNode::declareParameters()
{
  declare_parameter<int>("robots", 1);

  declare_parameter<bool>("enable_jump", true);
  declare_parameter<double>("jump_at_s", 8.0);
  declare_parameter<double>("jump_east_m", 2.5);
  declare_parameter<double>("jump_north_m", 0.5);
  declare_parameter<double>("jump_recovery_s", 2.0);

  declare_parameter<bool>("enable_noise", true);
  declare_parameter<double>("noise_east_std_m", 0.3);
  declare_parameter<double>("noise_north_std_m", 0.3);
  declare_parameter<double>("noise_alt_std_m", 0.6);

  declare_parameter<bool>("enable_dropout", false);
  declare_parameter<double>("dropout_at_s", 15.0);
  declare_parameter<double>("dropout_duration_s", 3.0);

  declare_parameter<bool>("enable_drift", false);
  declare_parameter<double>("drift_start_s", 0.0);
  declare_parameter<double>("drift_end_s", 60.0);
  declare_parameter<double>("drift_east_rate_m_s", 0.05);
}

void GpsJumpInjectorNode::wireUp()
{
  const int n = std::max(
    1, static_cast<int>(
      get_parameter("robots").as_int()));
  robots_.reserve(n);

  // Build a shared base config once.
  GpsDisturbance base;
  if (get_parameter("enable_jump").as_bool()) {
    JumpConfig jc;
    jc.at_sim_time_s = get_parameter("jump_at_s").as_double();
    jc.east_offset_m = get_parameter("jump_east_m").as_double();
    jc.north_offset_m = get_parameter("jump_north_m").as_double();
    jc.recovery_time_s = get_parameter("jump_recovery_s").as_double();
    base.withJump(jc);
  }
  if (get_parameter("enable_noise").as_bool()) {
    NoiseConfig nc;
    nc.east_std_m = get_parameter("noise_east_std_m").as_double();
    nc.north_std_m = get_parameter("noise_north_std_m").as_double();
    nc.altitude_std_m = get_parameter("noise_alt_std_m").as_double();
    nc.rng_seed = 42;
    base.withNoise(nc);
  }
  if (get_parameter("enable_dropout").as_bool()) {
    DropoutConfig dc;
    dc.start_at_s = get_parameter("dropout_at_s").as_double();
    dc.duration_s = get_parameter("dropout_duration_s").as_double();
    base.withDropout(dc);
  }
  if (get_parameter("enable_drift").as_bool()) {
    DriftConfig fc;
    fc.start_at_s = get_parameter("drift_start_s").as_double();
    fc.end_at_s = get_parameter("drift_end_s").as_double();
    fc.east_rate_m_per_s = get_parameter("drift_east_rate_m_s").as_double();
    base.withDrift(fc);
  }

  const rclcpp::QoS sensor_qos =
    rclcpp::QoS(rclcpp::KeepLast(5))
    .best_effort()             // GPS topics typically BEST_EFFORT
    .durability_volatile();

  for (int i = 1; i <= n; ++i) {
    auto r = std::make_unique<PerRobot>();
    // Per-robot disturbance with a seed offset (so different robots
    // don't have correlated noise sequences).
    r->disturbance = base;
    if (get_parameter("enable_noise").as_bool()) {
      NoiseConfig nc;
      nc.east_std_m = get_parameter("noise_east_std_m").as_double();
      nc.north_std_m = get_parameter("noise_north_std_m").as_double();
      nc.altitude_std_m = get_parameter("noise_alt_std_m").as_double();
      nc.rng_seed = 42u + static_cast<uint32_t>(i);
      r->disturbance.withNoise(nc);
    }

    const std::string sub_topic =
      "/robot_" + std::to_string(i) + "/gps/fix";
    const std::string pub_topic =
      "/robot_" + std::to_string(i) + "/gps/disturbed_fix";

    const auto idx = static_cast<std::size_t>(i - 1);
    r->sub = create_subscription<sensor_msgs::msg::NavSatFix>(
      sub_topic, sensor_qos,
      [this, idx](sensor_msgs::msg::NavSatFix::SharedPtr msg) {
        onFix(idx, msg);
      });
    r->pub = create_publisher<sensor_msgs::msg::NavSatFix>(
      pub_topic, sensor_qos);

    robots_.push_back(std::move(r));
    RCLCPP_INFO(
      get_logger(), "robot_%d: %s → %s",
      i, sub_topic.c_str(), pub_topic.c_str());
  }
}

void GpsJumpInjectorNode::onFix(
  std::size_t i,
  sensor_msgs::msg::NavSatFix::SharedPtr msg)
{
  if (i >= robots_.size()) {return;}

  const double t = now().seconds() - sim_start_s_;

  GpsFix in;
  in.latitude_deg = msg->latitude;
  in.longitude_deg = msg->longitude;
  in.altitude_m = msg->altitude;
  for (int k = 0; k < 9; ++k) {
    in.position_covariance[k] = msg->position_covariance[k];
  }
  in.status = static_cast<uint8_t>(msg->status.status);

  const auto disturbed_opt = robots_[i]->disturbance.apply(in, t);
  if (!disturbed_opt) {
    // Dropout window — emit NO_FIX status.
    auto out = *msg;
    out.status.status = sensor_msgs::msg::NavSatStatus::STATUS_NO_FIX;
    robots_[i]->pub->publish(out);
    return;
  }
  const auto & d = *disturbed_opt;

  auto out = *msg;
  out.latitude = d.latitude_deg;
  out.longitude = d.longitude_deg;
  out.altitude = d.altitude_m;
  for (int k = 0; k < 9; ++k) {
    out.position_covariance[k] = d.position_covariance[k];
  }
  // The covariance type indicates that the receiver knows the inflated
  // uncertainty (so downstream EKF properly weights this measurement).
  out.position_covariance_type =
    sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;
  robots_[i]->pub->publish(out);
}

uint64_t GpsJumpInjectorNode::jumpsApplied(std::size_t i) const
{
  if (i >= robots_.size()) {return 0;}
  return robots_[i]->disturbance.jumpsApplied();
}

uint64_t GpsJumpInjectorNode::dropoutsApplied(std::size_t i) const
{
  if (i >= robots_.size()) {return 0;}
  return robots_[i]->disturbance.dropoutsApplied();
}

}  // namespace san_sim_gazebo_helpers
