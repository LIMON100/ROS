// // UPDATED WORKABLE 03-11 wITH DIAMON and wedge
// #define _USE_MATH_DEFINES
// #include "skyhunter_control/follower_node.hpp"
// #include <pcl/common/transforms.h>
// #include <algorithm>
// #include <cmath>
// #include <tf2_eigen/tf2_eigen.hpp>
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

// // void RobustFollower::scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
// // {
// //   // unchanged (your latest version with proper leader transform)
// //   pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
// //   pcl::fromROSMsg(*msg, *raw_cloud);
// //   if (raw_cloud->empty() || !has_leader_) return;

// //   geometry_msgs::msg::PointStamped leader_map, leader_local;
// //   leader_map.header.frame_id = "map";
// //   leader_map.point = last_leader_msg_.pose.position;

// //   try {
// //     auto tf_map_to_local = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
// //     tf2::doTransform(leader_map, leader_local, tf_map_to_local);
// //   } catch (...) { return; }

// //   double lx_l = leader_local.point.x;
// //   double ly_l = leader_local.point.y;

// //   std::vector<pcl::PointXYZ> obs;
// // #pragma omp parallel
// //   {
// //     std::vector<pcl::PointXYZ> t_pts;
// // #pragma omp for nowait
// //     for (size_t i = 0; i < raw_cloud->size(); i += 10) {
// //       const auto& p = raw_cloud->points[i];
// //       if (p.z < -0.3 || p.z > 0.5) continue;
// //       if ((p.x * p.x + p.y * p.y) < 0.25) continue;
// //       if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue;
// //       t_pts.push_back(p);
// //     }
// // #pragma omp critical
// //     obs.insert(obs.end(), t_pts.begin(), t_pts.end());
// //   }
// //   obstacle_points_ = std::move(obs);
// //   has_scan_ = true;
// // }


// void RobustFollower::scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
// {
//   if (!has_leader_) return;

//   pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//   pcl::fromROSMsg(*msg, *raw_cloud);
//   if (raw_cloud->empty()) return;

//   geometry_msgs::msg::TransformStamped tf_lidar_to_base;
//   try { tf_lidar_to_base = tf_buffer_->lookupTransform(my_frame_, msg->header.frame_id, tf2::TimePointZero); } 
//   catch (...) { return; }

//   Eigen::Affine3d transform = tf2::transformToEigen(tf_lidar_to_base.transform);
//   pcl::PointCloud<pcl::PointXYZ>::Ptr aligned_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//   pcl::transformPointCloud(*raw_cloud, *aligned_cloud, transform);

//   geometry_msgs::msg::PointStamped leader_map, leader_local;
//   leader_map.header.frame_id = "map";
//   leader_map.point = last_leader_msg_.pose.position;

//   try {
//     auto tf_map_to_local = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
//     tf2::doTransform(leader_map, leader_local, tf_map_to_local);
//   } catch (...) { return; }

//   double lx_l = leader_local.point.x;
//   double ly_l = leader_local.point.y;

//   std::vector<pcl::PointXYZ> obs;

//   // --- TRACK GROUND DENSITY AHEAD ---
//   int ground_front = 0;
  
//   #pragma omp parallel
//   {
//     std::vector<pcl::PointXYZ> t_pts;
//     int local_ground_front = 0;

//     #pragma omp for nowait
//     for (size_t i = 0; i < aligned_cloud->size(); i += 5) {
//       const auto& p = aligned_cloud->points[i];
      
//       // Look further ahead (1.5m to 3.5m) to account for momentum!
//       if (p.x > 1.5 && p.x < 3.5 && std::abs(p.y) < 0.8) {
//           if (p.z > -0.6 && p.z < 0.6) local_ground_front++;
//       }

//       if (p.z < 0.2 || p.z > 1.2) continue; 
//       if ((p.x * p.x + p.y * p.y) < 0.25) continue; 
//       if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue; 
      
//       t_pts.push_back(p);
//     }

//     #pragma omp critical
//     {
//       obs.insert(obs.end(), t_pts.begin(), t_pts.end());
//       ground_front += local_ground_front;
//     }
//   }

//   // --- THE FOLLOWER CLIFF BRAKE ---
//   if (ground_front < 15) { // If the ground vanishes
//       // Inject a SOLID WALL of obstacles so the follower's steering math fails and forces a stop
//       for (double vy = -1.5; vy <= 1.5; vy += 0.2) {
//           obs.push_back(pcl::PointXYZ(1.0, vy, 0.5)); // 1.0m directly in front
//       }
//       RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 500, "FOLLOWER VOID DETECTED! Deploying Emergency Brake.");
//   }

//   obstacle_points_ = std::move(obs);
//   has_scan_ = true;
// }


// double normalize_angle(double angle) {
//   while (angle > M_PI) angle -= 2.0 * M_PI;
//   while (angle < -M_PI) angle += 2.0 * M_PI;
//   return angle;
// }

// // void RobustFollower::control_loop()
// // {
// //   if (!has_leader_ || !has_scan_) return;

// //   // 1. DATA ACCESS & INITIAL LOCKS
// //   double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
  
// //   // --- SAFETY LOCK 1: INITIAL SPAWN SETTLING ---
// //   static rclcpp::Time spawn_time = this->get_clock()->now();
// //   if ((this->get_clock()->now() - spawn_time).seconds() < 3.0) {
// //       stop_robot();
// //       RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "INITIALIZING: Settling...");
// //       return;
// //   }

// //   // --- SAFETY LOCK 2: LEADER STANDBY ---
// //   // Hold position until Leader starts moving or mission state changes
// //   if (leader_speed < 0.05 && last_leader_msg_.swarm_state == 0) {
// //       stop_robot();
// //       RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "STANDBY: Leader stationary.");
// //       return;
// //   }

// //   // if (last_leader_msg_.swarm_state == 3 && last_leader_msg_.target_locked) {
// //   //   geometry_msgs::msg::Twist stop_cmd;
// //   //   pub_cmd_->publish(stop_cmd);
// //   //   RCLCPP_INFO_THROTTLE(
// //   //     this->get_logger(), *this->get_clock(), 1000,
// //   //     "COMBAT: Halting tracks for Fire Net.");
// //   //   return;
// //   // }

// //   // if (last_leader_msg_.target_locked && last_leader_msg_.swarm_state >= 3) {
    
// //   //   // Extract my unique ID (SH_02 → 2, SH_03 → 3, ..., SH_07 → 7)
// //   //   int my_id = 2;
// //   //   size_t underscore_pos = my_ns_.find("_");
// //   //   if (underscore_pos != std::string::npos && underscore_pos + 1 < my_ns_.length()) {
// //   //       try { 
// //   //           my_id = std::stoi(my_ns_.substr(underscore_pos + 1)); 
// //   //       } catch (...) {}
// //   //   }

// //   //   // 6 followers → 60° spacing (SH_02 at 0°, SH_03 at 60°, ..., SH_07 at 300°)
// //   //   const double radius = 5.0;                    // Client spec: 5m radius circle
// //   //   const double angle_step = 2.0 * M_PI / 6.0;   // 60 degrees
// //   //   double my_angle = (my_id - 2) * angle_step;

// //   //   // Target position published by Leader (world frame)
// //   //   double target_x = last_leader_msg_.target_pos.x;
// //   //   double target_y = last_leader_msg_.target_pos.y;

// //   //   // Desired position on the circle
// //   //   double target_x_circle = target_x + radius * std::cos(my_angle);
// //   //   double target_y_circle = target_y + radius * std::sin(my_angle);

// //   //   RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
// //   //       "FIRE NET: SH_%02d moving to circle (%.1f°, radius=%.1fm)", 
// //   //       my_id, my_angle * 180.0 / M_PI, radius);

// //   //   // Override normal formation target with circle position
// //   //   target_x = target_x_circle;
// //   //   target_y = target_y_circle;
    
// //   // }

// //   if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) {
// //     stop_robot();
// //     return;
// //   }

// //   if (current_local_role_ == 2) {  // LEADER
// //     return;
// //   }


