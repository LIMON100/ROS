// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_surveillance/pan_tilt_driver_node.hpp"

#include <chrono>
#include <cmath>
#include <functional>
#include <optional>

namespace san_surveillance
{

namespace
{
constexpr double kDeg2Rad = M_PI / 180.0;
}  // namespace

PanTiltDriverNode::PanTiltDriverNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("pan_tilt_driver_node", opts)
{
  robot_id_ = static_cast<uint32_t>(declare_parameter<int>("robot_id", 1));
  rate_hz_ = declare_parameter<double>("rate_hz", 50.0);
  if (rate_hz_ < 1.0) {rate_hz_ = 50.0;}
  dt_s_ = static_cast<float>(1.0 / rate_hz_);

  // Track/Engage hold-down: if no fresh PanTiltCommand arrives within this
  // window the gimbal stops slewing after a stale target instead of chasing
  // it forever (commander died / comms loss). Sweep is deliberately exempt —
  // a single command starts an autonomous sweep (SDD §8.3). 0 disables.
  cmd_timeout_s_ = declare_parameter<double>("cmd_timeout_s", 3.0);

  cmd_sub_ = create_subscription<PanTiltCommandMsg>(
    "/swarm/cmd/pantilt", rclcpp::QoS(20).reliable(),
    std::bind(&PanTiltDriverNode::onCommand, this, std::placeholders::_1));

  pan_pub_ = create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
  tilt_pub_ = create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);

  timer_ = create_wall_timer(
    std::chrono::duration<double>(1.0 / rate_hz_),
    std::bind(&PanTiltDriverNode::onTick, this));

  RCLCPP_INFO(
    get_logger(),
    "PanTiltDriverNode UP: robot_id=%u rate=%.0fHz -> gimbal/pan_cmd,tilt_cmd",
    robot_id_, rate_hz_);
}

void PanTiltDriverNode::onCommand(const PanTiltCommandMsg::SharedPtr msg)
{
  if (msg->robot_id != robot_id_) {return;}        // not addressed to me
  std::lock_guard<std::mutex> g(cmd_mu_);
  have_cmd_ = true;
  cmd_mode_ = msg->mode;
  cmd_pan_deg_ = msg->target_pan_deg;
  cmd_tilt_deg_ = msg->target_tilt_deg;
  cmd_sweep_range_deg_ = msg->sweep_range_deg;
  cmd_stamp_ = now();
}

void PanTiltDriverNode::onTick()
{
  uint8_t mode;
  float pan, tilt, sweep_range;
  bool have;
  rclcpp::Time stamp;
  {
    std::lock_guard<std::mutex> g(cmd_mu_);
    have = have_cmd_;
    mode = cmd_mode_;
    pan = cmd_pan_deg_;
    tilt = cmd_tilt_deg_;
    sweep_range = cmd_sweep_range_deg_;
    stamp = cmd_stamp_;
  }
  if (!have) {return;}                             // nothing commanded yet

  state_.mode = static_cast<PanTiltMode>(mode);

  // Stale Track/Engage target → hold current attitude (Fixed) until a fresh
  // command arrives. Sweep keeps running on its latched sector.
  if (cmd_timeout_s_ > 0.0 &&
    (state_.mode == PanTiltMode::Track || state_.mode == PanTiltMode::Engage) &&
    (now() - stamp).seconds() > cmd_timeout_s_)
  {
    state_.mode = PanTiltMode::Fixed;
  }

  std::optional<SweepParams> sweep;
  std::optional<TrackTarget> track;
  std::optional<EngageTarget> engage;
  switch (state_.mode) {
    case PanTiltMode::Sweep: sweep = SweepParams{pan, sweep_range, false}; break;
    case PanTiltMode::Track: track = TrackTarget{pan, tilt}; break;
    case PanTiltMode::Engage: engage = EngageTarget{pan, tilt, 0.0f, 0.0f}; break;
    case PanTiltMode::Fixed:
    default: break;
  }

  stepController(cfg_, state_, dt_s_, sweep, track, engage);

  std_msgs::msg::Float64 p;
  p.data = state_.pan_deg * kDeg2Rad;
  std_msgs::msg::Float64 t;
  t.data = state_.tilt_deg * kDeg2Rad;
  pan_pub_->publish(p);
  tilt_pub_->publish(t);
}

}  // namespace san_surveillance
