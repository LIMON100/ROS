// #include <chrono>
// #include <functional>
// #include <memory>
// #include <string>
// #include <cmath>
// #include <limits>
// #include <vector>

// #include "rclcpp/rclcpp.hpp"
// #include "nav_msgs/msg/odometry.hpp"
// #include "nav_msgs/msg/path.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include "geometry_msgs/msg/pose.hpp"
// #include "visualization_msgs/msg/marker_array.hpp" 

// using namespace std::chrono_literals;

// class LeaderNode : public rclcpp::Node
// {
// public:
//   LeaderNode()
//   : Node("leader_node")
//   {
//     // --- Parameters ---
//     this->declare_parameter<std::string>("leader_state_topic", "leader_state");
//     this->declare_parameter<std::string>("odom_topic", "odom");
//     this->declare_parameter<std::string>("plan_topic", "plan");
    
//     this->declare_parameter<double>("lookahead_dist_1", 2.0); 
//     this->declare_parameter<double>("lookahead_dist_2", 4.0); 

//     std::string state_topic = this->get_parameter("leader_state_topic").as_string();
//     std::string odom_topic = this->get_parameter("odom_topic").as_string();
//     std::string plan_topic = this->get_parameter("plan_topic").as_string();

//     lookahead_dist_1_ = this->get_parameter("lookahead_dist_1").as_double();
//     lookahead_dist_2_ = this->get_parameter("lookahead_dist_2").as_double();

//     // --- Publishers & Subscribers ---
//     auto qos_best_effort = rclcpp::SensorDataQoS();
//     auto qos_reliable = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();

//     publisher_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>(state_topic, 10);
    
//     // NEW: Debug Publisher for RViz
//     viz_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("leader_waypoints_viz", 10);

//     sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(
//       odom_topic, qos_best_effort, std::bind(&LeaderNode::odom_callback, this, std::placeholders::_1));

//     sub_plan_ = this->create_subscription<nav_msgs::msg::Path>(
//       plan_topic, qos_reliable, std::bind(&LeaderNode::plan_callback, this, std::placeholders::_1));

//     timer_ = this->create_wall_timer(
//       50ms, std::bind(&LeaderNode::timer_callback, this));

//     RCLCPP_INFO(this->get_logger(), "Leader Node Active with Visualization.");
//   }

// private:
//   void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg) {
//     latest_odom_ = *msg;
//     has_odom_ = true;
//   }

//   void plan_callback(const nav_msgs::msg::Path::SharedPtr msg) {
//     latest_path_ = *msg;
//     has_path_ = true;
//   }

//   double get_distance(const geometry_msgs::msg::Point& p1, const geometry_msgs::msg::Point& p2) {
//     return std::hypot(p1.x - p2.x, p1.y - p2.y);
//   }

//   bool get_lookahead_point(double target_dist, size_t start_idx, geometry_msgs::msg::Pose& out_pose) {
//     if (!has_path_ || latest_path_.poses.empty()) return false;
//     for (size_t i = start_idx; i < latest_path_.poses.size(); ++i) {
//         double d = get_distance(latest_odom_.pose.pose.position, latest_path_.poses[i].pose.position);
//         if (d >= target_dist) {
//             out_pose = latest_path_.poses[i].pose;
//             return true;
//         }
//     }
//     out_pose = latest_path_.poses.back().pose; // End of path
//     return true;
//   }

//   void timer_callback()
//   {
//     if (!has_odom_) return;

//     auto message = skyhunter_msgs::msg::LeaderState();
//     message.header.stamp = latest_odom_.header.stamp;
//     message.header.frame_id = "map"; 
//     message.pose = latest_odom_.pose.pose;
//     message.velocity = latest_odom_.twist.twist;

//     // Visualization Array
//     visualization_msgs::msg::MarkerArray markers;

//     if (has_path_ && !latest_path_.poses.empty()) {
//         size_t closest_idx = 0;
//         double min_dist = std::numeric_limits<double>::max();

