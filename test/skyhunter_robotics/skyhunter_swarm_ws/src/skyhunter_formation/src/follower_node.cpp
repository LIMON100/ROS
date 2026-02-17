// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <string>
// #include <algorithm>
// #include <vector>

// #include "rclcpp/rclcpp.hpp"
// #include "geometry_msgs/msg/twist.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include "grid_map_ros/grid_map_ros.hpp"
// #include "tf2/utils.h"
// #include "tf2_ros/buffer.h"
// #include "tf2_ros/transform_listener.h"
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

// using namespace std::chrono_literals;

// class RobustFollower : public rclcpp::Node {
// public:
//   RobustFollower() : Node("follower_node") {
//     // Parameters
//     this->declare_parameter<double>("offset_dist", -2.5); // Meters behind leader
//     this->declare_parameter<double>("robot_width", 0.9);
    
//     tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//     tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//     auto qos = rclcpp::SensorDataQoS();
    
//     sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//         "/leader_state", qos, std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));
    
//     sub_map_ = this->create_subscription<grid_map_msgs::msg::GridMap>(
//         "elevation_map", qos, std::bind(&RobustFollower::map_cb, this, std::placeholders::_1));

//     pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
//     timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

//     // Fix the Namespace string handling
//     std::string ns_str = std::string(this->get_namespace());
//     if (ns_str.length() > 1 && ns_str[0] == '/') {
//         my_frame_ = ns_str.substr(1) + "/base_footprint";
//     } else {
//         my_frame_ = "base_footprint";
//     }

//     RCLCPP_INFO(this->get_logger(), "Robust Ray-Tracing Follower Started. My Frame: %s", my_frame_.c_str());
//   }

// private:
//   void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
//     last_leader_msg_ = *msg;
//     has_leader_ = true;
//     last_leader_time_ = this->get_clock()->now();
//   }

//   void map_cb(const grid_map_msgs::msg::GridMap::SharedPtr msg) {
//     grid_map::GridMapRosConverter::fromMessage(*msg, local_map_);
//     has_map_ = true;
//   }

//   void control_loop() {
//     if (!has_leader_ || !has_map_) return;
//     if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) { stop_robot(); return; }

//     // 1. Get current Pose in Global Map
//     geometry_msgs::msg::TransformStamped tf_now;
//     try {
//         tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero);
//     } catch (tf2::TransformException &ex) {
//         return; 
//     }

//     double my_x = tf_now.transform.translation.x;
//     double my_y = tf_now.transform.translation.y;
//     double my_yaw = tf2::getYaw(tf_now.transform.rotation);

//     // 2. Calculate Virtual Target (Behind Leader)
//     double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//     double target_x = last_leader_msg_.pose.position.x + (this->get_parameter("offset_dist").as_double() * cos(l_yaw));
//     double target_y = last_leader_msg_.pose.position.y + (this->get_parameter("offset_dist").as_double() * sin(l_yaw));

//     double dx = target_x - my_x;
//     double dy = target_y - my_y;
//     double dist_to_target = std::hypot(dx, dy);
//     double angle_to_target = std::atan2(dy, dx);

//     // 3. Robust Ray Sampling (Avoid Obstacles)
//     double best_yaw = my_yaw;
//     double min_score = 9999.0;
//     bool found_path = false;

//     // Scan angles
//     for (double angle = -1.57; angle <= 1.57; angle += 0.1) {
//         double check_yaw = my_yaw + angle;
//         double angle_cost = std::abs(check_yaw - angle_to_target);
        
//         bool collision = false;
//         // Check a CORRIDOR (Left, Center, Right) to account for robot width
//         std::vector<double> side_offsets = {-0.5, 0.0, 0.5}; // 1.0m total width check
        
//         for (double d = 0.5; d <= 3.0; d += 0.3) {
//             for (double offset : side_offsets) {
//                 double px = my_x + d * cos(check_yaw) + offset * cos(check_yaw + 1.57);
//                 double py = my_y + d * sin(check_yaw) + offset * sin(check_yaw + 1.57);
                
//                 grid_map::Position pos(px, py);
//                 if (local_map_.isInside(pos)) {
//                     if (local_map_.atPosition("traversability", pos) > 0.8) {
//                         collision = true; break;
//                     }
//                 }
//             }
//             if (collision) break;
//         }

