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

//             printf("OBSTACLE CHECKING.....Check d=%.2f m: drop=%.2f trav=%.2f\n", d, drop_risk, trav);

//             if (std::isnan(drop_risk) || std::isnan(trav)) {
//                 continue;  // UNKNOWN → don't block
//             }

//             if (drop_risk > 0.5 || trav > 0.9) {
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

//   // void control_loop()
//   // {
//   //   if (!has_leader_ || !has_odom_) {
//   //       return;
//   //   }

//   //   if ((this->get_clock()->now() - last_leader_time_).seconds() > 0.5) {
//   //       stop_robot(); 
//   //       return;
//   //   }

//   //   // --- 1. CALCULATE DESIRED TARGET (Formation Logic) ---
//   //   double leader_x = last_leader_msg_.pose.position.x;
//   //   double leader_y = last_leader_msg_.pose.position.y;
//   //   double leader_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//   //   double leader_v = last_leader_msg_.velocity.linear.x;

//   //   double my_x = current_pose_.position.x;
//   //   double my_y = current_pose_.position.y;
//   //   double my_yaw = tf2::getYaw(current_pose_.orientation);

//   //   double target_x = leader_x + (offset_x_ * cos(leader_yaw) - offset_y_ * sin(leader_yaw));
//   //   double target_y = leader_y + (offset_x_ * sin(leader_yaw) + offset_y_ * cos(leader_yaw));

//   //   double ex_global = target_x - my_x;
//   //   double ey_global = target_y - my_y;
//   //   double dist_error = sqrt(ex_global * ex_global + ey_global * ey_global);
    
//   //   double desired_heading_global = atan2(ey_global, ex_global);
//   //   double desired_heading_error = desired_heading_global - my_yaw;

//   //   while (desired_heading_error > M_PI) desired_heading_error -= 2.0 * M_PI;
//   //   while (desired_heading_error < -M_PI) desired_heading_error += 2.0 * M_PI;

//   //   // --- 2. OBSTACLE AVOIDANCE OVERRIDE ---
//   //   double steering_angle = find_safe_heading(desired_heading_error);
//   //   geometry_msgs::msg::Twist cmd;

//   //   if (steering_angle == -999.0) {
//   //       stop_robot();
//   //       return;
//   //   }
    
//   //   // --- 3. DRIVE ---
//   //   bool is_swerving = (std::abs(steering_angle - desired_heading_error) > 0.1);
    
//   //   cmd.angular.z = k_theta_ * steering_angle;

//   //   if (is_swerving) {
//   //        cmd.linear.x = 0.5;
//   //   } else {
//   //        cmd.linear.x = (leader_v * cos(desired_heading_error)) + (k_x_ * dist_error);
//   //   }
    
//   //   if (std::abs(steering_angle) > (M_PI / 2.0)) {
//   //       cmd.linear.x = 0.0;
//   //   }

//   //   cmd.linear.x = std::clamp(cmd.linear.x, 0.0, 1.5);
//   //   cmd.angular.z = std::clamp(cmd.angular.z, -2.0, 2.0);

//   //   cmd_vel_pub_->publish(cmd);
//   // }


//   void control_loop()
//   {
//     if (!has_leader_ || !has_odom_) {
//         return;
//     }

//     if ((this->get_clock()->now() - last_leader_time_).seconds() > 0.5) {
//         stop_robot(); 
//         return;
//     }

//     // --- 1. LEADER & SELF STATE ---
//     double leader_x = last_leader_msg_.pose.position.x;
//     double leader_y = last_leader_msg_.pose.position.y;
//     double leader_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//     double leader_v = last_leader_msg_.velocity.linear.x;

//     double my_x = current_pose_.position.x;
//     double my_y = current_pose_.position.y;
//     double my_yaw = tf2::getYaw(current_pose_.orientation);

//     // --- 2. FORMATION TARGET (GLOBAL FRAME) ---
//     double target_x = leader_x + (offset_x_ * cos(leader_yaw) - offset_y_ * sin(leader_yaw));
//     double target_y = leader_y + (offset_x_ * sin(leader_yaw) + offset_y_ * cos(leader_yaw));

//     double ex = target_x - my_x;
//     double ey = target_y - my_y;

