#include <chrono>
#include <cmath>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <nav2_msgs/action/navigate_through_poses.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

using NavigateThroughPoses = nav2_msgs::action::NavigateThroughPoses;

class EngagementCoordinator : public rclcpp::Node
{
public:
  EngagementCoordinator()
  : rclcpp::Node("engagement_coordinator")
  {
    leader_robot_id_ = declare_parameter<int>("leader_robot_id", 1);
    combat_standoff_m_ = declare_parameter<double>("combat_standoff_m", 2.5);
    person_threat_type_ = declare_parameter<int>("person_threat_type", 99);
    min_range_m_ = declare_parameter<double>("min_range_m", 0.5);
    max_range_m_ = declare_parameter<double>("max_range_m", 30.0);
    smoothing_ = declare_parameter<double>("smoothing", 0.3);
    map_frame_ = declare_parameter<std::string>("map_frame", "map");
    base_frame_ = declare_parameter<std::string>("leader_base_frame", "base_footprint");
    const double rate = declare_parameter<double>("publish_rate_hz", 5.0);
    const std::string nav_action = declare_parameter<std::string>("nav_action", "navigate_through_poses");

    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
    nav_client_ = rclcpp_action::create_client<NavigateThroughPoses>(this, nav_action);

    auto qos = rclcpp::QoS(10).reliable();
    threat_sub_ = create_subscription<combat_robot_msgs::msg::ThreatAlert>(
      "/swarm/threat_alert_raw", qos,
      std::bind(&EngagementCoordinator::onThreat, this, std::placeholders::_1));
    target_pub_ = create_publisher<geometry_msgs::msg::PointStamped>(
      "/swarm/encircle_target", qos);

    const auto period = std::chrono::duration<double>(1.0 / std::max(1.0, rate));
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period),
      std::bind(&EngagementCoordinator::onTick, this));

    RCLCPP_INFO(get_logger(),
      "EngagementCoordinator UP: leader=%d standoff=%.1fm person_type=%d",
      leader_robot_id_, combat_standoff_m_, person_threat_type_);
  }

private:
  void onThreat(const combat_robot_msgs::msg::ThreatAlert::SharedPtr msg)
  {
    if (static_cast<int>(msg->threat_type) != person_threat_type_ || !msg->has_position) {return;}
    if (msg->source_robot_id != std::to_string(leader_robot_id_)) {return;}
    if (msg->range_m < min_range_m_ || msg->range_m > max_range_m_) {return;}

    geometry_msgs::msg::TransformStamped tf;
    try {
      tf = tf_buffer_->lookupTransform(map_frame_, base_frame_, tf2::TimePointZero);
    } catch (const std::exception &) {
      return;
    }
    const double lx = tf.transform.translation.x;
    const double ly = tf.transform.translation.y;
    const double b = msg->bearing_deg * M_PI / 180.0;        // world-frame
    const double tx = lx + msg->range_m * std::cos(b);
    const double ty = ly + msg->range_m * std::sin(b);

    if (!have_target_) {
      target_x_ = tx; target_y_ = ty; have_target_ = true;
    } else {
      target_x_ = target_x_ * (1.0 - smoothing_) + tx * smoothing_;
      target_y_ = target_y_ * (1.0 - smoothing_) + ty * smoothing_;
    }
    if (!engaged_) {engageLeader(lx, ly, target_x_, target_y_);}
  }

  void engageLeader(double lx, double ly, double tx, double ty)
  {
    if (!nav_client_->wait_for_action_server(std::chrono::seconds(2))) {
      RCLCPP_ERROR(get_logger(), "Nav2 NavigateThroughPoses unavailable — cannot send standoff goal");
      return;
    }
    const double ang = std::atan2(ly - ty, lx - tx);          // target -> leader
    const double cx = tx + combat_standoff_m_ * std::cos(ang);
    const double cy = ty + combat_standoff_m_ * std::sin(ang);
    const double yaw = ang + M_PI;                            // face the target

    geometry_msgs::msg::PoseStamped ps;
    ps.header.frame_id = map_frame_;
    ps.header.stamp = now();
    ps.pose.position.x = cx;
    ps.pose.position.y = cy;
    ps.pose.orientation.z = std::sin(yaw * 0.5);
    ps.pose.orientation.w = std::cos(yaw * 0.5);

    NavigateThroughPoses::Goal goal;
    goal.poses.push_back(ps);
    nav_client_->async_send_goal(goal);                       // fire-and-forget
    engaged_ = true;
    RCLCPP_WARN(get_logger(),
      "ENGAGE: leader -> standoff (%.1f,%.1f) facing person; followers encircling", cx, cy);
  }

  void onTick()
  {
    if (!have_target_) {return;}                              // latched once engaged
    geometry_msgs::msg::PointStamped m;
    m.header.frame_id = map_frame_;
    m.header.stamp = now();
    m.point.x = target_x_; m.point.y = target_y_; m.point.z = 0.0;
    target_pub_->publish(m);
  }

  int leader_robot_id_{1};
  double combat_standoff_m_{2.5};
  int person_threat_type_{99};
  double min_range_m_{0.5}, max_range_m_{30.0}, smoothing_{0.3};
  std::string map_frame_{"map"}, base_frame_{"base_footprint"};
  bool have_target_{false}, engaged_{false};
  double target_x_{0.0}, target_y_{0.0};

  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
  rclcpp_action::Client<NavigateThroughPoses>::SharedPtr nav_client_;
  rclcpp::Subscription<combat_robot_msgs::msg::ThreatAlert>::SharedPtr threat_sub_;
  rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr target_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EngagementCoordinator>());
  rclcpp::shutdown();
  return 0;
}