// //   geometry_msgs::msg::TransformStamped tf_now;
// //   try {
// //     // 50ms TIMEOUT. If CPU lags, it waits gracefully.
// //     tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero, std::chrono::milliseconds(50));
// //   } catch (const tf2::TransformException & ex) {
// //     RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TF LAG: Brakes applied for safety.");
// //     stop_robot(); // CRITICAL: Stop wheels if lost!
// //     return;
// //   }

// //   double my_x = tf_now.transform.translation.x;
// //   double my_y = tf_now.transform.translation.y;
// //   double my_yaw = tf2::getYaw(tf_now.transform.rotation);

// //   double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
// //   // double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);

// //   // =========================================================================
// //   // FORMATION ENGINE: Extract Robot ID and set dynamic offsets
// //   // =========================================================================
// //   double current_off_back = offset_dist_;
// //   double current_off_side = offset_lateral_;
// //   std::string form_name = "Wedge";

// //   // Safely extract integer ID from namespace (e.g. "SH_02" -> 2)
// //   int my_id = 2; 
// //   size_t underscore_pos = my_ns_.find("_");
// //   if (underscore_pos != std::string::npos && underscore_pos + 1 < my_ns_.length()) {
// //     try { my_id = std::stoi(my_ns_.substr(underscore_pos + 1)); } catch (...) {}
// //   }

// //   switch (last_leader_msg_.formation_type) {
// //     case 1: // Column (Forced by Leader LiDAR) - Unchanged
// //       current_off_side = 0.0;
// //       form_name = "Column";
// //       break;

// //     case 2: // Diamond (VIP Escort / 360 Security)
// //       form_name = "Diamond";
// //       if (my_id == 2)      { current_off_back = -3.0; current_off_side =  3.0; } // Left Flank
// //       else if (my_id == 3) { current_off_back = -3.0; current_off_side = -3.0; } // Right Flank
// //       else if (my_id == 4) { current_off_back = -6.0; current_off_side =  0.0; } // Rear Guard
// //       else { current_off_back = -6.0 - (1.5 * (my_id - 4)); current_off_side = 0.0; } // Extras queue behind
// //       break;

// //     case 3: // True Forward V-Shape (Forward Assault)
// //       form_name = "Forward-V";
// //       current_off_back = std::abs(offset_dist_); // Positive pushes them IN FRONT of the leader
// //       current_off_side = offset_lateral_;
// //       break;

// //     case 0: // Default / Current (Wedge) - Unchanged
// //     default:
// //       current_off_back = offset_dist_;
// //       current_off_side = offset_lateral_;
// //       form_name = "Wedge";
// //       break;
// //   }
// //   // =========================================================================

// //   // =========================================================================
// //   // FORMATION ENGINE: All 5 Tactical Modes
// //   // =========================================================================
// //   // double current_off_back = offset_dist_;
// //   // double current_off_side = offset_lateral_;
// //   // std::string form_name = "Wedge";

// //   // // A. Get Robot ID (SH_02 -> 2)
// //   // int my_id = 2; 
// //   // size_t underscore_pos = my_ns_.find("_");
// //   // if (underscore_pos != std::string::npos && underscore_pos + 1 < my_ns_.length()) {
// //   //   try { my_id = std::stoi(my_ns_.substr(underscore_pos + 1)); } catch (...) {}
// //   // }

// //   // // B. Pre-calculate Row/Side for Column logic
// //   // // For 6 followers: Row 0 (SH_06,07), Row 1 (SH_04,05), Row 2 (SH_02,03)
// //   // int col_row = 0;
// //   // if (my_id == 2 || my_id == 3) col_row = 2;
// //   // else if (my_id == 4 || my_id == 5) col_row = 1;
// //   // else col_row = 0;

// //   // double side_sign = (my_id % 2 == 0) ? 1.0 : -1.0; // Even=Left(+), Odd=Right(-)

// //   // // C. THE MASTER SWITCH
// //   // switch (last_leader_msg_.formation_type) {
    
// //   //   case 0: // WEDGE (V-Shape / Default)
// //   //     current_off_back = offset_dist_ * (my_id / 2);
// //   //     current_off_side = offset_lateral_ * side_sign;
// //   //     form_name = "Wedge";
// //   //     break;

// //   //   case 1: // SINGLE COLUMN (Follower behind Follower)
// //   //     current_off_side = 0.0;
// //   //     current_off_back = offset_dist_ * (my_id - 1); 
// //   //     form_name = "Single-Column";
// //   //     break;

// //   //   case 2: // DIAMOND (VIP Guard Mode)
// //   //     form_name = "Diamond";
// //   //     if (my_id == 2)      { current_off_back = -3.0; current_off_side =  3.0; }
// //   //     else if (my_id == 3) { current_off_back = -3.0; current_off_side = -3.0; }
// //   //     else if (my_id == 4) { current_off_back = -6.0; current_off_side =  0.0; }
// //   //     else { current_off_back = -6.0 - (1.5 * (my_id - 4)); current_off_side = 0.0; }
// //   //     break;

// //   //   case 3: // FORWARD-V (Assault Mode - Followers in front)
// //   //     form_name = "Forward-V";
// //   //     current_off_back = std::abs(offset_dist_) * (my_id / 2); // Positive offset
// //   //     current_off_side = offset_lateral_ * side_sign;
// //   //     break;

// //   //   case 4: // DOUBLE COLUMN (Leader at Tail - Spec Phase 2)
// //   //     form_name = "Double-Column-Rear-Leader";
// //   //     // Push followers IN FRONT of leader (Positive X)
// //   //     // Row 0 (front): 9m | Row 1: 6m | Row 2 (back): 3m
// //   //     current_off_back = 3.0 + ( (2 - col_row) * 3.0 ); 
// //   //     // 3m lateral gap (1.5m each side)
// //   //     current_off_side = 1.5 * side_sign;
// //   //     break;

// //   //   default:
// //   //     form_name = "Unknown";
// //   //     break;
// //   // }

  
// //   // ─────────────── BOIDS SEPARATION ───────────────
// //   double repulse_x = 0.0, repulse_y = 0.0;
// //   double min_teammate_dist = 10.0;

// //   if (has_swarm_) {
// //     for (const auto& other_pose : swarm_poses_.poses) {
// //       double dx = other_pose.position.x - my_x;
// //       double dy = other_pose.position.y - my_y;
// //       double d = std::hypot(dx, dy);
// //       if (d < 0.1) continue;
// //       min_teammate_dist = std::min(min_teammate_dist, d);

// //       if (d < separation_dist_) {
// //         double force = (separation_dist_ - d) / d;
// //         repulse_x -= dx * force;
// //         repulse_y -= dy * force;
// //       }
// //     }
// //   }

// //   // ─────────────── TARGET POSITION ───────────────
// //   // double target_x, target_y;

// //   // // We must use the PATH, not the Leader's current physical rotation, to prevent the "Whip Effect"
// //   // if (last_leader_msg_.next_waypoints.size() >= 2) {
// //   //   // 1. Get the vector of the path (from wp[0] to wp[1])
// //   //   double path_dx = last_leader_msg_.next_waypoints[1].position.x - last_leader_msg_.next_waypoints[0].position.x;
// //   //   double path_dy = last_leader_msg_.next_waypoints[1].position.y - last_leader_msg_.next_waypoints[0].position.y;
    
// //   //   // 2. Calculate the steady angle of the path
// //   //   double path_yaw = std::atan2(path_dy, path_dx);

// //   //   // 3. Anchor the formation to the Leader's position, but orient it to the PATH
// //   //   // This stops the followers from swinging wildly when the leader spins in place.
// //   //   target_x = last_leader_msg_.pose.position.x +
// //   //              (current_off_back * std::cos(path_yaw)) -
// //   //              (current_off_side * std::sin(path_yaw)) + repulse_x;

// //   //   target_y = last_leader_msg_.pose.position.y +
// //   //              (current_off_back * std::sin(path_yaw)) +
// //   //              (current_off_side * std::cos(path_yaw)) + repulse_y;
               
// //   // } else {
// //   //   // Fallback if the path is empty (e.g., reached final goal)
// //   //   // Use heavy smoothing on the leader's yaw to prevent sudden snaps
// //   //   static double smoothed_yaw = l_yaw;
    
