
// // FULL WORKABLE with BOIDS 02-23
// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <string>
// #include <algorithm>
// #include <vector>
// #include <omp.h> 

// #include "rclcpp/rclcpp.hpp"
// #include "geometry_msgs/msg/twist.hpp"
// #include "sensor_msgs/msg/point_cloud2.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include "tf2/utils.h"
// #include "tf2_ros/buffer.h"
// #include "tf2_ros/transform_listener.h"
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <geometry_msgs/msg/pose_array.hpp> 

// using namespace std::chrono_literals;

// class RobustFollower : public rclcpp::Node {
// public:
//   RobustFollower() : Node("follower_node") {
//     this->declare_parameter<double>("offset_dist", -2.5);
//     this->declare_parameter<double>("offset_lateral", 0.0);
//     this->declare_parameter<double>("ttc_danger_dist", 4.1);
//     this->declare_parameter<double>("blocking_radius", 0.8);
//     this->declare_parameter<double>("separation_dist", 1.5); // Boids Bubble

//     tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//     tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//     std::string ns = std::string(this->get_namespace());
//     if (ns.length() > 1 && ns[0] == '/') ns = ns.substr(1);
//     my_ns_ = ns;
//     my_frame_ = my_ns_ + "/base_footprint";

//     auto qos = rclcpp::SensorDataQoS();
//     sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
//         "/swarm/poses", qos, std::bind(&RobustFollower::swarm_cb, this, std::placeholders::_1));
//     sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//         "/leader_state", qos, std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));
//     sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//         "scan/points", qos, std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

//     pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
//     timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

//     RCLCPP_INFO(this->get_logger(), "BOIDS Follower [%s] Online.", my_ns_.c_str());
//   }

// private:
//   void swarm_cb(const geometry_msgs::msg::PoseArray::SharedPtr msg) { swarm_poses_ = *msg; has_swarm_ = true; }
//   void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) 
//   { 
//     last_leader_msg_ = *msg; 
//     has_leader_ = true; 
//     last_leader_time_ = this->get_clock()->now(); 
//   }


//   bool is_point_blocked(double map_x, double map_y) {
//     if (obstacle_points_.empty()) return false;
//     geometry_msgs::msg::PointStamped p_map, p_local;
//     p_map.header.frame_id = "map"; p_map.point.x = map_x; p_map.point.y = map_y;
//     try { auto tf = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero); tf2::doTransform(p_map, p_local, tf); } catch (...) { return false; }
//     double r_sq = std::pow(this->get_parameter("blocking_radius").as_double(), 2);
//     for (const auto& obs : obstacle_points_) {
//         double dx = obs.x - p_local.point.x; double dy = obs.y - p_local.point.y;
//         if ((dx*dx + dy*dy) < r_sq) return true; 
//     }
//     return false;
//   }

//   void scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
//     pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//     pcl::fromROSMsg(*msg, *raw_cloud);
//     if (raw_cloud->empty() || !has_leader_) return;
//     geometry_msgs::msg::TransformStamped tf_l;
//     try { tf_l = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero); } catch (...) { return; }
//     double lx_l = last_leader_msg_.pose.position.x + tf_l.transform.translation.x;
//     double ly_l = last_leader_msg_.pose.position.y + tf_l.transform.translation.y;
//     std::vector<pcl::PointXYZ> obs;
//     #pragma omp parallel
//     {
//         std::vector<pcl::PointXYZ> t_pts;
//         #pragma omp for nowait
//         for (size_t i = 0; i < raw_cloud->size(); i += 10) {
//             const auto& p = raw_cloud->points[i];
//             if (p.z < -0.3 || p.z > 0.5) continue;
//             if ((p.x*p.x + p.y*p.y) < 0.25) continue; 
//             if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue; 
//             t_pts.push_back(p);
//         }
//         #pragma omp critical
//         obs.insert(obs.end(), t_pts.begin(), t_pts.end());
//     }
//     obstacle_points_ = obs;
//     has_scan_ = true;
//   }

//   void control_loop() {
//     if (!has_leader_ || !has_scan_) return;
//     if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) { stop_robot(); return; }

//     geometry_msgs::msg::TransformStamped tf_now;
//     try { tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero); } catch (...) { return; }
//     double my_x = tf_now.transform.translation.x;
//     double my_y = tf_now.transform.translation.y;
//     double my_yaw = tf2::getYaw(tf_now.transform.rotation);

//     double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//     double off_back = this->get_parameter("offset_dist").as_double();
//     double off_side = this->get_parameter("offset_lateral").as_double();
//     double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
//     double current_off_side = (last_leader_msg_.formation_type == 1) ? 0.0 : off_side;

//     // BOIDS: SEPARATION MATH
//     double repulse_x = 0.0, repulse_y = 0.0;
//     double min_teammate_dist = 10.0;
//     double sep_limit = this->get_parameter("separation_dist").as_double();
//     if (has_swarm_) {
//         for (const auto& other_pose : swarm_poses_.poses) {
//             double dx = other_pose.position.x - my_x;
//             double dy = other_pose.position.y - my_y;
//             double d = std::hypot(dx, dy);
//             if (d < 0.1) continue; 
//             if (d < min_teammate_dist) min_teammate_dist = d;
//             if (d < sep_limit) {
//                 double force = (sep_limit - d) / d;
//                 repulse_x -= dx * force; repulse_y -= dy * force;
//             }
//         }
//     }

//     double target_x, target_y;
//     if (last_leader_msg_.next_waypoints.size() >= 1) {
//         auto wp = last_leader_msg_.next_waypoints[0].position;
//         target_x = wp.x - (current_off_side * std::sin(l_yaw)) + repulse_x;
//         target_y = wp.y + (current_off_side * std::cos(l_yaw)) + repulse_y;
//     } else {
//         target_x = last_leader_msg_.pose.position.x + (off_back * std::cos(l_yaw)) - (current_off_side * std::sin(l_yaw)) + repulse_x;
//         target_y = last_leader_msg_.pose.position.y + (off_back * std::sin(l_yaw)) + (current_off_side * std::cos(l_yaw)) + repulse_y;
//     }

//     double dx_err = target_x - my_x; double dy_err = target_y - my_y;
//     double dist_err = std::hypot(dx_err, dy_err);
//     double angle_to_target = std::atan2(dy_err, dx_err);

//     double min_front_dist = 10.0;
//     for (const auto& p : obstacle_points_) {
//         float angle = std::atan2(p.y, p.x);
//         if (std::abs(angle) < 0.7) { double d = std::hypot(p.x, p.y); if (d < min_front_dist) min_front_dist = d; }
//     }
//     double ttc_scale = (min_front_dist < 4.1) ? std::max(0.2, min_front_dist / 4.1) : 1.0;
//     if (min_front_dist < 1.0 || min_teammate_dist < 0.8) ttc_scale = 0.0; 

//     double best_yaw = my_yaw; double min_score = 9999.0;
//     for (double angle = -1.57; angle <= 1.57; angle += 0.15) {
//         double check_yaw = my_yaw + angle;
//         double diff = check_yaw - angle_to_target;
//         while(diff > M_PI) diff -= 2*M_PI; 
//         while(diff < -M_PI) diff += 2*M_PI;
//         bool collision = false;
//         for (const auto& p : obstacle_points_) {
//             double px_r = p.x * cos(-angle) - p.y * sin(-angle);
//             double py_r = p.x * sin(-angle) + p.y * cos(-angle);
//             if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) { collision = true; break; }
//         }
//         if (!collision && std::abs(diff) < min_score) { min_score = std::abs(diff); best_yaw = check_yaw; }
//     }

//     geometry_msgs::msg::Twist cmd;
//     if (dist_err < 0.6 || (leader_speed < 0.05 && dist_err < 1.0)) { stop_robot(); } 
//     else {
//         double steer = best_yaw - my_yaw;
//         while(steer > M_PI) steer -= 2*M_PI; 
//         while(steer < -M_PI) steer += 2*M_PI;
//         cmd.linear.x = std::min(1.1, leader_speed + (0.25 * dist_err)) * ttc_scale;
//         cmd.angular.z = 1.8 * steer;
//         if (std::abs(steer) > 0.8) cmd.linear.x = 0.05; 
//     }
//     pub_cmd_->publish(cmd);

