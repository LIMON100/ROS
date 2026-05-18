
#include "skyhunter_control/leader_node.hpp"
#include <cmath>
#include <limits>

LeaderNode::LeaderNode(const rclcpp::NodeOptions & options) : Node("leader_node", options) {
  this->declare_parameter<double>("waypoint_spacing", 10.0);
  this->declare_parameter<std::string>("map_frame", "map");
  this->declare_parameter<int>("initial_formation", 0);
  
  cmd_formation_type_ = this->get_parameter("initial_formation").as_int();
  spacing_config_ = this->get_parameter("waypoint_spacing").as_double();
  map_frame_      = this->get_parameter("map_frame").as_string();

  // --- Initialize TF ---
  tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

  publisher_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
  viz_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("leader_waypoints_viz", 10);

  // pub_cmd_vel_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

  auto qos = rclcpp::SensorDataQoS();
  
  sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>("odom", qos, std::bind(&LeaderNode::odom_callback, this, std::placeholders::_1));
  sub_plan_ = this->create_subscription<nav_msgs::msg::Path>("plan", rclcpp::QoS(10).reliable(), std::bind(&LeaderNode::plan_callback, this, std::placeholders::_1));
  sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>("scan/points", qos, std::bind(&LeaderNode::scan_callback, this, std::placeholders::_1));
  sub_form_cmd_ = this->create_subscription<std_msgs::msg::Int8>("/swarm/formation_command", 10, std::bind(&LeaderNode::formation_command_callback, this, std::placeholders::_1));
  sub_role_ = this->create_subscription<std_msgs::msg::Int8>("local_role", 10, std::bind(&LeaderNode::role_callback, this, std::placeholders::_1));
  sub_combat_state_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>("perception/combat_state", qos, std::bind(&LeaderNode::combat_state_callback, this, std::placeholders::_1));
  sub_gps_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
    "/gps/fix", qos, 
    [this](const sensor_msgs::msg::NavSatFix::SharedPtr msg) {
      this->latest_gps_ = *msg;
      this->has_gps_ = true;
    });

  sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
    "/swarm/poses", 10, 
    [this](const geometry_msgs::msg::PoseArray::SharedPtr msg) {
      this->latest_swarm_ = *msg;
    });

  timer_ = this->create_wall_timer(50ms, std::bind(&LeaderNode::timer_callback, this));
}

void LeaderNode::combat_state_callback(const skyhunter_msgs::msg::LeaderState::SharedPtr msg) { latest_combat_state_ = *msg; }
void LeaderNode::odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; has_odom_ = true; }
void LeaderNode::plan_callback(const nav_msgs::msg::Path::SharedPtr msg) { latest_path_ = *msg; has_path_ = true; }
void LeaderNode::formation_command_callback(const std_msgs::msg::Int8::SharedPtr msg) { cmd_formation_type_ = msg->data; }
void LeaderNode::role_callback(const std_msgs::msg::Int8::SharedPtr msg) { current_role_ = msg->data; }

void LeaderNode::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    pcl::PointCloud<pcl::PointXYZ> cloud;
    pcl::fromROSMsg(*msg, cloud);
    
    bool gap_detected = false;
    int ground_points_left = 0;
    int ground_points_right = 0;
    int ground_points_center = 0;

    for (const auto& p : cloud.points) {
        // --- TACTICAL GROUND FILTER ---
        // In the lidar_link frame, the floor is at roughly Z = -0.5 to -0.1
        // We only look at points on the floor in front of the robot (0.5m to 5.0m)
        if (p.x > 0.5 && p.x < 5.0 && p.z < 0.0) {
            
            // Check Right Side (Follower 3's lane)
            if (p.y < -0.8 && p.y > -2.5) ground_points_right++;
            
            // Check Left Side (Follower 2's lane)
            if (p.y > 0.8 && p.y < 2.5) ground_points_left++;

            // Check Center (My lane)
            if (std::abs(p.y) < 0.5) ground_points_center++;
        }

        // --- WALL DETECTION (Existing logic) ---
        if (p.z >= 0.4 && p.z <= 1.0) { 
            if (p.x > 0.5 && p.x < 5.0 && std::abs(p.y) < 1.5) { 
                gap_detected = true; // Physical wall detected
            }
        }
    }

    // --- LOGIC: THE VOID DETECTION ---
    if (ground_points_center > 20) { // I am on a road...
        if (ground_points_left < 5 || ground_points_right < 5) { // ...but the edges are gone!
            gap_detected = true;
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                "SENTINEL: Void detected on flanks! Road width is < 2.0m.");
        }
    }

    narrow_gap_detected_ = gap_detected;
}

