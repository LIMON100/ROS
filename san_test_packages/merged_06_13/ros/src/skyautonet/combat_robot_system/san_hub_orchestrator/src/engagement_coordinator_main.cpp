// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.
//
// SAN v1.5 — Engagement coordinator (Phase 2: 2+2 multi-threat encircle).
// Leader-only. Clusters person threats into <=2 targets, assigns the
// followers 2+2 (cap-balanced nearest, threat_assignment.hpp), and
// publishes a per-robot /robot_<id>/encircle_target. Leader standoffs target 0.

#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <nav2_msgs/action/navigate_through_poses.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include "san_hub_orchestrator/threat_assignment.hpp"
#include <std_msgs/msg/u_int32_multi_array.hpp>

using NavigateThroughPoses = nav2_msgs::action::NavigateThroughPoses;
using ThreatAlert = combat_robot_msgs::msg::ThreatAlert;

class EngagementCoordinator : public rclcpp::Node
{
public:
  EngagementCoordinator()
  : rclcpp::Node("engagement_coordinator")
  {
    leader_robot_id_ = declare_parameter<int>("leader_robot_id", 1);
    follower_ids_ = declare_parameter<std::vector<int64_t>>(
      "follower_ids", std::vector<int64_t>{2, 3, 4, 5});
    combat_standoff_m_ = declare_parameter<double>("combat_standoff_m", 2.5);
    person_threat_type_ = declare_parameter<int>("person_threat_type", 99);
    min_range_m_ = declare_parameter<double>("min_range_m", 0.5);
    max_range_m_ = declare_parameter<double>("max_range_m", 30.0);
    smoothing_ = declare_parameter<double>("smoothing", 0.3);
    merge_radius_m_ = declare_parameter<double>("merge_radius_m", 2.5);
    max_targets_ = declare_parameter<int>("max_targets", 2);
    map_frame_ = declare_parameter<std::string>("map_frame", "map");
    base_frame_ = declare_parameter<std::string>("leader_base_frame", "base_footprint");
    const double rate = declare_parameter<double>("publish_rate_hz", 5.0);
    const std::string nav_action =
      declare_parameter<std::string>("nav_action", "navigate_through_poses");

    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
    nav_client_ = rclcpp_action::create_client<NavigateThroughPoses>(this, nav_action);
    auto qos = rclcpp::QoS(10).reliable();
    threat_sub_ = create_subscription<ThreatAlert>(
      "/swarm/threat_alert_raw", qos,
      std::bind(&EngagementCoordinator::onThreat, this, std::placeholders::_1));
    for (auto id64 : follower_ids_)
    {
      const int id = static_cast<int>(id64);
      target_pubs_[id] = create_publisher<geometry_msgs::msg::PointStamped>(
        "/robot_" + std::to_string(id) + "/encircle_target", qos);
    }
    engaged_pub_ = create_publisher<std_msgs::msg::UInt32MultiArray>(
    "/swarm/engaged_robots", qos);

    const auto period = std::chrono::duration<double>(1.0 / std::max(1.0, rate));
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period),
      std::bind(&EngagementCoordinator::onTick, this));

    RCLCPP_INFO(get_logger(),
      "EngagementCoordinator UP (2+2): leader=%d followers=%zu standoff=%.1fm max_targets=%d",
      leader_robot_id_, follower_ids_.size(), combat_standoff_m_, max_targets_);
  }

