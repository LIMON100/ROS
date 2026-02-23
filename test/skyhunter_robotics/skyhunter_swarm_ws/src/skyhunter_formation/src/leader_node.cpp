// #include <chrono>
// #include <functional>
// #include <memory>
// #include <string>
// #include <cmath>
// #include <limits>
// #include <vector>
// #include <numeric>

// #include "rclcpp/rclcpp.hpp"
// #include "nav_msgs/msg/odometry.hpp"
// #include "nav_msgs/msg/path.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include "geometry_msgs/msg/pose.hpp"
// #include "visualization_msgs/msg/marker_array.hpp"

// #include "std_msgs/msg/int8.hpp"

// using namespace std::chrono_literals;

// // State Constants
// const int8_t STATE_NAVIGATING = 0;
// const int8_t STATE_GOAL_REACHED = 1;
// const int8_t STATE_TRANSITIONING = 2;

// class LeaderNode : public rclcpp::Node
// {
// public:
//   LeaderNode() : Node("leader_node")
//   {
//     this->declare_parameter<double>("waypoint_spacing", 10.0);
//     this->declare_parameter<std::string>("map_frame", "map");

//     spacing_config_ = this->get_parameter("waypoint_spacing").as_double();
//     map_frame_ = this->get_parameter("map_frame").as_string();

//     publisher_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
//     viz_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("leader_waypoints_viz", 10);

//     sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(
//       "odom", rclcpp::SensorDataQoS(), std::bind(&LeaderNode::odom_callback, this, std::placeholders::_1));

//     sub_plan_ = this->create_subscription<nav_msgs::msg::Path>(
//       "plan", rclcpp::QoS(10).reliable(), std::bind(&LeaderNode::plan_callback, this, std::placeholders::_1));

//     // --- ADD THIS BLOCK HERE ---
//     sub_form_cmd_ = this->create_subscription<std_msgs::msg::Int8>(
//       "/swarm/formation_command", 10, 
//       [this](const std_msgs::msg::Int8::SharedPtr msg) {
//         this->cmd_formation_type_ = msg->data;
//         RCLCPP_INFO(this->get_logger(), "COMMAND RECEIVED: Switching Swarm to Mode %d", msg->data);
//       });
//     // ----------------------------

//     timer_ = this->create_wall_timer(100ms, std::bind(&LeaderNode::timer_callback, this));

//     RCLCPP_INFO(this->get_logger(), "Tactical Leader [Phase 1 Instrumentation] Online.");
//   }

// private:
//   int8_t cmd_formation_type_ = 0;
//   rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_form_cmd_;
//   void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; has_odom_ = true; }
//   void plan_callback(const nav_msgs::msg::Path::SharedPtr msg) { latest_path_ = *msg; has_path_ = true; }

//   // Helper to calculate distance remaining from a specific index on the path to the end
//   double calculate_remaining_dist(size_t start_idx) {
//     if (!has_path_ || start_idx >= latest_path_.poses.size() - 1) return 0.0;
//     double dist = 0.0;
//     for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i) {
//       dist += std::hypot(latest_path_.poses[i+1].pose.position.x - latest_path_.poses[i].pose.position.x,
//                          latest_path_.poses[i+1].pose.position.y - latest_path_.poses[i].pose.position.y);
//     }
//     return dist;
//   }

//   bool get_waypoint_at_dist(double target_m, size_t start_idx, geometry_msgs::msg::Pose& out_pose, size_t& out_idx) {
//     if (!has_path_ || latest_path_.poses.size() < 2) return false;
//     double acc = 0.0;
//     for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i) {
//       acc += std::hypot(latest_path_.poses[i+1].pose.position.x - latest_path_.poses[i].pose.position.x,
//                         latest_path_.poses[i+1].pose.position.y - latest_path_.poses[i].pose.position.y);
//       if (acc >= target_m) { out_pose = latest_path_.poses[i+1].pose; out_idx = i + 1; return true; }
//     }
//     out_pose = latest_path_.poses.back().pose;
//     out_idx = latest_path_.poses.size() - 1;
//     return true;
//   }

//   void timer_callback()
//   {
//     if (!has_odom_) return;