double LeaderNode::calculate_remaining_dist(size_t start_idx, double global_x, double global_y) const {
  if (!has_path_ || start_idx >= latest_path_.poses.size() - 1) return 0.0;
  double dist = std::hypot(latest_path_.poses[start_idx].pose.position.x - global_x,
                           latest_path_.poses[start_idx].pose.position.y - global_y);
  for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i) {
    dist += std::hypot(latest_path_.poses[i+1].pose.position.x - latest_path_.poses[i].pose.position.x,
                       latest_path_.poses[i+1].pose.position.y - latest_path_.poses[i].pose.position.y);
  }
  return dist;
}

bool LeaderNode::get_waypoint_at_dist(double target_m, size_t start_idx, geometry_msgs::msg::Pose & out_pose, size_t & out_idx, double global_x, double global_y) const {
  if (!has_path_ || latest_path_.poses.size() < 2) return false;
  double acc = std::hypot(latest_path_.poses[start_idx].pose.position.x - global_x,
                          latest_path_.poses[start_idx].pose.position.y - global_y);
  for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i) {
    if (acc >= target_m) { out_pose = latest_path_.poses[i].pose; out_idx = i; return true; }
    acc += std::hypot(latest_path_.poses[i+1].pose.position.x - latest_path_.poses[i].pose.position.x,
                      latest_path_.poses[i+1].pose.position.y - latest_path_.poses[i].pose.position.y);
  }
  out_pose = latest_path_.poses.back().pose; out_idx = latest_path_.poses.size() - 1;
  return true;
}