// //   //   // Low-pass filter for the angle to prevent whipping
// //   //   double angle_diff = normalize_angle(l_yaw - smoothed_yaw);
// //   //   smoothed_yaw = normalize_angle(smoothed_yaw + (angle_diff * 0.1));

// //   //   target_x = last_leader_msg_.pose.position.x +
// //   //              (current_off_back * std::cos(smoothed_yaw)) -
// //   //              (current_off_side * std::sin(smoothed_yaw)) + repulse_x;

// //   //   target_y = last_leader_msg_.pose.position.y +
// //   //              (current_off_back * std::sin(smoothed_yaw)) +
// //   //              (current_off_side * std::cos(smoothed_yaw)) + repulse_y;
// //   // }

// //   // ─────────────── TARGET POSITION ───────────────
// //   // double target_x, target_y;

// //   // if (last_leader_msg_.target_locked && last_leader_msg_.swarm_state >= 3) {
// //   //     // === FIRE NET / ENCIRCLE MODE ===
// //   //     int my_id = 2;
// //   //     size_t underscore_pos = my_ns_.find("_");
// //   //     if (underscore_pos != std::string::npos && underscore_pos + 1 < my_ns_.length()) {
// //   //         try { my_id = std::stoi(my_ns_.substr(underscore_pos + 1)); } catch (...) {}
// //   //     }

// //   //     const double radius = 5.0;
// //   //     const double angle_step = 2.0 * M_PI / 6.0;          // 60° per robot
// //   //     double my_angle = (my_id - 2) * angle_step;

// //   //     double tx = last_leader_msg_.target_pos.x;
// //   //     double ty = last_leader_msg_.target_pos.y;

// //   //     target_x = tx + radius * std::cos(my_angle);
// //   //     target_y = ty + radius * std::sin(my_angle);

// //   //     RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
// //   //         "FIRE NET: SH_%02d moving to circle (%.1f°, radius=%.1fm) @ (%.1f, %.1f)",
// //   //         my_id, my_angle * 180.0 / M_PI, radius, target_x, target_y);
// //   // } else {
// //   //     // === NORMAL FORMATION (original code) ===
// //   //     if (last_leader_msg_.next_waypoints.size() >= 2) {
// //   //         double path_dx = last_leader_msg_.next_waypoints[1].position.x - last_leader_msg_.next_waypoints[0].position.x;
// //   //         double path_dy = last_leader_msg_.next_waypoints[1].position.y - last_leader_msg_.next_waypoints[0].position.y;
// //   //         double path_yaw = std::atan2(path_dy, path_dx);

// //   //         target_x = last_leader_msg_.pose.position.x +
// //   //                   (current_off_back * std::cos(path_yaw)) -
// //   //                   (current_off_side * std::sin(path_yaw)) + repulse_x;
// //   //         target_y = last_leader_msg_.pose.position.y +
// //   //                   (current_off_back * std::sin(path_yaw)) +
// //   //                   (current_off_side * std::cos(path_yaw)) + repulse_y;
// //   //     } else {
// //   //         // fallback smoothed yaw (original)
// //   //         static double smoothed_yaw = l_yaw;
// //   //         double angle_diff = normalize_angle(l_yaw - smoothed_yaw);
// //   //         smoothed_yaw = normalize_angle(smoothed_yaw + (angle_diff * 0.1));

// //   //         target_x = last_leader_msg_.pose.position.x +
// //   //                   (current_off_back * std::cos(smoothed_yaw)) -
// //   //                   (current_off_side * std::sin(smoothed_yaw)) + repulse_x;
// //   //         target_y = last_leader_msg_.pose.position.y +
// //   //                   (current_off_back * std::sin(smoothed_yaw)) +
// //   //                   (current_off_side * std::cos(smoothed_yaw)) + repulse_y;
// //   //     }
// //   // }

// //   // double dx_err = target_x - my_x;
// //   // double dy_err = target_y - my_y;
// //   // double dist_err = std::hypot(dx_err, dy_err);
// //   // double angle_to_target = std::atan2(dy_err, dx_err);

// //   // // ─────────────── FRONT CLEARANCE CHECK ───────────────
// //   // double min_front_dist = 10.0;
// //   // for (const auto& p : obstacle_points_) {
// //   //   float angle = std::atan2(p.y, p.x);
// //   //   if (std::abs(angle) < 0.7) {
// //   //     double d = std::hypot(p.x, p.y);
// //   //     min_front_dist = std::min(min_front_dist, d);
// //   //   }
// //   // }

// //   // double ttc_scale = (min_front_dist < ttc_danger_dist_)
// //   //                      ? std::max(0.2, min_front_dist / ttc_danger_dist_)
// //   //                      : 1.0;

// //   // if (min_front_dist < 1.0 || min_teammate_dist < 0.8) {
// //   //   ttc_scale = 0.0;
// //   // }

// //   // // ─────────────── LOCAL PATH SAMPLING (simple) ───────────────
// //   // double best_yaw = my_yaw;
// //   // double min_score = 9999.0;

// //   // for (double angle = -M_PI/2; angle <= M_PI/2; angle += 0.15) {
// //   //   double check_yaw = my_yaw + angle;
// //   //   // double diff = tf2NormalizeAngle(check_yaw - angle_to_target);
// //   //   double diff = normalize_angle(check_yaw - angle_to_target);

// //   //   bool collision = false;
// //   //   for (const auto& p : obstacle_points_) {
// //   //     double px_r = p.x * cos(-angle) - p.y * sin(-angle);
// //   //     double py_r = p.x * sin(-angle) + p.y * cos(-angle);
// //   //     if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) {
// //   //       collision = true;
// //   //       break;
// //   //     }
// //   //   }

// //   //   if (!collision && std::abs(diff) < min_score) {
// //   //     min_score = std::abs(diff);
// //   //     best_yaw = check_yaw;
// //   //   }
// //   // }

// //   // ttc_scale = (min_front_dist < 4.1) ? std::max(0.2, min_front_dist / 4.1) : 1.0;

// //   // // --- THIS TACTICAL ALARM LOGIC ---
// //   // bool path_is_blocked = false;
// //   // if (min_front_dist < 1.5) {
// //   //     path_is_blocked = true;
// //   //     // Instead of 0.0, we use a "Crawl Speed" (0.1) so the robot can still steer
// //   //     ttc_scale = 0.1; 
// //   //     RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
// //   //         "!!! [PATH BLOCKED] Obstacle at %.2fm - Switching to CRAWL & DETOUR mode !!!", min_front_dist);
// //   // } 
  
// //   // // Hard Stop ONLY if something is physically touching our tracks (< 0.6m)
// //   // if (min_front_dist < 0.6 || min_teammate_dist < 0.6) {
// //   //     ttc_scale = 0.0;
// //   //     RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 500, "!!! EMERGENCY COLLISION BRAKE !!!");
// //   // }
// //   // // ------------------------------------

// //   // // ─────────────── COMMAND GENERATION ───────────────
// //   // geometry_msgs::msg::Twist cmd;

// //   // if (dist_err < 0.6 || (leader_speed < 0.05 && dist_err < 1.0)) {
// //   //   stop_robot();
// //   // } else {
// //   //   // double steer = tf2NormalizeAngle(best_yaw - my_yaw);
// //   //   double steer = normalize_angle(best_yaw - my_yaw);


// //   //   cmd.linear.x = std::min(1.1, leader_speed + 0.25 * dist_err) * ttc_scale;
// //   //   cmd.angular.z = 1.8 * steer;

// //   //   if (std::abs(steer) > 0.8) {
// //   //     cmd.linear.x = 0.05;
// //   //   }

// //   //   pub_cmd_->publish(cmd);
// //   // }

// //   // RCLCPP_INFO_THROTTLE(
// //   //   this->get_logger(), *this->get_clock(), 1000,
// //   //   "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s",
// //   //   form_name.c_str(), // Replaced the inline string with form_name
// //   //   cmd.linear.x, min_teammate_dist,
// //   //   (ttc_scale == 0.0 ? "HALT" : "OK"));

// //   // ─────────────── TARGET POSITION ───────────────
// //     // === FIXED TARGET POSITION (TERRAIN STABLE) ===
// //   bool is_combat = (last_leader_msg_.target_locked && last_leader_msg_.swarm_state >= 3);
// //   double target_x, target_y;

