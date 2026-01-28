// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <string>
// #include <algorithm>
// #include <vector>
// #include <deque>

// #include "rclcpp/rclcpp.hpp"
// #include "geometry_msgs/msg/twist.hpp"
// #include "geometry_msgs/msg/pose_stamped.hpp"
// #include "nav_msgs/msg/odometry.hpp"
// #include "nav_msgs/msg/path.hpp"
// #include "tf2/utils.h"
// #include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
// #include "tf2_ros/buffer.h"
// #include "tf2_ros/transform_listener.h"

// using namespace std::chrono_literals;

// class FollowerNode : public rclcpp::Node
// {
// public:
//     enum class FollowerState {
//         INITIALIZING,
//         TRACKING,
//         RECOVERING,
//         LOST,
//         STOPPED
//     };

//     FollowerNode() : Node("follower_node"), tf_buffer_(std::make_shared<tf2_ros::Buffer>(this->get_clock())),
//                      tf_listener_(std::make_shared<tf2_ros::TransformListener>(*tf_buffer_))
//     {
//         // --- Parameters ---
//         this->declare_parameter<double>("offset_x", -2.0);
//         this->declare_parameter<double>("offset_y", 2.0);
//         this->declare_parameter<double>("k_v", 2.0);
//         this->declare_parameter<double>("k_w", 3.0);
//         this->declare_parameter<double>("velocity_match_gain", 0.9);
//         this->declare_parameter<double>("angular_match_gain", 0.8);
//         this->declare_parameter<double>("max_linear_vel", 1.0);
//         this->declare_parameter<double>("max_angular_vel", 2.0);
//         this->declare_parameter<double>("stop_distance_threshold", 0.2);
//         this->declare_parameter<double>("slow_distance_threshold", 1.5);
//         this->declare_parameter<double>("fast_distance_threshold", 3.0);
//         this->declare_parameter<double>("lost_timeout", 1.5);
//         this->declare_parameter<double>("recovery_timeout", 3.0);
//         this->declare_parameter<double>("lookahead_time", 0.5);
//         this->declare_parameter<std::string>("leader_topic", "/robot1/odom");
//         this->declare_parameter<std::string>("leader_cmd_topic", "/robot1/cmd_vel");
//         this->declare_parameter<std::string>("leader_base_frame", "robot1/base_link");
//         this->declare_parameter<std::string>("follower_base_frame", "base_link");
//         this->declare_parameter<std::string>("odom_frame", "odom");
//         this->declare_parameter<bool>("use_tf", false);
//         this->declare_parameter<bool>("predictive_turning", true);

//         // Get parameters
//         offset_x_ = this->get_parameter("offset_x").as_double();
//         offset_y_ = this->get_parameter("offset_y").as_double();
//         k_v_ = this->get_parameter("k_v").as_double();
//         k_w_ = this->get_parameter("k_w").as_double();
//         velocity_match_gain_ = this->get_parameter("velocity_match_gain").as_double();
//         angular_match_gain_ = this->get_parameter("angular_match_gain").as_double();
//         max_linear_vel_ = this->get_parameter("max_linear_vel").as_double();
//         max_angular_vel_ = this->get_parameter("max_angular_vel").as_double();
//         stop_distance_threshold_ = this->get_parameter("stop_distance_threshold").as_double();
//         slow_distance_threshold_ = this->get_parameter("slow_distance_threshold").as_double();
//         fast_distance_threshold_ = this->get_parameter("fast_distance_threshold").as_double();
//         lost_timeout_ = this->get_parameter("lost_timeout").as_double();
//         recovery_timeout_ = this->get_parameter("recovery_timeout").as_double();
//         lookahead_time_ = this->get_parameter("lookahead_time").as_double();
//         leader_base_frame_ = this->get_parameter("leader_base_frame").as_string();
//         follower_base_frame_ = this->get_parameter("follower_base_frame").as_string();
//         odom_frame_ = this->get_parameter("odom_frame").as_string();
//         use_tf_ = this->get_parameter("use_tf").as_bool();
//         predictive_turning_ = this->get_parameter("predictive_turning").as_bool();
//         std::string leader_topic = this->get_parameter("leader_topic").as_string();
//         std::string leader_cmd_topic = this->get_parameter("leader_cmd_topic").as_string();