//     // Detect State
//     double vel = std::abs(latest_odom_.twist.twist.linear.x);
//     if (vel > 0.1) current_state_ = STATE_NAVIGATING;
//     else if (has_path_ && calculate_remaining_dist(0) < 0.5) current_state_ = STATE_GOAL_REACHED;
//     else current_state_ = STATE_TRANSITIONING;

//     auto state_msg = skyhunter_msgs::msg::LeaderState();
//     state_msg.formation_type = cmd_formation_type_;
//     state_msg.header.stamp = this->get_clock()->now();
//     state_msg.pose = latest_odom_.pose.pose;
//     state_msg.velocity = latest_odom_.twist.twist;
//     state_msg.swarm_state = current_state_;

//     visualization_msgs::msg::MarkerArray markers;

//     if (has_path_ && !latest_path_.poses.empty()) {
//         // Find closest point
//         size_t closest_idx = 0; double min_d = 1e9;
//         for (size_t i = 0; i < latest_path_.poses.size(); ++i) {
//             double d = std::hypot(latest_odom_.pose.pose.position.x - latest_path_.poses[i].pose.position.x,
//                                   latest_odom_.pose.pose.position.y - latest_path_.poses[i].pose.position.y);
//             if (d < min_d) { min_d = d; closest_idx = i; }
//         }

//         double remaining = calculate_remaining_dist(closest_idx);
        
//         // CLIENT REQUIREMENT: Clamp spacing to min(10m, remaining - 2m)
//         double tactical_spacing = std::min(spacing_config_, std::max(0.0, remaining - 2.0));

//         geometry_msgs::msg::Pose wp1, wp2;
//         size_t wp1_idx, wp2_idx;
        
//         if (get_waypoint_at_dist(tactical_spacing, closest_idx, wp1, wp1_idx)) {
//             state_msg.next_waypoints.push_back(wp1);
//             markers.markers.push_back(create_marker(0, wp1, 0.0, 0.0, 1.0)); // Blue
            
//             if (get_waypoint_at_dist(tactical_spacing, wp1_idx, wp2, wp2_idx)) {
//                 state_msg.next_waypoints.push_back(wp2);
//                 markers.markers.push_back(create_marker(1, wp2, 1.0, 0.0, 0.0)); // Red
//             }
//         }

//         // CLIENT REQUIREMENT: Debug Logging
//         if (state_msg.next_waypoints.size() >= 2) {
//             RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
//                 "[DEBUG] State: %d | RemainingPath: %.2fm | WP1: (%.1f, %.1f) | WP2: (%.1f, %.1f)",
//                 current_state_, remaining, wp1.position.x, wp1.position.y, wp2.position.x, wp2.position.y);
//         }
//     }

//     publisher_->publish(state_msg);
//     viz_pub_->publish(markers);
//   }

//   visualization_msgs::msg::Marker create_marker(int id, geometry_msgs::msg::Pose pose, float r, float g, float b) {
//     visualization_msgs::msg::Marker m;
//     m.header.frame_id = map_frame_; m.header.stamp = this->get_clock()->now();
//     m.ns = "tactical_wp"; m.id = id; m.type = visualization_msgs::msg::Marker::CYLINDER;
//     m.action = visualization_msgs::msg::Marker::ADD; m.pose = pose;
//     m.scale.x = 0.5; m.scale.y = 0.5; m.scale.z = 0.1;
//     m.color.a = 0.8; m.color.r = r; m.color.g = g; m.color.b = b;
//     return m;
//   }

//   double spacing_config_;
//   std::string map_frame_;
//   int8_t current_state_ = STATE_TRANSITIONING;
//   bool has_odom_ = false, has_path_ = false;
//   nav_msgs::msg::Odometry latest_odom_;
//   nav_msgs::msg::Path latest_path_;
//   rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr publisher_;
//   rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr viz_pub_;
//   rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
//   rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr sub_plan_;
//   rclcpp::TimerBase::SharedPtr timer_;
// };

// int main(int argc, char * argv[]) {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<LeaderNode>());
//   rclcpp::shutdown();
//   return 0;
// }




#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <cmath>
#include <limits>
#include <vector>
#include <numeric>

#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "nav_msgs/msg/path.hpp"
#include "skyhunter_msgs/msg/leader_state.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "std_msgs/msg/int8.hpp"

// CRITICAL: Added missing PCL headers for the narrow gap logic
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

