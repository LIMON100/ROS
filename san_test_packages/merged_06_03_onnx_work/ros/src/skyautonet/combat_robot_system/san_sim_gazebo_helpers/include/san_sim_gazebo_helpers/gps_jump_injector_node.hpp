// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — GPS jump injector rclcpp Node.
//
// Subscribes to /robot_{i}/gps/fix (raw Gazebo NavSat output) and
// republishes on /robot_{i}/gps/disturbed_fix with configured
// disturbances applied. Use the disturbed topic as input to the
// navsat_transform (instead of the raw fix) to test dual-EKF
// resilience.
//
// Parameters:
//   robots                   : N — number of robots to wrap
//   jump_at_s, jump_east_m   : single-shot jump at sim time
//   noise_east_std_m, ...    : continuous gaussian
//   dropout_at_s, dropout_duration_s : NO_FIX window

#ifndef SAN_SIM_GAZEBO_HELPERS__GPS_JUMP_INJECTOR_NODE_HPP_
#define SAN_SIM_GAZEBO_HELPERS__GPS_JUMP_INJECTOR_NODE_HPP_

#include <memory>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>

#include "san_sim_gazebo_helpers/gps_disturbance.hpp"

namespace san_sim_gazebo_helpers
{

class GpsJumpInjectorNode : public rclcpp::Node
{
public:
  explicit GpsJumpInjectorNode(
    const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  // ─── Test accessors ────────────────────────────────────
  uint64_t jumpsApplied(std::size_t i) const;
  uint64_t dropoutsApplied(std::size_t i) const;

private:
  struct PerRobot
  {
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr sub;
    rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr pub;
    GpsDisturbance disturbance;
  };

  void declareParameters();
  void wireUp();
  void onFix(std::size_t i, sensor_msgs::msg::NavSatFix::SharedPtr msg);

  std::vector<std::unique_ptr<PerRobot>> robots_;
  double sim_start_s_ = 0.0;
};

}  // namespace san_sim_gazebo_helpers

#endif  // SAN_SIM_GAZEBO_HELPERS__GPS_JUMP_INJECTOR_NODE_HPP_