//         if (!collision) {
//             if (angle_cost < min_score) {
//                 min_score = angle_cost;
//                 best_yaw = check_yaw;
//                 found_path = true;
//             }
//         }
//     }

//     // 4. Drive Logic
//     geometry_msgs::msg::Twist cmd;
//     if (dist_to_target < 0.7) {
//         stop_robot(); // Arrived at V-offset
//     } else if (!found_path) {
//         // TRAPPED RECOVERY: Back up and spin
//         cmd.linear.x = -0.2;
//         cmd.angular.z = 0.6;
//         RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TRAPPED! Executing recovery...");
//     } else {
//         double steering_error = best_yaw - my_yaw;
//         while(steering_error > M_PI) steering_error -= 2*M_PI;
//         while(steering_error < -M_PI) steering_error += 2*M_PI;

//         // Smooth P-Control
//         cmd.linear.x = std::min(0.8, 0.4 * dist_to_target);
//         cmd.angular.z = 2.0 * steering_error;

//         // If turn is sharp, stop and rotate
//         if (std::abs(steering_error) > 0.7) {
//             cmd.linear.x = 0.1;
//         }
//     }

//     pub_cmd_->publish(cmd);
    
//     RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
//         "Target Dist: %.2fm | Steering: %.2f", dist_to_target, cmd.angular.z);
//   }

//   void stop_robot() {
//     pub_cmd_->publish(geometry_msgs::msg::Twist());
//   }

//   skyhunter_msgs::msg::LeaderState last_leader_msg_;
//   grid_map::GridMap local_map_;
//   rclcpp::Time last_leader_time_;
//   bool has_leader_ = false, has_map_ = false;
//   std::string my_frame_;
//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
//   rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr sub_map_;
//   rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
//   rclcpp::TimerBase::SharedPtr timer_;
//   std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
//   std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
// };

// int main(int argc, char * argv[]) {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<RobustFollower>());
//   rclcpp::shutdown();
//   return 0;
// }



// //GPT
// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <string>
// #include <vector>
// #include <algorithm>
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

// using namespace std::chrono_literals;

// class RobustFollower : public rclcpp::Node {
// public:
//   RobustFollower() : Node("follower_node") {

//     // ─── Parameters ─────────────────────────────────────────────
//     this->declare_parameter<double>("offset_dist", -2.5);
//     this->declare_parameter<double>("safety_margin", 0.6);

//     // ─── TF ─────────────────────────────────────────────────────
//     tf_buffer_   = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//     tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//     auto qos = rclcpp::SensorDataQoS();

//     // ─── Subscriptions ──────────────────────────────────────────
//     sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//       "/leader_state", qos,
//       std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));

//     sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//       "scan/points", qos,
//       std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

//     // ─── Publisher ──────────────────────────────────────────────
//     pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

//     timer_ = this->create_wall_timer(
//       50ms, std::bind(&RobustFollower::control_loop, this));

//     // ─── Frame Name ─────────────────────────────────────────────
//     std::string ns = std::string(this->get_namespace());
//     my_frame_ = (ns.length() > 1) ? ns.substr(1) + "/base_footprint"
//                                  : "base_footprint";

//     RCLCPP_INFO(this->get_logger(),
//       "Robust Follower started (HYBRID LiDAR MODE, OpenMP enabled)");
//   }

// private:
//   // ==============================================================
//   // LEADER CALLBACK
//   // ==============================================================
//   void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
//     last_leader_msg_ = *msg;
//     has_leader_ = true;
//     last_leader_time_ = this->get_clock()->now();
//   }

//   // ==============================================================
//   // LIDAR CALLBACK (FAST + OBJECT AWARE)
//   // ==============================================================
//   void scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {

//     pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(
//       new pcl::PointCloud<pcl::PointXYZ>);

//     pcl::fromROSMsg(*msg, *cloud);
//     if (cloud->empty()) return;

//     obstacle_points_.clear();
//     object_centers_.clear();

//     // ─── FAST PARALLEL FILTER ───────────────────────────────────
//     #pragma omp parallel
//     {
//       std::vector<pcl::PointXYZ> local_pts;

//       #pragma omp for nowait
//       for (size_t i = 0; i < cloud->size(); i += 8) {
//         const auto& p = cloud->points[i];

//         // Z filter
//         if (p.z < -0.4 || p.z > 0.4) continue;