//     RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
//     "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s", 
//     (current_off_side == 0.0 ? "Column" : "V-Shape"), cmd.linear.x, min_teammate_dist, (ttc_scale == 0.0 ? "HALT" : "OK"));
//   }

//   void stop_robot() { pub_cmd_->publish(geometry_msgs::msg::Twist()); }

//   // --- MEMBERS ---
//   std::vector<geometry_msgs::msg::Pose> shadow_mission_buffer_;

//   std::string my_ns_, my_frame_;
//   skyhunter_msgs::msg::LeaderState last_leader_msg_;
//   std::vector<pcl::PointXYZ> obstacle_points_;
//   rclcpp::Time last_leader_time_;
//   bool has_leader_ = false, has_scan_ = false, has_swarm_ = false;
//   geometry_msgs::msg::PoseArray swarm_poses_;

//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
//   rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
//   rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr sub_swarm_;
//   rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
//   rclcpp::TimerBase::SharedPtr timer_;
//   std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
//   std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
// };

// int main(int argc, char * argv[]) { rclcpp::init(argc, argv); rclcpp::spin(std::make_shared<RobustFollower>()); rclcpp::shutdown(); return 0; }






// 02-27 - FULLY workable with leader chagnge+BOIDS+TTC+ROLE AWARENESS
// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <string>
// #include <algorithm>
// #include <vector>
// #include <omp.h> 

// #include "rclcpp/rclcpp.hpp"
// #include "geometry_msgs/msg/twist.hpp"
// #include "sensor_msgs/msg/point_cloud2.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include "tf2/utils.h"
// #include "tf2_ros/buffer.h"
// #include "tf2_ros/transform_listener.h"
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <geometry_msgs/msg/pose_array.hpp> 
// #include "std_msgs/msg/int8.hpp"

// using namespace std::chrono_literals;

// class RobustFollower : public rclcpp::Node {
// public:
//   RobustFollower() : Node("follower_node") {
//     this->declare_parameter<double>("offset_dist", -2.5);
//     this->declare_parameter<double>("offset_lateral", 0.0);
//     this->declare_parameter<double>("ttc_danger_dist", 4.1);
//     this->declare_parameter<double>("blocking_radius", 0.8);
//     this->declare_parameter<double>("separation_dist", 1.5); // Boids Bubble

//     tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//     tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//     std::string ns = std::string(this->get_namespace());
//     if (ns.length() > 1 && ns[0] == '/') ns = ns.substr(1);
//     my_ns_ = ns;
//     my_frame_ = my_ns_ + "/base_footprint";

//     auto qos = rclcpp::SensorDataQoS();
//     sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
//         "/swarm/poses", qos, std::bind(&RobustFollower::swarm_cb, this, std::placeholders::_1));

//     sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//     "/swarm/virtual_leader/state", qos, std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));

//     sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//         "scan/points", qos, std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

//     sub_role_ = this->create_subscription<std_msgs::msg::Int8>(
//     "local_role", 10, [this](const std_msgs::msg::Int8::SharedPtr msg) {
//         this->current_local_role_ = msg->data;
//     });

//     pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
//     timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

//     RCLCPP_INFO(this->get_logger(), "BOIDS Follower [%s] Online.", my_ns_.c_str());
//   }

// private:
//   void swarm_cb(const geometry_msgs::msg::PoseArray::SharedPtr msg) { swarm_poses_ = *msg; has_swarm_ = true; }
//   void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) 
//   { 
//     last_leader_msg_ = *msg; 
//     has_leader_ = true; 
//     last_leader_time_ = this->get_clock()->now(); 
//   }


//   bool is_point_blocked(double map_x, double map_y) {
//     if (obstacle_points_.empty()) return false;
//     geometry_msgs::msg::PointStamped p_map, p_local;
//     p_map.header.frame_id = "map"; p_map.point.x = map_x; p_map.point.y = map_y;
//     try { auto tf = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero); tf2::doTransform(p_map, p_local, tf); } catch (...) { return false; }
//     double r_sq = std::pow(this->get_parameter("blocking_radius").as_double(), 2);
//     for (const auto& obs : obstacle_points_) {
//         double dx = obs.x - p_local.point.x; double dy = obs.y - p_local.point.y;
//         if ((dx*dx + dy*dy) < r_sq) return true; 
//     }
//     return false;
//   }

//   void scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
//     pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//     pcl::fromROSMsg(*msg, *raw_cloud);
//     if (raw_cloud->empty() || !has_leader_) return;
//     geometry_msgs::msg::TransformStamped tf_l;
//     try { tf_l = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero); } catch (...) { return; }
//     double lx_l = last_leader_msg_.pose.position.x + tf_l.transform.translation.x;
//     double ly_l = last_leader_msg_.pose.position.y + tf_l.transform.translation.y;
//     std::vector<pcl::PointXYZ> obs;
//     #pragma omp parallel
//     {
//         std::vector<pcl::PointXYZ> t_pts;
//         #pragma omp for nowait
//         for (size_t i = 0; i < raw_cloud->size(); i += 10) {
//             const auto& p = raw_cloud->points[i];
//             if (p.z < -0.3 || p.z > 0.5) continue;
//             if ((p.x*p.x + p.y*p.y) < 0.25) continue; 
//             if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue; 
//             t_pts.push_back(p);
//         }
//         #pragma omp critical
//         obs.insert(obs.end(), t_pts.begin(), t_pts.end());
//     }
//     obstacle_points_ = obs;
//     has_scan_ = true;
//   }

//   void control_loop() {
//     if (!has_leader_ || !has_scan_) return;
//     if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) { stop_robot(); return; }

//     if (current_local_role_ == 2) { // 2 = LEADER role
//         // I am the leader now. I must stop following and let my LeaderNode take over.
//         // stop_robot(); 
//         return; 
//     }

//     geometry_msgs::msg::TransformStamped tf_now;
//     try { tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero); } catch (...) { return; }
//     double my_x = tf_now.transform.translation.x;
//     double my_y = tf_now.transform.translation.y;
//     double my_yaw = tf2::getYaw(tf_now.transform.rotation);

//     double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//     double off_back = this->get_parameter("offset_dist").as_double();
//     double off_side = this->get_parameter("offset_lateral").as_double();
//     double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
//     double current_off_side = (last_leader_msg_.formation_type == 1) ? 0.0 : off_side;

//     // BOIDS: SEPARATION MATH
//     double repulse_x = 0.0, repulse_y = 0.0;
//     double min_teammate_dist = 10.0;
//     double sep_limit = this->get_parameter("separation_dist").as_double();
//     if (has_swarm_) {
//         for (const auto& other_pose : swarm_poses_.poses) {
//             double dx = other_pose.position.x - my_x;
//             double dy = other_pose.position.y - my_y;
//             double d = std::hypot(dx, dy);
//             if (d < 0.1) continue; 
//             if (d < min_teammate_dist) min_teammate_dist = d;
//             if (d < sep_limit) {
//                 double force = (sep_limit - d) / d;
//                 repulse_x -= dx * force; repulse_y -= dy * force;
//             }
//         }
//     }

//     double target_x, target_y;
//     if (last_leader_msg_.next_waypoints.size() >= 1) {
//         auto wp = last_leader_msg_.next_waypoints[0].position;
//         target_x = wp.x - (current_off_side * std::sin(l_yaw)) + repulse_x;
//         target_y = wp.y + (current_off_side * std::cos(l_yaw)) + repulse_y;
//     } else {
//         target_x = last_leader_msg_.pose.position.x + (off_back * std::cos(l_yaw)) - (current_off_side * std::sin(l_yaw)) + repulse_x;
//         target_y = last_leader_msg_.pose.position.y + (off_back * std::sin(l_yaw)) + (current_off_side * std::cos(l_yaw)) + repulse_y;
//     }