//         // State initialization
//         current_state_ = FollowerState::INITIALIZING;
//         last_leader_time_ = this->get_clock()->now();
//         last_state_change_time_ = this->get_clock()->now();
//         last_control_time_ = this->get_clock()->now();
//         leader_velocity_.linear.x = 0.0;
//         leader_velocity_.angular.z = 0.0;
//         last_leader_cmd_vel_.linear.x = 0.0;
//         last_leader_cmd_vel_.angular.z = 0.0;

//         // --- Communications ---
//         // Subscribe to leader's odometry
//         leader_odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
//             leader_topic, 10, 
//             std::bind(&FollowerNode::leader_odom_callback, this, std::placeholders::_1));

//         // Subscribe to leader's cmd_vel to predict movement
//         leader_cmd_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
//             leader_cmd_topic, 10,
//             std::bind(&FollowerNode::leader_cmd_callback, this, std::placeholders::_1));

//         odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
//             "odom", 10, 
//             std::bind(&FollowerNode::odom_callback, this, std::placeholders::_1));

//         cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

//         // Control timer (20Hz)
//         control_timer_ = this->create_wall_timer(
//             50ms, std::bind(&FollowerNode::control_loop, this));

//         // State monitor timer (5Hz)
//         state_timer_ = this->create_wall_timer(
//             200ms, std::bind(&FollowerNode::state_monitor, this));

//         RCLCPP_INFO(this->get_logger(), "Follower Node Started with offset: [%.2f, %.2f]", offset_x_, offset_y_);
//         RCLCPP_INFO(this->get_logger(), "State: INITIALIZING");
//     }

// private:
//     struct PoseHistory {
//         geometry_msgs::msg::Pose pose;
//         rclcpp::Time timestamp;
//     };

//     void leader_odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
//     {
//         std::lock_guard<std::mutex> lock(mutex_);
        
//         // Store in history (simple vector with max size)
//         PoseHistory history;
//         history.pose = msg->pose.pose;
//         history.timestamp = this->get_clock()->now();
        
//         leader_pose_history_.push_back(history);
        
//         // Keep only recent history (last 20 entries)
//         if (leader_pose_history_.size() > 20) {
//             leader_pose_history_.erase(leader_pose_history_.begin());
//         }
        
//         // Update current leader state
//         last_leader_pose_ = msg->pose.pose;
//         last_leader_odom_ = *msg;
//         last_leader_time_ = history.timestamp;
//         leader_velocity_ = msg->twist.twist;
        
//         // Calculate leader acceleration from history
//         if (leader_pose_history_.size() >= 2) {
//             auto& current = leader_pose_history_.back();
//             auto& previous = leader_pose_history_[leader_pose_history_.size() - 2];
            
//             double dt = (current.timestamp - previous.timestamp).seconds();
//             if (dt > 0.01) {
//                 double dx = current.pose.position.x - previous.pose.position.x;
//                 double dy = current.pose.position.y - previous.pose.position.y;
//                 double distance = sqrt(dx*dx + dy*dy);
//                 leader_speed_ = distance / dt;
//             }
//         }
        
//         // If we were LOST and got a leader update, try to recover
//         if (current_state_ == FollowerState::LOST) {
//             change_state(FollowerState::RECOVERING);
//         } else if (current_state_ == FollowerState::INITIALIZING) {
//             change_state(FollowerState::TRACKING);
//         }
//     }

//     void leader_cmd_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
//     {
//         std::lock_guard<std::mutex> lock(mutex_);
//         last_leader_cmd_vel_ = *msg;
//         last_leader_cmd_time_ = this->get_clock()->now();
//     }

//     void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
//     {
//         std::lock_guard<std::mutex> lock(mutex_);
//         current_pose_ = msg->pose.pose;
//         current_velocity_ = msg->twist.twist;
//         last_odom_time_ = this->get_clock()->now();
//     }

