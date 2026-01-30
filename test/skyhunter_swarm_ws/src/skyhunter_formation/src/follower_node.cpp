// // WORKABLE
// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <string>
// #include <algorithm>

// #include "rclcpp/rclcpp.hpp"
// #include "geometry_msgs/msg/twist.hpp"
// #include "nav_msgs/msg/odometry.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include "tf2/utils.h"
// #include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

// // Grid Map Headers for Safety
// #include <grid_map_ros/grid_map_ros.hpp>
// #include <grid_map_msgs/msg/grid_map.hpp>

// using namespace std::chrono_literals;

// class FollowerNode : public rclcpp::Node
// {
// public:
//   FollowerNode()
//   : Node("follower_node")
//   {
//     // --- Parameters ---
//     this->declare_parameter<double>("offset_x", -2.0); 
//     this->declare_parameter<double>("offset_y", 2.0);  
//     this->declare_parameter<double>("k_x", 1.5); 
//     this->declare_parameter<double>("k_y", 2.0); 
//     this->declare_parameter<double>("k_theta", 4.0);
//     this->declare_parameter<double>("min_safe_dist", 1.0); 
//     this->declare_parameter<std::string>("leader_topic", "/robot1/leader_state");
//     this->declare_parameter<std::string>("map_topic", "/elevation_map"); 

//     offset_x_ = this->get_parameter("offset_x").as_double();
//     offset_y_ = this->get_parameter("offset_y").as_double();
//     k_x_ = this->get_parameter("k_x").as_double();
//     k_y_ = this->get_parameter("k_y").as_double();
//     k_theta_ = this->get_parameter("k_theta").as_double();
//     min_safe_dist_ = this->get_parameter("min_safe_dist").as_double();
    
//     // --- Communication ---
//     auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    
//     leader_sub_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//       this->get_parameter("leader_topic").as_string(), qos, 
//       std::bind(&FollowerNode::leader_callback, this, std::placeholders::_1));

//     odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
//       "odom", qos, 
//       std::bind(&FollowerNode::odom_callback, this, std::placeholders::_1));

//     // NEW: Subscribe to local elevation map for safety
//     map_sub_ = this->create_subscription<grid_map_msgs::msg::GridMap>(
//       this->get_parameter("map_topic").as_string(), qos,
//       std::bind(&FollowerNode::map_callback, this, std::placeholders::_1));

//     cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

//     timer_ = this->create_wall_timer(
//       50ms, std::bind(&FollowerNode::control_loop, this)); // 20Hz

//     RCLCPP_INFO(this->get_logger(), "Safety-Aware Follower Started.");
//   }

// private:
//   void leader_callback(const skyhunter_msgs::msg::LeaderState::SharedPtr msg)
//   {
//     last_leader_msg_ = *msg;
//     last_leader_time_ = this->get_clock()->now();
//     has_leader_ = true;
//   }

//   void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
//   {
//     current_pose_ = msg->pose.pose;
//     has_odom_ = true;
//   }

//   void map_callback(const grid_map_msgs::msg::GridMap::SharedPtr msg) {
//     if (!has_map_) {
//         RCLCPP_INFO(this->get_logger(), "Elevation Map Received! Safety System ACTIVE.");
//     }
//     grid_map::GridMapRosConverter::fromMessage(*msg, local_map_);
//     has_map_ = true;
//   }

//   // --- THE NEW SAFETY CHECK FUNCTION ---
//   bool is_path_safe()
//   {
//     if (!has_map_ || !has_odom_) return true; // Assume safe if no data yet

//     // Get my current position and yaw
//     double x = current_pose_.position.x;
//     double y = current_pose_.position.y;
//     double yaw = tf2::getYaw(current_pose_.orientation);

//     // Check a rectangular "Safety Curtain" in front of the robot
//     // 0.2m to 1.5m ahead, 0.8m wide
//     double check_dist = 1.5; 
//     double step = 0.1;
    
//     for (double d = 0.2; d <= check_dist; d += step) {
//         // Project point ahead of robot
//         double px = x + d * cos(yaw);
//         double py = y + d * sin(yaw);
//         grid_map::Position pos(px, py);

