// //WORKABLE
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
//             if (trav > 0.98) {
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















// // WORKABLE 02-02
// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <string>
// #include <algorithm>
// #include <vector>

// #include "rclcpp/rclcpp.hpp"
// #include "geometry_msgs/msg/twist.hpp"
// #include "nav_msgs/msg/odometry.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include "tf2/utils.h"
// #include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

// // Grid Map Headers
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
    
//     // Safety & Avoidance Parameters
//     this->declare_parameter<std::string>("map_topic", "elevation_map"); 
//     this->declare_parameter<double>("safety_lookahead", 2.0); // Check further ahead for smooth turns
//     this->declare_parameter<double>("safety_width", 0.8);     // Width of robot + margin

//     offset_x_ = this->get_parameter("offset_x").as_double();
//     offset_y_ = this->get_parameter("offset_y").as_double();
//     k_x_ = this->get_parameter("k_x").as_double();
//     k_y_ = this->get_parameter("k_y").as_double();
//     k_theta_ = this->get_parameter("k_theta").as_double();
//     min_safe_dist_ = this->get_parameter("min_safe_dist").as_double();
//     safety_lookahead_ = this->get_parameter("safety_lookahead").as_double();
//     safety_width_ = this->get_parameter("safety_width").as_double();
    
//     auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    
//     leader_sub_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//       this->get_parameter("leader_topic").as_string(), qos, 
//       std::bind(&FollowerNode::leader_callback, this, std::placeholders::_1));

//     odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
//       "odom", qos, 
//       std::bind(&FollowerNode::odom_callback, this, std::placeholders::_1));

//     map_sub_ = this->create_subscription<grid_map_msgs::msg::GridMap>(
//       this->get_parameter("map_topic").as_string(), rclcpp::QoS(1).best_effort(), 
//       std::bind(&FollowerNode::map_callback, this, std::placeholders::_1));

//     cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

//     timer_ = this->create_wall_timer(
//       50ms, std::bind(&FollowerNode::control_loop, this)); 

//     RCLCPP_INFO(this->get_logger(), "Active Avoidance Follower Started.");
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

//   void map_callback(const grid_map_msgs::msg::GridMap::SharedPtr msg)
//   {
//     grid_map::GridMapRosConverter::fromMessage(*msg, local_map_);
//     has_map_ = true;
//   }

//   // --- CHECK A SPECIFIC ANGLE FOR SAFETY ---
//   // Returns TRUE if the path along 'check_angle' is safe
//   bool is_direction_safe(double check_angle)
//   {
//     if (!has_map_ || !has_odom_) return true; 

//     double x = current_pose_.position.x;
//     double y = current_pose_.position.y;
//     double yaw = tf2::getYaw(current_pose_.orientation);
    
//     // Global angle to check
//     double global_angle = yaw + check_angle;

//     // Start checking OUTSIDE robot footprint (from 1.3m ahead)
//     double start_dist = 1.3; 
//     double step_length = 0.2; 

//     for (double d = start_dist; d <= safety_lookahead_; d += step_length) {
//         // We check a single line for the "feeler"
//         double px = x + d * cos(global_angle);
//         double py = y + d * sin(global_angle);
//         grid_map::Position pos(px, py);

//         if (local_map_.isInside(pos)) {
//             float drop_risk = 0.0;
//             float trav = 0.0;

//             try {
//                 if (local_map_.exists("drop_risk")) drop_risk = local_map_.atPosition("drop_risk", pos);
//                 if (local_map_.exists("traversability")) trav = local_map_.atPosition("traversability", pos);
//             } catch (...) { continue; }

//             if (drop_risk > 0.5 || trav > 0.8) {
//                 return false; // BLOCKED
//             }
//         }
//     }
//     return true; // CLEAR
//   }

//   // --- FIND BEST DETOUR ANGLE ---
//   // If center is blocked, look left and right for an opening
//   double find_safe_heading(double desired_heading_error)
//   {
//       // 1. Check straight towards target first
//       if (is_direction_safe(desired_heading_error)) {
//           return desired_heading_error; 
//       }