//         // Self filter (40 cm)
//         float d2 = p.x*p.x + p.y*p.y;
//         if (d2 < 0.16) continue;

//         local_pts.push_back(p);
//       }

//       #pragma omp critical
//       obstacle_points_.insert(
//         obstacle_points_.end(),
//         local_pts.begin(), local_pts.end());
//     }

//     // ─── LIGHT OBJECT CLUSTERING (NO PCL HEAVY OPS) ──────────────
//     std::vector<bool> used(obstacle_points_.size(), false);
//     const float cluster_tol2 = 0.4f * 0.4f;
//     const int   min_pts = 6;

//     for (size_t i = 0; i < obstacle_points_.size(); ++i) {
//       if (used[i]) continue;

//       float cx = 0, cy = 0;
//       int count = 0;

//       for (size_t j = i; j < obstacle_points_.size(); ++j) {
//         float dx = obstacle_points_[i].x - obstacle_points_[j].x;
//         float dy = obstacle_points_[i].y - obstacle_points_[j].y;

//         if (dx*dx + dy*dy < cluster_tol2) {
//           used[j] = true;
//           cx += obstacle_points_[j].x;
//           cy += obstacle_points_[j].y;
//           count++;
//         }
//       }

//       if (count >= min_pts) {
//         object_centers_.emplace_back(cx / count, cy / count);
//       }
//     }

//     has_scan_ = true;
//   }

//   // ==============================================================
//   // CONTROL LOOP
//   // ==============================================================
//   void control_loop() {

//     if (!has_leader_ || !has_scan_) return;
//     if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) {
//       stop_robot(); return;
//     }

//     // ─── TF ─────────────────────────────────────────────────────
//     geometry_msgs::msg::TransformStamped tf;
//     try {
//       tf = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero);
//     } catch (...) { return; }

//     double my_x = tf.transform.translation.x;
//     double my_y = tf.transform.translation.y;
//     double my_yaw = tf2::getYaw(tf.transform.rotation);

//     // ─── Target Behind Leader ───────────────────────────────────
//     double lyaw = tf2::getYaw(last_leader_msg_.pose.orientation);
//     double offset = this->get_parameter("offset_dist").as_double();

//     double tx = last_leader_msg_.pose.position.x + offset * cos(lyaw);
//     double ty = last_leader_msg_.pose.position.y + offset * sin(lyaw);

//     double dx = tx - my_x;
//     double dy = ty - my_y;

//     double dist_to_target = std::hypot(dx, dy);
//     double angle_to_target = std::atan2(dy, dx);

//     // ─── Corridor Sampling Planner ──────────────────────────────
//     double best_yaw = my_yaw;
//     double best_score = 1e9;
//     bool found = false;

//     for (double a = -1.57; a <= 1.57; a += 0.15) {

//       double test_yaw = my_yaw + a;
//       double err = test_yaw - angle_to_target;
//       while (err > M_PI) err -= 2*M_PI;
//       while (err < -M_PI) err += 2*M_PI;

//       bool collision = false;

//       for (const auto& c : object_centers_) {
//         double px =  c.first * cos(-a) - c.second * sin(-a);
//         double py =  c.first * sin(-a) + c.second * cos(-a);

//         if (px > 0.0 && px < 3.0 && std::abs(py) < 0.6) {
//           collision = true;
//           break;
//         }
//       }

//       if (!collision && std::abs(err) < best_score) {
//         best_score = std::abs(err);
//         best_yaw = test_yaw;
//         found = true;
//       }
//     }

//     // ─── Command ────────────────────────────────────────────────
//     geometry_msgs::msg::Twist cmd;

//     if (dist_to_target < 0.6) {
//       stop_robot();
//       return;
//     }

//     if (!found) {
//       cmd.angular.z = 0.6;
//     } else {
//       double steer = best_yaw - my_yaw;
//       while (steer > M_PI) steer -= 2*M_PI;
//       while (steer < -M_PI) steer += 2*M_PI;

//       cmd.linear.x  = std::min(0.8, 0.4 * dist_to_target);
//       cmd.angular.z = 1.8 * steer;

//       if (std::abs(steer) > 0.7) cmd.linear.x = 0.1;
//     }

//     pub_cmd_->publish(cmd);
//   }

