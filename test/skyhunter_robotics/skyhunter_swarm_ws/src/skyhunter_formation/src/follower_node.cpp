
// FULL WORKABLE with BOIDS 02-23
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
#include <geometry_msgs/msg/pose_array.hpp> 
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

using namespace std::chrono_literals;

class RobustFollower : public rclcpp::Node {
public:
  using NavigateToPose = nav2_msgs::action::NavigateToPose;

  RobustFollower() : Node("follower_node") {
    this->declare_parameter<double>("offset_dist", -2.5);      
    this->declare_parameter<double>("offset_lateral", 0.0);   
    this->declare_parameter<double>("ttc_danger_dist", 4.1);  
    this->declare_parameter<double>("blocking_radius", 0.8);
    this->declare_parameter<double>("separation_dist", 1.5); 

    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    std::string ns = std::string(this->get_namespace());
    if (ns.length() > 1 && ns[0] == '/') ns = ns.substr(1);
    my_ns_ = ns;
    my_frame_ = my_ns_ + "/base_footprint";

    auto qos = rclcpp::SensorDataQoS();
    sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
        "/swarm/poses", qos, std::bind(&RobustFollower::swarm_cb, this, std::placeholders::_1));
    sub_leader_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
        "/leader_state", qos, std::bind(&RobustFollower::leader_cb, this, std::placeholders::_1));
    sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
        "scan/points", qos, std::bind(&RobustFollower::scan_cb, this, std::placeholders::_1));

    // Monitor the leader every 1 second
    succession_timer_ = this->create_wall_timer(
        1s, std::bind(&RobustFollower::check_leader_health, this));

    pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    timer_ = this->create_wall_timer(50ms, std::bind(&RobustFollower::control_loop, this));

    RCLCPP_INFO(this->get_logger(), "TACTICAL Follower [%s] Online.", my_ns_.c_str());
  }