//         // Find closest point on path
//         for (size_t i = 0; i < latest_path_.poses.size(); ++i) {
//             double d = get_distance(latest_odom_.pose.pose.position, latest_path_.poses[i].pose.position);
//             if (d < min_dist) { min_dist = d; closest_idx = i; }
//         }

//         geometry_msgs::msg::Pose wp1, wp2;
        
//         // --- WAYPOINT 1 (Blue) ---
//         if (get_lookahead_point(lookahead_dist_1_, closest_idx, wp1)) {
//             message.next_waypoints.push_back(wp1);
//             markers.markers.push_back(create_marker(0, wp1, 1.0, 0.0, 0.0)); // Blue
//         }

//         // --- WAYPOINT 2 (Red) ---
//         if (get_lookahead_point(lookahead_dist_2_, closest_idx, wp2)) {
//             message.next_waypoints.push_back(wp2);
//             markers.markers.push_back(create_marker(1, wp2, 0.0, 1.0, 0.0)); // Red (Green actually, RGB: 0,1,0)
//         }
//     }

//     // Publish State and Markers
//     publisher_->publish(message);
//     viz_pub_->publish(markers);
//   }

//   visualization_msgs::msg::Marker create_marker(int id, geometry_msgs::msg::Pose pose, float r, float g, float b) {
//       visualization_msgs::msg::Marker marker;
//       marker.header.frame_id = "map";
//       marker.header.stamp = this->get_clock()->now();
//       marker.ns = "leader_lookahead";
//       marker.id = id;
//       marker.type = visualization_msgs::msg::Marker::SPHERE;
//       marker.action = visualization_msgs::msg::Marker::ADD;
//       marker.pose = pose;
//       marker.scale.x = 0.5; marker.scale.y = 0.5; marker.scale.z = 0.5; // 0.5m size
//       marker.color.a = 1.0; marker.color.r = r; marker.color.g = g; marker.color.b = b;
//       return marker;
//   }

//   rclcpp::TimerBase::SharedPtr timer_;
//   rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr publisher_;
//   rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr viz_pub_; // Viz Publisher
  
//   rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
//   rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr sub_plan_;

//   nav_msgs::msg::Odometry latest_odom_;
//   nav_msgs::msg::Path latest_path_;
  
//   bool has_odom_ = false;
//   bool has_path_ = false;
  
//   double lookahead_dist_1_;
//   double lookahead_dist_2_;
// };

// int main(int argc, char * argv[])
// {
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

#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "nav_msgs/msg/path.hpp"
#include "skyhunter_msgs/msg/leader_state.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "visualization_msgs/msg/marker_array.hpp"

using namespace std::chrono_literals;

class LeaderNode : public rclcpp::Node
{
public:
  LeaderNode() : Node("leader_node")
  {
    // --- Parameters ---
    this->declare_parameter<double>("waypoint_spacing", 10.0); // Strict 10m requirement
    this->declare_parameter<std::string>("map_frame", "map");

    spacing_ = this->get_parameter("waypoint_spacing").as_double();
    map_frame_ = this->get_parameter("map_frame").as_string();

    auto qos = rclcpp::SensorDataQoS();

    // Publishers
    publisher_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
    viz_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("leader_waypoints_viz", 10);

    // Subscribers
    sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "odom", qos, std::bind(&LeaderNode::odom_callback, this, std::placeholders::_1));

    sub_plan_ = this->create_subscription<nav_msgs::msg::Path>(
      "plan", qos, std::bind(&LeaderNode::plan_callback, this, std::placeholders::_1));

    timer_ = this->create_wall_timer(100ms, std::bind(&LeaderNode::timer_callback, this));

    RCLCPP_INFO(this->get_logger(), "Tactical Leader Online. Slicing path into %fm segments.", spacing_);
  }