//         if (local_map_.isInside(pos)) {
//             // Check hazards
//             float drop_risk = 0.0;
//             float trav = 0.0;

//             try {
//                 if (local_map_.exists("drop_risk"))
//                     drop_risk = local_map_.atPosition("drop_risk", pos);
//                 if (local_map_.exists("traversability"))
//                     trav = local_map_.atPosition("traversability", pos);
//             } catch (...) { continue; }

//             // --- CRITICAL ALARM ---
//             if (drop_risk > 0.5) {
//                 RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
//                     "!!! DITCH DETECTED AHEAD (%.1fm)! STOPPING !!!", d);
//                 return false; // UNSAFE
//             }
//             if (trav > 0.8) {
//                 RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
//                     "!!! OBSTACLE DETECTED AHEAD (%.1fm)! STOPPING !!!", d);
//                 return false; // UNSAFE
//             }
//         }
//     }
//     return true; // SAFE
//   }

//   void control_loop()
//   {
//     if (!has_leader_ || !has_odom_) return;

//     // Safety Timeout
//     if ((this->get_clock()->now() - last_leader_time_).seconds() > 0.5) {
//         stop_robot(); 
//         return;
//     }

//     // --- STEP 1: CHECK SAFETY FIRST ---
//     if (!is_path_safe()) {
//         stop_robot();
//         return; // Do not proceed to formation logic
//     }

//     // --- STEP 2: Normal Formation Logic ---
//     double leader_x = last_leader_msg_.pose.position.x;
//     double leader_y = last_leader_msg_.pose.position.y;
//     double leader_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//     double leader_v = last_leader_msg_.velocity.linear.x;
//     double leader_w = last_leader_msg_.velocity.angular.z;

//     double my_x = current_pose_.position.x;
//     double my_y = current_pose_.position.y;
//     double my_yaw = tf2::getYaw(current_pose_.orientation);

//     double target_x = leader_x + (offset_x_ * cos(leader_yaw) - offset_y_ * sin(leader_yaw));
//     double target_y = leader_y + (offset_x_ * sin(leader_yaw) + offset_y_ * cos(leader_yaw));

//     double ex_global = target_x - my_x;
//     double ey_global = target_y - my_y;
//     double etheta_global = leader_yaw - my_yaw;

//     while (etheta_global > M_PI) etheta_global -= 2.0 * M_PI;
//     while (etheta_global < -M_PI) etheta_global += 2.0 * M_PI;

//     double ex_local = ex_global * cos(my_yaw) + ey_global * sin(my_yaw);
//     double ey_local = -ex_global * sin(my_yaw) + ey_global * cos(my_yaw);

//     geometry_msgs::msg::Twist cmd;
//     cmd.linear.x = (leader_v * cos(etheta_global)) + (k_x_ * ex_local);
//     cmd.angular.z = leader_w + (k_y_ * ey_local) + (k_theta_ * etheta_global);

//     cmd.linear.x = std::clamp(cmd.linear.x, -0.5, 2.0);
//     cmd.angular.z = std::clamp(cmd.angular.z, -2.0, 2.0);

//     cmd_vel_pub_->publish(cmd);
//   }

//   void stop_robot()
//   {
//     geometry_msgs::msg::Twist cmd;
//     cmd.linear.x = 0.0;
//     cmd.angular.z = 0.0;
//     cmd_vel_pub_->publish(cmd);
//   }

//   // Vars
//   skyhunter_msgs::msg::LeaderState last_leader_msg_;
//   rclcpp::Time last_leader_time_;
//   bool has_leader_ = false;

//   geometry_msgs::msg::Pose current_pose_;
//   bool has_odom_ = false;

//   grid_map::GridMap local_map_;
//   bool has_map_ = false;

//   double offset_x_, offset_y_;
//   double k_x_, k_y_, k_theta_;
//   double min_safe_dist_;

//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr leader_sub_;
//   rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
//   rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr map_sub_;
//   rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
//   rclcpp::TimerBase::SharedPtr timer_;
// };

// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<FollowerNode>());
//   rclcpp::shutdown();
//   return 0;
// }

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <algorithm>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "skyhunter_msgs/msg/leader_state.hpp"
#include "tf2/utils.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