//     double dx_err = target_x - my_x; double dy_err = target_y - my_y;
//     double dist_err = std::hypot(dx_err, dy_err);
//     double angle_to_target = std::atan2(dy_err, dx_err);

//     double min_front_dist = 10.0;
//     for (const auto& p : obstacle_points_) {
//         float angle = std::atan2(p.y, p.x);
//         if (std::abs(angle) < 0.7) { double d = std::hypot(p.x, p.y); if (d < min_front_dist) min_front_dist = d; }
//     }
//     double ttc_scale = (min_front_dist < 4.1) ? std::max(0.2, min_front_dist / 4.1) : 1.0;
//     if (min_front_dist < 1.0 || min_teammate_dist < 0.8) ttc_scale = 0.0; 

//     double best_yaw = my_yaw; double min_score = 9999.0;
//     for (double angle = -1.57; angle <= 1.57; angle += 0.15) {
//         double check_yaw = my_yaw + angle;
//         double diff = check_yaw - angle_to_target;
//         while(diff > M_PI) diff -= 2*M_PI; 
//         while(diff < -M_PI) diff += 2*M_PI;
//         bool collision = false;
//         for (const auto& p : obstacle_points_) {
//             double px_r = p.x * cos(-angle) - p.y * sin(-angle);
//             double py_r = p.x * sin(-angle) + p.y * cos(-angle);
//             if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) { collision = true; break; }
//         }
//         if (!collision && std::abs(diff) < min_score) { min_score = std::abs(diff); best_yaw = check_yaw; }
//     }

//     geometry_msgs::msg::Twist cmd;
//     if (dist_err < 0.6 || (leader_speed < 0.05 && dist_err < 1.0)) { stop_robot(); } 
//     else {
//         double steer = best_yaw - my_yaw;
//         while(steer > M_PI) steer -= 2*M_PI; 
//         while(steer < -M_PI) steer += 2*M_PI;
//         cmd.linear.x = std::min(1.1, leader_speed + (0.25 * dist_err)) * ttc_scale;
//         cmd.angular.z = 1.8 * steer;
//         if (std::abs(steer) > 0.8) cmd.linear.x = 0.05; 
//     }
//     pub_cmd_->publish(cmd);

//     RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
//     "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s", 
//     (current_off_side == 0.0 ? "Column" : "V-Shape"), cmd.linear.x, min_teammate_dist, (ttc_scale == 0.0 ? "HALT" : "OK"));
//   }

//   void stop_robot() { pub_cmd_->publish(geometry_msgs::msg::Twist()); }

//   // --- MEMBERS ---
//   std::vector<geometry_msgs::msg::Pose> shadow_mission_buffer_;

//   std::string my_ns_, my_frame_;
//   skyhunter_msgs::msg::LeaderState last_leader_msg_;
//   std::vector<pcl::PointXYZ> obstacle_points_;
//   rclcpp::Time last_leader_time_;
//   bool has_leader_ = false, has_scan_ = false, has_swarm_ = false;
//   geometry_msgs::msg::PoseArray swarm_poses_;

//   int8_t current_local_role_ = 0;
//   rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_role_;

//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
//   rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
//   rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr sub_swarm_;
//   rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
//   rclcpp::TimerBase::SharedPtr timer_;
//   std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
//   std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
// };

// int main(int argc, char * argv[]) { rclcpp::init(argc, argv); rclcpp::spin(std::make_shared<RobustFollower>()); rclcpp::shutdown(); return 0; }






// 03_09 - with tracking and follower getting detection command
// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <string>
// #include <algorithm>
// #include <vector>
// #include <omp.h> 

// #include "rclcpp/rclcpp.hpp"
// #include "geometry_msgs/msg/twist.hpp"
// #include "sensor_msgs/msg/point_cloud2.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include "tf2/utils.h"
// #include "tf2_ros/buffer.h"
// #include "tf2_ros/transform_listener.h"
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <geometry_msgs/msg/pose_array.hpp> 
// #include "std_msgs/msg/int8.hpp"
// #include <std_msgs/msg/float64.hpp>
// #include <cmath>
// #include "std_msgs/msg/float64.hpp"

// using namespace std::chrono_literals;

// class RobustFollower : public rclcpp::Node {
// public:
//   RobustFollower() : Node("follower_node") {
//     this->declare_parameter<double>("offset_dist", -2.5);
//     this->declare_parameter<double>("offset_lateral", 0.0);
//     this->declare_parameter<double>("ttc_danger_dist", 4.1);
//     this->declare_parameter<double>("blocking_radius", 0.8);
//     this->declare_parameter<double>("separation_dist", 1.5); // Boids Bubble
    

//     tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//     tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//     std::string ns = std::string(this->get_namespace());
//     if (ns.length() > 1 && ns[0] == '/') ns = ns.substr(1);
//     my_ns_ = ns;
//     my_frame_ = my_ns_ + "/base_footprint";

//     auto qos = rclcpp::SensorDataQoS();
//     sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
//         "/swarm/poses", qos, std::bind(&RobustFollower::swarm_cb, this, std::placeholders::_1));

//     sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//     "/swarm/virtual_leader/state", qos, std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));

//     sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//         "scan/points", qos, std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

//     sub_role_ = this->create_subscription<std_msgs::msg::Int8>(
//     "local_role", 10, [this](const std_msgs::msg::Int8::SharedPtr msg) {
//         this->current_local_role_ = msg->data;
//     });

//     pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
//     pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);

//     pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
//     timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

//     RCLCPP_INFO(this->get_logger(), "BOIDS Follower [%s] Online.", my_ns_.c_str());
//   }

// private:
//   void swarm_cb(const geometry_msgs::msg::PoseArray::SharedPtr msg) { swarm_poses_ = *msg; has_swarm_ = true; }
//   void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) 
//   { 
//     last_leader_msg_ = *msg; 
//     has_leader_ = true; 
//     last_leader_time_ = this->get_clock()->now(); 
//   }


//   bool is_point_blocked(double map_x, double map_y) {
//     if (obstacle_points_.empty()) return false;
//     geometry_msgs::msg::PointStamped p_map, p_local;
//     p_map.header.frame_id = "map"; p_map.point.x = map_x; p_map.point.y = map_y;
//     try { auto tf = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero); tf2::doTransform(p_map, p_local, tf); } catch (...) { return false; }
//     double r_sq = std::pow(this->get_parameter("blocking_radius").as_double(), 2);
//     for (const auto& obs : obstacle_points_) {
//         double dx = obs.x - p_local.point.x; double dy = obs.y - p_local.point.y;
//         if ((dx*dx + dy*dy) < r_sq) return true; 
//     }
//     return false;
//   }

//   void scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
//     pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//     pcl::fromROSMsg(*msg, *raw_cloud);
//     if (raw_cloud->empty() || !has_leader_) return;
//     geometry_msgs::msg::TransformStamped tf_l;
//     try { tf_l = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero); } catch (...) { return; }
//     double lx_l = last_leader_msg_.pose.position.x + tf_l.transform.translation.x;
//     double ly_l = last_leader_msg_.pose.position.y + tf_l.transform.translation.y;
//     std::vector<pcl::PointXYZ> obs;
//     #pragma omp parallel
//     {
//         std::vector<pcl::PointXYZ> t_pts;
//         #pragma omp for nowait
//         for (size_t i = 0; i < raw_cloud->size(); i += 10) {
//             const auto& p = raw_cloud->points[i];
//             if (p.z < -0.3 || p.z > 0.5) continue;
//             if ((p.x*p.x + p.y*p.y) < 0.25) continue; 
//             if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue; 
//             t_pts.push_back(p);
//         }
//         #pragma omp critical
//         obs.insert(obs.end(), t_pts.begin(), t_pts.end());
//     }
//     obstacle_points_ = obs;
//     has_scan_ = true;
//   }

