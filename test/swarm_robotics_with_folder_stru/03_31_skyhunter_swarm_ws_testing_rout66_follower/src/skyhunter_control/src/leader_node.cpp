#include "skyhunter_control/leader_node.hpp"

#include <cmath>
#include <limits>

LeaderNode::LeaderNode(const rclcpp::NodeOptions & options)
: Node("leader_node", options)
{
  // Parameters
  this->declare_parameter<double>("waypoint_spacing", 10.0);
  this->declare_parameter<std::string>("map_frame", "map");
  this->declare_parameter<int>("initial_formation", 0);
  cmd_formation_type_ = this->get_parameter("initial_formation").as_int();

  spacing_config_ = this->get_parameter("waypoint_spacing").as_double();
  map_frame_      = this->get_parameter("map_frame").as_string();

  // Publishers
  publisher_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>(
    "/leader_state", 10);

  viz_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(
    "leader_waypoints_viz", 10);

  // Subscribers
  sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(
    "odom", rclcpp::SensorDataQoS(),
    std::bind(&LeaderNode::odom_callback, this, std::placeholders::_1));

  sub_plan_ = this->create_subscription<nav_msgs::msg::Path>(
    "plan", rclcpp::QoS(10).reliable(),
    std::bind(&LeaderNode::plan_callback, this, std::placeholders::_1));

  sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
    "scan/points", rclcpp::SensorDataQoS(),
    std::bind(&LeaderNode::scan_callback, this, std::placeholders::_1));

  sub_form_cmd_ = this->create_subscription<std_msgs::msg::Int8>(
    "/swarm/formation_command", 10,
    std::bind(&LeaderNode::formation_command_callback, this, std::placeholders::_1));

  sub_role_ = this->create_subscription<std_msgs::msg::Int8>(
    "local_role", 10,
    std::bind(&LeaderNode::role_callback, this, std::placeholders::_1));

  sub_combat_state_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
    "perception/combat_state", rclcpp::SensorDataQoS(),
    std::bind(&LeaderNode::combat_state_callback, this, std::placeholders::_1));

  // Main loop — 20 Hz
  timer_ = this->create_wall_timer(
    50ms, std::bind(&LeaderNode::timer_callback, this));

  RCLCPP_INFO(this->get_logger(), "Tactical Intelligent Leader Online.");
}

void LeaderNode::combat_state_callback(const skyhunter_msgs::msg::LeaderState::SharedPtr msg)
{
  latest_combat_state_ = *msg;
}

void LeaderNode::odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
{
  latest_odom_ = *msg;
  has_odom_ = true;
}

void LeaderNode::plan_callback(const nav_msgs::msg::Path::SharedPtr msg)
{
  latest_path_ = *msg;
  has_path_ = true;
}

// void LeaderNode::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
// {
//   pcl::PointCloud<pcl::PointXYZ> cloud;
//   pcl::fromROSMsg(*msg, cloud);

//   bool narrow = false;
//   for (const auto& p : cloud.points)
//   {
//     if (p.z < -0.3 || p.z > 0.5) continue;

//     // ORIGINAL WIDTH CHECK
//     if (p.x > 0.5 && p.x < 4.0 && std::abs(p.y) < 1.8)
//     {
//       narrow = true;
//       break;
//     }
//   }
//   narrow_gap_detected_ = narrow;
// }


void LeaderNode::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    pcl::PointCloud<pcl::PointXYZ> cloud;
    pcl::fromROSMsg(*msg, cloud);
    
    bool narrow = false;
    for (const auto& p : cloud.points) {
        // Road surface on a slope usually appears between Z -0.4 and Z 0.2
        // Only look for obstacles that are at "Chassis" level (0.4m to 0.8m)
        if (p.z < 0.4 || p.z > 1.0) continue; // <--- STRICTER FILTER

        if (p.x > 0.5 && p.x < 5.0 && std::abs(p.y) < 1.5) {
            narrow = true;
            break;
        }
    }
    narrow_gap_detected_ = narrow;
}

void LeaderNode::formation_command_callback(const std_msgs::msg::Int8::SharedPtr msg)
{
  cmd_formation_type_ = msg->data;
  RCLCPP_INFO(
    this->get_logger(),
    "COMMAND RECEIVED: Switching Swarm to Mode %d",
    msg->data);
}

void LeaderNode::role_callback(const std_msgs::msg::Int8::SharedPtr msg)
{
  current_role_ = msg->data;
}

double LeaderNode::calculate_remaining_dist(size_t start_idx) const
{
  if (!has_path_ || start_idx >= latest_path_.poses.size() - 1)
    return 0.0;

  double dist = 0.0;
  for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i)
  {
    double dx = latest_path_.poses[i+1].pose.position.x -
                latest_path_.poses[i].pose.position.x;
    double dy = latest_path_.poses[i+1].pose.position.y -
                latest_path_.poses[i].pose.position.y;
    dist += std::hypot(dx, dy);
  }
  return dist;
}