using namespace std::chrono_literals;

const int8_t STATE_NAVIGATING = 0;
const int8_t STATE_GOAL_REACHED = 1;
const int8_t STATE_TRANSITIONING = 2;

class LeaderNode : public rclcpp::Node
{
public:
  LeaderNode() : Node("leader_node")
  {
    this->declare_parameter<double>("waypoint_spacing", 10.0);
    this->declare_parameter<std::string>("map_frame", "map");

    spacing_config_ = this->get_parameter("waypoint_spacing").as_double();
    map_frame_ = this->get_parameter("map_frame").as_string();

    publisher_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
    viz_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("leader_waypoints_viz", 10);

    sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "odom", rclcpp::SensorDataQoS(), std::bind(&LeaderNode::odom_callback, this, std::placeholders::_1));

    sub_plan_ = this->create_subscription<nav_msgs::msg::Path>(
      "plan", rclcpp::QoS(10).reliable(), std::bind(&LeaderNode::plan_callback, this, std::placeholders::_1));

    // NEW: Leader LiDAR Subscriber (To detect narrow gaps)
    sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
      "scan/points", rclcpp::SensorDataQoS(), std::bind(&LeaderNode::scan_callback, this, std::placeholders::_1));

    sub_form_cmd_ = this->create_subscription<std_msgs::msg::Int8>(
      "/swarm/formation_command", 10, 
      [this](const std_msgs::msg::Int8::SharedPtr msg) {
        this->cmd_formation_type_ = msg->data;
        RCLCPP_INFO(this->get_logger(), "COMMAND RECEIVED: Switching Swarm to Mode %d", msg->data);
      });

    sub_mission_ = this->create_subscription<nav_msgs::msg::Path>(
      "/swarm/global_mission", 10, [this](const nav_msgs::msg::Path::SharedPtr msg) {
        this->global_mission_poses_ = msg->poses;
        RCLCPP_INFO(this->get_logger(), "Mission Synchronized: %zu goals loaded.", msg->poses.size());
      });

    // CLIENT REQUIREMENT: Broadcast at 20 Hz (50ms)
    timer_ = this->create_wall_timer(50ms, std::bind(&LeaderNode::timer_callback, this));

    RCLCPP_INFO(this->get_logger(), "Tactical Intelligent Leader Online.");
  }

