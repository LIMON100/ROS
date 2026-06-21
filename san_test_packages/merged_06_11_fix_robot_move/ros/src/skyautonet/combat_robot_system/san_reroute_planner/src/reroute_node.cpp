// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — RerouteNode implementation (PATCHED 2026-05-13).

#include "san_reroute_planner/reroute_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "san_reroute_planner/cost_path_checker.hpp"
#include "san_reroute_planner/lateral_evasion.hpp"

namespace san_reroute_planner
{

using namespace std::chrono_literals;

namespace
{

uint64_t monoUs()
{
  return static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

/// Extract yaw from a quaternion (z component of rotation about +z).
/// PATCH 2026-05-13 (M10).
float yawFromQuat(double qx, double qy, double qz, double qw)
{
  const double siny_cosp = 2.0 * (qw * qz + qx * qy);
  const double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
  return static_cast<float>(std::atan2(siny_cosp, cosy_cosp));
}

float wrapPi(float a)
{
  while (a > static_cast<float>(M_PI)) {a -= 2.0f * static_cast<float>(M_PI);}
  while (a <= -static_cast<float>(M_PI)) {a += 2.0f * static_cast<float>(M_PI);}
  return a;
}

}  // namespace

RerouteNode::RerouteNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("reroute_node", opts)
{
  declareParameters();
  loadParameters();

  cost_sub_ = create_subscription<combat_robot_msgs::msg::CostMapUpdate>(
    "~/cost_map_update", rclcpp::QoS(5).reliable(),
    std::bind(&RerouteNode::onCostMap, this, std::placeholders::_1));

  target_sub_ = create_subscription<combat_robot_msgs::msg::FollowerTargetMessage>(
    "/swarm/formation/follower_target",
    rclcpp::QoS(20).reliable(),
    std::bind(&RerouteNode::onFollowerTarget, this, std::placeholders::_1));

  status_sub_ = create_subscription<combat_robot_msgs::msg::RobotStatus>(
    "~/robot_status", rclcpp::QoS(20).best_effort(),
    std::bind(&RerouteNode::onRobotStatus, this, std::placeholders::_1));

  tier_sub_ = create_subscription<combat_robot_msgs::msg::TierStatusChange>(
    "~/tier_status_change", rclcpp::QoS(20).reliable(),
    std::bind(
      &RerouteNode::onTierStatusChange, this,
      std::placeholders::_1));

  obstacle_pub_ = create_publisher<std_msgs::msg::Bool>(
    "~/obstacle_on_path", rclcpp::QoS(5).reliable());

  cmd_vel_pub_ = create_publisher<geometry_msgs::msg::Twist>(
    "~/cmd_vel", rclcpp::QoS(10).reliable());

  tick_timer_ = create_wall_timer(
    std::chrono::milliseconds(tick_period_ms_),
    std::bind(&RerouteNode::onTick, this));

  RCLCPP_INFO(
    get_logger(),
    "RerouteNode UP: robot_id=%u tick=%ums evasion_speed=%.2fm/s "
    "raw_grid=%d heading_aware=%d",
    robot_id_, tick_period_ms_, evasion_linear_speed_,
    raw_grid_mode_ ? 1 : 0, heading_aware_evasion_ ? 1 : 0);
}

void RerouteNode::declareParameters()
{
  declare_parameter<int>("robot_id", 3);
  declare_parameter<int>("tick_period_ms", 100);
  declare_parameter<double>("evasion_linear_speed_mps", 1.0);
  declare_parameter<double>("evasion_angular_max_rps", 1.0);
  // PATCH 2026-05-13.
  declare_parameter<bool>("raw_grid_mode", false);
  declare_parameter<bool>("heading_aware_evasion", true);
}

void RerouteNode::loadParameters()
{
  robot_id_ = static_cast<uint32_t>(get_parameter("robot_id").as_int());
  tick_period_ms_ = static_cast<uint32_t>(
    get_parameter("tick_period_ms").as_int());
  evasion_linear_speed_ = static_cast<float>(
    get_parameter("evasion_linear_speed_mps").as_double());
  evasion_angular_max_ = static_cast<float>(
    get_parameter("evasion_angular_max_rps").as_double());
  raw_grid_mode_ = get_parameter("raw_grid_mode").as_bool();
  heading_aware_evasion_ = get_parameter("heading_aware_evasion").as_bool();
}

// ─── Cost-grid decode ────────────────────────────────────────────────

bool RerouteNode::decodeCostGrid(
  const combat_robot_msgs::msg::CostMapUpdate & msg,
  CostMapView & out) const
{
  out.width = msg.width_cells;
  out.height = msg.height_cells;
  out.resolution_m = msg.resolution_m;
  out.origin_x_m = static_cast<float>(msg.origin.x);
  out.origin_y_m = static_cast<float>(msg.origin.y);

  const std::size_t expected =
    static_cast<std::size_t>(out.width) * out.height;
  if (expected == 0) {return false;}

  // ★ PATCH 2026-05-13 (C3 + C4):
  // Two explicit modes — no dead-code "if size==expected, else if
  // size!=expected" pair. Caller selects via `raw_grid_mode` param.
  //
  // Raw mode    : payload IS the cost grid (uint8 row-major)
  // PNG mode    : payload is a PNG image; decoded inline.
  //
  // PNG decoding via stb_image is intentionally NOT bundled in this
  // patch (header would need vendoring). For CDR we treat any
  // non-raw payload as decode-failure and rely on san_costmap being
  // configured to publish raw. This is a documented limitation, not
  // a silent failure as in v1.5.0.
  if (raw_grid_mode_) {
    if (msg.cost_grid_png.size() != expected) {
      return false;       // size mismatch — refuse to interpret
    }
    out.grid = msg.cost_grid_png;
    return true;
  }

  // PNG mode — also accept exact-size raw payload as a convenience
  // (some integration tests publish raw on the same topic regardless
  // of the param).
  if (msg.cost_grid_png.size() == expected) {
    out.grid = msg.cost_grid_png;
    return true;
  }
  return false;             // honest "cannot decode" — TODO PNG (CDR)
}

// ─── Subscription callbacks ──────────────────────────────────────────

void RerouteNode::onCostMap(
  const combat_robot_msgs::msg::CostMapUpdate::SharedPtr msg)
{
  CostMapView local;
  if (!decodeCostGrid(*msg, local)) {
    if (!png_decode_warned_) {
      RCLCPP_WARN(
        get_logger(),
        "CostMapUpdate decode failed (raw_grid_mode=%d). "
        "Configure san_costmap to publish raw uint8 grid, or wait "
        "for PNG decode (CDR phase).",
        raw_grid_mode_ ? 1 : 0);
      png_decode_warned_ = true;
    }
    return;
  }
  std::lock_guard<std::mutex> g(state_mu_);
  cost_map_ = std::move(local);
  cost_map_received_us_ = monoUs();
}

void RerouteNode::onFollowerTarget(
  const combat_robot_msgs::msg::FollowerTargetMessage::SharedPtr msg)
{
  if (msg->target_robot_id != robot_id_) {return;}
  std::lock_guard<std::mutex> g(state_mu_);
  target_x_ = static_cast<float>(msg->target_pose_pred_1s.position.x);
  target_y_ = static_cast<float>(msg->target_pose_pred_1s.position.y);
  target_valid_ = true;
}

void RerouteNode::onRobotStatus(
  const combat_robot_msgs::msg::RobotStatus::SharedPtr msg)
{
  if (msg->robot_id != robot_id_) {return;}
  std::lock_guard<std::mutex> g(state_mu_);
  current_x_ = static_cast<float>(msg->pose.position.x);
  current_y_ = static_cast<float>(msg->pose.position.y);
  // ★ PATCH 2026-05-13 (M10): capture yaw for correct angular cmd.
  current_yaw_ = yawFromQuat(
    msg->pose.orientation.x, msg->pose.orientation.y,
    msg->pose.orientation.z, msg->pose.orientation.w);
}

void RerouteNode::onTierStatusChange(
  const combat_robot_msgs::msg::TierStatusChange::SharedPtr msg)
{
  if (msg->robot_id != robot_id_) {return;}
  std::lock_guard<std::mutex> g(state_mu_);
  tier_state_ = msg->current_tier;
}

// ─── 100 ms tick ─────────────────────────────────────────────────────

void RerouteNode::onTick()
{
  // Snapshot under lock.
  CostMapView map_local;
  float sx, sy, ex, ey, yaw;
  uint64_t cost_recv_us;
  bool target_ok;
  uint8_t tier;
  {
    std::lock_guard<std::mutex> g(state_mu_);
    if (!cost_map_.valid() || !target_valid_) {return;}
    map_local = cost_map_;
    sx = current_x_;  sy = current_y_;
    ex = target_x_;   ey = target_y_;
    yaw = current_yaw_;
    cost_recv_us = cost_map_received_us_;
    target_ok = target_valid_;
    tier = tier_state_;
  }
  if (!target_ok) {return;}

  // ─── Step 1: cost-path check (KPP-2 hot path start) ──────────────
  const uint64_t t_check_start = monoUs();
  auto check = checkPath(map_local, sx, sy, ex, ey);

  // ★ PATCH 2026-05-13 (M7): publish lethal-only, not inflated.
  // T1.5 must be triggered by genuine obstacles, not by inflation
  // band (which is a soft cost hint, not a blocker).
  std_msgs::msg::Bool ob;
  ob.data = check.obstacle_detected;
  obstacle_pub_->publish(ob);

  if (!check.obstacle_detected) {
    return;   // no T1.5 needed
  }

  // ─── Step 2: lateral evasion search ─────────────────────────────
  EvasionConfig ev_cfg;
  ev_cfg.heading_aware = heading_aware_evasion_;

  EvasionStatus status = EvasionStatus::Ok;
  auto cand = findBestEvasion(
    map_local, sx, sy, ex, ey, ev_cfg, &status, yaw);

  if (!cand.has_value()) {
    // ★ PATCH 2026-05-13 (C5): differentiate StartCellLethal from
    // AllBlocked. Both → STOP, but logs are different.
    if (status == EvasionStatus::StartCellLethal) {
      RCLCPP_ERROR_THROTTLE(
        get_logger(), *get_clock(), 1000,
        "T1.5: robot is INSIDE lethal cell — emergency STOP "
        "(operator intervention required)");
    } else {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000,
        "T1.5: all evasion candidates blocked (status=%u) — STOP",
        static_cast<unsigned>(status));
    }
    geometry_msgs::msg::Twist stop;
    cmd_vel_pub_->publish(stop);
    return;
  }

