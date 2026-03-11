#ifndef SKYHUNTER_CONTROL__LEADER_NODE_HPP_
#define SKYHUNTER_CONTROL__LEADER_NODE_HPP_

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include <nav_msgs/msg/odometry.hpp>
#include <nav_msgs/msg/path.hpp>
#include <skyhunter_msgs/msg/leader_state.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <std_msgs/msg/int8.hpp>

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

using namespace std::chrono_literals;

class LeaderNode : public rclcpp::Node
{
public:
  explicit LeaderNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  ~LeaderNode() override = default;

private:
  // Constants (can be moved to class if needed)
  static constexpr int8_t STATE_NAVIGATING     = 0;
  static constexpr int8_t STATE_GOAL_REACHED   = 1;
  static constexpr int8_t STATE_TRANSITIONING  = 2;

  // Callbacks
  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg);
  void plan_callback(const nav_msgs::msg::Path::SharedPtr msg);
  void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg);
  void formation_command_callback(const std_msgs::msg::Int8::SharedPtr msg);
  void role_callback(const std_msgs::msg::Int8::SharedPtr msg);

  // Timer callback — main logic
  void timer_callback();

  // Helper functions
  double calculate_remaining_dist(size_t start_idx) const;
  bool get_waypoint_at_dist(
    double target_m,
    size_t start_idx,
    geometry_msgs::msg::Pose & out_pose,
    size_t & out_idx) const;

  visualization_msgs::msg::Marker create_marker(
    int id,
    const geometry_msgs::msg::Pose & pose,
    float r, float g, float b);

  // Parameters & state
  double spacing_config_{10.0};
  std::string map_frame_{"map"};

  int8_t current_state_{STATE_TRANSITIONING};
  int8_t cmd_formation_type_{0};
  int8_t current_role_{0};

  bool has_odom_{false};
  bool has_path_{false};
  bool narrow_gap_detected_{false};

  nav_msgs::msg::Odometry latest_odom_;
  nav_msgs::msg::Path latest_path_;

  // Publishers
  rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr publisher_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr viz_pub_;

  // Subscribers
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr sub_plan_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_form_cmd_;
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_role_;

  // Timer
  rclcpp::TimerBase::SharedPtr timer_;
};

#endif  // SKYHUNTER_CONTROL__LEADER_NODE_HPP_