// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>
#include <utility>

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>

#include "san_follower_tier/cmd_smoother.hpp"


namespace
{

double yawFromQuat(const geometry_msgs::msg::Quaternion & q)
{
  const double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
  const double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
  return std::atan2(siny_cosp, cosy_cosp);
}

double wrapPi(double a)
{
  while (a > M_PI) {a -= 2.0 * M_PI;}
  while (a <= -M_PI) {a += 2.0 * M_PI;}
  return a;
}

}  // namespace

class FollowerPursuitNode : public rclcpp::Node
{
public:
  FollowerPursuitNode()
  : rclcpp::Node("follower_pursuit_node")
  {
    robot_id_ = declare_parameter<int>("robot_id", 2);

    // Formation offset behind the leader. Defaults mirror the sim spawn
    // formula in swarm_sim.launch.py so followers hold roughly where they
    // start. Row/side derived from robot_id; magnitudes are tunable.
    const double offset_back = declare_parameter<double>("offset_back_m", -3.0);
    const double offset_side = declare_parameter<double>("offset_side_m", 2.5);
    const int row = robot_id_ / 2;
    const int side = (robot_id_ % 2 == 0) ? 1 : -1;
    offset_back_ = offset_back * static_cast<double>(row);
    offset_side_ = offset_side * static_cast<double>(side);

    max_linear_ = declare_parameter<double>("max_linear_mps", 1.2);
    max_angular_ = declare_parameter<double>("max_angular_rps", 1.5);
    kp_linear_ = declare_parameter<double>("kp_linear", 0.8);
    kp_angular_ = declare_parameter<double>("kp_angular", 2.0);
    stop_distance_ = declare_parameter<double>("stop_distance_m", 0.4);
    leader_timeout_s_ = declare_parameter<double>("leader_timeout_s", 1.5);
    const std::string leader_topic =
      declare_parameter<std::string>("leader_odom_topic", "/odom");

    // Own-odom → world offset. In Gazebo each robot's DiffDrive odom frame
    // starts at that robot's spawn point, so a follower's odom origin is
    // shifted from the leader's. Without this the follower compares its
    // spawn-relative pose against the leader's world pose and holds the
    // wrong slot. Defaults (0) leave the single-shared-frame case (real
    // hardware / squadron bring-up) unchanged — set these to the follower's
    // spawn pose so its pose is reported in the leader's world frame.
    own_off_x_ = declare_parameter<double>("own_odom_offset_x", 0.0);
    own_off_y_ = declare_parameter<double>("own_odom_offset_y", 0.0);
    own_off_yaw_ = declare_parameter<double>("own_odom_offset_yaw", 0.0);

    obstacle_brake_dist_ = declare_parameter<double>("obstacle_brake_dist_m", 2.5);
    obstacle_stop_dist_ = declare_parameter<double>("obstacle_stop_dist_m", 0.8);
    obstacle_cone_rad_ = declare_parameter<double>("obstacle_cone_rad", 0.6);

    // Anti-collision with the leader: the follower never drives closer than
    // this to the leader (0 disables). Prevents rear-ending / pushing the
    // leader when the follower out-runs it to close the formation gap.
    min_leader_dist_ = declare_parameter<double>("min_leader_dist_m", 0.0);

    // Path (breadcrumb) following: when true the follower trails the leader
    // along the leader's ACTUAL driven path (so it rounds the same corners),
    // instead of aiming at a fixed point offset_back behind the leader's
    // current pose (which cuts corners with a large radius). offset_side is
    // ignored in this mode — the follower stays on the leader's track.
    path_follow_ = declare_parameter<bool>("path_follow", false);
    trail_min_step_ = declare_parameter<double>("trail_min_step_m", 0.15);

    // cmd_vel smoothing (slew-rate limit + low-pass) to damp the angular
    // command chatter that shows up as follower "shaking while moving". Linear
    // decel is allowed faster than accel so obstacle/anti-collision stops stay
    // responsive. Defaults are mild; raise the accel limits and set the alphas
    // to 1.0 to disable (pass-through).
    smoother_.max_lin_accel = declare_parameter<double>("smooth_max_lin_accel", 2.5);
    smoother_.max_lin_decel = declare_parameter<double>("smooth_max_lin_decel", 5.0);
    smoother_.max_ang_accel = declare_parameter<double>("smooth_max_ang_accel", 6.0);
    smoother_.lp_alpha_v = declare_parameter<double>("smooth_lp_alpha_v", 1.0);
    smoother_.lp_alpha_w = declare_parameter<double>("smooth_lp_alpha_w", 0.5);

    leader_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      leader_topic, rclcpp::SensorDataQoS(),
      [this](nav_msgs::msg::Odometry::SharedPtr m) {
        leader_x_ = m->pose.pose.position.x;
        leader_y_ = m->pose.pose.position.y;
        leader_yaw_ = yawFromQuat(m->pose.pose.orientation);
        leader_stamp_ = now();
        have_leader_ = true;

        // Drop a breadcrumb whenever the leader has moved far enough; the
        // follower targets a point this trail at the formation distance back.
        if (path_follow_) {
          if (leader_trail_.empty() ||
          std::hypot(
            leader_x_ - leader_trail_.back().first,
            leader_y_ - leader_trail_.back().second) > trail_min_step_)
          {
            leader_trail_.emplace_back(leader_x_, leader_y_);
            if (leader_trail_.size() > 4000) {leader_trail_.pop_front();}
          }
        }
      });

