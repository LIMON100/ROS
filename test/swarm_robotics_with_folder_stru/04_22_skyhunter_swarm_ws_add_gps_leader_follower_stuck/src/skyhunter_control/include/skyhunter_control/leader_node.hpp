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

// --- NEW: Add TF2 ---
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2/utils.h>

#include <sensor_msgs/msg/imu.hpp>              
#include <geometry_msgs/msg/twist.hpp>         
#include <tf2_ros/transform_broadcaster.h>
#include <sensor_msgs/msg/nav_sat_fix.hpp>

using namespace std::chrono_literals;

class LeaderNode : public rclcpp::Node
{
public:
  explicit LeaderNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~LeaderNode() override = default;

private:
  static constexpr int8_t STATE_NAVIGATING     = 0;
  static constexpr int8_t STATE_GOAL_REACHED   = 1;
  static constexpr int8_t STATE_TRANSITIONING  = 2;

  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg);
  void plan_callback(const nav_msgs::msg::Path::SharedPtr msg);
  void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg);
  void formation_command_callback(const std_msgs::msg::Int8::SharedPtr msg);
  void role_callback(const std_msgs::msg::Int8::SharedPtr msg);
  void timer_callback();
  void handle_physical_recovery();

  double calculate_remaining_dist(size_t start_idx, double global_x, double global_y) const;
  bool get_waypoint_at_dist(double target_m, size_t start_idx, geometry_msgs::msg::Pose & out_pose, size_t & out_idx, double global_x, double global_y) const;
  visualization_msgs::msg::Marker create_marker(int id, const geometry_msgs::msg::Pose & pose, float r, float g, float b);

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

  skyhunter_msgs::msg::LeaderState latest_combat_state_;
  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_combat_state_;
  void combat_state_callback(const skyhunter_msgs::msg::LeaderState::SharedPtr msg);

  // TF Variables ---
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr sub_imu_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_nav_cmd_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_vel_;

  rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr sub_gps_;
  sensor_msgs::msg::NavSatFix latest_gps_;
  bool has_gps_ = false;

  double current_pitch_ = 0.0;
  geometry_msgs::msg::Twist last_nav_cmd_;
  int stall_counter_ = 0;
  bool is_recovering_ = false;


  rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr publisher_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr viz_pub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr sub_plan_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_form_cmd_;
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_role_;
  rclcpp::TimerBase::SharedPtr timer_;
};

#endif  // SKYHUNTER_CONTROL__LEADER_NODE_HPP_