private:
  struct Target
  {
    double x;
    double y;
    rclcpp::Time last_seen;
  };

  void onThreat(const ThreatAlert::SharedPtr msg)
  {
    if (static_cast<int>(msg->threat_type) != person_threat_type_ || !msg->has_position)
    {
      return;
    }
    if (msg->source_robot_id != std::to_string(leader_robot_id_))
    {
      return;
    }
    if (msg->range_m < min_range_m_ || msg->range_m > max_range_m_)
    {
      return;
    }

    geometry_msgs::msg::TransformStamped tf;
    try
    {
      tf = tf_buffer_->lookupTransform(map_frame_, base_frame_, tf2::TimePointZero);
    }
    catch (const std::exception &)
    {
      return;
    }
    const double lx = tf.transform.translation.x;
    const double ly = tf.transform.translation.y;
    const double b = msg->bearing_deg * M_PI / 180.0;
    const double tx = lx + msg->range_m * std::cos(b);
    const double ty = ly + msg->range_m * std::sin(b);

    // Cluster: match to nearest existing target within merge radius; else
    // create a new one (stable threat_id = vector index), up to max_targets_.
    int best = -1;
    double bestd = 1e9;
    for (size_t i = 0; i < targets_.size(); ++i)
    {
      const double d = std::hypot(targets_[i].x - tx, targets_[i].y - ty);
      if (d < bestd)
      {
        bestd = d;
        best = static_cast<int>(i);
      }
    }
    if (best >= 0 && bestd <= merge_radius_m_)
    {
      targets_[best].x = targets_[best].x * (1.0 - smoothing_) + tx * smoothing_;
      targets_[best].y = targets_[best].y * (1.0 - smoothing_) + ty * smoothing_;
      targets_[best].last_seen = now();
    }
    else if (static_cast<int>(targets_.size()) < max_targets_)
    {
      targets_.push_back({tx, ty, now()});
      RCLCPP_WARN(get_logger(), "New target #%zu at (%.1f,%.1f)", targets_.size() - 1, tx, ty);
    }

    if (!engaged_ && !targets_.empty())
    {
      engageLeader(lx, ly, targets_[0].x, targets_[0].y);
    }
  }

  void engageLeader(double lx, double ly, double tx, double ty)
  {
    if (!nav_client_->wait_for_action_server(std::chrono::seconds(2)))
    {
      RCLCPP_ERROR(get_logger(), "Nav2 unavailable — cannot send leader standoff goal");
      return;
    }
    const double ang = std::atan2(ly - ty, lx - tx);
    const double cx = tx + combat_standoff_m_ * std::cos(ang);
    const double cy = ty + combat_standoff_m_ * std::sin(ang);
    const double yaw = ang + M_PI;

    geometry_msgs::msg::PoseStamped ps;
    ps.header.frame_id = map_frame_;
    ps.header.stamp = now();
    ps.pose.position.x = cx;
    ps.pose.position.y = cy;
    ps.pose.orientation.z = std::sin(yaw * 0.5);
    ps.pose.orientation.w = std::cos(yaw * 0.5);

    NavigateThroughPoses::Goal goal;
    goal.poses.push_back(ps);
    nav_client_->async_send_goal(goal);
    engaged_ = true;
    RCLCPP_WARN(get_logger(), "ENGAGE: leader -> standoff (%.1f,%.1f)", cx, cy);
  }

  void onTick()
  {
    if (targets_.empty())
    {
      engaged_pub_->publish(std_msgs::msg::UInt32MultiArray());  // none engaged
      return;
    }

    // Followers whose pose is known via TF.
    std::vector<int> ids;
    std::vector<std::array<double, 2>> fpos;
    for (auto id64 : follower_ids_)
    {
      const int id = static_cast<int>(id64);
      try
      {
        auto tf = tf_buffer_->lookupTransform(
          map_frame_, "robot_" + std::to_string(id) + "/base_footprint", tf2::TimePointZero);
        ids.push_back(id);
        fpos.push_back({tf.transform.translation.x, tf.transform.translation.y});
      }
      catch (const std::exception &)
      {
        /* not spawned yet */
      }
    }
    if (ids.empty())
    {
      return;
    }

    // cost[i][j] = distance(follower_i, target_j) → 2+2 cap-balanced assignment.
    std::vector<std::vector<float>> cost(ids.size(), std::vector<float>(targets_.size()));
    for (size_t i = 0; i < ids.size(); ++i)
    {
      for (size_t j = 0; j < targets_.size(); ++j)
      {
        cost[i][j] = static_cast<float>(
          std::hypot(fpos[i][0] - targets_[j].x, fpos[i][1] - targets_[j].y));
      }
    }
    const auto assign = san_hub_orchestrator::assignWithCap(cost);

    for (size_t i = 0; i < ids.size(); ++i)
    {
      const int j = assign[i];
      geometry_msgs::msg::PointStamped m;
      m.header.frame_id = map_frame_;
      m.header.stamp = now();
      m.point.x = targets_[j].x;
      m.point.y = targets_[j].y;
      m.point.z = 0.0;
      target_pubs_[ids[i]]->publish(m);
    }
    std_msgs::msg::UInt32MultiArray engaged;
    for (int id : ids) { engaged.data.push_back(static_cast<uint32_t>(id)); }
    engaged_pub_->publish(engaged);

  }

  int leader_robot_id_{1};
  std::vector<int64_t> follower_ids_;
  double combat_standoff_m_{2.5};
  int person_threat_type_{99};
  double min_range_m_{0.5}, max_range_m_{30.0}, smoothing_{0.3}, merge_radius_m_{2.5};
  int max_targets_{2};
  std::string map_frame_{"map"}, base_frame_{"base_footprint"};
  bool engaged_{false};
  std::vector<Target> targets_;

  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
  rclcpp_action::Client<NavigateThroughPoses>::SharedPtr nav_client_;
  rclcpp::Subscription<ThreatAlert>::SharedPtr threat_sub_;
  std::map<int, rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr> target_pubs_;
  rclcpp::Publisher<std_msgs::msg::UInt32MultiArray>::SharedPtr engaged_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EngagementCoordinator>());
  rclcpp::shutdown();
  return 0;
}