// Grid Map Headers for Safety
#include <grid_map_ros/grid_map_ros.hpp>
#include <grid_map_msgs/msg/grid_map.hpp>

using namespace std::chrono_literals;

class FollowerNode : public rclcpp::Node
{
public:
  FollowerNode()
  : Node("follower_node")
  {
    // --- Parameters ---
    this->declare_parameter<double>("offset_x", -2.0); 
    this->declare_parameter<double>("offset_y", 2.0);  
    this->declare_parameter<double>("k_x", 1.5); 
    this->declare_parameter<double>("k_y", 2.0); 
    this->declare_parameter<double>("k_theta", 4.0);
    this->declare_parameter<double>("min_safe_dist", 1.0); 
    this->declare_parameter<std::string>("leader_topic", "/robot1/leader_state");
    this->declare_parameter<double>("kp_angular", 4.0);
    this->declare_parameter<double>("kp_linear", 1.5);
    
    // Safety Parameters
    this->declare_parameter<std::string>("map_topic", "elevation_map"); // Local map topic
    this->declare_parameter<double>("safety_lookahead", 1.5); // Check 1.5m ahead
    this->declare_parameter<double>("safety_width", 0.6);     // Width of safety curtain

    offset_x_ = this->get_parameter("offset_x").as_double();
    offset_y_ = this->get_parameter("offset_y").as_double();
    k_x_ = this->get_parameter("k_x").as_double();
    k_y_ = this->get_parameter("k_y").as_double();
    k_theta_ = this->get_parameter("k_theta").as_double();
    min_safe_dist_ = this->get_parameter("min_safe_dist").as_double();
    safety_lookahead_ = this->get_parameter("safety_lookahead").as_double();
    safety_width_ = this->get_parameter("safety_width").as_double();
    
    // --- Communication ---
    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    
    leader_sub_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
      this->get_parameter("leader_topic").as_string(), qos, 
      std::bind(&FollowerNode::leader_callback, this, std::placeholders::_1));

    odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "odom", qos, 
      std::bind(&FollowerNode::odom_callback, this, std::placeholders::_1));

    // NEW: Subscribe to local elevation map for safety
    // Note: The topic is relative, so it will be /robotX/elevation_map
    map_sub_ = this->create_subscription<grid_map_msgs::msg::GridMap>(
      this->get_parameter("map_topic").as_string(), rclcpp::QoS(1).best_effort(), // Map is often Best Effort
      std::bind(&FollowerNode::map_callback, this, std::placeholders::_1));

    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

    timer_ = this->create_wall_timer(
      50ms, std::bind(&FollowerNode::control_loop, this)); // 20Hz

    RCLCPP_INFO(this->get_logger(), "Safety-Aware Follower Started. Offset: [%.1f, %.1f]", offset_x_, offset_y_);
  }