    // Relative — resolves to /robot_N/odom inside the per-robot namespace.
    own_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      "odom", rclcpp::SensorDataQoS(),
      [this](nav_msgs::msg::Odometry::SharedPtr m) {
        // Rotate+translate the spawn-relative odom pose into the leader's
        // world frame (offsets default to 0 → identity).
        const double rx = m->pose.pose.position.x;
        const double ry = m->pose.pose.position.y;
        const double c = std::cos(own_off_yaw_);
        const double s = std::sin(own_off_yaw_);
        own_x_ = own_off_x_ + (c * rx - s * ry);
        own_y_ = own_off_y_ + (s * rx + c * ry);
        own_yaw_ = wrapPi(own_off_yaw_ + yawFromQuat(m->pose.pose.orientation));
        have_own_ = true;
      });

    scan_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      "scan/points_filtered", rclcpp::SensorDataQoS(),
      std::bind(&FollowerPursuitNode::onScan, this, std::placeholders::_1));

    cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

    timer_ = create_wall_timer(
      std::chrono::milliseconds(50),
      std::bind(&FollowerPursuitNode::onTick, this));

    RCLCPP_INFO(
      get_logger(),
      "FollowerPursuitNode UP: robot_id=%d offset=(back %.1fm, side %.1fm) "
      "leader_topic=%s", robot_id_, offset_back_, offset_side_,
      leader_topic.c_str());
  }

