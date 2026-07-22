// SAN v1.5 — RerouteNode implementation.

#include "san_reroute_planner/reroute_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "san_reroute_planner/cost_path_checker.hpp"
#include "san_reroute_planner/lateral_evasion.hpp"

namespace san_reroute_planner {

using namespace std::chrono_literals;

namespace {

uint64_t monoUs() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace

RerouteNode::RerouteNode(const rclcpp::NodeOptions& opts)
    : rclcpp::Node("reroute_node", opts) {
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
      std::bind(&RerouteNode::onTierStatusChange, this,
                 std::placeholders::_1));

  obstacle_pub_ = create_publisher<std_msgs::msg::Bool>(
      "~/obstacle_on_path", rclcpp::QoS(5).reliable());

  cmd_vel_pub_ = create_publisher<geometry_msgs::msg::Twist>(
      "~/cmd_vel", rclcpp::QoS(10).reliable());


  gps_sub_ = create_subscription<sensor_msgs::msg::NavSatFix>(
      "/gps/fix", rclcpp::SensorDataQoS(),
      [this](const sensor_msgs::msg::NavSatFix::SharedPtr msg) { latest_gps_ = *msg; has_gps_ = true; });
  
  swarm_sub_ = create_subscription<geometry_msgs::msg::PoseArray>(
      "/swarm/poses", 10,
      [this](const geometry_msgs::msg::PoseArray::SharedPtr msg) { swarm_poses_ = *msg; has_swarm_ = true; });
  
  combat_sub_ = create_subscription<combat_robot_msgs::msg::LeaderState>(
      "/perception/combat_state", 10,
      [this](const combat_robot_msgs::msg::LeaderState::SharedPtr msg) { combat_state_ = *msg; is_combat_ = msg->target_locked; });
      
  last_stuck_check_time_ = now();

  tick_timer_ = create_wall_timer(
      std::chrono::milliseconds(tick_period_ms_),
      std::bind(&RerouteNode::onTick, this));