//       // 2. If blocked, check angles in increments
//       // We check +/- 15, +/- 30, +/- 45, +/- 60 degrees, up to 90
//       double scan_step = 0.26; // ~15 degrees
//       int max_steps = 6;

//       for (int i = 1; i <= max_steps; i++) {
//           double left_angle = desired_heading_error + (i * scan_step);
//           double right_angle = desired_heading_error - (i * scan_step);

//           if (is_direction_safe(left_angle)) {
//               RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Obstacle! Swerving LEFT.");
//               return left_angle;
//           }
//           if (is_direction_safe(right_angle)) {
//               RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Obstacle! Swerving RIGHT.");
//               return right_angle;
//           }
//       }

//       // 3. If everything is blocked -> Emergency Stop
//       RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TRAPPED! No safe path found.");
//       return -999.0; // Error code for "trapped"
//   }

//   void control_loop()
//   {
//     if (!has_leader_ || !has_odom_) {
//         return;
//     }

//     if ((this->get_clock()->now() - last_leader_time_).seconds() > 0.5) {
//         stop_robot(); 
//         return;
//     }

//     // --- 1. CALCULATE DESIRED TARGET (Formation Logic) ---
//     double leader_x = last_leader_msg_.pose.position.x;
//     double leader_y = last_leader_msg_.pose.position.y;
//     double leader_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//     double leader_v = last_leader_msg_.velocity.linear.x;

//     double my_x = current_pose_.position.x;
//     double my_y = current_pose_.position.y;
//     double my_yaw = tf2::getYaw(current_pose_.orientation);

//     double target_x = leader_x + (offset_x_ * cos(leader_yaw) - offset_y_ * sin(leader_yaw));
//     double target_y = leader_y + (offset_x_ * sin(leader_yaw) + offset_y_ * cos(leader_yaw));

//     double ex_global = target_x - my_x;
//     double ey_global = target_y - my_y;
//     double dist_error = sqrt(ex_global * ex_global + ey_global * ey_global);
    
//     double desired_heading_global = atan2(ey_global, ex_global);
//     double desired_heading_error = desired_heading_global - my_yaw;

//     while (desired_heading_error > M_PI) desired_heading_error -= 2.0 * M_PI;
//     while (desired_heading_error < -M_PI) desired_heading_error += 2.0 * M_PI;

//     // --- 2. OBSTACLE AVOIDANCE OVERRIDE ---
//     double steering_angle = find_safe_heading(desired_heading_error);
//     geometry_msgs::msg::Twist cmd;

//     if (steering_angle == -999.0) {
//         stop_robot();
//         return;
//     }
    
//     // --- 3. DRIVE ---
//     bool is_swerving = (std::abs(steering_angle - desired_heading_error) > 0.1);
    
//     cmd.angular.z = k_theta_ * steering_angle;

//     if (is_swerving) {
//          cmd.linear.x = 0.5;
//     } else {
//          cmd.linear.x = (leader_v * cos(desired_heading_error)) + (k_x_ * dist_error);
//     }
    
//     if (std::abs(steering_angle) > (M_PI / 2.0)) {
//         cmd.linear.x = 0.0;
//     }

//     cmd.linear.x = std::clamp(cmd.linear.x, 0.0, 1.5);
//     cmd.angular.z = std::clamp(cmd.angular.z, -2.0, 2.0);

//     cmd_vel_pub_->publish(cmd);
//   }

//   void stop_robot()
//   {
//     geometry_msgs::msg::Twist cmd; // Defaults to zero
//     cmd_vel_pub_->publish(cmd);
//   }

//   // Member Variables
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
//   double safety_lookahead_, safety_width_;

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

// NEW: TF2 includes for listening to transforms
#include "tf2_ros/buffer.hh"
#include "tf2_ros/transform_listener.h"

// Grid Map Headers
#include <grid_map_ros/grid_map_ros.hpp>
#include <grid_map_msgs/msg/grid_map.hpp>

using namespace std::chrono_literals;