//     void state_monitor()
//     {
//         std::lock_guard<std::mutex> lock(mutex_);
//         auto now = this->get_clock()->now();
        
//         // Check for leader timeout
//         double time_since_last_leader = (now - last_leader_time_).seconds();
        
//         switch (current_state_) {
//             case FollowerState::TRACKING:
//                 if (time_since_last_leader > lost_timeout_) {
//                     change_state(FollowerState::LOST);
//                     RCLCPP_WARN(this->get_logger(), "Leader lost! Transitioning to LOST state.");
//                 } else if (time_since_last_leader > 0.8) {
//                     // Leader is not updating frequently, slow down
//                     change_state(FollowerState::RECOVERING);
//                 }
//                 break;
                
//             case FollowerState::RECOVERING:
//                 if (time_since_last_leader < 0.3) {
//                     // Got fresh leader data, go back to tracking
//                     change_state(FollowerState::TRACKING);
//                 } else if ((now - last_state_change_time_).seconds() > recovery_timeout_) {
//                     change_state(FollowerState::LOST);
//                 }
//                 break;
                
//             case FollowerState::LOST:
//                 // Check if we can get leader data again
//                 if (time_since_last_leader < 0.3) {
//                     change_state(FollowerState::RECOVERING);
//                 }
//                 break;
                
//             default:
//                 break;
//         }
//     }

//     void control_loop()
//     {
//         std::lock_guard<std::mutex> lock(mutex_);
        
//         geometry_msgs::msg::Twist cmd;
        
//         switch (current_state_) {
//             case FollowerState::INITIALIZING:
//                 initialize_behavior(cmd);
//                 break;
                
//             case FollowerState::TRACKING:
//                 tracking_behavior(cmd);
//                 break;
                
//             case FollowerState::RECOVERING:
//                 recovering_behavior(cmd);
//                 break;
                
//             case FollowerState::LOST:
//                 lost_behavior(cmd);
//                 break;
                
//             case FollowerState::STOPPED:
//                 stop_behavior(cmd);
//                 break;
//         }
        
//         cmd_vel_pub_->publish(cmd);
//         last_control_time_ = this->get_clock()->now();
//     }

//     void initialize_behavior(geometry_msgs::msg::Twist &cmd)
//     {
//         auto now = this->get_clock()->now();
//         double time_since_last_leader = (now - last_leader_time_).seconds();
        
//         if (time_since_last_leader < 0.5) {
//             change_state(FollowerState::TRACKING);
//             RCLCPP_INFO(this->get_logger(), "Initialization complete. Transitioning to TRACKING.");
//         } else {
//             cmd.linear.x = 0.0;
//             cmd.angular.z = 0.0;
//             RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000, 
//                                 "Waiting for leader data...");
//         }
//     }

//     void tracking_behavior(geometry_msgs::msg::Twist &cmd)
//     {
//         auto now = this->get_clock()->now();
//         double time_since_last_leader = (now - last_leader_time_).seconds();
        
//         if (time_since_last_leader > 1.0) {
//             RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
//                                 "Leader data stale, reducing speed");
//             cmd.linear.x = 0.0;
//             cmd.angular.z = 0.0;
//             return;
//         }
        
//         try {
//             // Calculate target position with offset
//             double leader_yaw = tf2::getYaw(last_leader_pose_.orientation);
            
//             // PREDICTIVE OFFSET: If leader is turning, adjust offset for smoother following
//             double predicted_yaw = leader_yaw;
//             if (predictive_turning_ && fabs(last_leader_cmd_vel_.angular.z) > 0.1) {
//                 // Predict leader's future orientation based on angular velocity
//                 double time_since_last_cmd = (now - last_leader_cmd_time_).seconds();
//                 if (time_since_last_cmd < 0.5) {
//                     predicted_yaw = leader_yaw + last_leader_cmd_vel_.angular.z * lookahead_time_;
//                 }
//             }
            
//             // Calculate target position
//             double target_x = last_leader_pose_.position.x + 
//                             (offset_x_ * cos(predicted_yaw) - offset_y_ * sin(predicted_yaw));
//             double target_y = last_leader_pose_.position.y + 
//                             (offset_x_ * sin(predicted_yaw) + offset_y_ * cos(predicted_yaw));
            