bool LeaderNode::get_waypoint_at_dist(
  double target_m,
  size_t start_idx,
  geometry_msgs::msg::Pose & out_pose,
  size_t & out_idx) const
{
  if (!has_path_ || latest_path_.poses.size() < 2)
    return false;

  double acc = 0.0;
  for (size_t i = start_idx; i < latest_path_.poses.size() - 1; ++i)
  {
    double dx = latest_path_.poses[i+1].pose.position.x -
                latest_path_.poses[i].pose.position.x;
    double dy = latest_path_.poses[i+1].pose.position.y -
                latest_path_.poses[i].pose.position.y;
    acc += std::hypot(dx, dy);

    if (acc >= target_m)
    {
      out_pose = latest_path_.poses[i+1].pose;
      out_idx = i + 1;
      return true;
    }
  }

  // If we reach here, return the last pose
  out_pose = latest_path_.poses.back().pose;
  out_idx = latest_path_.poses.size() - 1;
  return true;
}

void LeaderNode::timer_callback()
{
  if (!has_odom_) return;

  // Detect current state
  double vel = std::abs(latest_odom_.twist.twist.linear.x);
  if (vel > 0.1)
    current_state_ = STATE_NAVIGATING;
  else if (has_path_ && calculate_remaining_dist(0) < 0.5)
    current_state_ = STATE_GOAL_REACHED;
  else
    current_state_ = STATE_TRANSITIONING;

  auto state_msg = skyhunter_msgs::msg::LeaderState();
  state_msg.header.stamp = this->get_clock()->now();
  state_msg.pose = latest_odom_.pose.pose;
  state_msg.velocity = latest_odom_.twist.twist;
  // state_msg.swarm_state = current_state_;

  // ==========================================
  // --- DATA MERGE: OVERRIDE WITH COMBAT STATE
  // ==========================================
  // If YOLO says we are locked, pass the combat state to the swarm!
  if (latest_combat_state_.target_locked) {
      state_msg.target_locked = true;
      state_msg.target_pos = latest_combat_state_.target_pos;
      state_msg.swarm_state = latest_combat_state_.swarm_state;
  } else {
      // Normal Navigation
      state_msg.target_locked = false;
      state_msg.swarm_state = current_state_;
  }

  // Formation logic override (narrow gap detection has priority)
  if (narrow_gap_detected_)
  {
    state_msg.formation_type = 1;  // FORCE COLUMN
    RCLCPP_WARN_THROTTLE(
      this->get_logger(), *this->get_clock(), 2000,
      "AUTO-SWITCH: Narrow Gap! Forcing Column.");
  }
  else
  {
    state_msg.formation_type = cmd_formation_type_;
  }

  // Waypoint visualization & next waypoints
  visualization_msgs::msg::MarkerArray markers;

  if (has_path_ && !latest_path_.poses.empty())
  {
    // Find closest point on path
    size_t closest_idx = 0;
    double min_d = std::numeric_limits<double>::max();

    for (size_t i = 0; i < latest_path_.poses.size(); ++i)
    {
      double dx = latest_odom_.pose.pose.position.x -
                  latest_path_.poses[i].pose.position.x;
      double dy = latest_odom_.pose.pose.position.y -
                  latest_path_.poses[i].pose.position.y;
      double d = std::hypot(dx, dy);
      if (d < min_d)
      {
        min_d = d;
        closest_idx = i;
      }
    }

    double remaining = calculate_remaining_dist(closest_idx);
    double tactical_spacing = std::min(spacing_config_, std::max(0.0, remaining - 2.0));

    geometry_msgs::msg::Pose wp1, wp2;
    size_t wp1_idx = 0, wp2_idx = 0;

    if (get_waypoint_at_dist(tactical_spacing, closest_idx, wp1, wp1_idx))
    {
      state_msg.next_waypoints.push_back(wp1);
      markers.markers.push_back(create_marker(0, wp1, 0.0f, 0.0f, 1.0f));

      if (get_waypoint_at_dist(tactical_spacing, wp1_idx, wp2, wp2_idx))
      {
        state_msg.next_waypoints.push_back(wp2);
        markers.markers.push_back(create_marker(1, wp2, 1.0f, 0.0f, 0.0f));
      }
    }
  }

  publisher_->publish(state_msg);
  viz_pub_->publish(markers);
}

visualization_msgs::msg::Marker LeaderNode::create_marker(
  int id,
  const geometry_msgs::msg::Pose & pose,
  float r, float g, float b)
{
  visualization_msgs::msg::Marker m;
  m.header.frame_id = map_frame_;
  m.header.stamp = this->get_clock()->now();
  m.ns = "tactical_wp";
  m.id = id;
  m.type = visualization_msgs::msg::Marker::CYLINDER;
  m.action = visualization_msgs::msg::Marker::ADD;
  m.pose = pose;
  m.scale.x = 0.5;
  m.scale.y = 0.5;
  m.scale.z = 0.1;
  m.color.a = 0.8f;
  m.color.r = r;
  m.color.g = g;
  m.color.b = b;
  return m;
}

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LeaderNode>());
  rclcpp::shutdown();
  return 0;
}