//   void control_loop() {
//     if (!has_leader_ || !has_scan_) return;

//     if (last_leader_msg_.swarm_state == 3 && last_leader_msg_.target_locked) {
//         geometry_msgs::msg::Twist stop_cmd;
//         pub_cmd_->publish(stop_cmd); // Freeze the robot
//         RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "COMBAT: Halting tracks for Fire Net.");
//         return; // Skip normal V-formation driving
//     }

//     if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) { stop_robot(); return; }

//     if (current_local_role_ == 2) { // 2 = LEADER role
//         return; 
//     }

//     geometry_msgs::msg::TransformStamped tf_now;
//     try { tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero); } catch (...) { return; }
//     double my_x = tf_now.transform.translation.x;
//     double my_y = tf_now.transform.translation.y;
//     double my_yaw = tf2::getYaw(tf_now.transform.rotation);

//     double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//     double off_back = this->get_parameter("offset_dist").as_double();
//     double off_side = this->get_parameter("offset_lateral").as_double();
//     double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
//     double current_off_side = (last_leader_msg_.formation_type == 1) ? 0.0 : off_side;

//     // BOIDS: SEPARATION MATH
//     double repulse_x = 0.0, repulse_y = 0.0;
//     double min_teammate_dist = 10.0;
//     double sep_limit = this->get_parameter("separation_dist").as_double();
//     if (has_swarm_) {
//         for (const auto& other_pose : swarm_poses_.poses) {
//             double dx = other_pose.position.x - my_x;
//             double dy = other_pose.position.y - my_y;
//             double d = std::hypot(dx, dy);
//             if (d < 0.1) continue; 
//             if (d < min_teammate_dist) min_teammate_dist = d;
//             if (d < sep_limit) {
//                 double force = (sep_limit - d) / d;
//                 repulse_x -= dx * force; repulse_y -= dy * force;
//             }
//         }
//     }

//     double target_x, target_y;
//     if (last_leader_msg_.next_waypoints.size() >= 1) {
//         auto wp = last_leader_msg_.next_waypoints[0].position;
//         target_x = wp.x - (current_off_side * std::sin(l_yaw)) + repulse_x;
//         target_y = wp.y + (current_off_side * std::cos(l_yaw)) + repulse_y;
//     } else {
//         target_x = last_leader_msg_.pose.position.x + (off_back * std::cos(l_yaw)) - (current_off_side * std::sin(l_yaw)) + repulse_x;
//         target_y = last_leader_msg_.pose.position.y + (off_back * std::sin(l_yaw)) + (current_off_side * std::cos(l_yaw)) + repulse_y;
//     }

//     double dx_err = target_x - my_x; double dy_err = target_y - my_y;
//     double dist_err = std::hypot(dx_err, dy_err);
//     double angle_to_target = std::atan2(dy_err, dx_err);

//     double min_front_dist = 10.0;
//     for (const auto& p : obstacle_points_) {
//         float angle = std::atan2(p.y, p.x);
//         if (std::abs(angle) < 0.7) { double d = std::hypot(p.x, p.y); if (d < min_front_dist) min_front_dist = d; }
//     }
//     double ttc_scale = (min_front_dist < 4.1) ? std::max(0.2, min_front_dist / 4.1) : 1.0;
//     if (min_front_dist < 1.0 || min_teammate_dist < 0.8) ttc_scale = 0.0; 

//     double best_yaw = my_yaw; double min_score = 9999.0;
//     for (double angle = -1.57; angle <= 1.57; angle += 0.15) {
//         double check_yaw = my_yaw + angle;
//         double diff = check_yaw - angle_to_target;
//         while(diff > M_PI) diff -= 2*M_PI; 
//         while(diff < -M_PI) diff += 2*M_PI;
//         bool collision = false;
//         for (const auto& p : obstacle_points_) {
//             double px_r = p.x * cos(-angle) - p.y * sin(-angle);
//             double py_r = p.x * sin(-angle) + p.y * cos(-angle);
//             if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) { collision = true; break; }
//         }
//         if (!collision && std::abs(diff) < min_score) { min_score = std::abs(diff); best_yaw = check_yaw; }
//     }

//     geometry_msgs::msg::Twist cmd;
//     if (dist_err < 0.6 || (leader_speed < 0.05 && dist_err < 1.0)) { stop_robot(); } 
//     else {
//         double steer = best_yaw - my_yaw;
//         while(steer > M_PI) steer -= 2*M_PI; 
//         while(steer < -M_PI) steer += 2*M_PI;
//         cmd.linear.x = std::min(1.1, leader_speed + (0.25 * dist_err)) * ttc_scale;
//         cmd.angular.z = 1.8 * steer;
//         if (std::abs(steer) > 0.8) cmd.linear.x = 0.05; 
//     }
//     pub_cmd_->publish(cmd);

//     RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
//     "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s", 
//     (current_off_side == 0.0 ? "Column" : "V-Shape"), cmd.linear.x, min_teammate_dist, (ttc_scale == 0.0 ? "HALT" : "OK"));
//   }

//   void stop_robot() { pub_cmd_->publish(geometry_msgs::msg::Twist()); }

//   // --- MEMBERS ---
//   std::vector<geometry_msgs::msg::Pose> shadow_mission_buffer_;

//   std::string my_ns_, my_frame_;
//   skyhunter_msgs::msg::LeaderState last_leader_msg_;
//   std::vector<pcl::PointXYZ> obstacle_points_;
//   rclcpp::Time last_leader_time_;
//   bool has_leader_ = false, has_scan_ = false, has_swarm_ = false;
//   geometry_msgs::msg::PoseArray swarm_poses_;

//   int8_t current_local_role_ = 0;
//   rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_role_;

//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
//   rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
//   rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr sub_swarm_;
//   rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
//   rclcpp::TimerBase::SharedPtr timer_;
  
//   // --- ADD THESE TWO LINES ---
//   rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_pan_;
//   rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_tilt_;
  

//   std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
//   std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
// };

// int main(int argc, char * argv[]) { rclcpp::init(argc, argv); rclcpp::spin(std::make_shared<RobustFollower>()); rclcpp::shutdown(); return 0; }




// #include "skyhunter_control/follower_node.hpp"

// #include <algorithm>
// #include <cmath>

// using namespace std::chrono_literals;

// RobustFollower::RobustFollower(const rclcpp::NodeOptions & options)
// : Node("follower_node", options)
// {
//   // Parameters
//   this->declare_parameter<double>("offset_dist", -2.5);
//   this->declare_parameter<double>("offset_lateral", 0.0);
//   this->declare_parameter<double>("ttc_danger_dist", 4.1);
//   this->declare_parameter<double>("blocking_radius", 0.8);
//   this->declare_parameter<double>("separation_dist", 1.5);

//   offset_dist_      = this->get_parameter("offset_dist").as_double();
//   offset_lateral_   = this->get_parameter("offset_lateral").as_double();
//   ttc_danger_dist_  = this->get_parameter("ttc_danger_dist").as_double();
//   blocking_radius_  = this->get_parameter("blocking_radius").as_double();
//   separation_dist_  = this->get_parameter("separation_dist").as_double();

//   // TF
//   tf_buffer_   = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//   tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//   // Namespace handling
//   std::string ns = this->get_namespace();
//   if (ns.length() > 1 && ns[0] == '/') ns = ns.substr(1);
//   my_ns_ = ns;
//   my_frame_ = my_ns_ + "/base_footprint";

//   // QoS
//   auto qos = rclcpp::SensorDataQoS();

//   // Subscribers
//   sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
//     "/swarm/poses", qos,
//     std::bind(&RobustFollower::swarm_cb, this, std::placeholders::_1));

//   sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//     "/swarm/virtual_leader/state", qos,
//     std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));

//   sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//     "scan/points", qos,
//     std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

//   sub_role_ = this->create_subscription<std_msgs::msg::Int8>(
//     "local_role", 10,
//     std::bind(&RobustFollower::role_cb, this, std::placeholders::_1));