private:
  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; has_odom_ = true; }
  void plan_callback(const nav_msgs::msg::Path::SharedPtr msg) { latest_path_ = *msg; has_path_ = true; }

  void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    pcl::PointCloud<pcl::PointXYZ> cloud;
    pcl::fromROSMsg(*msg, cloud);
    
    bool narrow = false;
    for (const auto& p : cloud.points) {
        // ignore points near the ground (p.z < -0.3)
        // ignore points way above the robot (p.z > 0.5)
        if (p.z < -0.3 || p.z > 0.5) continue; 

        // Now check the X and Y "Narrow Passage" area
        if (p.x > 0.5 && p.x < 4.0 && std::abs(p.y) < 1.8) {
            narrow = true;
            break;
        }
    }
    narrow_gap_detected_ = narrow;
  }

  double calculate_remaining_dist(size_t start_idx) {
    if (!has_path_ || start_idx >= latest_path_.poses.size() - 1) return 0.0;
    double dist = 0.0;
    for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i) {
      dist += std::hypot(latest_path_.poses[i+1].pose.position.x - latest_path_.poses[i].pose.position.x,
                         latest_path_.poses[i+1].pose.position.y - latest_path_.poses[i].pose.position.y);
    }
    return dist;
  }

  bool get_waypoint_at_dist(double target_m, size_t start_idx, geometry_msgs::msg::Pose& out_pose, size_t& out_idx) {
    if (!has_path_ || latest_path_.poses.size() < 2) return false;
    double acc = 0.0;
    for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i) {
      acc += std::hypot(latest_path_.poses[i+1].pose.position.x - latest_path_.poses[i].pose.position.x,
                        latest_path_.poses[i+1].pose.position.y - latest_path_.poses[i].pose.position.y);
      if (acc >= target_m) { out_pose = latest_path_.poses[i+1].pose; out_idx = i + 1; return true; }
    }
    out_pose = latest_path_.poses.back().pose;
    out_idx = latest_path_.poses.size() - 1;
    return true;
  }

  void timer_callback()
  {
    if (!has_odom_) return;

    auto state_msg = skyhunter_msgs::msg::LeaderState();
    state_msg.header.stamp = this->get_clock()->now();

    state_msg.next_waypoints.clear();
    for (size_t i = 0; i < global_mission_poses_.size(); ++i) {
        state_msg.next_waypoints.push_back(global_mission_poses_[i].pose);
    }

    // Detect State
    double vel = std::abs(latest_odom_.twist.twist.linear.x);
    if (vel > 0.1) current_state_ = STATE_NAVIGATING;
    else if (has_path_ && calculate_remaining_dist(0) < 0.5) current_state_ = STATE_GOAL_REACHED;
    else current_state_ = STATE_TRANSITIONING;

    state_msg.pose = latest_odom_.pose.pose;
    state_msg.velocity = latest_odom_.twist.twist;
    state_msg.swarm_state = current_state_;

    // --- FORMATION LOGIC OVERRIDE ---
    if (narrow_gap_detected_) {
        state_msg.formation_type = 1; // FORCE COLUMN
        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "AUTO-SWITCH: Narrow Gap! Forcing Column.");
    } else {
        state_msg.formation_type = cmd_formation_type_; 
    }

    visualization_msgs::msg::MarkerArray markers;
    if (has_path_ && !latest_path_.poses.empty()) {
        size_t closest_idx = 0; double min_d = 1e9;
        for (size_t i = 0; i < latest_path_.poses.size(); ++i) {
            double d = std::hypot(latest_odom_.pose.pose.position.x - latest_path_.poses[i].pose.position.x,
                                  latest_odom_.pose.pose.position.y - latest_path_.poses[i].pose.position.y);
            if (d < min_d) { min_d = d; closest_idx = i; }
        }
        double remaining = calculate_remaining_dist(closest_idx);
        double tactical_spacing = std::min(spacing_config_, std::max(0.0, remaining - 2.0));
        geometry_msgs::msg::Pose wp1, wp2;
        size_t wp1_idx, wp2_idx;
        if (get_waypoint_at_dist(tactical_spacing, closest_idx, wp1, wp1_idx)) {
            state_msg.next_waypoints.push_back(wp1);
            markers.markers.push_back(create_marker(0, wp1, 0.0, 0.0, 1.0));
            if (get_waypoint_at_dist(tactical_spacing, wp1_idx, wp2, wp2_idx)) {
                state_msg.next_waypoints.push_back(wp2);
                markers.markers.push_back(create_marker(1, wp2, 1.0, 0.0, 0.0));
            }
        }
    }
    publisher_->publish(state_msg);
    viz_pub_->publish(markers);
  }

  visualization_msgs::msg::Marker create_marker(int id, geometry_msgs::msg::Pose pose, float r, float g, float b) {
    visualization_msgs::msg::Marker m;
    m.header.frame_id = map_frame_; m.header.stamp = this->get_clock()->now();
    m.ns = "tactical_wp"; m.id = id; m.type = visualization_msgs::msg::Marker::CYLINDER;
    m.action = visualization_msgs::msg::Marker::ADD; m.pose = pose;
    m.scale.x = 0.5; m.scale.y = 0.5; m.scale.z = 0.1;
    m.color.a = 0.8; m.color.r = r; m.color.g = g; m.color.b = b;
    return m;
  }

  // --- MEMBER DECLARATIONS (CRITICAL FIX) ---
  std::vector<geometry_msgs::msg::PoseStamped> global_mission_poses_;


  double spacing_config_;
  std::string map_frame_;
  int8_t current_state_ = STATE_TRANSITIONING;
  int8_t cmd_formation_type_ = 0;
  bool has_odom_ = false, has_path_ = false, narrow_gap_detected_ = false;
  nav_msgs::msg::Odometry latest_odom_;
  nav_msgs::msg::Path latest_path_;

  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr sub_mission_;

  rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr publisher_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr viz_pub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr sub_plan_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_; // Missing before
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_form_cmd_;
  rclcpp::TimerBase::SharedPtr timer_;
  
};

int main(int argc, char * argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LeaderNode>());
  rclcpp::shutdown();
  return 0;
}