class FollowerNode : public rclcpp::Node
{
public:
  FollowerNode()
  : Node("follower_node")
  {
    // --- TF2 Listener Setup ---
    tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    // --- Parameters ---
    this->declare_parameter<double>("offset_x", -2.0); 
    this->declare_parameter<double>("offset_y", 2.0);  
    this->declare_parameter<double>("k_x", 1.5); 
    this->declare_parameter<double>("k_theta", 4.0);
    this->declare_parameter<std::string>("leader_topic", "/leader_state");
    this->declare_parameter<std::string>("map_topic", "elevation_map"); 
    this->declare_parameter<double>("safety_lookahead", 2.0);
    this->declare_parameter<std::string>("global_frame", "map");
    this->declare_parameter<std::string>("robot_base_frame", "base_footprint");

    offset_x_ = this->get_parameter("offset_x").as_double();
    offset_y_ = this->get_parameter("offset_y").as_double();
    k_x_ = this->get_parameter("k_x").as_double();
    k_theta_ = this->get_parameter("k_theta").as_double();
    safety_lookahead_ = this->get_parameter("safety_lookahead").as_double();
    global_frame_ = this->get_parameter("global_frame").as_string();
    robot_base_frame_ = this->get_parameter("robot_base_frame").as_string();

    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    
    leader_sub_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
      this->get_parameter("leader_topic").as_string(), qos, 
      std::bind(&FollowerNode::leader_callback, this, std::placeholders::_1));