//             // Calculate errors
//             double my_x = current_pose_.position.x;
//             double my_y = current_pose_.position.y;
//             double my_yaw = tf2::getYaw(current_pose_.orientation);
            
//             double error_x = target_x - my_x;
//             double error_y = target_y - my_y;
//             double distance_error = sqrt(error_x*error_x + error_y*error_y);
            
//             double target_heading = atan2(error_y, error_x);
//             double heading_error = target_heading - my_yaw;
            
//             // Normalize angle
//             while (heading_error > M_PI) heading_error -= 2.0 * M_PI;
//             while (heading_error < -M_PI) heading_error += 2.0 * M_PI;
            
//             // Adaptive control gains based on distance
//             double adaptive_k_v = k_v_;
//             double adaptive_k_w = k_w_;
            
//             if (distance_error > fast_distance_threshold_) {
//                 // Far away - increase gains
//                 adaptive_k_v *= 1.5;
//                 adaptive_k_w *= 1.2;
//             } else if (distance_error < slow_distance_threshold_) {
//                 // Close - reduce gains for smooth approach
//                 adaptive_k_v *= 0.7;
//                 adaptive_k_w *= 0.8;
//             }
            
//             // Calculate base velocities
//             double base_linear = 0.0;
//             double base_angular = 0.0;
            
//             if (distance_error < stop_distance_threshold_) {
//                 // At target position
//                 base_linear = 0.0;
//                 base_angular = 0.0;
//             } else {
//                 // Position control
//                 if (fabs(heading_error) < M_PI/3.0) {
//                     // Facing approximately the right direction
//                     base_linear = adaptive_k_v * distance_error;
//                     base_angular = adaptive_k_w * heading_error;
//                 } else {
//                     // Need to turn first
//                     base_linear = 0.0;
//                     base_angular = (heading_error > 0) ? max_angular_vel_/2 : -max_angular_vel_/2;
//                 }
//             }
            
//             // MATCH LEADER'S VELOCITY - CRITICAL FIX
//             double leader_linear = leader_velocity_.linear.x;
//             double leader_angular = leader_velocity_.angular.z;
            
//             // Use command velocity if available (more responsive)
//             double time_since_last_cmd = (now - last_leader_cmd_time_).seconds();
//             if (time_since_last_cmd < 0.3) {
//                 leader_linear = last_leader_cmd_vel_.linear.x;
//                 leader_angular = last_leader_cmd_vel_.angular.z;
//             }
            
//             // Combine position control with velocity matching
//             cmd.linear.x = base_linear + (velocity_match_gain_ * leader_linear);
//             cmd.angular.z = base_angular + (angular_match_gain_ * leader_angular);
            
//             // If leader is turning, add extra angular velocity for coordinated turning
//             if (fabs(leader_angular) > 0.2) {
//                 cmd.angular.z += leader_angular * 0.5;
//             }
            
//             // Apply velocity limits (allow some negative for quick corrections)
//             cmd.linear.x = std::clamp(cmd.linear.x, -max_linear_vel_ * 0.3, max_linear_vel_);
//             cmd.angular.z = std::clamp(cmd.angular.z, -max_angular_vel_, max_angular_vel_);
            
//             // Log for debugging
//             RCLCPP_DEBUG_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
//                                  "Tracking: dist=%.2f, h_err=%.2f, v_lead=%.2f, v_fol=%.2f",
//                                  distance_error, heading_error, leader_linear, cmd.linear.x);
            
//         } catch (const std::exception &ex) {
//             RCLCPP_ERROR(this->get_logger(), "Error in tracking: %s", ex.what());
//             cmd.linear.x = 0.0;
//             cmd.angular.z = 0.0;
//         }
//     }

//     void recovering_behavior(geometry_msgs::msg::Twist &cmd)
//     {
//         auto now = this->get_clock()->now();
//         double time_since_last_leader = (now - last_leader_time_).seconds();
        