void LeaderNode::timer_callback() {
  if (!has_odom_) return;

  // GET TRUE GLOBAL MAP POSITION ---
  geometry_msgs::msg::TransformStamped tf_now;
  try {
      // Leader is always "" namespace, so frame is base_footprint
      tf_now = tf_buffer_->lookupTransform("map", "base_footprint", tf2::TimePointZero);
  } catch (...) {
      return; // Wait until TF is ready
  }

  double global_x = tf_now.transform.translation.x;
  double global_y = tf_now.transform.translation.y;


  // =========================================================================
  // STEP 3: LEADER "ANTI-STUCK" POSITION DETECTOR (GPS-BASED TRUTH)
  // =========================================================================
  static double last_lat = 0.0;
  static double last_lon = 0.0;
  static rclcpp::Time last_stuck_check_time = this->get_clock()->now();
  static int high_cmd_ticks = 0;
  static int total_ticks = 0;

  if (has_gps_) {
      total_ticks++;
      // Check if Nav2 is actively commanding the robot to move
      double current_cmd_vel = std::abs(latest_odom_.twist.twist.linear.x);
      if (current_cmd_vel > 0.2) high_cmd_ticks++;

      double dt_stuck = (this->get_clock()->now() - last_stuck_check_time).seconds();
      
      if (dt_stuck >= 3.0) {
          if (last_lat != 0.0 && last_lon != 0.0) {
              // Calculate TRUE PHYSICAL move in meters from satellites
              double d_lat = (latest_gps_.latitude - last_lat) * 111320.0;
              double d_lon = (latest_gps_.longitude - last_lon) * 111320.0 * std::cos(latest_gps_.latitude * M_PI / 180.0);
              double physical_move = std::hypot(d_lat, d_lon);

              // If Nav2 is pushing, but GPS says we didn't move 0.3m
              if ((float)high_cmd_ticks / total_ticks > 0.8 && physical_move < 0.3 && !is_recovering_) {
                  RCLCPP_ERROR(this->get_logger(), "!!! LEADER PHYSICALLY STUCK !!! Move: %.2fm in 3s. Overriding Nav2.", physical_move);
                  is_recovering_ = true;
                  
                  // Send emergency reverse + wiggle
                  geometry_msgs::msg::Twist rev_cmd;
                  rev_cmd.linear.x = -0.8;
                  rev_cmd.angular.z = -0.6;
                  for(int i=0; i<20; i++) pub_cmd_vel_->publish(rev_cmd);
                  
                  auto timer = this->create_wall_timer(2s, [this]() { this->is_recovering_ = false; });
              }
          }
          last_lat = latest_gps_.latitude;
          last_lon = latest_gps_.longitude;
          last_stuck_check_time = this->get_clock()->now();
          high_cmd_ticks = 0; total_ticks = 0;
      }
  }

  double vel = std::abs(latest_odom_.twist.twist.linear.x);
  if (vel > 0.1) current_state_ = STATE_NAVIGATING;
  else if (has_path_ && calculate_remaining_dist(0, global_x, global_y) < 0.5) current_state_ = STATE_GOAL_REACHED;
  else current_state_ = STATE_TRANSITIONING;

  auto state_msg = skyhunter_msgs::msg::LeaderState();
  state_msg.header.stamp = this->get_clock()->now();
  
  // --- SEND TRUE GLOBAL POSE TO FOLLOWERS ---
  state_msg.pose.position.x = tf_now.transform.translation.x;
  state_msg.pose.position.y = tf_now.transform.translation.y;
  state_msg.pose.position.z = tf_now.transform.translation.z;
  state_msg.pose.orientation = tf_now.transform.rotation;
  
  // Velocity is local, so latest_odom_ is fine
  state_msg.velocity = latest_odom_.twist.twist;

  if (latest_combat_state_.target_locked) {
      state_msg.target_locked = true;
      state_msg.target_pos = latest_combat_state_.target_pos;
      state_msg.swarm_state = latest_combat_state_.swarm_state;
  } else {
      state_msg.target_locked = false;
      state_msg.swarm_state = current_state_;
  }

  if (narrow_gap_detected_) state_msg.formation_type = 1;  
  else state_msg.formation_type = cmd_formation_type_;

  visualization_msgs::msg::MarkerArray markers;

  // ---(DRIVING DIRECTION ARROW) ---
  visualization_msgs::msg::Marker arrow;
  arrow.header.frame_id = map_frame_;
  arrow.header.stamp = this->get_clock()->now();
  arrow.ns = "leader_direction";
  arrow.id = 999;
  arrow.type = visualization_msgs::msg::Marker::ARROW;
  arrow.action = visualization_msgs::msg::Marker::ADD;
  arrow.pose = latest_odom_.pose.pose; // Start at leader
  // Scale: x is length, y is width, z is height
  arrow.scale.x = 2.0; 
  arrow.scale.y = 0.3;
  arrow.scale.z = 0.3;
  arrow.color.a = 1.0; arrow.color.r = 0.0; arrow.color.g = 1.0; arrow.color.b = 0.0; // Green Arrow
  markers.markers.push_back(arrow);
  // --------------------------------------------------------

  if (has_path_ && !latest_path_.poses.empty()) {
    size_t closest_idx = 0;
    double min_d = std::numeric_limits<double>::max();

    for (size_t i = 0; i < latest_path_.poses.size(); ++i) {
      double d = std::hypot(global_x - latest_path_.poses[i].pose.position.x,
                            global_y - latest_path_.poses[i].pose.position.y);
      if (d < min_d) { min_d = d; closest_idx = i; }
    }

    double remaining = calculate_remaining_dist(closest_idx, global_x, global_y);
    double tactical_spacing = std::min(spacing_config_, std::max(0.0, remaining - 2.0));

    geometry_msgs::msg::Pose wp1, wp2; size_t wp1_idx = 0, wp2_idx = 0;
    if (get_waypoint_at_dist(tactical_spacing, closest_idx, wp1, wp1_idx, global_x, global_y)) {
      state_msg.next_waypoints.push_back(wp1);
      markers.markers.push_back(create_marker(0, wp1, 0.0f, 0.0f, 1.0f));
      if (get_waypoint_at_dist(tactical_spacing, wp1_idx, wp2, wp2_idx, wp1.position.x, wp1.position.y)) {
        state_msg.next_waypoints.push_back(wp2);
        markers.markers.push_back(create_marker(1, wp2, 1.0f, 0.0f, 0.0f));
      }
    }
  }

  int follower_id = 100;
  for (const auto& pose : latest_swarm_.poses) {
      // Don't draw a marker on the leader itself
      double dist = std::hypot(pose.position.x - global_x, pose.position.y - global_y);
      if (dist > 1.0) {
          visualization_msgs::msg::Marker f_marker;
          f_marker.header.frame_id = map_frame_;
          f_marker.header.stamp = this->get_clock()->now();
          f_marker.ns = "friendly_tracker";
          f_marker.id = follower_id++;
          f_marker.type = visualization_msgs::msg::Marker::CYLINDER;
          f_marker.action = visualization_msgs::msg::Marker::ADD;
          f_marker.pose = pose;
          f_marker.scale.x = 0.8; f_marker.scale.y = 0.8; f_marker.scale.z = 1.0;
          f_marker.color.a = 0.9; f_marker.color.r = 0.0; f_marker.color.g = 1.0; f_marker.color.b = 0.0; // Bright Green
          markers.markers.push_back(f_marker);
      }
  }

  publisher_->publish(state_msg);
  viz_pub_->publish(markers);
}

visualization_msgs::msg::Marker LeaderNode::create_marker(int id, const geometry_msgs::msg::Pose & pose, float r, float g, float b) {
  visualization_msgs::msg::Marker m;
  m.header.frame_id = map_frame_; m.header.stamp = this->get_clock()->now();
  m.ns = "tactical_wp"; m.id = id; m.type = visualization_msgs::msg::Marker::CYLINDER;
  m.action = visualization_msgs::msg::Marker::ADD; m.pose = pose;
  m.scale.x = 0.5; m.scale.y = 0.5; m.scale.z = 0.1;
  m.color.a = 0.8f; m.color.r = r; m.color.g = g; m.color.b = b;
  return m;
}

int main(int argc, char * argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LeaderNode>());
  rclcpp::shutdown();
  return 0;
}