    // Odom is now only used for getting our local velocity and for the elevation mapper
    odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "odom", qos, 
      std::bind(&FollowerNode::odom_callback, this, std::placeholders::_1));

    map_sub_ = this->create_subscription<grid_map_msgs::msg::GridMap>(
      this->get_parameter("map_topic").as_string(), rclcpp::QoS(1).best_effort(), 
      std::bind(&FollowerNode::map_callback, this, std::placeholders::_1));

    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

    timer_ = this->create_wall_timer(
      50ms, std::bind(&FollowerNode::control_loop, this)); 

    RCLCPP_INFO(this->get_logger(), "TF-Aware Follower Node Started.");
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
    // We only care about our current velocity from odom now
    current_velocity_ = msg->twist.twist;
    has_odom_ = true;
  }

  void map_callback(const grid_map_msgs::msg::GridMap::SharedPtr msg)
  {
    grid_map::GridMapRosConverter::fromMessage(*msg, local_map_);
    has_map_ = true;
  }

  bool is_direction_safe(double check_angle, const geometry_msgs::msg::Pose& my_pose)
  {
    if (!has_map_) return true; 

    double x = my_pose.position.x;
    double y = my_pose.position.y;
    double yaw = tf2::getYaw(my_pose.orientation);
    
    double global_angle = yaw + check_angle;

    double start_dist = 0.8; 
    double step_length = 0.2; 

    for (double d = start_dist; d <= safety_lookahead_; d += step_length) {
        double px = x + d * cos(global_angle);
        double py = y + d * sin(global_angle);
        grid_map::Position pos(px, py);

        if (local_map_.isInside(pos)) {
            float drop_risk = 0.0, trav = 0.0;
            try {
                if (local_map_.exists("drop_risk")) drop_risk = local_map_.atPosition("drop_risk", pos);
                if (local_map_.exists("traversability")) trav = local_map_.atPosition("traversability", pos);
            } catch (...) { continue; }

            if (drop_risk > 0.5 || trav > 0.8) return false;
        }
    }
    return true;
  }

  double find_safe_heading(double desired_heading_error, const geometry_msgs::msg::Pose& my_pose)
  {
      if (is_direction_safe(desired_heading_error, my_pose)) {
          return desired_heading_error; 
      }

      double scan_step = 0.26;
      int max_steps = 6;

      for (int i = 1; i <= max_steps; i++) {
          double left_angle = desired_heading_error + (i * scan_step);
          if (is_direction_safe(left_angle, my_pose)) {
              RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Obstacle! Swerving LEFT.");
              return left_angle;
          }
          double right_angle = desired_heading_error - (i * scan_step);
          if (is_direction_safe(right_angle, my_pose)) {
              RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Obstacle! Swerving RIGHT.");
              return right_angle;
          }
      }

      RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TRAPPED! No safe path found.");
      return -999.0;
  }

  void control_loop()
  {
    if (!has_leader_ || !has_odom_) return;

    if ((this->get_clock()->now() - last_leader_time_).seconds() > 0.5) {
        stop_robot(); 
        return;
    }

    // --- STEP 1: GET OWN POSE in the GLOBAL FRAME using TF2 ---
    geometry_msgs::msg::Pose my_pose_global;
    try {
        // This is the most important change. We look up our own position in the map frame.
        geometry_msgs::msg::TransformStamped t = tf_buffer_->lookupTransform(
            global_frame_, // Target frame (e.g., "map")
            this->get_namespace() + std::string("/") + robot_base_frame_, // Source frame (e.g., "/robot_02/base_footprint")
            tf2::TimePointZero);

        my_pose_global.position.x = t.transform.translation.x;
        my_pose_global.position.y = t.transform.translation.y;
        my_pose_global.orientation = t.transform.rotation;
    } catch (const tf2::TransformException & ex) {
        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Could not transform %s to %s: %s",
             (this->get_namespace() + std::string("/") + robot_base_frame_).c_str(), global_frame_.c_str(), ex.what());
        return;
    }

    // --- 2. CALCULATE DESIRED TARGET (Now in the same frame!) ---
    double leader_x = last_leader_msg_.pose.position.x;
    double leader_y = last_leader_msg_.pose.position.y;
    double leader_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);

    double my_x = my_pose_global.position.x;
    double my_y = my_pose_global.position.y;
    double my_yaw = tf2::getYaw(my_pose_global.orientation);

    double target_x = leader_x + (offset_x_ * cos(leader_yaw) - offset_y_ * sin(leader_yaw));
    double target_y = leader_y + (offset_x_ * sin(leader_yaw) + offset_y_ * cos(leader_yaw));

    double ex_global = target_x - my_x;
    double ey_global = target_y - my_y;
    double dist_error = sqrt(ex_global * ex_global + ey_global * ey_global);
    
    double desired_heading_global = atan2(ey_global, ex_global);
    double desired_heading_error = desired_heading_global - my_yaw;

    while (desired_heading_error > M_PI) desired_heading_error -= 2.0 * M_PI;
    while (desired_heading_error < -M_PI) desired_heading_error += 2.0 * M_PI;

    // --- 3. OBSTACLE AVOIDANCE (Uses our correct global pose now) ---
    double steering_angle = find_safe_heading(desired_heading_error, my_pose_global);
    
    geometry_msgs::msg::Twist cmd;
    if (steering_angle == -999.0) {
        stop_robot();
        return;
    }
    
    // --- 4. DRIVE ---
    cmd.angular.z = k_theta_ * steering_angle;
    cmd.linear.x = k_x_ * dist_error; // Simple P-controller on distance error

    if (std::abs(steering_angle) > (M_PI / 2.0)) {
        cmd.linear.x = 0.0;
    }

    cmd.linear.x = std::clamp(cmd.linear.x, 0.0, 1.5);
    cmd.angular.z = std::clamp(cmd.angular.z, -2.0, 2.0);

    cmd_vel_pub_->publish(cmd);
  }

  void stop_robot()
  {
    geometry_msgs::msg::Twist cmd;
    cmd_vel_pub_->publish(cmd);
  }

  // Member Variables
  skyhunter_msgs::msg::LeaderState last_leader_msg_;
  rclcpp::Time last_leader_time_;
  bool has_leader_ = false;

  geometry_msgs::msg::Twist current_velocity_;
  bool has_odom_ = false;

  grid_map::GridMap local_map_;
  bool has_map_ = false;

  double offset_x_, offset_y_, k_x_, k_theta_, safety_lookahead_;
  std::string global_frame_, robot_base_frame_;

  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr leader_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr map_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  // TF2 Listener
  std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_{nullptr};
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FollowerNode>());
  rclcpp::shutdown();
  return 0;
}