//         if (time_since_last_leader < 0.5) {
//             // Got recent data, do tracking but at reduced speed
//             tracking_behavior(cmd);
//             cmd.linear.x *= 0.5;
//             cmd.angular.z *= 0.7;
//         } else {
//             // No recent data, gentle scanning
//             cmd.linear.x = 0.0;
//             double scan_speed = 0.4 * sin(now.seconds() * 0.8);
//             cmd.angular.z = scan_speed;
//         }
//     }

//     void lost_behavior(geometry_msgs::msg::Twist &cmd)
//     {
//         cmd.linear.x = 0.0;
//         cmd.angular.z = 0.0;
        
//         RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
//                             "LOST: Waiting for leader...");
//     }

//     void stop_behavior(geometry_msgs::msg::Twist &cmd)
//     {
//         cmd.linear.x = 0.0;
//         cmd.angular.z = 0.0;
//     }

//     void change_state(FollowerState new_state)
//     {
//         if (current_state_ != new_state) {
//             current_state_ = new_state;
//             last_state_change_time_ = this->get_clock()->now();
            
//             std::string state_str;
//             switch (new_state) {
//                 case FollowerState::INITIALIZING: state_str = "INITIALIZING"; break;
//                 case FollowerState::TRACKING: state_str = "TRACKING"; break;
//                 case FollowerState::RECOVERING: state_str = "RECOVERING"; break;
//                 case FollowerState::LOST: state_str = "LOST"; break;
//                 case FollowerState::STOPPED: state_str = "STOPPED"; break;
//             }
//             RCLCPP_INFO(this->get_logger(), "State changed to: %s", state_str.c_str());
//         }
//     }

//     // Variables
//     std::mutex mutex_;
//     FollowerState current_state_;
//     geometry_msgs::msg::Pose last_leader_pose_;
//     nav_msgs::msg::Odometry last_leader_odom_;
//     geometry_msgs::msg::Twist last_leader_cmd_vel_;
//     geometry_msgs::msg::Twist leader_velocity_;
//     rclcpp::Time last_leader_time_;
//     rclcpp::Time last_state_change_time_;
//     rclcpp::Time last_control_time_;
//     rclcpp::Time last_odom_time_;
//     rclcpp::Time last_leader_cmd_time_;
//     geometry_msgs::msg::Pose current_pose_;
//     geometry_msgs::msg::Twist current_velocity_;
//     double leader_speed_ = 0.0;
    
//     // Simple vector for pose history
//     std::vector<PoseHistory> leader_pose_history_;
    
//     // Parameters
//     double offset_x_, offset_y_;
//     double k_v_, k_w_;
//     double velocity_match_gain_;
//     double angular_match_gain_;
//     double max_linear_vel_, max_angular_vel_;
//     double stop_distance_threshold_;
//     double slow_distance_threshold_;
//     double fast_distance_threshold_;
//     double lost_timeout_;
//     double recovery_timeout_;
//     double lookahead_time_;
//     std::string leader_base_frame_;
//     std::string follower_base_frame_;
//     std::string odom_frame_;
//     bool use_tf_;
//     bool predictive_turning_;
    
//     // TF2
//     std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
//     std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
    
//     // ROS2
//     rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr leader_odom_sub_;
//     rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr leader_cmd_sub_;
//     rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
//     rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
//     rclcpp::TimerBase::SharedPtr control_timer_;
//     rclcpp::TimerBase::SharedPtr state_timer_;
// };

// int main(int argc, char * argv[])
// {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<FollowerNode>());
//     rclcpp::shutdown();
//     return 0;
// }