  RCLCPP_INFO(get_logger(),
      "RerouteNode UP: robot_id=%u tick=%ums "
      "evasion_speed=%.2fm/s",
      robot_id_, tick_period_ms_, evasion_linear_speed_);
}

void RerouteNode::declareParameters() {
  declare_parameter<int>("robot_id", 3);
  declare_parameter<int>("tick_period_ms", 100);
  declare_parameter<double>("evasion_linear_speed_mps", 1.0);
  declare_parameter<double>("evasion_angular_max_rps", 1.0);
}

void RerouteNode::loadParameters() {
  robot_id_ = static_cast<uint32_t>(get_parameter("robot_id").as_int());
  tick_period_ms_ = static_cast<uint32_t>(
      get_parameter("tick_period_ms").as_int());
  evasion_linear_speed_ = static_cast<float>(
      get_parameter("evasion_linear_speed_mps").as_double());
  evasion_angular_max_ = static_cast<float>(
      get_parameter("evasion_angular_max_rps").as_double());
}

// ─── Subscription callbacks ────────────────────────────────────────────

bool RerouteNode::decodeCostGrid(
    const combat_robot_msgs::msg::CostMapUpdate& msg,
    CostMapView& out) const {
  out.width = msg.width_cells;
  out.height = msg.height_cells;
  out.resolution_m = msg.resolution_m;
  out.origin_x_m = static_cast<float>(msg.origin.x);
  out.origin_y_m = static_cast<float>(msg.origin.y);

  const size_t expected = static_cast<size_t>(out.width) * out.height;

  // PDR-5: PNG decode fallback path.
  // If the payload length exactly matches expected cells, treat as RAW.
  // This is the "san_costmap raw publish" integration option (set a
  // parameter on san_costmap to skip PNG encoding for testing).
  // PNG-encoded payloads need libpng/stb_image — done in CDR phase.
  if (msg.cost_grid_png.size() == expected) {
    out.grid = msg.cost_grid_png;
    return true;
  }
  // PNG payload — currently not decoded. Log once.
  if (msg.cost_grid_png.size() != expected) {
    // Cannot decode — bail. Caller will skip this update.
    return false;
  }
  return true;
}

void RerouteNode::onCostMap(
    const combat_robot_msgs::msg::CostMapUpdate::SharedPtr msg) {
  CostMapView local;
  if (!decodeCostGrid(*msg, local)) {
    if (!png_decode_warned_) {
      RCLCPP_WARN(get_logger(),
          "CostMapUpdate PNG decode not implemented (CDR phase). "
          "Configure san_costmap to publish raw uint8 grid for now.");
      png_decode_warned_ = true;
    }
    return;
  }
  std::lock_guard<std::mutex> g(state_mu_);
  cost_map_ = std::move(local);
  cost_map_received_us_ = monoUs();
}

void RerouteNode::onFollowerTarget(
    const combat_robot_msgs::msg::FollowerTargetMessage::SharedPtr msg) {
  if (msg->target_robot_id != robot_id_) return;
  std::lock_guard<std::mutex> g(state_mu_);
  // Use the 1-second predicted target (SDD §6.3)
  target_x_ = static_cast<float>(msg->target_pose_pred_1s.position.x);
  target_y_ = static_cast<float>(msg->target_pose_pred_1s.position.y);
  target_vx_ = static_cast<float>(msg->target_velocity.linear.x);
  target_valid_ = true;
}

void RerouteNode::onRobotStatus(
    const combat_robot_msgs::msg::RobotStatus::SharedPtr msg) {
  if (msg->robot_id != robot_id_) return;
  std::lock_guard<std::mutex> g(state_mu_);
  current_x_ = static_cast<float>(msg->pose.position.x);
  current_y_ = static_cast<float>(msg->pose.position.y);

  auto q = msg->pose.orientation;
  current_yaw_ = std::atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
}

void RerouteNode::onTierStatusChange(
    const combat_robot_msgs::msg::TierStatusChange::SharedPtr msg) {
  if (msg->robot_id != robot_id_) return;
  std::lock_guard<std::mutex> g(state_mu_);
  tier_state_ = msg->current_tier;
}

// ─── 100 ms tick ────────────────────────────────────────────────────────

// void RerouteNode::onTick() {
//   // Snapshot under lock
//   CostMapView map_local;
//   float       sx, sy, ex, ey;
//   uint64_t    cost_recv_us;
//   bool        target_ok;
//   uint8_t     tier;
//   {
//     std::lock_guard<std::mutex> g(state_mu_);
//     if (!cost_map_.valid() || !target_valid_) return;
//     map_local = cost_map_;                    // copy is intentional
//     sx = current_x_; sy = current_y_;
//     ex = target_x_;  ey = target_y_;
//     cost_recv_us = cost_map_received_us_;
//     target_ok = target_valid_;
//     tier = tier_state_;
//   }
//   if (!target_ok) return;

//   // ─── Step 1: cost path check (KPP-2 timer start point) ────────────
//   const uint64_t t_check_start = monoUs();
//   auto check = checkPath(map_local, sx, sy, ex, ey);

//   // Publish obstacle flag (consumed by tier_node)
//   std_msgs::msg::Bool ob;
//   ob.data = check.obstacle_detected || check.inflated_detected;
//   obstacle_pub_->publish(ob);

//   if (!check.obstacle_detected) {
//     // T0 free path — no evasion needed
//     return;
//   }

//   // ─── Step 2: lateral evasion search ───────────────────────────────
//   auto cand = findBestEvasion(map_local, sx, sy, ex, ey);

//   if (!cand.has_value()) {
//     RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
//         "T1.5: all evasion candidates blocked — falling back to STOP");
//     geometry_msgs::msg::Twist stop;
//     cmd_vel_pub_->publish(stop);
//     return;
//   }

//   // ─── Step 3: emit /cmd_vel toward evasion waypoint ────────────────
//   const float wp_dx = cand->waypoint_x_m - sx;
//   const float wp_dy = cand->waypoint_y_m - sy;
//   const float wp_yaw = std::atan2(wp_dy, wp_dx);
//   const float wp_dist = std::sqrt(wp_dx * wp_dx + wp_dy * wp_dy);

//   geometry_msgs::msg::Twist cmd;
//   // Simple turn-then-drive: angular toward waypoint, then linear
//   cmd.linear.x = std::min(evasion_linear_speed_, wp_dist);
//   // Cap angular velocity
//   cmd.angular.z = std::max(-evasion_angular_max_,
//                             std::min(evasion_angular_max_, wp_yaw));
//   cmd_vel_pub_->publish(cmd);

//   // ─── KPP-2 measurement ────────────────────────────────────────────
//   const uint64_t t_done = monoUs();
//   const uint64_t fsm_us = t_done - t_check_start;
//   const uint64_t total_us = t_done - cost_recv_us;
//   RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 1000,
//       "[KPP-2] obstacle@%.2fm offset=%+.2fm fsm=%lums total=%lums "
//       "(budget 300000us) tier=%u",
//       check.obstacle_distance_m, cand->lateral_offset_m,
//       fsm_us, total_us, tier);
// }

void RerouteNode::onTick() {
  CostMapView map_local;
  float       sx, sy, ex, ey, syaw, tvx;
  uint64_t    cost_recv_us;
  bool        target_ok;
  uint8_t     tier;
  {
    std::lock_guard<std::mutex> g(state_mu_);
    if (!cost_map_.valid() || !target_valid_) return;
    map_local = cost_map_;                    
    sx = current_x_; sy = current_y_; syaw = current_yaw_;
    ex = target_x_;  ey = target_y_;  tvx = target_vx_;
    cost_recv_us = cost_map_received_us_;
    target_ok = target_valid_;
    tier = tier_state_;
  }
  if (!target_ok) return;

  // =========================================================================
  // 1. ANTI-STUCK SLIP DETECTOR
  // =========================================================================
  if (has_gps_) {
      total_ticks_++;
      if (std::abs(last_cmd_vel_x_) > 0.2) high_cmd_ticks_++;

      double dt_stuck = (now() - last_stuck_check_time_).seconds();
      if (dt_stuck >= 3.0) { 
          if (last_lat_ != 0.0 && last_lon_ != 0.0) {
              double d_lat = (latest_gps_.latitude - last_lat_) * 111320.0;
              double d_lon = (latest_gps_.longitude - last_lon_) * 111320.0 * std::cos(latest_gps_.latitude * M_PI / 180.0);
              double physical_dist = std::hypot(d_lat, d_lon);

              if ((float)high_cmd_ticks_ / total_ticks_ > 0.7 && physical_dist < 0.4 && !is_reversing_) {
                  RCLCPP_ERROR(get_logger(), "!!! GPS PHYSICAL STUCK !!! Moved %.2fm. Reversing...", physical_dist);
                  is_reversing_ = true;
                  reverse_start_time_ = now();
              }
          }
          last_lat_ = latest_gps_.latitude;
          last_lon_ = latest_gps_.longitude;
          last_stuck_check_time_ = now();
          high_cmd_ticks_ = 0;
          total_ticks_ = 0;
      }
  }

  if (is_reversing_) {
      if ((now() - reverse_start_time_).seconds() < 2.0) {
          geometry_msgs::msg::Twist rev_cmd;
          rev_cmd.linear.x = -0.8; 
          rev_cmd.angular.z = 0.8; 
          cmd_vel_pub_->publish(rev_cmd);
          return; 
      } else {
          is_reversing_ = false; 
      }
  }

  // =========================================================================
  // 2. BOIDS SEPARATION & COMBAT ENCIRCLE
  // =========================================================================
  double repulse_x = 0.0, repulse_y = 0.0;
  if (has_swarm_) {
    for (const auto& pose : swarm_poses_.poses) {
      double dx = pose.position.x - sx;
      double dy = pose.position.y - sy;
      double d = std::hypot(dx, dy);
      if (d > 0.6 && d < 1.5) { // 1.5m separation dist
        double force = (1.5 - d) / d;
        repulse_x -= dx * force;
        repulse_y -= dy * force;
      }
    }
  }

  if (is_combat_ && combat_state_.swarm_state >= 3) {
      double tx = combat_state_.target_pos.x;
      double ty = combat_state_.target_pos.y;
      double base_angle = std::atan2(combat_state_.pose.position.y - ty, combat_state_.pose.position.x - tx);
      
      double angle_offset = 0.0;
      if (robot_id_ == 2) angle_offset = -M_PI / 3.0;
      else if (robot_id_ == 3) angle_offset = M_PI / 3.0;
      else if (robot_id_ == 4) angle_offset = -2.0 * M_PI / 3.0;
      else if (robot_id_ == 5) angle_offset = 2.0 * M_PI / 3.0;
      
      double final_angle = base_angle + angle_offset;
      while (final_angle > M_PI) final_angle -= 2.0 * M_PI;
      while (final_angle < -M_PI) final_angle += 2.0 * M_PI;

      ex = tx + 5.0 * std::cos(final_angle);
      ey = ty + 5.0 * std::sin(final_angle);
  } else {
      ex += repulse_x;
      ey += repulse_y;
  }

  // =========================================================================
  // 3. PATH CHECK & MOTION CONTROLLER
  // =========================================================================
  const uint64_t t_check_start = monoUs();
  auto check = checkPath(map_local, sx, sy, ex, ey);

  std_msgs::msg::Bool ob;
  ob.data = check.obstacle_detected || check.inflated_detected;
  obstacle_pub_->publish(ob);

  geometry_msgs::msg::Twist cmd;

  if (check.obstacle_detected) {
      // --- TIER 1.5 EVASION (CLIENT LOGIC) ---
      auto cand = findBestEvasion(map_local, sx, sy, ex, ey);
      if (!cand.has_value()) {
          cmd.linear.x = 0.0;
          cmd.angular.z = 0.0;
      } else {
          float wp_dx = cand->waypoint_x_m - sx;
          float wp_dy = cand->waypoint_y_m - sy;
          float wp_yaw = std::atan2(wp_dy, wp_dx);
          cmd.linear.x = std::min(evasion_linear_speed_, std::hypot(wp_dx, wp_dy));
          cmd.angular.z = std::max(-evasion_angular_max_, std::min(evasion_angular_max_, wp_yaw - syaw));
      }
  } else {
      // --- NORMAL FOLLOWER P-CONTROLLER (YOUR LOGIC) ---
      double dx_err = ex - sx;
      double dy_err = ey - sy;
      double dist_err = std::hypot(dx_err, dy_err);
      double angle_to_target = std::atan2(dy_err, dx_err);
      
      double steer = angle_to_target - syaw;
      while (steer > M_PI) steer -= 2.0 * M_PI;
      while (steer < -M_PI) steer += 2.0 * M_PI;

      if (dist_err < 0.3) {
          cmd.linear.x = 0.0;
          cmd.angular.z = is_combat_ ? 1.5 * steer : 0.0; // Face enemy if combat
      } else {
          double desired_speed = is_combat_ ? (0.8 * dist_err) : (std::abs(tvx) + 0.35 * dist_err);
          cmd.linear.x = std::min(1.5, desired_speed);
          cmd.angular.z = 2.5 * steer;
          if (std::abs(steer) > 0.5) cmd.linear.x = std::max(0.15, cmd.linear.x * 0.5); 
      }
  }

  cmd_vel_pub_->publish(cmd);
  last_cmd_vel_x_ = cmd.linear.x;
}

}  // namespace san_reroute_planner