//   void stop_robot() {
//     pub_cmd_->publish(geometry_msgs::msg::Twist());
//   }

//   // ==============================================================
//   // MEMBERS
//   // ==============================================================
//   skyhunter_msgs::msg::LeaderState last_leader_msg_;
//   rclcpp::Time last_leader_time_;

//   std::vector<pcl::PointXYZ> obstacle_points_;
//   std::vector<std::pair<float,float>> object_centers_;

//   bool has_leader_ = false;
//   bool has_scan_   = false;

//   std::string my_frame_;

//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
//   rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr   sub_scan_;
//   rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr          pub_cmd_;
//   rclcpp::TimerBase::SharedPtr timer_;

//   std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
//   std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
// };

// // ==============================================================
// int main(int argc, char** argv) {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<RobustFollower>());
//   rclcpp::shutdown();
//   return 0;
// }



// workable gemini-r1
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <algorithm>
#include <vector>
#include <omp.h> 

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "skyhunter_msgs/msg/leader_state.hpp"
#include "tf2/utils.h"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

using namespace std::chrono_literals;

class RobustFollower : public rclcpp::Node {
public:
  RobustFollower() : Node("follower_node") {
    // --- Parameters ---
    this->declare_parameter<double>("offset_dist", -2.5);
    this->declare_parameter<double>("ttc_danger_dist", 4.1); // Requirement: 4.1m
    this->declare_parameter<double>("blocking_radius", 0.8);
    
    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    std::string ns = std::string(this->get_namespace());
    if (ns.length() > 1 && ns[0] == '/') ns = ns.substr(1);
    my_ns_ = ns;
    my_frame_ = my_ns_ + "/base_footprint";

    auto qos = rclcpp::SensorDataQoS();
    sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
        "/leader_state", qos, std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));
    sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
        "scan/points", qos, std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

    pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

    RCLCPP_INFO(this->get_logger(), "TTC-CONTROLLED Follower [%s] Online.", my_ns_.c_str());
  }