// //   if (is_combat) {
// //     // === FIRE NET / ENCIRCLE MODE === (your existing combat code - unchanged)
// //     int my_id = 2;
// //     size_t underscore_pos = my_ns_.find("_");
// //     if (underscore_pos != std::string::npos && underscore_pos + 1 < my_ns_.length()) {
// //       try { my_id = std::stoi(my_ns_.substr(underscore_pos + 1)); } catch (...) {}
// //     }
// //     double tx = last_leader_msg_.target_pos.x;
// //     double ty = last_leader_msg_.target_pos.y;
// //     double lx = last_leader_msg_.pose.position.x;
// //     double ly = last_leader_msg_.pose.position.y;
// //     double base_angle = std::atan2(ly - ty, lx - tx);

// //     double angle_offset = 0.0;
// //     if (my_id == 2) angle_offset = -M_PI / 3.0;
// //     else if (my_id == 3) angle_offset = M_PI / 3.0;
// //     else if (my_id == 4) angle_offset = -2.0 * M_PI / 3.0;
// //     else if (my_id == 5) angle_offset = 2.0 * M_PI / 3.0;
// //     else if (my_id == 6) angle_offset = -M_PI;
// //     else if (my_id == 7) angle_offset = M_PI;

// //     double final_angle = normalize_angle(base_angle + angle_offset);
// //     const double radius = 5.0;
// //     target_x = tx + radius * std::cos(final_angle);
// //     target_y = ty + radius * std::sin(final_angle);
// //   } else {
// //     // === NORMAL FORMATION - STABILIZED FOR HILLY TERRAIN ===
// //     double formation_yaw;
// //     if (last_leader_msg_.next_waypoints.size() >= 2) {
// //       double path_dx = last_leader_msg_.next_waypoints[1].position.x - last_leader_msg_.next_waypoints[0].position.x;
// //       double path_dy = last_leader_msg_.next_waypoints[1].position.y - last_leader_msg_.next_waypoints[0].position.y;
// //       double raw_path_yaw = std::atan2(path_dy, path_dx);

// //       if (!has_smoothed_path_) {
// //         smoothed_path_yaw_ = raw_path_yaw;
// //         has_smoothed_path_ = true;
// //       } else {
// //         double angle_diff = normalize_angle(raw_path_yaw - smoothed_path_yaw_);
// //         smoothed_path_yaw_ = normalize_angle(smoothed_path_yaw_ + angle_diff * 0.18);  // ← key fix
// //       }
// //       formation_yaw = smoothed_path_yaw_;
// //     } else {
// //       // fallback smoothed leader yaw
// //       static double smoothed_leader_yaw = l_yaw;
// //       double angle_diff = normalize_angle(l_yaw - smoothed_leader_yaw);
// //       smoothed_leader_yaw = normalize_angle(smoothed_leader_yaw + angle_diff * 0.1);
// //       formation_yaw = smoothed_leader_yaw;
// //     }

// //     target_x = last_leader_msg_.pose.position.x +
// //                (current_off_back * std::cos(formation_yaw)) -
// //                (current_off_side * std::sin(formation_yaw)) + repulse_x;

// //     target_y = last_leader_msg_.pose.position.y +
// //                (current_off_back * std::sin(formation_yaw)) +
// //                (current_off_side * std::cos(formation_yaw)) + repulse_y;
// //   }
  
// //   double dx_err = target_x - my_x;
// //   double dy_err = target_y - my_y;
// //   double dist_err = std::hypot(dx_err, dy_err);
// //   double angle_to_target = std::atan2(dy_err, dx_err);

// //   // ─────────────── FRONT CLEARANCE CHECK ───────────────
// //   double min_front_dist = 10.0;
// //   for (const auto& p : obstacle_points_) {
// //     float angle = std::atan2(p.y, p.x);
// //     if (std::abs(angle) < 0.7) {
// //       double d = std::hypot(p.x, p.y);
// //       min_front_dist = std::min(min_front_dist, d);
// //     }
// //   }

// //   double ttc_scale = (min_front_dist < ttc_danger_dist_) ? std::max(0.2, min_front_dist / ttc_danger_dist_) : 1.0;
// //   if (min_front_dist < 1.0 || min_teammate_dist < 0.8) { ttc_scale = 0.0; }

// //   // ─────────────── LOCAL PATH SAMPLING (simple) ───────────────
// //   double best_yaw = my_yaw;
// //   double min_score = 9999.0;

// //   for (double angle = -M_PI/2; angle <= M_PI/2; angle += 0.15) {
// //     double check_yaw = my_yaw + angle;
// //     double diff = normalize_angle(check_yaw - angle_to_target);

// //     bool collision = false;
// //     for (const auto& p : obstacle_points_) {
// //       double px_r = p.x * cos(-angle) - p.y * sin(-angle);
// //       double py_r = p.x * sin(-angle) + p.y * cos(-angle);
// //       if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) {
// //         collision = true; break;
// //       }
// //     }
// //     if (!collision && std::abs(diff) < min_score) { min_score = std::abs(diff); best_yaw = check_yaw; }
// //   }

// //   // --- TACTICAL ALARM LOGIC ---
// //   if (min_front_dist < 1.5) {
// //       ttc_scale = 0.1; 
// //       RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
// //           "!!! [PATH BLOCKED] Obstacle at %.2fm - Switching to CRAWL mode !!!", min_front_dist);
// //   } 
// //   if (min_front_dist < 0.6 || min_teammate_dist < 0.6) {
// //       ttc_scale = 0.0;
// //       RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 500, "!!! EMERGENCY COLLISION BRAKE !!!");
// //   }

// //   // ─────────────── COMMAND GENERATION ───────────────
// //   geometry_msgs::msg::Twist cmd;

// //   // Widen the combat goal tolerance to 0.8m to stop "stuttering/wiggling"
// //   bool reached_nav_goal = (!is_combat && leader_speed < 0.05 && dist_err < 1.0);
// //   bool reached_combat_goal = (is_combat && dist_err < 0.8); 

// //   if (dist_err < 0.3 || reached_nav_goal || reached_combat_goal) {
// //       if (is_combat) {
// //           // WE REACHED OUR COMBAT SLOT -> SPIN TO FACE THE ENEMY!
// //           double angle_to_enemy = std::atan2(last_leader_msg_.target_pos.y - my_y, 
// //                                              last_leader_msg_.target_pos.x - my_x);
          
// //           // CRITICAL FIX: The Shortest-Path Angular Wrapper
// //           // This prevents SH_04 from getting stuck at the -180 / +180 boundary
// //           double steer_to_enemy = angle_to_enemy - my_yaw;
// //           while (steer_to_enemy > M_PI) steer_to_enemy -= 2.0 * M_PI;
// //           while (steer_to_enemy < -M_PI) steer_to_enemy += 2.0 * M_PI;
          
// //           if (std::abs(steer_to_enemy) > 0.08) {
// //               cmd.linear.x = 0.0;
// //               cmd.angular.z = 1.5 * steer_to_enemy; // Spin in place to aim
// //           } else {
// //               cmd.linear.x = 0.0;
// //               cmd.angular.z = 0.0; // Locked on and ready to fire!
// //           }
// //       } else {
// //           cmd.linear.x = 0.0;
// //           cmd.angular.z = 0.0;
// //       }
// //       pub_cmd_->publish(cmd);
// //   } else {
// //       // Move towards target
// //       // Use the same robust angular wrapper for driving
// //       double steer = best_yaw - my_yaw;
// //       while (steer > M_PI) steer -= 2.0 * M_PI;
// //       while (steer < -M_PI) steer += 2.0 * M_PI;
      
// //       // If in combat, ignore leader speed! Just drive aggressively to the slot.
// //       double desired_speed = is_combat ? (0.8 * dist_err) : (leader_speed + 0.25 * dist_err);
      
// //       cmd.linear.x = std::min(1.1, desired_speed) * ttc_scale;
// //       cmd.angular.z = 1.8 * steer;

// //       if (std::abs(steer) > 0.8) {
// //           cmd.linear.x = 0.05; // Pivot heavily if we need to turn sharply
// //       }
// //       pub_cmd_->publish(cmd);
// //   }