  // ─── Step 3: emit /cmd_vel toward evasion waypoint ──────────────
  const float wp_dx = cand->waypoint_x_m - sx;
  const float wp_dy = cand->waypoint_y_m - sy;
  const float wp_yaw = std::atan2(wp_dy, wp_dx);
  const float wp_dist = std::sqrt(wp_dx * wp_dx + wp_dy * wp_dy);

  // ★ PATCH 2026-05-13 (M10): use RELATIVE yaw (target - current),
  // not absolute. The previous code clamped the absolute path
  // bearing as if it were a velocity, producing erratic rotation
  // whenever the robot wasn't facing east.
  const float yaw_err = wrapPi(wp_yaw - yaw);

  geometry_msgs::msg::Twist cmd;
  // Slow linear when far from waypoint heading — gives priority to
  // turning into the right direction before driving.
  const float turn_factor =
    std::max(
    0.0f, 1.0f - std::fabs(yaw_err) /
    static_cast<float>(M_PI));
  cmd.linear.x = std::min(evasion_linear_speed_, wp_dist) * turn_factor;
  cmd.angular.z = std::max(
    -evasion_angular_max_,
    std::min(evasion_angular_max_, yaw_err));
  cmd_vel_pub_->publish(cmd);

  // ─── KPP-2 measurement ─────────────────────────────────────────
  const uint64_t t_done = monoUs();
  const uint64_t fsm_us = t_done - t_check_start;
  const uint64_t total_us = t_done - cost_recv_us;
  RCLCPP_INFO_THROTTLE(
    get_logger(), *get_clock(), 1000,
    "[KPP-2] obstacle@%.2fm offset=%+.2fm fsm=%luus total=%luus "
    "(budget 300000us) tier=%u yaw_err=%+.2frad",
    check.obstacle_distance_m, cand->lateral_offset_m,
    static_cast<unsigned long>(fsm_us),
    static_cast<unsigned long>(total_us),
    tier, yaw_err);
}

}  // namespace san_reroute_planner