//   // Publishers
//   pub_pan_  = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
//   pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
//   pub_cmd_  = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

//   // Timer ~ 20 Hz
//   timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

//   RCLCPP_INFO(this->get_logger(), "BOIDS Follower [%s] Online.", my_ns_.c_str());
// }

// void RobustFollower::swarm_cb(const geometry_msgs::msg::PoseArray::SharedPtr msg)
// {
//   swarm_poses_ = *msg;
//   has_swarm_ = true;
// }

// void RobustFollower::leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg)
// {
//   last_leader_msg_ = *msg;
//   has_leader_ = true;
//   last_leader_time_ = this->get_clock()->now();
// }

// void RobustFollower::role_cb(const std_msgs::msg::Int8::SharedPtr msg)
// {
//   current_local_role_ = msg->data;
// }

// bool RobustFollower::is_point_blocked(double map_x, double map_y)
// {
//   if (obstacle_points_.empty()) return false;

//   geometry_msgs::msg::PointStamped p_map, p_local;
//   p_map.header.frame_id = "map";
//   p_map.point.x = map_x;
//   p_map.point.y = map_y;

//   try {
//     auto tf = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
//     tf2::doTransform(p_map, p_local, tf);
//   } catch (...) {
//     return false;
//   }

//   double r_sq = std::pow(blocking_radius_, 2);
//   for (const auto& obs : obstacle_points_) {
//     double dx = obs.x - p_local.point.x;
//     double dy = obs.y - p_local.point.y;
//     if ((dx * dx + dy * dy) < r_sq) {
//       return true;
//     }
//   }
//   return false;
// }

// void RobustFollower::scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
// {
//   pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//   pcl::fromROSMsg(*msg, *raw_cloud);

//   if (raw_cloud->empty() || !has_leader_) return;

//   geometry_msgs::msg::TransformStamped tf_l;
//   try {
//     tf_l = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
//   } catch (...) {
//     return;
//   }

//   double lx_l = last_leader_msg_.pose.position.x + tf_l.transform.translation.x;
//   double ly_l = last_leader_msg_.pose.position.y + tf_l.transform.translation.y;

//   std::vector<pcl::PointXYZ> obs;

// #pragma omp parallel
//   {
//     std::vector<pcl::PointXYZ> t_pts;

// #pragma omp for nowait
//     for (size_t i = 0; i < raw_cloud->size(); i += 10) {
//       const auto& p = raw_cloud->points[i];
//       if (p.z < -0.3 || p.z > 0.5) continue;
//       if ((p.x * p.x + p.y * p.y) < 0.25) continue;
//       if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue;
//       t_pts.push_back(p);
//     }

// #pragma omp critical
//     obs.insert(obs.end(), t_pts.begin(), t_pts.end());
//   }

//   obstacle_points_ = std::move(obs);
//   has_scan_ = true;
// }

// void RobustFollower::control_loop()
// {
//   if (!has_leader_ || !has_scan_) return;

//   if (last_leader_msg_.swarm_state == 3 && last_leader_msg_.target_locked) {
//     geometry_msgs::msg::Twist stop_cmd;
//     pub_cmd_->publish(stop_cmd);
//     RCLCPP_INFO_THROTTLE(
//       this->get_logger(), *this->get_clock(), 1000,
//       "COMBAT: Halting tracks for Fire Net.");
//     return;
//   }

//   if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) {
//     stop_robot();
//     return;
//   }

//   if (current_local_role_ == 2) {  // LEADER
//     return;
//   }

//   geometry_msgs::msg::TransformStamped tf_now;
//   try {
//     tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero);
//   } catch (...) {
//     return;
//   }

//   double my_x = tf_now.transform.translation.x;
//   double my_y = tf_now.transform.translation.y;
//   double my_yaw = tf2::getYaw(tf_now.transform.rotation);

//   double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//   double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
//   double current_off_side = (last_leader_msg_.formation_type == 1) ? 0.0 : offset_lateral_;

//   // ─────────────── BOIDS SEPARATION ───────────────
//   double repulse_x = 0.0, repulse_y = 0.0;
//   double min_teammate_dist = 10.0;

//   if (has_swarm_) {
//     for (const auto& other_pose : swarm_poses_.poses) {
//       double dx = other_pose.position.x - my_x;
//       double dy = other_pose.position.y - my_y;
//       double d = std::hypot(dx, dy);
//       if (d < 0.1) continue;
//       min_teammate_dist = std::min(min_teammate_dist, d);

//       if (d < separation_dist_) {
//         double force = (separation_dist_ - d) / d;
//         repulse_x -= dx * force;
//         repulse_y -= dy * force;
//       }
//     }
//   }

//   // ─────────────── TARGET POSITION ───────────────
//   double target_x, target_y;

//   if (!last_leader_msg_.next_waypoints.empty()) {
//     auto wp = last_leader_msg_.next_waypoints[0].position;
//     target_x = wp.x - (current_off_side * std::sin(l_yaw)) + repulse_x;
//     target_y = wp.y + (current_off_side * std::cos(l_yaw)) + repulse_y;
//   } else {
//     target_x = last_leader_msg_.pose.position.x +
//                (offset_dist_ * std::cos(l_yaw)) -
//                (current_off_side * std::sin(l_yaw)) + repulse_x;

//     target_y = last_leader_msg_.pose.position.y +
//                (offset_dist_ * std::sin(l_yaw)) +
//                (current_off_side * std::cos(l_yaw)) + repulse_y;
//   }

//   double dx_err = target_x - my_x;
//   double dy_err = target_y - my_y;
//   double dist_err = std::hypot(dx_err, dy_err);
//   double angle_to_target = std::atan2(dy_err, dx_err);

//   // ─────────────── FRONT CLEARANCE CHECK ───────────────
//   double min_front_dist = 10.0;
//   for (const auto& p : obstacle_points_) {
//     float angle = std::atan2(p.y, p.x);
//     if (std::abs(angle) < 0.7) {
//       double d = std::hypot(p.x, p.y);
//       min_front_dist = std::min(min_front_dist, d);
//     }
//   }

//   double ttc_scale = (min_front_dist < ttc_danger_dist_)
//                        ? std::max(0.2, min_front_dist / ttc_danger_dist_)
//                        : 1.0;

//   if (min_front_dist < 1.0 || min_teammate_dist < 0.8) {
//     ttc_scale = 0.0;
//   }

//   // ─────────────── LOCAL PATH SAMPLING (simple) ───────────────
//   double best_yaw = my_yaw;
//   double min_score = 9999.0;

//   for (double angle = -M_PI/2; angle <= M_PI/2; angle += 0.15) {
//     double check_yaw = my_yaw + angle;
//     double diff = tf2NormalizeAngle(check_yaw - angle_to_target);

//     bool collision = false;
//     for (const auto& p : obstacle_points_) {
//       double px_r = p.x * cos(-angle) - p.y * sin(-angle);
//       double py_r = p.x * sin(-angle) + p.y * cos(-angle);
//       if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) {
//         collision = true;
//         break;
//       }
//     }

//     if (!collision && std::abs(diff) < min_score) {
//       min_score = std::abs(diff);
//       best_yaw = check_yaw;
//     }
//   }

//   // ─────────────── COMMAND GENERATION ───────────────
//   geometry_msgs::msg::Twist cmd;

//   if (dist_err < 0.6 || (leader_speed < 0.05 && dist_err < 1.0)) {
//     stop_robot();
//   } else {
//     double steer = tf2NormalizeAngle(best_yaw - my_yaw);

//     cmd.linear.x = std::min(1.1, leader_speed + 0.25 * dist_err) * ttc_scale;
//     cmd.angular.z = 1.8 * steer;

//     if (std::abs(steer) > 0.8) {
//       cmd.linear.x = 0.05;
//     }

//     pub_cmd_->publish(cmd);
//   }

//   RCLCPP_INFO_THROTTLE(
//     this->get_logger(), *this->get_clock(), 1000,
//     "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s",
//     (current_off_side == 0.0 ? "Column" : "V-Shape"),
//     cmd.linear.x, min_teammate_dist,
//     (ttc_scale == 0.0 ? "HALT" : "OK"));
// }