//     double dist_error = std::hypot(ex, ey);
//     double desired_heading = std::atan2(ey, ex);
//     double heading_error = desired_heading - my_yaw;

//     while (heading_error > M_PI) heading_error -= 2.0 * M_PI;
//     while (heading_error < -M_PI) heading_error += 2.0 * M_PI;

//     // --- 3. OBSTACLE AVOIDANCE ---
//     double safe_heading = find_safe_heading(heading_error);
//     geometry_msgs::msg::Twist cmd;

//     if (safe_heading == -999.0) {
//         stop_robot();
//         return;
//     }

//     // --- 4. ANGULAR CONTROL ---
//     cmd.angular.z = k_theta_ * safe_heading;

//     // --- 5. LINEAR CONTROL (CRITICAL FIX) ---
//     // Rotate first, translate later
//     if (std::abs(safe_heading) > 0.6) {
//         cmd.linear.x = 0.2;   // slow creep while turning
//     } 
//     else {
//         cmd.linear.x = leader_v + k_x_ * dist_error;
//     }

//     // --- 6. SAFETY CLAMPS ---
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
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"
#include "grid_map_ros/grid_map_ros.hpp"
#include "visualization_msgs/msg/marker_array.hpp"

using namespace std::chrono_literals;

class FollowerNode : public rclcpp::Node
{
public:
  FollowerNode() : Node("follower_node")
  {
    // --- PARAMS ---
    this->declare_parameter<double>("offset_x", -2.0); 
    this->declare_parameter<double>("offset_y", 2.0);  
    this->declare_parameter<double>("robot_width", 0.9); 
    this->declare_parameter<std::string>("leader_topic", "/leader_state");
    this->declare_parameter<std::string>("map_topic", "elevation_map"); 
    
    offset_x_ = this->get_parameter("offset_x").as_double();
    offset_y_ = this->get_parameter("offset_y").as_double();
    robot_width_ = this->get_parameter("robot_width").as_double();
    
    // TF Listener
    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    auto qos = rclcpp::SensorDataQoS();
    
    sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
        this->get_parameter("leader_topic").as_string(), qos, 
        std::bind(&FollowerNode::leader_cb, this, std::placeholders::_1));

    sub_map_ = this->create_subscription<grid_map_msgs::msg::GridMap>(
        this->get_parameter("map_topic").as_string(), rclcpp::QoS(1).best_effort(), 
        std::bind(&FollowerNode::map_cb, this, std::placeholders::_1));
        
    // Dummy sub for heartbeat
    sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(
        "odom", qos, [](const nav_msgs::msg::Odometry::SharedPtr){});

    pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    pub_debug_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("debug_vis", 10);

    timer_ = this->create_wall_timer(50ms, std::bind(&FollowerNode::control_loop, this)); 
    
    // Resolve Frame ID
    std::string ns = this->get_namespace();
    // Handles "/robot_02" -> "robot_02/base_footprint"
    if (ns == "/") my_frame_ = "base_footprint";
    else {
        if (ns[0] == '/') ns.erase(0,1);
        my_frame_ = ns + "/base_footprint";
    }

    RCLCPP_INFO(this->get_logger(), "Robust Follower Started. Frame: %s", my_frame_.c_str());
  }