// //   RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
// //     "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s",
// //     is_combat ? "ENCIRCLE" : form_name.c_str(), cmd.linear.x, min_teammate_dist, (ttc_scale == 0.0 ? "HALT" : "OK"));

// // }


// void RobustFollower::control_loop()
// {
//   if (!has_leader_ || !has_scan_) return;

//   double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
  
//   // --- SAFETY LOCKS ---
//   static rclcpp::Time spawn_time = this->get_clock()->now();
//   if ((this->get_clock()->now() - spawn_time).seconds() < 3.0) {
//       stop_robot();
//       RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "INITIALIZING: Settling...");
//       return;
//   }
//   if (leader_speed < 0.05 && last_leader_msg_.swarm_state == 0) {
//       stop_robot();
//       RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "STANDBY: Leader stationary.");
//       return;
//   }
//   if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) {
//     stop_robot(); return;
//   }
//   if (current_local_role_ == 2) return;  // LEADER

//   geometry_msgs::msg::TransformStamped tf_now;
//   try {
//     tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero, std::chrono::milliseconds(50));
//   } catch (const tf2::TransformException & ex) {
//     RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TF LAG: Brakes applied.");
//     stop_robot(); return;
//   }

//   double my_x = tf_now.transform.translation.x;
//   double my_y = tf_now.transform.translation.y;

//   // =========================================================================
//   // CRITICAL FIX 1: 3D-SAFE YAW EXTRACTION (fixes pitch/roll on hills)
//   // =========================================================================
//   tf2::Quaternion q_my, q_lead;
//   tf2::fromMsg(tf_now.transform.rotation, q_my);
//   tf2::fromMsg(last_leader_msg_.pose.orientation, q_lead);

//   tf2::Vector3 v_forward(1.0, 0.0, 0.0);
//   tf2::Vector3 my_fwd = tf2::quatRotate(q_my, v_forward);
//   tf2::Vector3 l_fwd = tf2::quatRotate(q_lead, v_forward);

//   double my_yaw = std::atan2(my_fwd.y(), my_fwd.x());
//   double l_yaw  = std::atan2(l_fwd.y(), l_fwd.x());
//   // =========================================================================

//   // FORMATION ENGINE (your existing code)
//   double current_off_back = offset_dist_;
//   double current_off_side = offset_lateral_;
//   std::string form_name = "Wedge";

//   int my_id = 2; 
//   size_t underscore_pos = my_ns_.find("_");
//   if (underscore_pos != std::string::npos && underscore_pos + 1 < my_ns_.length()) {
//     try { my_id = std::stoi(my_ns_.substr(underscore_pos + 1)); } catch (...) {}
//   }

//   switch (last_leader_msg_.formation_type) {
//     case 1: current_off_side = 0.0; form_name = "Column"; break;
//     case 2: form_name = "Diamond";
//       if (my_id == 2)      { current_off_back = -3.0; current_off_side =  3.0; }
//       else if (my_id == 3) { current_off_back = -3.0; current_off_side = -3.0; }
//       else if (my_id == 4) { current_off_back = -6.0; current_off_side =  0.0; }
//       else { current_off_back = -6.0 - (1.5 * (my_id - 4)); current_off_side = 0.0; }
//       break;
//     case 3: form_name = "Forward-V"; current_off_back = std::abs(offset_dist_); current_off_side = offset_lateral_; break;
//     default: current_off_back = offset_dist_; current_off_side = offset_lateral_; form_name = "Wedge"; break;
//   }

//   // ─────────────── BOIDS SEPARATION ───────────────
//   double repulse_x = 0.0, repulse_y = 0.0;
//   double min_teammate_dist = 10.0;

//   if (has_swarm_) {
//     for (const auto& other_pose : swarm_poses_.poses) {
//       double dx = other_pose.position.x - my_x;
//       double dy = other_pose.position.y - my_y;
//       double d = std::hypot(dx, dy);
      
//       // --- CRITICAL FIX 1: INCREASE TO 0.6m TO IGNORE GPS GHOSTS ---
//       if (d < 0.6) continue; 
      
//       min_teammate_dist = std::min(min_teammate_dist, d);

//       if (d < separation_dist_) {
//         double force = (separation_dist_ - d) / d;
//         repulse_x -= dx * force;
//         repulse_y -= dy * force;
//       }
//     }
//   }

//   // TARGET POSITION
//   bool is_combat = (last_leader_msg_.target_locked && last_leader_msg_.swarm_state >= 3);
//   double target_x, target_y;

//   if (is_combat) {
//     double tx = last_leader_msg_.target_pos.x;
//     double ty = last_leader_msg_.target_pos.y;
//     double lx = last_leader_msg_.pose.position.x;
//     double ly = last_leader_msg_.pose.position.y;
//     double base_angle = std::atan2(ly - ty, lx - tx);

//     double angle_offset = 0.0;
//     if (my_id == 2) angle_offset = -M_PI / 3.0;
//     else if (my_id == 3) angle_offset = M_PI / 3.0;
//     else if (my_id == 4) angle_offset = -2.0 * M_PI / 3.0;
//     else if (my_id == 5) angle_offset = 2.0 * M_PI / 3.0;
//     else if (my_id == 6) angle_offset = -M_PI;
//     else if (my_id == 7) angle_offset = M_PI;

//     double final_angle = normalize_angle(base_angle + angle_offset);
//     target_x = tx + 5.0 * std::cos(final_angle);
//     target_y = ty + 5.0 * std::sin(final_angle);
//   } else {
//     double formation_yaw;
//     if (last_leader_msg_.next_waypoints.size() >= 2) {
//       double path_dx = last_leader_msg_.next_waypoints[1].position.x - last_leader_msg_.next_waypoints[0].position.x;
//       double path_dy = last_leader_msg_.next_waypoints[1].position.y - last_leader_msg_.next_waypoints[0].position.y;
//       double raw_path_yaw = std::atan2(path_dy, path_dx);

//       if (!has_smoothed_path_) {
//         smoothed_path_yaw_ = raw_path_yaw;
//         has_smoothed_path_ = true;
//       } else {
//         double angle_diff = normalize_angle(raw_path_yaw - smoothed_path_yaw_);
//         smoothed_path_yaw_ = normalize_angle(smoothed_path_yaw_ + angle_diff * 0.12);  // tuned for Route-66
//       }
//       formation_yaw = smoothed_path_yaw_;
//     } else {
//       static double smoothed_leader_yaw = l_yaw;
//       double angle_diff = normalize_angle(l_yaw - smoothed_leader_yaw);
//       smoothed_leader_yaw = normalize_angle(smoothed_leader_yaw + angle_diff * 0.1);
//       formation_yaw = smoothed_leader_yaw;
//     }

//     target_x = last_leader_msg_.pose.position.x +
//                (current_off_back * std::cos(formation_yaw)) -
//                (current_off_side * std::sin(formation_yaw)) + repulse_x;
//     target_y = last_leader_msg_.pose.position.y +
//                (current_off_back * std::sin(formation_yaw)) +
//                (current_off_side * std::cos(formation_yaw)) + repulse_y;
//   }

//   double dx_err = target_x - my_x;
//   double dy_err = target_y - my_y;
//   double dist_err = std::hypot(dx_err, dy_err);
//   double angle_to_target = std::atan2(dy_err, dx_err);

//   // FRONT CLEARANCE + TTC + LOCAL SAMPLING (your code)
//   double min_front_dist = 10.0;
//   for (const auto& p : obstacle_points_) {
//     float angle = std::atan2(p.y, p.x);
//     if (std::abs(angle) < 0.7) {
//       double d = std::hypot(p.x, p.y);
//       min_front_dist = std::min(min_front_dist, d);
//     }
//   }

//   double ttc_scale = (min_front_dist < ttc_danger_dist_) ? std::max(0.2, min_front_dist / ttc_danger_dist_) : 1.0;
//   if (min_front_dist < 1.0 || min_teammate_dist < 0.8) { ttc_scale = 0.0; }