// void RobustFollower::stop_robot()
// {
//   pub_cmd_->publish(geometry_msgs::msg::Twist());
// }

// // ────────────────────────────────────────────────
// //                   MAIN
// // ────────────────────────────────────────────────

// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<RobustFollower>());
//   rclcpp::shutdown();
//   return 0;
// }



// UPDATED WORKABLE 03-11 wITH DIAMON and wedge
#define _USE_MATH_DEFINES
#include "skyhunter_control/follower_node.hpp"

#include <algorithm>
#include <cmath>

using namespace std::chrono_literals;

RobustFollower::RobustFollower(const rclcpp::NodeOptions & options)
: Node("follower_node", options)
{
  // Parameters
  this->declare_parameter<double>("offset_dist", -2.5);
  this->declare_parameter<double>("offset_lateral", 0.0);
  this->declare_parameter<double>("ttc_danger_dist", 4.1);
  this->declare_parameter<double>("blocking_radius", 0.8);
  this->declare_parameter<double>("separation_dist", 1.5);

  offset_dist_      = this->get_parameter("offset_dist").as_double();
  offset_lateral_   = this->get_parameter("offset_lateral").as_double();
  ttc_danger_dist_  = this->get_parameter("ttc_danger_dist").as_double();
  blocking_radius_  = this->get_parameter("blocking_radius").as_double();
  separation_dist_  = this->get_parameter("separation_dist").as_double();

  // TF
  tf_buffer_   = std::make_shared<tf2_ros::Buffer>(this->get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

  // Namespace handling
  std::string ns = this->get_namespace();
  if (ns.length() > 1 && ns[0] == '/') ns = ns.substr(1);
  my_ns_ = ns;
  my_frame_ = my_ns_ + "/base_footprint";

  // QoS
  auto qos = rclcpp::SensorDataQoS();

  // Subscribers
  sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
    "/swarm/poses", qos,
    std::bind(&RobustFollower::swarm_cb, this, std::placeholders::_1));

  sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
    "/swarm/virtual_leader/state", qos,
    std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));

  sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
    "scan/points", qos,
    std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

  sub_role_ = this->create_subscription<std_msgs::msg::Int8>(
    "local_role", 10,
    std::bind(&RobustFollower::role_cb, this, std::placeholders::_1));

  // Publishers
  pub_pan_  = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
  pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
  pub_cmd_  = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

  // Timer ~ 20 Hz
  timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

  RCLCPP_INFO(this->get_logger(), "BOIDS Follower [%s] Online.", my_ns_.c_str());
}

void RobustFollower::swarm_cb(const geometry_msgs::msg::PoseArray::SharedPtr msg)
{
  swarm_poses_ = *msg;
  has_swarm_ = true;
}

void RobustFollower::leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg)
{
  last_leader_msg_ = *msg;
  has_leader_ = true;
  last_leader_time_ = this->get_clock()->now();
}

void RobustFollower::role_cb(const std_msgs::msg::Int8::SharedPtr msg)
{
  current_local_role_ = msg->data;
}

bool RobustFollower::is_point_blocked(double map_x, double map_y)
{
  if (obstacle_points_.empty()) return false;

  geometry_msgs::msg::PointStamped p_map, p_local;
  p_map.header.frame_id = "map";
  p_map.point.x = map_x;
  p_map.point.y = map_y;

  try {
    auto tf = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
    tf2::doTransform(p_map, p_local, tf);
  } catch (...) {
    return false;
  }

  double r_sq = std::pow(blocking_radius_, 2);
  for (const auto& obs : obstacle_points_) {
    double dx = obs.x - p_local.point.x;
    double dy = obs.y - p_local.point.y;
    if ((dx * dx + dy * dy) < r_sq) {
      return true;
    }
  }
  return false;
}

void RobustFollower::scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::fromROSMsg(*msg, *raw_cloud);

  if (raw_cloud->empty() || !has_leader_) return;

  geometry_msgs::msg::TransformStamped tf_l;
  try {
    tf_l = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
  } catch (...) {
    return;
  }

  double lx_l = last_leader_msg_.pose.position.x + tf_l.transform.translation.x;
  double ly_l = last_leader_msg_.pose.position.y + tf_l.transform.translation.y;

  std::vector<pcl::PointXYZ> obs;

#pragma omp parallel
  {
    std::vector<pcl::PointXYZ> t_pts;

#pragma omp for nowait
    for (size_t i = 0; i < raw_cloud->size(); i += 10) {
      const auto& p = raw_cloud->points[i];
      if (p.z < -0.3 || p.z > 0.5) continue;
      if ((p.x * p.x + p.y * p.y) < 0.25) continue;
      if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue;
      t_pts.push_back(p);
    }

#pragma omp critical
    obs.insert(obs.end(), t_pts.begin(), t_pts.end());
  }

  obstacle_points_ = std::move(obs);
  has_scan_ = true;
}

double normalize_angle(double angle) {
  while (angle > M_PI) angle -= 2.0 * M_PI;
  while (angle < -M_PI) angle += 2.0 * M_PI;
  return angle;
}