private:
  void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
    last_leader_msg_ = *msg;
    last_leader_time_ = this->get_clock()->now();
    has_leader_ = true;
  }

  void map_cb(const grid_map_msgs::msg::GridMap::SharedPtr msg) {
    grid_map::GridMapRosConverter::fromMessage(*msg, local_map_);
    has_map_ = true;
  }

  // --- SAFETY CHECKER ---
  // Returns collision distance. If > max_dist, path is clear.
  double check_path_clearance(double angle, double max_dist)
  {
      if (!has_map_) return max_dist; 

      // Transform variables
      geometry_msgs::msg::TransformStamped tf;
      try {
          // We need Map -> Robot transform to check grid map
          tf = tf_buffer_->lookupTransform(local_map_.getFrameId(), my_frame_, tf2::TimePointZero);
      } catch (...) { return max_dist; }

      double robot_x = tf.transform.translation.x;
      double robot_y = tf.transform.translation.y;
      double robot_yaw = tf2::getYaw(tf.transform.rotation);
      
      double check_yaw = robot_yaw + angle;
      
      // Safety Width: Robot Width + 0.2m margin
      double safety_width = robot_width_ + 0.2; 
      std::vector<double> lateral_offsets = {0.0, safety_width/2.0, -safety_width/2.0};

      // Step along the path
      for (double d = 0.5; d < max_dist; d += 0.1) {
          double cx = robot_x + d * cos(check_yaw);
          double cy = robot_y + d * sin(check_yaw);

          for (double off : lateral_offsets) {
              // Check lateral points relative to path centerline
              double px = cx - off * sin(check_yaw);
              double py = cy + off * cos(check_yaw);
              
              grid_map::Position pos(px, py);
              if (local_map_.isInside(pos)) {
                  float drop = 0.0, trav = 0.0;
                  try {
                      if (local_map_.exists("drop_risk")) drop = local_map_.atPosition("drop_risk", pos);
                      if (local_map_.exists("traversability")) trav = local_map_.atPosition("traversability", pos);
                  } catch (...) { continue; }

                  if (drop > 0.5 || trav > 0.5) {
                      return d; // Collision detected at distance d
                  }
              }
          }
      }
      return max_dist; // No collision found
  }

 void control_loop() {
    if (!has_leader_) return;

    if ((this->get_clock()->now() - last_leader_time_).seconds() > 2.0) {
        stop_robot(); return;
    }

    // 1. Calculate Target position in Global Map frame
    double leader_x = last_leader_msg_.pose.position.x;
    double leader_y = last_leader_msg_.pose.position.y;
    tf2::Quaternion q_l; tf2::fromMsg(last_leader_msg_.pose.orientation, q_l);
    double leader_yaw = tf2::getYaw(q_l);

    double target_wx = leader_x + (offset_x_ * cos(leader_yaw) - offset_y_ * sin(leader_yaw));
    double target_wy = leader_y + (offset_x_ * sin(leader_yaw) + offset_y_ * cos(leader_yaw));

    // 2. Transform the Global Target into my LOCAL robot frame
    geometry_msgs::msg::PoseStamped goal_w, goal_r;
    goal_w.header.frame_id = "map"; 
    goal_w.header.stamp = rclcpp::Time(0); // Get latest available TF
    goal_w.pose.position.x = target_wx; 
    goal_w.pose.position.y = target_wy; 
    goal_w.pose.orientation.w = 1.0;

    try {
        // This calculates exactly where the target is relative to the follower's body
        tf_buffer_->transform(goal_w, goal_r, my_frame_, tf2::durationFromSec(0.1));
    } catch (tf2::TransformException &ex) {
        RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 2000, 
            "TF ERROR: Cannot find map to %s. QoS Mismatch?", my_frame_.c_str());
        stop_robot();
        return; 
    }

    // 3. Extract errors from the transformed pose
    double dx = goal_r.pose.position.x;
    double dy = goal_r.pose.position.y;
    double dist_error = std::hypot(dx, dy);
    double heading_error = std::atan2(dy, dx);

    // 4. Movement Logic
    geometry_msgs::msg::Twist cmd;
    
    if (dist_error < 0.4) { // Arrived
        stop_robot();
        return;
    }

    // P-Controller for steering and speed
    cmd.angular.z = 2.0 * heading_error; 
    cmd.linear.x = 0.6 * dist_error; // Move faster if far away

    // Limit speeds
    cmd.linear.x = std::clamp(cmd.linear.x, 0.0, 1.2);
    cmd.angular.z = std::clamp(cmd.angular.z, -1.5, 1.5);
    
    // Slow down speed if turning sharply
    if (std::abs(heading_error) > 0.6) cmd.linear.x *= 0.3;

    pub_cmd_->publish(cmd);
  }

  void stop_robot() {
    geometry_msgs::msg::Twist cmd; 
    pub_cmd_->publish(cmd);
  }

  skyhunter_msgs::msg::LeaderState last_leader_msg_;
  rclcpp::Time last_leader_time_;
  bool has_leader_ = false;
  bool has_map_ = false;
  grid_map::GridMap local_map_;
  
  double offset_x_, offset_y_, robot_width_;
  std::string my_frame_;

  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
  rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr sub_map_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr pub_debug_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FollowerNode>());
  rclcpp::shutdown();
  return 0;
}