private:
  void swarm_cb(const geometry_msgs::msg::PoseArray::SharedPtr msg) { swarm_poses_ = *msg; has_swarm_ = true; }
  
  void leader_cb(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
    last_leader_msg_ = *msg;
    has_leader_ = true;
    last_leader_time_ = this->get_clock()->now();
    if (!msg->next_waypoints.empty()) { shadow_mission_buffer_ = msg->next_waypoints; }
  }

  void check_leader_health() {
    if (is_promoted_to_leader_) return;
    auto now = this->get_clock()->now();
    double seconds_since_last_msg = (now - last_leader_time_).seconds();

    // 10 second timeout check (Requirement 2.13)
    if (has_leader_ && seconds_since_last_msg > 10.0 && my_ns_ == "SH_02") {
        initiate_succession();
    }
  }

  void initiate_succession() {
      if (is_promoted_to_leader_) return;
      is_promoted_to_leader_ = true;

      RCLCPP_WARN(this->get_logger(), "!!! LEADER LOST !!! SH_02 is taking command.");

      // Start the Leader Broadcast immediately so others follow me
      takeover_pub_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);

      // Give Nav2 a moment to finish its auto-activation from the launch file
      std::this_thread::sleep_for(std::chrono::seconds(3));

      if (!shadow_mission_buffer_.empty()) {
          std::string action_path = "/" + my_ns_ + "/navigate_to_pose";
          auto action_client = rclcpp_action::create_client<NavigateToPose>(this, action_path);
          
          RCLCPP_INFO(this->get_logger(), "Connecting to SH_02 Action Server...");

          // Increase wait time to 15 seconds to allow for Lifecycle Manager completion
          if (action_client->wait_for_action_server(std::chrono::seconds(15))) {
              auto goal = NavigateToPose::Goal();
              goal.pose.header.frame_id = "map";
              goal.pose.pose = shadow_mission_buffer_[0]; 
              
              action_client->async_send_goal(goal);
              RCLCPP_INFO(this->get_logger(), "SUCCESSION: Navigating to next mission waypoint.");
          } else {
              RCLCPP_ERROR(this->get_logger(), "TAKEOVER FAILED: SH_02 Action Server not found. Check lifecycle status!");
          }
      }
  }

  void scan_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(*msg, *raw_cloud);
    if (raw_cloud->empty() || !has_leader_) return;
    geometry_msgs::msg::TransformStamped tf_l;
    try { tf_l = tf_buffer_->lookupTransform(my_frame_, "map", tf2::TimePointZero); } catch (...) { return; }
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

    geometry_msgs::msg::TransformStamped tf_now;
    try {
        tf_now = tf_buffer_->lookupTransform("map", my_frame_, tf2::TimePointZero);
    } catch (...) {
        return; // Wait for TF to be available
    }

    // --- 2. TAKEOVER BROADCAST (If I am now the Leader) ---
    if (is_promoted_to_leader_ && takeover_pub_) {
        auto msg = skyhunter_msgs::msg::LeaderState();
        msg.header.stamp = this->get_clock()->now();
        msg.header.frame_id = "map";
        
        // Use the tf_now we looked up at the start
        msg.pose.position.x = tf_now.transform.translation.x;
        msg.pose.position.y = tf_now.transform.translation.y;
        msg.pose.position.z = tf_now.transform.translation.z;
        msg.pose.orientation = tf_now.transform.rotation;
        
        msg.velocity = last_leader_msg_.velocity; 
        msg.next_waypoints = shadow_mission_buffer_; 
        msg.formation_type = 1; // Force Column
        
        takeover_pub_->publish(msg);
        return; // EXIT: We stop the follower math so Nav2 can drive the wheels
    }

    if (!has_leader_ || !has_scan_) return;

    if ((this->get_clock()->now() - last_leader_time_).seconds() > 1.5) { stop_robot(); return; }

    double my_x = tf_now.transform.translation.x;
    double my_y = tf_now.transform.translation.y;
    double my_yaw = tf2::getYaw(tf_now.transform.rotation);

    double l_yaw = tf2::getYaw(last_leader_msg_.pose.orientation);
    double off_back = this->get_parameter("offset_dist").as_double();
    double off_side = this->get_parameter("offset_lateral").as_double();
    double leader_speed = std::abs(last_leader_msg_.velocity.linear.x);
    double current_off_side = (last_leader_msg_.formation_type == 1) ? 0.0 : off_side;

    double repulse_x = 0.0, repulse_y = 0.0;
    double min_teammate_dist = 10.0;
    if (has_swarm_) {
        for (const auto& other_pose : swarm_poses_.poses) {
            double dx = other_pose.position.x - my_x;
            double dy = other_pose.position.y - my_y;
            double d = std::hypot(dx, dy);
            if (d < 0.5) continue; 
            if (d < min_teammate_dist) min_teammate_dist = d;
            double sep_limit = this->get_parameter("separation_dist").as_double();
            if (d < sep_limit) {
                double force = (sep_limit - d) / d;
                repulse_x -= dx * force; repulse_y -= dy * force;
            }
        }
    }

    double target_x, target_y;
    if (last_leader_msg_.next_waypoints.size() >= 1) {
        auto wp = last_leader_msg_.next_waypoints[0].position;
        target_x = wp.x - (current_off_side * std::sin(l_yaw)) + repulse_x;
        target_y = wp.y + (current_off_side * std::cos(l_yaw)) + repulse_y;
    } else {
        target_x = last_leader_msg_.pose.position.x + (off_back * std::cos(l_yaw)) - (current_off_side * std::sin(l_yaw)) + repulse_x;
        target_y = last_leader_msg_.pose.position.y + (off_back * std::sin(l_yaw)) + (current_off_side * std::cos(l_yaw)) + repulse_y;
    }

    double dx_err = target_x - my_x; double dy_err = target_y - my_y;
    double dist_err = std::hypot(dx_err, dy_err);
    double angle_to_target = std::atan2(dy_err, dx_err);

    double min_front_dist = 10.0;
    for (const auto& p : obstacle_points_) {
        float angle = std::atan2(p.y, p.x);
        if (std::abs(angle) < 0.7) { double d = std::hypot(p.x, p.y); if (d < min_front_dist) min_front_dist = d; }
    }
    double ttc_scale = (min_front_dist < 4.1) ? std::max(0.2, min_front_dist / 4.1) : 1.0;
    if (min_front_dist < 1.0 || min_teammate_dist < 0.8) ttc_scale = 0.0; 

    double best_yaw = my_yaw; double min_score = 9999.0;
    for (double angle = -1.57; angle <= 1.57; angle += 0.15) {
        double check_yaw = my_yaw + angle;
        double diff = check_yaw - angle_to_target;
        while(diff > M_PI) { diff -= 2*M_PI; }
        while(diff < -M_PI) { diff += 2*M_PI; }
        bool collision = false;
        for (const auto& p : obstacle_points_) {
            double px_r = p.x * cos(-angle) - p.y * sin(-angle);
            double py_r = p.x * sin(-angle) + p.y * cos(-angle);
            if (px_r > 0.0 && px_r < 3.0 && std::abs(py_r) < 0.6) { collision = true; break; }
        }
        if (!collision && std::abs(diff) < min_score) { min_score = std::abs(diff); best_yaw = check_yaw; }
    }

    geometry_msgs::msg::Twist cmd;
    if (dist_err < 0.6 || (leader_speed < 0.05 && dist_err < 1.0)) {
        stop_robot();
    } else {
        double steer = best_yaw - my_yaw;
        while(steer > M_PI) { steer -= 2*M_PI; } 
        while(steer < -M_PI) { steer += 2*M_PI; }
        cmd.linear.x = std::min(1.1, leader_speed + (0.25 * dist_err)) * ttc_scale;
        cmd.angular.z = 1.8 * steer;
        if (std::abs(steer) > 0.8) cmd.linear.x = 0.05; 
    }
    pub_cmd_->publish(cmd);
  }

  void stop_robot() { pub_cmd_->publish(geometry_msgs::msg::Twist()); }

  // --- MEMBERS ---
  std::vector<geometry_msgs::msg::Pose> shadow_mission_buffer_;
  bool is_promoted_to_leader_ = false, has_leader_ = false, has_scan_ = false, has_swarm_ = false;
  std::string my_ns_, my_frame_;
  skyhunter_msgs::msg::LeaderState last_leader_msg_;
  std::vector<pcl::PointXYZ> obstacle_points_;
  geometry_msgs::msg::PoseArray swarm_poses_;
  rclcpp::Time last_leader_time_;
  rclcpp::TimerBase::SharedPtr timer_, succession_timer_;
  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_leader_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
  rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr sub_swarm_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
  rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr takeover_pub_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

int main(int argc, char * argv[]) { rclcpp::init(argc, argv); rclcpp::spin(std::make_shared<RobustFollower>()); rclcpp::shutdown(); return 0; }