#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <algorithm>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "skyhunter_msgs/msg/leader_state.hpp"
#include "tf2/utils.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

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
    
    // TUNING GAINS (Aggressive for tight following)
    this->declare_parameter<double>("k_x", 1.5); // Longitudinal (Forward/Back) Gain
    this->declare_parameter<double>("k_y", 2.0); // Lateral (Left/Right) Gain
    this->declare_parameter<double>("k_theta", 4.0); // Heading Gain
    
    this->declare_parameter<double>("min_safe_dist", 1.0); 
    this->declare_parameter<std::string>("leader_topic", "/robot1/leader_state");

    offset_x_ = this->get_parameter("offset_x").as_double();
    offset_y_ = this->get_parameter("offset_y").as_double();
    k_x_ = this->get_parameter("k_x").as_double();
    k_y_ = this->get_parameter("k_y").as_double();
    k_theta_ = this->get_parameter("k_theta").as_double();
    min_safe_dist_ = this->get_parameter("min_safe_dist").as_double();
    std::string leader_topic = this->get_parameter("leader_topic").as_string();

    // --- Communication ---
    // Best Effort QoS for lowest latency
    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    
    leader_sub_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
      leader_topic, qos, 
      std::bind(&FollowerNode::leader_callback, this, std::placeholders::_1));

    odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "odom", qos, 
      std::bind(&FollowerNode::odom_callback, this, std::placeholders::_1));

    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

    // 50Hz Control Loop (Faster update rate = Faster reaction)
    timer_ = this->create_wall_timer(
      20ms, std::bind(&FollowerNode::control_loop, this));

    RCLCPP_INFO(this->get_logger(), "High-Performance Follower Started.");
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

  void control_loop()
  {
    if (!has_leader_ || !has_odom_) return;

    // Safety Timeout (0.2s - instant stop if connection lags)
    if ((this->get_clock()->now() - last_leader_time_).seconds() > 0.2) {
        stop_robot(); 
        return;
    }

    // --- 1. Get States ---
    double leader_x = last_leader_msg_.pose.position.x;
    double leader_y = last_leader_msg_.pose.position.y;
    double leader_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
    double leader_v = last_leader_msg_.velocity.linear.x;
    double leader_w = last_leader_msg_.velocity.angular.z;

    double my_x = current_pose_.position.x;
    double my_y = current_pose_.position.y;
    double my_yaw = tf2::getYaw(current_pose_.orientation);

    // --- 2. Calculate Virtual Target (Global Frame) ---
    double target_x = leader_x + (offset_x_ * cos(leader_yaw) - offset_y_ * sin(leader_yaw));
    double target_y = leader_y + (offset_x_ * sin(leader_yaw) + offset_y_ * cos(leader_yaw));

    // --- 3. Safety Check ---
    double dist_to_physical_leader = std::hypot(leader_x - my_x, leader_y - my_y);
    if (dist_to_physical_leader < min_safe_dist_) {
        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Too Close! Emergency Brake.");
        stop_robot();
        return;
    }

    // --- 4. Error Calculation (Global Frame) ---
    double ex_global = target_x - my_x;
    double ey_global = target_y - my_y;
    double etheta_global = leader_yaw - my_yaw;

    // Normalize angle
    while (etheta_global > M_PI) etheta_global -= 2.0 * M_PI;
    while (etheta_global < -M_PI) etheta_global += 2.0 * M_PI;

    // --- 5. Transform Error to Follower's Local Frame (Crucial Step) ---
    // e_x_local: Error in front of me
    // e_y_local: Error to the left of me
    double ex_local = ex_global * cos(my_yaw) + ey_global * sin(my_yaw);
    double ey_local = -ex_global * sin(my_yaw) + ey_global * cos(my_yaw);

    // --- 6. Feed-Forward Control Law ---
    geometry_msgs::msg::Twist cmd;

    // LINEAR VELOCITY
    // Base: Leader's speed * cos(error) (If I'm facing wrong way, slow down base speed)
    // Plus: Correction for longitudinal error (catch up)
    cmd.linear.x = (leader_v * cos(etheta_global)) + (k_x_ * ex_local);

    // ANGULAR VELOCITY
    // Base: Leader's turning speed (Instant reaction to turns)
    // Plus: Correction for lateral error (I'm too far left/right)
    // Plus: Correction for angle error (I'm facing wrong way)
    cmd.angular.z = leader_w + (k_y_ * ey_local) + (k_theta_ * etheta_global);

    // --- 7. Speed Limits ---
    // Allow faster catch-up speed (max 2.0)
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

  double offset_x_, offset_y_;
  double k_x_, k_y_, k_theta_;
  double min_safe_dist_;

  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr leader_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
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