//   double best_yaw = my_yaw;
//   double min_score = 9999.0;
//   for (double angle = -M_PI/2; angle <= M_PI/2; angle += 0.15) {
//     double check_yaw = my_yaw + angle;
//     double diff = normalize_angle(check_yaw - angle_to_target);
//     bool collision = false;
//     for (const auto& p : obstacle_points_) {
//       double px_r = p.x * cos(-angle) - p.y * sin(-angle);
//       double py_r = p.x * sin(-angle) + p.y * cos(-angle);
//       if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) { collision = true; break; }
//     }
//     if (!collision && std::abs(diff) < min_score) { min_score = std::abs(diff); best_yaw = check_yaw; }
//   }

//   if (min_front_dist < 1.5) {
//       ttc_scale = 0.1; 
//       RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "[PATH BLOCKED] Switching to CRAWL mode.");
//   } 
//   if (min_front_dist < 0.6 || min_teammate_dist < 0.6) {
//       ttc_scale = 0.0;
//       RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 500, "!!! EMERGENCY COLLISION BRAKE !!!");
//   }

//   // =========================================================================
//   // CORNERING MOMENTUM (prevents sliding on hills)
//   // =========================================================================
//   geometry_msgs::msg::Twist cmd;
//   bool reached_nav_goal = (!is_combat && leader_speed < 0.05 && dist_err < 1.0);
//   bool reached_combat_goal = (is_combat && dist_err < 0.8); 

//   if (dist_err < 0.3 || reached_nav_goal || reached_combat_goal) {
//       if (is_combat) {
//           double angle_to_enemy = std::atan2(last_leader_msg_.target_pos.y - my_y, last_leader_msg_.target_pos.x - my_x);
//           double steer_to_enemy = normalize_angle(angle_to_enemy - my_yaw);
//           if (std::abs(steer_to_enemy) > 0.08) {
//               cmd.linear.x = 0.0; cmd.angular.z = 1.5 * steer_to_enemy; 
//           } else {
//               cmd.linear.x = 0.0; cmd.angular.z = 0.0; 
//           }
//       } else { 
//           cmd.linear.x = 0.0; cmd.angular.z = 0.0; 
//       }
//       pub_cmd_->publish(cmd);
//   } else {
//       double steer = normalize_angle(best_yaw - my_yaw);
//       double desired_speed = is_combat ? (0.8 * dist_err) : (leader_speed + 0.35 * dist_err);
      
//       if (dist_err > 1.5 && desired_speed < 0.35) desired_speed = 0.35;  // keep momentum

//       cmd.linear.x = std::min(1.5, desired_speed) * ttc_scale;
//       cmd.angular.z = 2.8 * steer;

//       // DO NOT drop to 0.05 on sharp turns — keep traction on slopes
//       if (std::abs(steer) > 0.6) {
//           cmd.linear.x = std::max(0.3, cmd.linear.x * 0.4); 
//       }
//       pub_cmd_->publish(cmd);
//   }

//   RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
//     "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s",
//     is_combat ? "ENCIRCLE" : form_name.c_str(), cmd.linear.x, min_teammate_dist, (ttc_scale == 0.0 ? "HALT" : "OK"));
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





#define _USE_MATH_DEFINES
#include "skyhunter_control/follower_node.hpp"
#include <pcl/common/transforms.h>
#include <algorithm>
#include <cmath>
#include <tf2_eigen/tf2_eigen.hpp>
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

// void RobustFollower::leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg)
// {
//   last_leader_msg_ = *msg;
//   has_leader_ = true;
//   last_leader_time_ = this->get_clock()->now();
// }