private:
  void leader_callback(const skyhunter_msgs::msg::LeaderState::SharedPtr msg)
  {
    last_leader_msg_ = *msg;
    last_leader_time_ = this->get_clock()->now();
    has_leader_ = true;
  }

  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
  {
    current_pose_ = msg->pose.pose;
    has_odom_ = true;
  }

  void map_callback(const grid_map_msgs::msg::GridMap::SharedPtr msg)
  {
    // Convert ROS msg to GridMap object
    grid_map::GridMapRosConverter::fromMessage(*msg, local_map_);
    has_map_ = true;
  }

  // --- THE SAFETY CHECK ---
  bool is_path_unsafe()
  {
    // if (!has_map_ || !has_odom_) return false;

    if (!has_map_ || !has_odom_) {
        // Debug print to check if we are even getting data
        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "Waiting for Map/Odom...");
        return false; 
    }

    // Get current robot state
    double x = current_pose_.position.x;
    double y = current_pose_.position.y;
    double yaw = tf2::getYaw(current_pose_.orientation);

    // Define search resolution
    double step_length = 0.1; // Check every 10cm
    int width_steps = 3;      // Check center, left, right

    for (double d = 0.3; d <= safety_lookahead_; d += step_length) {
        for (int w = -1; w <= 1; w++) {
            
            // Calculate check point in World Frame
            double lateral_offset = w * (safety_width_ / 2.0);
            double px_local = d;
            double py_local = lateral_offset;
            
            double px_global = x + (px_local * cos(yaw) - py_local * sin(yaw));
            double py_global = y + (px_local * sin(yaw) + py_local * cos(yaw));
            
            grid_map::Position pos(px_global, py_global);

            if (local_map_.isInside(pos)) {
                // Check Drop Risk (Ditch)
                if (local_map_.exists("drop_risk")) {
                    float risk = local_map_.atPosition("drop_risk", pos);
                    if (risk > 0.5) {
                        RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                            "!!! DITCH DETECTED AHEAD (%.1fm)! STOPPING !!!", d);
                        return true; // UNSAFE
                    }
                }

                // Check Traversability (Walls/Obstacles)
                if (local_map_.exists("traversability")) {
                    float cost = local_map_.atPosition("traversability", pos);
                    if (cost > 0.5) {
                        RCLCPP_INFO(this->get_logger(), "Scan(%.1fm): Cost=%.2f", d, cost);
                    }

                    if (cost > 0.8) { // Lethal threshold
                        RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                            "!!! OBSTACLE AHEAD (%.1fm)! STOPPING !!!", d);
                        return true; // UNSAFE
                    }
                }
            }
        }
    }
    return false; // SAFE
  }

  void control_loop()
  {
    if (!has_leader_ || !has_odom_) return;

    // Safety Timeout
    if ((this->get_clock()->now() - last_leader_time_).seconds() > 0.5) {
        stop_robot(); 
        return;
    }

    // --- SAFETY CHECK ---
    if (is_path_unsafe()) {
        stop_robot();
        return; // Override formation control
    }

    // --- NORMAL FORMATION LOGIC ---
    double leader_x = last_leader_msg_.pose.position.x;
    double leader_y = last_leader_msg_.pose.position.y;
    double leader_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
    double leader_v = last_leader_msg_.velocity.linear.x;
    double leader_w = last_leader_msg_.velocity.angular.z;

    double my_x = current_pose_.position.x;
    double my_y = current_pose_.position.y;
    double my_yaw = tf2::getYaw(current_pose_.orientation);

    double target_x = leader_x + (offset_x_ * cos(leader_yaw) - offset_y_ * sin(leader_yaw));
    double target_y = leader_y + (offset_x_ * sin(leader_yaw) + offset_y_ * cos(leader_yaw));

    double ex_global = target_x - my_x;
    double ey_global = target_y - my_y;
    double etheta_global = leader_yaw - my_yaw;

    while (etheta_global > M_PI) etheta_global -= 2.0 * M_PI;
    while (etheta_global < -M_PI) etheta_global += 2.0 * M_PI;

    double ex_local = ex_global * cos(my_yaw) + ey_global * sin(my_yaw);
    double ey_local = -ex_global * sin(my_yaw) + ey_global * cos(my_yaw);

    geometry_msgs::msg::Twist cmd;
    cmd.linear.x = (leader_v * cos(etheta_global)) + (k_x_ * ex_local);
    cmd.angular.z = leader_w + (k_y_ * ey_local) + (k_theta_ * etheta_global);

    cmd.linear.x = std::clamp(cmd.linear.x, -0.5, 2.0);
    cmd.angular.z = std::clamp(cmd.angular.z, -2.0, 2.0);

    cmd_vel_pub_->publish(cmd);
  }

  void stop_robot()
  {
    geometry_msgs::msg::Twist cmd;
    cmd.linear.x = 0.0;
    cmd.angular.z = 0.0;
    cmd_vel_pub_->publish(cmd);
  }

  // Vars
  skyhunter_msgs::msg::LeaderState last_leader_msg_;
  rclcpp::Time last_leader_time_;
  bool has_leader_ = false;

  geometry_msgs::msg::Pose current_pose_;
  bool has_odom_ = false;

  grid_map::GridMap local_map_;
  bool has_map_ = false;

  double offset_x_, offset_y_;
  double k_x_, k_y_, k_theta_;
  double min_safe_dist_;
  double safety_lookahead_, safety_width_;

  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr leader_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr map_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FollowerNode>());
  rclcpp::shutdown();
  return 0;
}