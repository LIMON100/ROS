#ifndef ROBUST_FOLLOWER__ROBUST_FOLLOWER_HPP_
#define ROBUST_FOLLOWER__ROBUST_FOLLOWER_HPP_

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <skyhunter_msgs/msg/leader_state.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <std_msgs/msg/int8.hpp>
#include <std_msgs/msg/float64.hpp>

#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>

#include <omp.h>
#include <sensor_msgs/msg/nav_sat_fix.hpp>

class RobustFollower : public rclcpp::Node
{
public:
  explicit RobustFollower(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  ~RobustFollower() override = default;

private:
  // Callbacks
  void swarm_cb(const geometry_msgs::msg::PoseArray::SharedPtr msg);
  void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg);
  void scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg);
  void role_cb(const std_msgs::msg::Int8::SharedPtr msg);

  // Main control timer callback
  void control_loop();

  // Helper functions
  void stop_robot();
  bool is_point_blocked(double map_x, double map_y);

  // Parameters
  double offset_dist_{};
  double offset_lateral_{};
  double ttc_danger_dist_{};
  double blocking_radius_{};
  double separation_dist_{};

  static constexpr double PI = 3.141592653589793;

  // TF
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

  // State
  std::string my_ns_;
  std::string my_frame_;

  skyhunter_msgs::msg::LeaderState last_leader_msg_;
  rclcpp::Time last_leader_time_;

  std::vector<pcl::PointXYZ> obstacle_points_;
  geometry_msgs::msg::PoseArray swarm_poses_;

  bool has_leader_  = false;
  bool has_scan_    = false;
  bool has_swarm_   = false;

  int8_t current_local_role_ = 0;
  double smoothed_path_yaw_ = 0.0;
  bool has_smoothed_path_ = false;
  int floor_points_ahead_ = 100; 

  double last_cmd_vel_x_ = 0.0;

  // --- Convoy Breadcrumb Trail ---
  std::vector<geometry_msgs::msg::Point> leader_breadcrumbs_;
  bool is_lost_ = false;

  sensor_msgs::msg::NavSatFix latest_gps_;
  bool has_gps_ = false;

  // Publishers
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_pan_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_tilt_;

  // Subscribers
  rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr sub_swarm_;
  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_role_;
  rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr sub_gps_;

  // Timer
  rclcpp::TimerBase::SharedPtr timer_;
};

#endif  // ROBUST_FOLLOWER__ROBUST_FOLLOWER_HPP_