void RobustFollower::control_loop()
{
  if (!has_leader_ || !has_scan_) return;

  if (last_leader_msg_.swarm_state == 3 && last_leader_msg_.target_locked) {
    geometry_msgs::msg::Twist stop_cmd;
    pub_cmd_->publish(stop_cmd);
    RCLCPP_INFO_THROTTLE(
      this->get_logger(), *this->get_clock(), 1000,
      "COMBAT: Halting tracks for Fire Net.");
    return;
  }

  if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) {
    stop_robot();
    return;
  }

  if (current_local_role_ == 2) {  // LEADER
    return;
  }

  geometry_msgs::msg::TransformStamped tf_now;
  try {
    tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero);
  } catch (...) {
    return;
  }

  double my_x = tf_now.transform.translation.x;
  double my_y = tf_now.transform.translation.y;
  double my_yaw = tf2::getYaw(tf_now.transform.rotation);

  double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
  double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);

  // =========================================================================
  // FORMATION ENGINE: Extract Robot ID and set dynamic offsets
  // =========================================================================
  double current_off_back = offset_dist_;
  double current_off_side = offset_lateral_;
  std::string form_name = "Wedge";

  // Safely extract integer ID from namespace (e.g. "SH_02" -> 2)
  int my_id = 2; 
  size_t underscore_pos = my_ns_.find("_");
  if (underscore_pos != std::string::npos && underscore_pos + 1 < my_ns_.length()) {
    try { my_id = std::stoi(my_ns_.substr(underscore_pos + 1)); } catch (...) {}
  }

  switch (last_leader_msg_.formation_type) {
    case 1: // Column (Forced by Leader LiDAR) - Unchanged
      current_off_side = 0.0;
      form_name = "Column";
      break;

    case 2: // Diamond (VIP Escort / 360 Security)
      form_name = "Diamond";
      if (my_id == 2)      { current_off_back = -3.0; current_off_side =  3.0; } // Left Flank
      else if (my_id == 3) { current_off_back = -3.0; current_off_side = -3.0; } // Right Flank
      else if (my_id == 4) { current_off_back = -6.0; current_off_side =  0.0; } // Rear Guard
      else { current_off_back = -6.0 - (1.5 * (my_id - 4)); current_off_side = 0.0; } // Extras queue behind
      break;

    case 3: // True Forward V-Shape (Forward Assault)
      form_name = "Forward-V";
      current_off_back = std::abs(offset_dist_); // Positive pushes them IN FRONT of the leader
      current_off_side = offset_lateral_;
      break;

    case 0: // Default / Current (Wedge) - Unchanged
    default:
      current_off_back = offset_dist_;
      current_off_side = offset_lateral_;
      form_name = "Wedge";
      break;
  }
  // =========================================================================

  // ─────────────── BOIDS SEPARATION ───────────────
  double repulse_x = 0.0, repulse_y = 0.0;
  double min_teammate_dist = 10.0;

  if (has_swarm_) {
    for (const auto& other_pose : swarm_poses_.poses) {
      double dx = other_pose.position.x - my_x;
      double dy = other_pose.position.y - my_y;
      double d = std::hypot(dx, dy);
      if (d < 0.1) continue;
      min_teammate_dist = std::min(min_teammate_dist, d);

      if (d < separation_dist_) {
        double force = (separation_dist_ - d) / d;
        repulse_x -= dx * force;
        repulse_y -= dy * force;
      }
    }
  }

  // ─────────────── TARGET POSITION ───────────────
  double target_x, target_y;

  // We must use the PATH, not the Leader's current physical rotation, to prevent the "Whip Effect"
  if (last_leader_msg_.next_waypoints.size() >= 2) {
    // 1. Get the vector of the path (from wp[0] to wp[1])
    double path_dx = last_leader_msg_.next_waypoints[1].position.x - last_leader_msg_.next_waypoints[0].position.x;
    double path_dy = last_leader_msg_.next_waypoints[1].position.y - last_leader_msg_.next_waypoints[0].position.y;
    
    // 2. Calculate the steady angle of the path
    double path_yaw = std::atan2(path_dy, path_dx);

    // 3. Anchor the formation to the Leader's position, but orient it to the PATH
    // This stops the followers from swinging wildly when the leader spins in place.
    target_x = last_leader_msg_.pose.position.x +
               (current_off_back * std::cos(path_yaw)) -
               (current_off_side * std::sin(path_yaw)) + repulse_x;

    target_y = last_leader_msg_.pose.position.y +
               (current_off_back * std::sin(path_yaw)) +
               (current_off_side * std::cos(path_yaw)) + repulse_y;
               
  } else {
    // Fallback if the path is empty (e.g., reached final goal)
    // Use heavy smoothing on the leader's yaw to prevent sudden snaps
    static double smoothed_yaw = l_yaw;
    
    // Low-pass filter for the angle to prevent whipping
    double angle_diff = normalize_angle(l_yaw - smoothed_yaw);
    smoothed_yaw = normalize_angle(smoothed_yaw + (angle_diff * 0.1));

    target_x = last_leader_msg_.pose.position.x +
               (current_off_back * std::cos(smoothed_yaw)) -
               (current_off_side * std::sin(smoothed_yaw)) + repulse_x;

    target_y = last_leader_msg_.pose.position.y +
               (current_off_back * std::sin(smoothed_yaw)) +
               (current_off_side * std::cos(smoothed_yaw)) + repulse_y;
  }

  double dx_err = target_x - my_x;
  double dy_err = target_y - my_y;
  double dist_err = std::hypot(dx_err, dy_err);
  double angle_to_target = std::atan2(dy_err, dx_err);

  // ─────────────── FRONT CLEARANCE CHECK ───────────────
  double min_front_dist = 10.0;
  for (const auto& p : obstacle_points_) {
    float angle = std::atan2(p.y, p.x);
    if (std::abs(angle) < 0.7) {
      double d = std::hypot(p.x, p.y);
      min_front_dist = std::min(min_front_dist, d);
    }
  }

  double ttc_scale = (min_front_dist < ttc_danger_dist_)
                       ? std::max(0.2, min_front_dist / ttc_danger_dist_)
                       : 1.0;

  if (min_front_dist < 1.0 || min_teammate_dist < 0.8) {
    ttc_scale = 0.0;
  }

  // ─────────────── LOCAL PATH SAMPLING (simple) ───────────────
  double best_yaw = my_yaw;
  double min_score = 9999.0;

  for (double angle = -M_PI/2; angle <= M_PI/2; angle += 0.15) {
    double check_yaw = my_yaw + angle;
    // double diff = tf2NormalizeAngle(check_yaw - angle_to_target);
    double diff = normalize_angle(check_yaw - angle_to_target);

    bool collision = false;
    for (const auto& p : obstacle_points_) {
      double px_r = p.x * cos(-angle) - p.y * sin(-angle);
      double py_r = p.x * sin(-angle) + p.y * cos(-angle);
      if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) {
        collision = true;
        break;
      }
    }

    if (!collision && std::abs(diff) < min_score) {
      min_score = std::abs(diff);
      best_yaw = check_yaw;
    }
  }

  // ─────────────── COMMAND GENERATION ───────────────
  geometry_msgs::msg::Twist cmd;

  if (dist_err < 0.6 || (leader_speed < 0.05 && dist_err < 1.0)) {
    stop_robot();
  } else {
    // double steer = tf2NormalizeAngle(best_yaw - my_yaw);
    double steer = normalize_angle(best_yaw - my_yaw);


    cmd.linear.x = std::min(1.1, leader_speed + 0.25 * dist_err) * ttc_scale;
    cmd.angular.z = 1.8 * steer;

    if (std::abs(steer) > 0.8) {
      cmd.linear.x = 0.05;
    }

    pub_cmd_->publish(cmd);
  }

  RCLCPP_INFO_THROTTLE(
    this->get_logger(), *this->get_clock(), 1000,
    "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s",
    form_name.c_str(), // Replaced the inline string with form_name
    cmd.linear.x, min_teammate_dist,
    (ttc_scale == 0.0 ? "HALT" : "OK"));
}

void RobustFollower::stop_robot()
{
  pub_cmd_->publish(geometry_msgs::msg::Twist());
}

// ────────────────────────────────────────────────
//                   MAIN
// ────────────────────────────────────────────────

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RobustFollower>());
  rclcpp::shutdown();
  return 0;
}



// OLD without diamond
// #include "skyhunter_control/follower_node.hpp"

// #include <algorithm>
// #include <cmath>

// using namespace std::chrono_literals;

// RobustFollower::RobustFollower(const rclcpp::NodeOptions & options)
// : Node("follower_node", options)
// {
//   // Parameters
//   this->declare_parameter<double>("offset_dist", -2.5);
//   this->declare_parameter<double>("offset_lateral", 0.0);
//   this->declare_parameter<double>("ttc_danger_dist", 4.1);
//   this->declare_parameter<double>("blocking_radius", 0.8);
//   this->declare_parameter<double>("separation_dist", 1.5);

//   offset_dist_      = this->get_parameter("offset_dist").as_double();
//   offset_lateral_   = this->get_parameter("offset_lateral").as_double();
//   ttc_danger_dist_  = this->get_parameter("ttc_danger_dist").as_double();
//   blocking_radius_  = this->get_parameter("blocking_radius").as_double();
//   separation_dist_  = this->get_parameter("separation_dist").as_double();

//   // TF
//   tf_buffer_   = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//   tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//   // Namespace handling
//   std::string ns = this->get_namespace();
//   if (ns.length() > 1 && ns[0] == '/') ns = ns.substr(1);
//   my_ns_ = ns;
//   my_frame_ = my_ns_ + "/base_footprint";

//   // QoS
//   auto qos = rclcpp::SensorDataQoS();

//   // Subscribers
//   sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
//     "/swarm/poses", qos,
//     std::bind(&RobustFollower::swarm_cb, this, std::placeholders::_1));

//   sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//     "/swarm/virtual_leader/state", qos,
//     std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));

//   sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//     "scan/points", qos,
//     std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

//   sub_role_ = this->create_subscription<std_msgs::msg::Int8>(
//     "local_role", 10,
//     std::bind(&RobustFollower::role_cb, this, std::placeholders::_1));

//   // Publishers
//   pub_pan_  = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
//   pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
//   pub_cmd_  = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

//   // Timer ~ 20 Hz
//   timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

//   RCLCPP_INFO(this->get_logger(), "BOIDS Follower [%s] Online.", my_ns_.c_str());
// }

// void RobustFollower::swarm_cb(const geometry_msgs::msg::PoseArray::SharedPtr msg)
// {
//   swarm_poses_ = *msg;
//   has_swarm_ = true;
// }