void RobustFollower::leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg)
{
  last_leader_msg_ = *msg;
  has_leader_ = true;
  last_leader_time_ = this->get_clock()->now();

  // --- RECORD BREADCRUMBS ---
  geometry_msgs::msg::Point curr_l_pos = msg->pose.position;
  if (leader_breadcrumbs_.empty()) {
      leader_breadcrumbs_.push_back(curr_l_pos);
  } else {
      auto last_pos = leader_breadcrumbs_.back();
      double dist = std::hypot(curr_l_pos.x - last_pos.x, curr_l_pos.y - last_pos.y);
      if (dist > 0.2) { // Drop a breadcrumb every 20cm
          leader_breadcrumbs_.push_back(curr_l_pos);
          // Keep only the last 60 crumbs (12 meters of history)
          if (leader_breadcrumbs_.size() > 60) {
              leader_breadcrumbs_.erase(leader_breadcrumbs_.begin());
          }
      }
  }
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


// void RobustFollower::scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
// {
//   if (!has_leader_) return;

//   pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//   pcl::fromROSMsg(*msg, *raw_cloud);
//   if (raw_cloud->empty()) return;

//   geometry_msgs::msg::TransformStamped tf_lidar_to_base;
//   try { tf_lidar_to_base = tf_buffer_->lookupTransform(my_frame_, msg->header.frame_id, tf2::TimePointZero); } 
//   catch (...) { return; }

//   Eigen::Affine3d transform = tf2::transformToEigen(tf_lidar_to_base.transform);
//   pcl::PointCloud<pcl::PointXYZ>::Ptr aligned_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//   pcl::transformPointCloud(*raw_cloud, *aligned_cloud, transform);

//   geometry_msgs::msg::PointStamped leader_map, leader_local;
//   leader_map.header.frame_id = "map";
//   leader_map.point = last_leader_msg_.pose.position;

//   try {
//     auto tf_map_to_local = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
//     tf2::doTransform(leader_map, leader_local, tf_map_to_local);
//   } catch (...) { return; }

//   double lx_l = leader_local.point.x;
//   double ly_l = leader_local.point.y;

//   std::vector<pcl::PointXYZ> obs;

//   // --- TRACK GROUND DENSITY AHEAD ---
//   int ground_front = 0;
  
//   #pragma omp parallel
//   {
//     std::vector<pcl::PointXYZ> t_pts;
//     int local_ground_front = 0;

//     #pragma omp for nowait
//     for (size_t i = 0; i < aligned_cloud->size(); i += 5) {
//       const auto& p = aligned_cloud->points[i];
      
//       // Look further ahead (1.5m to 4.5m)
//       if (p.x > 1.5 && p.x < 4.5 && std::abs(p.y) < 0.8) {
//           if (p.z > -1.5 && p.z < 0.8) local_ground_front++;
//       }

//       // if (p.z < 0.7 || p.z > 2.0) continue;
//       if (p.z < 1.2 || p.z > 2.5) continue;

//       if ((p.x * p.x + p.y * p.y) < 0.25) continue; 
//       if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue; 
      
//       t_pts.push_back(p);
//     }

//     #pragma omp critical
//     {
//       obs.insert(obs.end(), t_pts.begin(), t_pts.end());
//       ground_front += local_ground_front;
//     }
//   }

//   // --- THE FOLLOWER CLIFF BRAKE ---
//   if (ground_front < 15) { // If the ground vanishes
//       // ONLY apply the brake if we are NOT in Convoy Mode
//       if (last_leader_msg_.formation_type == 1) {
//           RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
//               "CONVOY MODE: Ignoring Void, trusting Leader's Breadcrumbs.");
//       } else {
//           // Inject a SOLID WALL of obstacles so the follower stops
//           for (double vy = -1.5; vy <= 1.5; vy += 0.2) {
//               obs.push_back(pcl::PointXYZ(1.0, vy, 0.5)); 
//           }
//           RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 500, 
//               "FOLLOWER VOID DETECTED! Deploying Emergency Brake.");
//       }
//   }

//   obstacle_points_ = std::move(obs);
//   has_scan_ = true;
// }

void RobustFollower::scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
{
  if (!has_leader_) return;

  pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::fromROSMsg(*msg, *raw_cloud);
  if (raw_cloud->empty()) return;

  geometry_msgs::msg::TransformStamped tf_lidar_to_base;
  try { tf_lidar_to_base = tf_buffer_->lookupTransform(my_frame_, msg->header.frame_id, tf2::TimePointZero); } 
  catch (...) { return; }

  Eigen::Affine3d transform = tf2::transformToEigen(tf_lidar_to_base.transform);
  pcl::PointCloud<pcl::PointXYZ>::Ptr aligned_cloud(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::transformPointCloud(*raw_cloud, *aligned_cloud, transform);

  geometry_msgs::msg::PointStamped leader_map, leader_local;
  leader_map.header.frame_id = "map";
  leader_map.point = last_leader_msg_.pose.position;

  try {
    auto tf_map_to_local = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero);
    tf2::doTransform(leader_map, leader_local, tf_map_to_local);
  } catch (...) { return; }

  double lx_l = leader_local.point.x;
  double ly_l = leader_local.point.y;

  std::vector<pcl::PointXYZ> obs;

  // --- TRACK GROUND DENSITY AHEAD ---
  int ground_front = 0;
  
  #pragma omp parallel
  {
    std::vector<pcl::PointXYZ> t_pts;
    int local_ground_front = 0;

    #pragma omp for nowait
    for (size_t i = 0; i < aligned_cloud->size(); i += 5) {
      const auto& p = aligned_cloud->points[i];
      
      // --- 1. CLIFF DETECTION LOGIC ---
      // Look further ahead (1.5m to 4.5m) to check if ground exists
      if (p.x > 1.5 && p.x < 4.5 && std::abs(p.y) < 0.8) {
          if (p.z > -1.5 && p.z < 0.8) local_ground_front++;
      }

      // --- 2. OBSTACLE AVOIDANCE LOGIC (RESTORED) ---
      // Ignore Floor and Sky. Only look at objects at chassis height (0.2m to 1.2m)
      if (p.z < 0.2 || p.z > 1.2) continue; 
      
      // Ignore Self (points too close to the sensor)
      if ((p.x * p.x + p.y * p.y) < 0.25) continue; 
      
      // Ignore the Leader (so we don't treat the leader as an obstacle to avoid)
      if (std::hypot(p.x - lx_l, p.y - ly_l) < 1.2) continue; 
      
      // Keep this point as a valid obstacle!
      t_pts.push_back(p);
    }

    #pragma omp critical
    {
      obs.insert(obs.end(), t_pts.begin(), t_pts.end());
      ground_front += local_ground_front;
    }
  }

  // --- 3. THE FOLLOWER CLIFF BRAKE ---
  if (ground_front < 15) { // If the ground vanishes
      // ONLY apply the brake if we are NOT in Convoy Mode
      if (last_leader_msg_.formation_type == 1) {
          RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
              "CONVOY MODE: Ignoring Void, trusting Leader's Breadcrumbs.");
      } else {
          // Inject a SOLID WALL of obstacles so the follower stops
          for (double vy = -1.5; vy <= 1.5; vy += 0.2) {
              obs.push_back(pcl::PointXYZ(1.0, vy, 0.5)); // 1.0m directly in front
          }
          RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 500, 
              "FOLLOWER VOID DETECTED! Deploying Emergency Brake.");
      }
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

  double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
  
  // --- SAFETY LOCKS ---
  static rclcpp::Time spawn_time = this->get_clock()->now();
  if ((this->get_clock()->now() - spawn_time).seconds() < 3.0) {
      stop_robot();
      RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "INITIALIZING: Settling...");
      return;
  }
  if (leader_speed < 0.05 && last_leader_msg_.swarm_state == 0) {
      stop_robot();
      RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "STANDBY: Leader stationary.");
      return;
  }
  if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) {
    stop_robot(); return;
  }
  if (current_local_role_ == 2) return;  // LEADER

  geometry_msgs::msg::TransformStamped tf_now;
  try {
    tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero, std::chrono::milliseconds(50));
  } catch (const tf2::TransformException & ex) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TF LAG: Brakes applied.");
    stop_robot(); return;
  }

  double my_x = tf_now.transform.translation.x;
  double my_y = tf_now.transform.translation.y;
  // --- DIAGNOSTIC: REAL DISTANCE TO LEADER BODY ---
  double dist_to_leader = std::hypot(last_leader_msg_.pose.position.x - my_x,
                                      last_leader_msg_.pose.position.y - my_y);

  // =========================================================================
  //  3D-SAFE YAW EXTRACTION 
  // =========================================================================
  tf2::Quaternion q_my, q_lead;
  tf2::fromMsg(tf_now.transform.rotation, q_my);
  tf2::fromMsg(last_leader_msg_.pose.orientation, q_lead);

  tf2::Vector3 v_forward(1.0, 0.0, 0.0);
  tf2::Vector3 my_fwd = tf2::quatRotate(q_my, v_forward);
  tf2::Vector3 l_fwd = tf2::quatRotate(q_lead, v_forward);

  double my_yaw = std::atan2(my_fwd.y(), my_fwd.x());
  double l_yaw  = std::atan2(l_fwd.y(), l_fwd.x());
  // =========================================================================

  static int last_form = 0;
  bool is_transitioning = false;
  if (last_leader_msg_.formation_type == 1 && last_form == 0) {
      RCLCPP_WARN(this->get_logger(), "NARROW GAP! Aligning behind leader...");
  }
  last_form = last_leader_msg_.formation_type;

  // FORMATION ENGINE (your existing code)
  double current_off_back = offset_dist_;
  double current_off_side = offset_lateral_;
  std::string form_name = "Wedge";

  int my_id = 2; 
  size_t underscore_pos = my_ns_.find("_");
  if (underscore_pos != std::string::npos && underscore_pos + 1 < my_ns_.length()) {
    try { my_id = std::stoi(my_ns_.substr(underscore_pos + 1)); } catch (...) {}
  }

  switch (last_leader_msg_.formation_type) {
    case 1: current_off_side = 0.0; form_name = "Column"; break;
    case 2: form_name = "Diamond";
      if (my_id == 2)      { current_off_back = -3.0; current_off_side =  3.0; }
      else if (my_id == 3) { current_off_back = -3.0; current_off_side = -3.0; }
      else if (my_id == 4) { current_off_back = -6.0; current_off_side =  0.0; }
      else { current_off_back = -6.0 - (1.5 * (my_id - 4)); current_off_side = 0.0; }
      break;
    case 3: form_name = "Forward-V"; current_off_back = std::abs(offset_dist_); current_off_side = offset_lateral_; break;
    default: current_off_back = offset_dist_; current_off_side = offset_lateral_; form_name = "Wedge"; break;
  }

  // ─────────────── BOIDS SEPARATION ───────────────
  double repulse_x = 0.0, repulse_y = 0.0;
  double min_teammate_dist = 10.0;

  if (has_swarm_) {
    for (const auto& other_pose : swarm_poses_.poses) {
      double dx = other_pose.position.x - my_x;
      double dy = other_pose.position.y - my_y;
      double d = std::hypot(dx, dy);
      
      // --- CRITICAL FIX 1: INCREASE TO 0.6m TO IGNORE GPS GHOSTS ---
      if (d < 0.6) continue; 
      
      min_teammate_dist = std::min(min_teammate_dist, d);

      if (d < separation_dist_) {
        double force = (separation_dist_ - d) / d;
        repulse_x -= dx * force;
        repulse_y -= dy * force;
      }
    }
  }
  
  // ─────────────── TARGET POSITION ───────────────
  bool is_combat = (last_leader_msg_.target_locked && last_leader_msg_.swarm_state >= 3);
  double target_x, target_y;

  if (is_combat) {
    // ... (Keep your existing Fire Net / Encircle code here exactly as it is) ...
    double tx = last_leader_msg_.target_pos.x;
    double ty = last_leader_msg_.target_pos.y;
    double lx = last_leader_msg_.pose.position.x;
    double ly = last_leader_msg_.pose.position.y;
    double base_angle = std::atan2(ly - ty, lx - tx);

    double angle_offset = 0.0;
    if (my_id == 2) angle_offset = -M_PI / 3.0;
    else if (my_id == 3) angle_offset = M_PI / 3.0;
    else if (my_id == 4) angle_offset = -2.0 * M_PI / 3.0;
    else if (my_id == 5) angle_offset = 2.0 * M_PI / 3.0;
    else if (my_id == 6) angle_offset = -M_PI;
    else if (my_id == 7) angle_offset = M_PI;

    double final_angle = normalize_angle(base_angle + angle_offset);
    target_x = tx + 5.0 * std::cos(final_angle);
    target_y = ty + 5.0 * std::sin(final_angle);
  } 
  else if (last_leader_msg_.formation_type == 1 && !leader_breadcrumbs_.empty()) {
      // =====================================================================
      // CONVOY PROTOCOL: THE SMOOTH HIGHWAY MERGE
      // =====================================================================
      double target_distance_behind = std::abs(current_off_back);
      
      // Calculate lateral error to see if we are transitioning
      double dx_me = my_x - last_leader_msg_.pose.position.x;
      double dy_me = my_y - last_leader_msg_.pose.position.y;
      double lateral_error = std::abs(dx_me * std::sin(-l_yaw) + dy_me * std::cos(-l_yaw));
      
      if (lateral_error > 0.4) {
          is_transitioning = true;
          // THE FIX: Aim at a breadcrumb 1.5m AHEAD of my target slot.
          // This forces a smooth diagonal merge instead of a 90-degree spin!
          target_distance_behind = std::max(0.5, std::abs(current_off_back) - 1.5); 
      }

      geometry_msgs::msg::Point target_crumb = leader_breadcrumbs_.front(); 
      double accumulated_dist = 0.0;
      
      // Traverse history backwards to find the target point
      for (int i = leader_breadcrumbs_.size() - 1; i >= 1; --i) {
          double segment = std::hypot(leader_breadcrumbs_[i].x - leader_breadcrumbs_[i-1].x,
                                      leader_breadcrumbs_[i].y - leader_breadcrumbs_[i-1].y);
          accumulated_dist += segment;
          if (accumulated_dist >= target_distance_behind) {
              target_crumb = leader_breadcrumbs_[i-1];
              break;
          }
      }
      target_x = target_crumb.x;
      target_y = target_crumb.y;
      
      // Override Boids Repulsion during narrow cliff driving
      repulse_x = 0.0; repulse_y = 0.0; 
  }
  else {
    // === NORMAL V-SHAPE FORMATION ===
    double formation_yaw;
    if (last_leader_msg_.next_waypoints.size() >= 2) {
      double path_dx = last_leader_msg_.next_waypoints[1].position.x - last_leader_msg_.next_waypoints[0].position.x;
      double path_dy = last_leader_msg_.next_waypoints[1].position.y - last_leader_msg_.next_waypoints[0].position.y;
      double raw_path_yaw = std::atan2(path_dy, path_dx);

      if (!has_smoothed_path_) {
        smoothed_path_yaw_ = raw_path_yaw;
        has_smoothed_path_ = true;
      } else {
        double angle_diff = normalize_angle(raw_path_yaw - smoothed_path_yaw_);
        smoothed_path_yaw_ = normalize_angle(smoothed_path_yaw_ + angle_diff * 0.12); 
      }
      formation_yaw = smoothed_path_yaw_;
    } else {
      static double smoothed_leader_yaw = l_yaw;
      double angle_diff = normalize_angle(l_yaw - smoothed_leader_yaw);
      smoothed_leader_yaw = normalize_angle(smoothed_leader_yaw + angle_diff * 0.1);
      formation_yaw = smoothed_leader_yaw;
    }

    target_x = last_leader_msg_.pose.position.x +
               (current_off_back * std::cos(formation_yaw)) -
               (current_off_side * std::sin(formation_yaw)) + repulse_x;
    target_y = last_leader_msg_.pose.position.y +
               (current_off_back * std::sin(formation_yaw)) +
               (current_off_side * std::cos(formation_yaw)) + repulse_y;
  }



  double dx_err = target_x - my_x;
  double dy_err = target_y - my_y;
  double dist_err = std::hypot(dx_err, dy_err);
  double angle_to_target = std::atan2(dy_err, dx_err);

  if (last_leader_msg_.formation_type == 1) {
      // Rotate the error into the Leader's frame to find "Side Error"
      double lateral_error = std::abs(dx_err * std::sin(-l_yaw) + dy_err * std::cos(-l_yaw));
      if (lateral_error > 0.3) { 
          is_transitioning = true; 
      }
  }

  // FRONT CLEARANCE + TTC + LOCAL SAMPLING (your code)
  double min_front_dist = 10.0;
  for (const auto& p : obstacle_points_) {
    float angle = std::atan2(p.y, p.x);
    if (std::abs(angle) < 0.7) {
      double d = std::hypot(p.x, p.y);
      min_front_dist = std::min(min_front_dist, d);
    }
  }

  double ttc_scale = (min_front_dist < ttc_danger_dist_) ? std::max(0.2, min_front_dist / ttc_danger_dist_) : 1.0;
  if (min_front_dist < 1.0 || min_teammate_dist < 0.8) { ttc_scale = 0.0; }

  double best_yaw = my_yaw;
  double min_score = 9999.0;
  for (double angle = -M_PI/2; angle <= M_PI/2; angle += 0.15) {
    double check_yaw = my_yaw + angle;
    double diff = normalize_angle(check_yaw - angle_to_target);
    bool collision = false;
    for (const auto& p : obstacle_points_) {
      double px_r = p.x * cos(-angle) - p.y * sin(-angle);
      double py_r = p.x * sin(-angle) + p.y * cos(-angle);
      if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) { collision = true; break; }
    }
    if (!collision && std::abs(diff) < min_score) { min_score = std::abs(diff); best_yaw = check_yaw; }
  }

  if (min_front_dist < 1.5) {
      ttc_scale = 0.1; 
      RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "[PATH BLOCKED] Switching to CRAWL mode.");
  } 
  if (min_front_dist < 0.6 || min_teammate_dist < 0.6) {
      ttc_scale = 0.0;
      RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 500, "!!! EMERGENCY COLLISION BRAKE !!!");
  }

  // =========================================================================
  // CORNERING MOMENTUM (prevents sliding on hills)
  // =========================================================================
  geometry_msgs::msg::Twist cmd;
  bool reached_nav_goal = (!is_combat && leader_speed < 0.05 && dist_err < 1.0);
  bool reached_combat_goal = (is_combat && dist_err < 0.8); 

  if (dist_err < 0.3 || reached_nav_goal || reached_combat_goal) {
      if (is_combat) {
          double angle_to_enemy = std::atan2(last_leader_msg_.target_pos.y - my_y, last_leader_msg_.target_pos.x - my_x);
          double steer_to_enemy = normalize_angle(angle_to_enemy - my_yaw);
          if (std::abs(steer_to_enemy) > 0.08) {
              cmd.linear.x = 0.0; cmd.angular.z = 1.5 * steer_to_enemy; 
          } else {
              cmd.linear.x = 0.0; cmd.angular.z = 0.0; 
          }
      } else { 
          cmd.linear.x = 0.0; cmd.angular.z = 0.0; 
      }
      pub_cmd_->publish(cmd);
  } else {
      double steer = normalize_angle(best_yaw - my_yaw);
      double desired_speed = is_combat ? (0.8 * dist_err) : (leader_speed + 0.35 * dist_err);
      
      if (is_transitioning) {
          // Smooth merge speed. Keep moving forward while turning inward!
          desired_speed = 0.3; 
          RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TRANSITION: Diagonal merge into column...");
      } else {
          // Normal driving
          if (dist_err > 1.5 && desired_speed < 0.35) desired_speed = 0.35;  
      }

      cmd.linear.x = std::min(1.5, desired_speed) * ttc_scale;
      cmd.angular.z = 2.5 * steer; // Slightly softer steer for smoother curves

      // Soften the speed penalty on sharp turns so we don't stall on the edge
      if (std::abs(steer) > 0.5) {
          cmd.linear.x = std::max(0.15, cmd.linear.x * 0.5); 
      }
      pub_cmd_->publish(cmd);
  }

  // RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
  //   "Form: %s | Spd: %.2f | TeamDist: %.2f | TTC: %s",
  //   is_combat ? "ENCIRCLE" : form_name.c_str(), cmd.linear.x, min_teammate_dist, (ttc_scale == 0.0 ? "HALT" : "OK"));

  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
    "Form: %s | Spd: %.2f | LdrDist: %.2fm | TeamDist: %.2f | TTC: %s", 
    is_combat ? "ENCIRCLE" : form_name.c_str(), cmd.linear.x, dist_to_leader, min_teammate_dist, (ttc_scale == 0.0 ? "HALT" : "OK"));
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