private:
  // Check if a map point is blocked
  bool is_point_blocked(double map_x, double map_y) {
    if (obstacle_points_.empty()) return false;
    geometry_msgs::msg::PointStamped p_map, p_local;
    p_map.header.frame_id = "map";
    p_map.point.x = map_x; p_map.point.y = map_y;
    try {
        auto tf = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
        tf2::doTransform(p_map, p_local, tf);
    } catch (...) { return false; }
    double r_sq = std::pow(this->get_parameter("blocking_radius").as_double(), 2);
    for (const auto& obs : obstacle_points_) {
        double dx = obs.x - p_local.point.x;
        double dy = obs.y - p_local.point.y;
        if ((dx*dx + dy*dy) < r_sq) return true; 
    }
    return false;
  }

  void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
    last_leader_msg_ = *msg;
    has_leader_ = true;
    last_leader_time_ = this->get_clock()->now();
  }

  void scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(*msg, *raw_cloud);
    if (raw_cloud->empty() || !has_leader_) return;

    geometry_msgs::msg::TransformStamped tf_l;
    try { tf_l = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero); } 
    catch (...) { return; }

    double lx_l = last_leader_msg_.pose.position.x + tf_l.transform.translation.x;
    double ly_l = last_leader_msg_.pose.position.y + tf_l.transform.translation.y;

    std::vector<pcl::PointXYZ> obs;
    #pragma omp parallel
    {
        std::vector<pcl::PointXYZ> t_pts;
        #pragma omp for nowait
        for (size_t i = 0; i < raw_cloud->size(); i += 8) {
            const auto& p = raw_cloud->points[i];
            if (p.z < -0.4 || p.z > 0.4) continue;
            if ((p.x*p.x + p.y*p.y) < 0.25) continue; 
            if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue; 
            t_pts.push_back(p);
        }
        #pragma omp critical
        obs.insert(obs.end(), t_pts.begin(), t_pts.end());
    }
    obstacle_points_ = obs;
    has_scan_ = true;
  }

  void control_loop() {
    if (!has_leader_ || !has_scan_) return;
    if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) { stop_robot(); return; }

    // 1. Target Selection (Slicing Logic)
    double target_x, target_y;
    if (last_leader_msg_.next_waypoints.size() >= 2) {
        auto wp1 = last_leader_msg_.next_waypoints[0].position;
        auto wp2 = last_leader_msg_.next_waypoints[1].position;
        target_x = is_point_blocked(wp1.x, wp1.y) ? wp2.x : wp1.x;
        target_y = is_point_blocked(wp1.x, wp1.y) ? wp2.y : wp1.y;
    } else {
        double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
        target_x = last_leader_msg_.pose.position.x + (this->get_parameter("offset_dist").as_double() * cos(l_yaw));
        target_y = last_leader_msg_.pose.position.y + (this->get_parameter("offset_dist").as_double() * sin(l_yaw));
    }

    // 2. Localization
    geometry_msgs::msg::TransformStamped tf_now;
    try { tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero); } catch (...) { return; }
    double my_x = tf_now.transform.translation.x;
    double my_y = tf_now.transform.translation.y;
    double my_yaw = tf2::getYaw(tf_now.transform.rotation);

    double dx = target_x - my_x; double dy = target_y - my_y;
    double dist_err = std::hypot(dx, dy);
    double angle_to_target = std::atan2(dy, dx);

    // 3. TTC CALCULATION (Client Rule: 4.1m)
    // Find the closest obstacle in our current path corridor
    double min_front_dist = 10.0;
    for (const auto& p : obstacle_points_) {
        // If point is in front of us (within 40 degree cone)
        float angle = std::atan2(p.y, p.x);
        if (std::abs(angle) < 0.7) { // ~40 deg
            double d = std::hypot(p.x, p.y);
            if (d < min_front_dist) min_front_dist = d;
        }
    }

    // TTC Speed Multiplier
    double ttc_limit = this->get_parameter("ttc_danger_dist").as_double();
    double ttc_scale = 1.0;
    if (min_front_dist < ttc_limit) {
        // Scale speed linearly: 4.1m = 100% speed, 1.0m = 20% speed
        ttc_scale = std::max(0.2, min_front_dist / ttc_limit);
    }

    // 4. Corridor Sampling (Steering)
    double best_yaw = my_yaw; double min_score = 9999.0; bool path_found = false;
    for (double angle = -1.57; angle <= 1.57; angle += 0.15) {
        double check_yaw = my_yaw + angle;
        double diff = check_yaw - angle_to_target;
        while(diff > M_PI) diff -= 2*M_PI; while(diff < -M_PI) diff += 2*M_PI;
        bool collision = false;
        for (const auto& p : obstacle_points_) {
            double px_r = p.x * cos(-angle) - p.y * sin(-angle);
            double py_r = p.x * sin(-angle) + p.y * cos(-angle);
            if (px_r > 0.0 && px_r < 3.5 && std::abs(py_r) < 0.6) { collision = true; break; }
        }
        if (!collision && std::abs(diff) < min_score) {
            min_score = std::abs(diff); best_yaw = check_yaw; path_found = true;
        }
    }

    // 5. Execution with TTC Scaling
    geometry_msgs::msg::Twist cmd;
    double l_vel = std::abs(last_leader_msg_.velocity.linear.x);

    if (dist_err < 0.6) {
        stop_robot();
    } else {
        double steer = best_yaw - my_yaw;
        while(steer > M_PI) steer -= 2*M_PI; while(steer < -M_PI) steer += 2*M_PI;

        double base_speed = l_vel + (0.3 * dist_err);
        
        // --- APPLY TTC SCALE ---
        cmd.linear.x = std::min(1.1, base_speed) * ttc_scale;
        
        cmd.angular.z = 1.8 * steer;
        if (std::abs(steer) > 0.8) cmd.linear.x = 0.05; 
    }
    pub_cmd_->publish(cmd);

    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
        "TTC Status: %s | Dist: %.2fm | Speed Scale: %.1f%%", 
        (ttc_scale < 0.9 ? "BRAKING" : "CLEAR"), min_front_dist, ttc_scale * 100.0);
  }

  void stop_robot() { pub_cmd_->publish(geometry_msgs::msg::Twist()); }

  std::string my_ns_, my_frame_;
  skyhunter_msgs::msg::LeaderState last_leader_msg_;
  std::vector<pcl::PointXYZ> obstacle_points_;
  rclcpp::Time last_leader_time_;
  bool has_leader_ = false, has_scan_ = false;
  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

int main(int argc, char * argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RobustFollower>());
  rclcpp::shutdown();
  return 0;
}