// void RobustFollower::leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg)
// {
//   last_leader_msg_ = *msg;
//   has_leader_ = true;
//   last_leader_time_ = this->get_clock()->now();
// }

// void RobustFollower::role_cb(const std_msgs::msg::Int8::SharedPtr msg)
// {
//   current_local_role_ = msg->data;
// }

// bool RobustFollower::is_point_blocked(double map_x, double map_y)
// {
//   if (obstacle_points_.empty()) return false;

//   geometry_msgs::msg::PointStamped p_map, p_local;
//   p_map.header.frame_id = "map";
//   p_map.point.x = map_x;
//   p_map.point.y = map_y;

//   try {
//     auto tf = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
//     tf2::doTransform(p_map, p_local, tf);
//   } catch (...) {
//     return false;
//   }

//   double r_sq = std::pow(blocking_radius_, 2);
//   for (const auto& obs : obstacle_points_) {
//     double dx = obs.x - p_local.point.x;
//     double dy = obs.y - p_local.point.y;
//     if ((dx * dx + dy * dy) < r_sq) {
//       return true;
//     }
//   }
//   return false;
// }

// void RobustFollower::scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
// {
//   pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//   pcl::fromROSMsg(*msg, *raw_cloud);

//   if (raw_cloud->empty() || !has_leader_) return;

//   geometry_msgs::msg::TransformStamped tf_l;
//   try {
//     tf_l = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
//   } catch (...) {
//     return;
//   }

//   double lx_l = last_leader_msg_.pose.position.x + tf_l.transform.translation.x;
//   double ly_l = last_leader_msg_.pose.position.y + tf_l.transform.translation.y;

//   std::vector<pcl::PointXYZ> obs;

// #pragma omp parallel
//   {
//     std::vector<pcl::PointXYZ> t_pts;

// #pragma omp for nowait
//     for (size_t i = 0; i < raw_cloud->size(); i += 10) {
//       const auto& p = raw_cloud->points[i];
//       if (p.z < -0.3 || p.z > 0.5) continue;
//       if ((p.x * p.x + p.y * p.y) < 0.25) continue;
//       if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue;
//       t_pts.push_back(p);
//     }

// #pragma omp critical
//     obs.insert(obs.end(), t_pts.begin(), t_pts.end());
//   }

//   obstacle_points_ = std::move(obs);
//   has_scan_ = true;
// }

// void RobustFollower::control_loop()
// {
//   if (!has_leader_ || !has_scan_) return;

//   if (last_leader_msg_.swarm_state == 3 && last_leader_msg_.target_locked) {
//     geometry_msgs::msg::Twist stop_cmd;
//     pub_cmd_->publish(stop_cmd);
//     RCLCPP_INFO_THROTTLE(
//       this->get_logger(), *this->get_clock(), 1000,
//       "COMBAT: Halting tracks for Fire Net.");
//     return;
//   }

//   if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) {
//     stop_robot();
//     return;
//   }

//   if (current_local_role_ == 2) {  // LEADER
//     return;
//   }

//   geometry_msgs::msg::TransformStamped tf_now;
//   try {
//     tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero);
//   } catch (...) {
//     return;
//   }

//   double my_x = tf_now.transform.translation.x;
//   double my_y = tf_now.transform.translation.y;
//   double my_yaw = tf2::getYaw(tf_now.transform.rotation);

//   double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//   double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
//   double current_off_side = (last_leader_msg_.formation_type == 1) ? 0.0 : offset_lateral_;

//   // ─────────────── BOIDS SEPARATION ───────────────
//   double repulse_x = 0.0, repulse_y = 0.0;
//   double min_teammate_dist = 10.0;

//   if (has_swarm_) {
//     for (const auto& other_pose : swarm_poses_.poses) {
//       double dx = other_pose.position.x - my_x;
//       double dy = other_pose.position.y - my_y;
//       double d = std::hypot(dx, dy);
//       if (d < 0.1) continue;
//       min_teammate_dist = std::min(min_teammate_dist, d);

//       if (d < separation_dist_) {
//         double force = (separation_dist_ - d) / d;
//         repulse_x -= dx * force;
//         repulse_y -= dy * force;
//       }
//     }
//   }

//   // ─────────────── TARGET POSITION ───────────────
//   double target_x, target_y;

//   if (!last_leader_msg_.next_waypoints.empty()) {
//     auto wp = last_leader_msg_.next_waypoints[0].position;
//     target_x = wp.x - (current_off_side * std::sin(l_yaw)) + repulse_x;
//     target_y = wp.y + (current_off_side * std::cos(l_yaw)) + repulse_y;
//   } else {
//     target_x = last_leader_msg_.pose.position.x +
//                (offset_dist_ * std::cos(l_yaw)) -
//                (current_off_side * std::sin(l_yaw)) + repulse_x;

//     target_y = last_leader_msg_.pose.position.y +
//                (offset_dist_ * std::sin(l_yaw)) +
//                (current_off_side * std::cos(l_yaw)) + repulse_y;
//   }

//   double dx_err = target_x - my_x;
//   double dy_err = target_y - my_y;
//   double dist_err = std::hypot(dx_err, dy_err);
//   double angle_to_target = std::atan2(dy_err, dx_err);

//   // ─────────────── FRONT CLEARANCE CHECK ───────────────
//   double min_front_dist = 10.0;
//   for (const auto& p : obstacle_points_) {
//     float angle = std::atan2(p.y, p.x);
//     if (std::abs(angle) < 0.7) {
//       double d = std::hypot(p.x, p.y);
//       min_front_dist = std::min(min_front_dist, d);
//     }
//   }

//   double ttc_scale = (min_front_dist < ttc_danger_dist_)
//                        ? std::max(0.2, min_front_dist / ttc_danger_dist_)
//                        : 1.0;

//   if (min_front_dist < 1.0 || min_teammate_dist < 0.8) {
//     ttc_scale = 0.0;
//   }

//   // ─────────────── LOCAL PATH SAMPLING (simple) ───────────────
//   double best_yaw = my_yaw;
//   double min_score = 9999.0;

//   for (double angle = -M_PI/2; angle <= M_PI/2; angle += 0.15) {
//     double check_yaw = my_yaw + angle;
//     double diff = tf2NormalizeAngle(check_yaw - angle_to_target);

//     bool collision = false;
//     for (const auto& p : obstacle_points_) {
//       double px_r = p.x * cos(-angle) - p.y * sin(-angle);
//       double py_r = p.x * sin(-angle) + p.y * cos(-angle);
//       if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) {
//         collision = true;
//         break;
//       }
//     }

//     if (!collision && std::abs(diff) < min_score) {
//       min_score = std::abs(diff);
//       best_yaw = check_yaw;
//     }
//   }

//   // ─────────────── COMMAND GENERATION ───────────────
//   geometry_msgs::msg::Twist cmd;

//   if (dist_err < 0.6 || (leader_speed < 0.05 && dist_err < 1.0)) {
//     stop_robot();
//   } else {
//     double steer = tf2NormalizeAngle(best_yaw - my_yaw);

//     cmd.linear.x = std::min(1.1, leader_speed + 0.25 * dist_err) * ttc_scale;
//     cmd.angular.z = 1.8 * steer;

//     if (std::abs(steer) > 0.8) {
//       cmd.linear.x = 0.05;
//     }

//     pub_cmd_->publish(cmd);
//   }

//   RCLCPP_INFO_THROTTLE(
//     this->get_logger(), *this->get_clock(), 1000,
//     "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s",
//     (current_off_side == 0.0 ? "Column" : "V-Shape"),
//     cmd.linear.x, min_teammate_dist,
//     (ttc_scale == 0.0 ? "HALT" : "OK"));
// }

// void RobustFollower::stop_robot()
// {
//   pub_cmd_->publish(geometry_msgs::msg::Twist());
// }

// // ────────────────────────────────────────────────
// //                   MAIN
// // ────────────────────────────────────────────────

// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<RobustFollower>());
//   rclcpp::shutdown();
//   return 0;
// }