private:
  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg) {
    latest_odom_ = *msg;
    has_odom_ = true;
  }

  void plan_callback(const nav_msgs::msg::Path::SharedPtr msg) {
    latest_path_ = *msg;
    has_path_ = true;
  }

  // --- TACTICAL ALGORITHM: Distance-Along-Path ---
  // This finds a point exactly X meters along the path, following every curve.
  bool get_waypoint_at_distance(double target_metres, size_t start_idx, geometry_msgs::msg::Pose& out_pose, size_t& out_idx) {
    if (!has_path_ || latest_path_.poses.size() < 2) return false;

    double accumulated_dist = 0.0;
    for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i) {
        double d = std::hypot(
            latest_path_.poses[i+1].pose.position.x - latest_path_.poses[i].pose.position.x,
            latest_path_.poses[i+1].pose.position.y - latest_path_.poses[i].pose.position.y
        );
        accumulated_dist += d;

        if (accumulated_dist >= target_metres) {
            out_pose = latest_path_.poses[i+1].pose;
            out_idx = i + 1;
            return true;
        }
    }
    // If path is shorter than requested distance, return the final goal
    out_pose = latest_path_.poses.back().pose;
    out_idx = latest_path_.poses.size() - 1;
    return true;
  }

  void timer_callback()
  {
    if (!has_odom_) return;

    auto state_msg = skyhunter_msgs::msg::LeaderState();
    state_msg.header.stamp = this->get_clock()->now();
    state_msg.header.frame_id = map_frame_;
    state_msg.pose = latest_odom_.pose.pose;
    state_msg.velocity = latest_odom_.twist.twist;

    visualization_msgs::msg::MarkerArray markers;

    if (has_path_ && !latest_path_.poses.empty()) {
        // 1. Find the index on the path closest to the robot's current position
        size_t closest_idx = 0;
        double min_dist = std::numeric_limits<double>::max();
        for (size_t i = 0; i < latest_path_.poses.size(); ++i) {
            double d = std::hypot(
                latest_odom_.pose.pose.position.x - latest_path_.poses[i].pose.position.x,
                latest_odom_.pose.pose.position.y - latest_path_.poses[i].pose.position.y
            );
            if (d < min_dist) { min_dist = d; closest_idx = i; }
        }

        // 2. Slice path: Waypoint 1 (at 10m)
        geometry_msgs::msg::Pose wp1, wp2;
        size_t wp1_idx;
        if (get_waypoint_at_distance(spacing_, closest_idx, wp1, wp1_idx)) {
            state_msg.next_waypoints.push_back(wp1);
            markers.markers.push_back(create_marker(0, wp1, 0.0, 0.0, 1.0)); // Blue
            
            // 3. Slice path: Waypoint 2 (at 20m - start from wp1)
            size_t wp2_idx;
            if (get_waypoint_at_distance(spacing_, wp1_idx, wp2, wp2_idx)) {
                state_msg.next_waypoints.push_back(wp2);
                markers.markers.push_back(create_marker(1, wp2, 1.0, 0.0, 0.0)); // Red
            }
        }
    }

    publisher_->publish(state_msg);
    viz_pub_->publish(markers);
  }

  visualization_msgs::msg::Marker create_marker(int id, geometry_msgs::msg::Pose pose, float r, float g, float b) {
      visualization_msgs::msg::Marker m;
      m.header.frame_id = map_frame_;
      m.header.stamp = this->get_clock()->now();
      m.ns = "tactical_waypoints";
      m.id = id;
      m.type = visualization_msgs::msg::Marker::CYLINDER;
      m.action = visualization_msgs::msg::Marker::ADD;
      m.pose = pose;
      m.scale.x = 0.6; m.scale.y = 0.6; m.scale.z = 0.1;
      m.color.a = 0.8; m.color.r = r; m.color.g = g; m.color.b = b;
      return m;
  }

  // Members
  double spacing_;
  std::string map_frame_;
  bool has_odom_ = false, has_path_ = false;
  nav_msgs::msg::Odometry latest_odom_;
  nav_msgs::msg::Path latest_path_;
  
  rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr publisher_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr viz_pub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr sub_plan_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LeaderNode>());
  rclcpp::shutdown();
  return 0;
}