private:
  void onTick()
  {
    geometry_msgs::msg::Twist cmd;  // zero by default

    const rclcpp::Time t_now = now();
    double dt = 0.05;
    if (last_tick_.nanoseconds() != 0) {
      const double measured = (t_now - last_tick_).seconds();
      if (measured > 1e-3 && measured < 0.5) {dt = measured;}
    }
    last_tick_ = t_now;

    // Safety holds: no feeds yet, or the leader feed went stale (leader killed
    // / comms loss). Stop now and reset the smoother so the next motion ramps
    // from rest rather than being slew-limited down from a stale command.
    if (!have_leader_ || !have_own_ ||
      (t_now - leader_stamp_).seconds() > leader_timeout_s_)
    {
      smoother_.reset();
      cmd_pub_->publish(cmd);
      return;
    }

    // Formation target in world frame. In path-follow mode the follower aims
    // at the point on the leader's DRIVEN track that is |offset_back_| behind
    // the leader (so it rounds the same corners instead of cutting them);
    // otherwise it aims at a fixed offset behind the leader's current pose.
    double tx, ty;
    if (path_follow_ && leader_trail_.size() >= 2) {
      double rem = std::fabs(offset_back_);
      tx = leader_x_;
      ty = leader_y_;
      for (std::size_t i = leader_trail_.size() - 1; i > 0; --i) {
        const double ax = leader_trail_[i].first;
        const double ay = leader_trail_[i].second;
        const double bx = leader_trail_[i - 1].first;
        const double by = leader_trail_[i - 1].second;
        const double seg = std::hypot(ax - bx, ay - by);
        if (seg >= rem) {
          const double t = (seg > 1e-6) ? (rem / seg) : 0.0;
          tx = ax + t * (bx - ax);
          ty = ay + t * (by - ay);
          break;
        }
        rem -= seg;
        tx = bx;
        ty = by;
      }
    } else {
      const double cy = std::cos(leader_yaw_);
      const double sy = std::sin(leader_yaw_);
      tx = leader_x_ + (offset_back_ * cy - offset_side_ * sy);
      ty = leader_y_ + (offset_back_ * sy + offset_side_ * cy);
    }

    const double dx = tx - own_x_;
    const double dy = ty - own_y_;
    const double dist = std::hypot(dx, dy);

    double linear = 0.0;
    double angular = 0.0;

    if (dist < stop_distance_) {
      // Arrived at slot — hold position, only align heading to the leader.
      angular = std::clamp(
        kp_angular_ * wrapPi(leader_yaw_ - own_yaw_), -max_angular_, max_angular_);
    } else {
      const double desired_yaw = std::atan2(dy, dx);
      const double yaw_err = wrapPi(desired_yaw - own_yaw_);

      // Turn-in-place priority: scale linear down as heading error grows.
      const double turn_factor = std::max(0.0, 1.0 - std::fabs(yaw_err) / M_PI);
      linear = std::clamp(kp_linear_ * dist, 0.0, max_linear_) * turn_factor;
      angular = std::clamp(kp_angular_ * yaw_err, -max_angular_, max_angular_);

      // ── Obstacle braking / steer-around (from scan/points_filtered) ──
      const bool obs_fresh = have_obstacle_ &&
        (now() - obstacle_stamp_).seconds() < 0.5;
      if (obs_fresh && nearest_front_ < obstacle_brake_dist_) {
        if (nearest_front_ < obstacle_stop_dist_) {
          linear = 0.0;                                  // too close → stop
          angular = (nearest_left_ < nearest_right_) ?   // turn to clearer side
            -max_angular_ : max_angular_;
        } else {
          const double scale =
            (nearest_front_ - obstacle_stop_dist_) /
            (obstacle_brake_dist_ - obstacle_stop_dist_);
          linear *= std::clamp(scale, 0.0, 1.0);         // slow down
          angular += (nearest_left_ < nearest_right_) ? -1.0 : 1.0;
          angular = std::clamp(angular, -max_angular_, max_angular_);
        }
      }

      // Hard anti-collision: stop forward motion if too close to the leader,
      // independent of the formation target (handles overshoot / leader halt).
      if (min_leader_dist_ > 0.0) {
        const double dlead = std::hypot(leader_x_ - own_x_, leader_y_ - own_y_);
        if (dlead < min_leader_dist_ && linear > 0.0) {linear = 0.0;}
      }
    }

    // Smooth (slew-rate limit + low-pass) to damp the angular chatter that
    // reads as follower "shaking"; asymmetric limits keep braking responsive.
    const std::pair<double, double> sc = smoother_.apply(linear, angular, dt);
    cmd.linear.x = sc.first;
    cmd.angular.z = sc.second;
    cmd_pub_->publish(cmd);
  }

  void onScan(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
  {
    // Undo the LiDAR's fixed -15.6° mount tilt (sensors.xacro lidar_pitch=-0.272)
    // so x = true horizontal-forward. Raw tilted points mis-range obstacles.
    constexpr double kPitch = -0.272;
    const double cp = std::cos(kPitch), sp = std::sin(kPitch);
    double mf = 1e9, ml = 1e9, mr = 1e9;
    sensor_msgs::PointCloud2ConstIterator<float> ix(*msg, "x");
    sensor_msgs::PointCloud2ConstIterator<float> iy(*msg, "y");
    sensor_msgs::PointCloud2ConstIterator<float> iz(*msg, "z");
    for (; ix != ix.end(); ++ix, ++iy, ++iz) {
      const double xr = *ix, y = *iy, zr = *iz;
      if (!std::isfinite(xr) || !std::isfinite(y) || !std::isfinite(zr)) {continue;}
      const double x = cp * xr + sp * zr;      // de-tilted forward
      if (x <= 0.0) {continue;}                 // only points ahead
      if (std::fabs(std::atan2(y, x)) > obstacle_cone_rad_) {continue;}
      const double d = std::hypot(x, y);
      if (d < mf) {mf = d;}
      if (y >= 0.0) {if (d < ml) {ml = d;}} else {if (d < mr) {mr = d;}}
    }
    nearest_front_ = mf;
    nearest_left_ = ml;
    nearest_right_ = mr;
    obstacle_stamp_ = now();
    have_obstacle_ = true;
  }


  double obstacle_brake_dist_{2.5}, obstacle_stop_dist_{0.8}, obstacle_cone_rad_{0.6};
  double min_leader_dist_{0.0};
  bool path_follow_{false};
  double trail_min_step_{0.15};
  std::deque<std::pair<double, double>> leader_trail_;
  double nearest_front_{1e9}, nearest_left_{1e9}, nearest_right_{1e9};
  bool have_obstacle_{false};
  rclcpp::Time obstacle_stamp_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr scan_sub_;

  int robot_id_{2};
  double offset_back_{-3.0}, offset_side_{2.5};
  double max_linear_{1.2}, max_angular_{1.5};
  double kp_linear_{0.8}, kp_angular_{2.0};
  double stop_distance_{0.4}, leader_timeout_s_{1.5};

  double leader_x_{0.0}, leader_y_{0.0}, leader_yaw_{0.0};
  double own_x_{0.0}, own_y_{0.0}, own_yaw_{0.0};
  double own_off_x_{0.0}, own_off_y_{0.0}, own_off_yaw_{0.0};
  bool have_leader_{false}, have_own_{false};
  rclcpp::Time leader_stamp_;

  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr leader_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr own_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  san_follower_tier::CmdSmoother smoother_;
  rclcpp::Time last_tick_{0, 0, RCL_ROS_TIME};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<FollowerPursuitNode>());
  } catch (const std::exception & e) {
    RCLCPP_FATAL(
      rclcpp::get_logger("follower_pursuit_main"),
      "FollowerPursuitNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
