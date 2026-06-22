// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_rtk_gnss/rtk_quality_manager_node.hpp"

#include <algorithm>
#include <chrono>

namespace san_rtk_gnss
{

using RtkStatusMsg = combat_robot_msgs::msg::RtkFixStatus;
using NavConstraintsMsg = combat_robot_msgs::msg::NavConstraints;
using ThreatAlertMsg = combat_robot_msgs::msg::ThreatAlert;
using namespace std::chrono_literals;

namespace
{
uint64_t toMs(const rclcpp::Time & t)
{
  const int64_t ns = t.nanoseconds();
  return ns > 0 ? static_cast<uint64_t>(ns / 1'000'000LL) : 0ULL;
}
}  // namespace

RtkQualityManagerNode::RtkQualityManagerNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("rtk_quality_manager", opts)
{
  declareParameters();
  loadParameters();

  constraints_pub_ = create_publisher<NavConstraintsMsg>(
    "~/nav_constraints", rclcpp::QoS(5).reliable());
  threat_pub_ = create_publisher<ThreatAlertMsg>(
    "~/threat_alert", rclcpp::QoS(10).reliable());

  status_sub_ = create_subscription<RtkStatusMsg>(
    "rtk_status", rclcpp::QoS(5).reliable(),
    std::bind(&RtkQualityManagerNode::onStatus, this, std::placeholders::_1));

  const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::duration<double>(1.0 / std::max(0.1, tick_hz_)));
  tick_timer_ = create_wall_timer(
    period, std::bind(&RtkQualityManagerNode::onTick, this));

  RCLCPP_INFO(
    get_logger(),
    "rtk_quality_manager UP: robot=%s tick=%.1fHz timeout=%.1fs "
    "grace=%.0fs lost=%.0fs",
    robot_id_.c_str(), tick_hz_, sensor_timeout_sec_,
    fsm_.params().grace_sec, fsm_.params().active_lost_sec);
}

void RtkQualityManagerNode::declareParameters()
{
  declare_parameter<std::string>("robot_id", "");
  declare_parameter<double>("tick_hz", 5.0);
  declare_parameter<double>("sensor_timeout_sec", 2.0);
  declare_parameter<double>("grace_sec", 5.0);
  declare_parameter<double>("active_lost_sec", 30.0);
  declare_parameter<double>("recover_sec", 2.0);
  declare_parameter<double>("ok_speed_mps", 1.5);
  declare_parameter<double>("degraded_speed_mps", 0.5);
  declare_parameter<double>("ok_tolerance_m", 0.3);
  declare_parameter<double>("degraded_tolerance_m", 1.0);
}

void RtkQualityManagerNode::loadParameters()
{
  robot_id_ = get_parameter("robot_id").as_string();
  tick_hz_ = get_parameter("tick_hz").as_double();
  sensor_timeout_sec_ = get_parameter("sensor_timeout_sec").as_double();

  RtkQualityParams p;
  p.grace_sec = get_parameter("grace_sec").as_double();
  p.active_lost_sec = get_parameter("active_lost_sec").as_double();
  p.recover_sec = get_parameter("recover_sec").as_double();
  p.ok_speed_mps =
    static_cast<float>(get_parameter("ok_speed_mps").as_double());
  p.degraded_speed_mps =
    static_cast<float>(get_parameter("degraded_speed_mps").as_double());
  p.ok_tolerance_m =
    static_cast<float>(get_parameter("ok_tolerance_m").as_double());
  p.degraded_tolerance_m =
    static_cast<float>(get_parameter("degraded_tolerance_m").as_double());
  fsm_ = RtkQualityFsm(p);
}

void RtkQualityManagerNode::onStatus(RtkStatusMsg::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  last_fix_type_ = msg->fix_type;
  last_msg_sec_ = now().seconds();
  have_msg_ = true;
}

void RtkQualityManagerNode::onTick()
{
  computeAndPublish(now().seconds());
}

NavConstraintsMsg RtkQualityManagerNode::computeAndPublish(double now_sec)
{
  // A stale (or never-received) fix counts as NOT Fixed, so a silent RTK
  // receiver still escalates through the FSM timeouts.
  const bool fresh =
    have_msg_ && (now_sec - last_msg_sec_) <= sensor_timeout_sec_;
  const bool good = fresh && (last_fix_type_ == RtkStatusMsg::FIX_RTK_FIX);

  const RtkQualityState state = fsm_.update(good, now_sec);

  NavConstraintsMsg msg;
  msg.header.stamp = now();
  msg.header.frame_id = robot_id_;
  msg.rtk_state = static_cast<uint8_t>(state);
  msg.max_speed_mps = fsm_.maxSpeed();
  msg.path_tolerance_m = fsm_.pathTolerance();
  msg.formation_loose = fsm_.formationLoose();
  msg.source_robot_id = robot_id_;
  msg.timestamp_ms = toMs(now());
  if (constraints_pub_) {constraints_pub_->publish(msg);}

  // Emit a ThreatAlert once, on the OK/degraded → LOST transition.
  if (state == RtkQualityState::Lost && last_state_ != RtkQualityState::Lost) {
    ThreatAlertMsg ta;
    ta.header.stamp = msg.header.stamp;
    ta.severity = ThreatAlertMsg::SEVERITY_WARNING;
    ta.threat_type = ThreatAlertMsg::TYPE_RTK_LOST;
    ta.source_robot_id = robot_id_.empty() ? "hub" : robot_id_;
    ta.message_ko = "RTK 측위 상실 — 정밀 위치 불가, 방어 주행 전환";
    ta.detail = "{\"layer\":\"rtk_quality_fsm\",\"state\":\"LOST\"}";
    ta.timestamp_ms = msg.timestamp_ms;
    ta.instance_count = 1;
    ta.has_position = false;
    if (threat_pub_) {threat_pub_->publish(ta);}
    ++threat_alerts_emitted_;
    RCLCPP_WARN(
      get_logger(), "RTK_LOST — ThreatAlert emitted (robot=%s)",
      robot_id_.c_str());
  }
  last_state_ = state;
  return msg;
}

}  // namespace san_rtk_gnss
