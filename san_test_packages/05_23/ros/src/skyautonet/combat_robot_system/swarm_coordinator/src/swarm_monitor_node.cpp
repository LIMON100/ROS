#include "swarm_coordinator/swarm_monitor_node.hpp"

using namespace std::chrono_literals;

SwarmMonitorNode::SwarmMonitorNode(const rclcpp::NodeOptions & options)
: Node("swarm_monitor_node", options)
{
  tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

  swarm_pub_ = this->create_publisher<geometry_msgs::msg::PoseArray>("/swarm/poses", 10);

  // Timer to broadcast the array at 10Hz
  timer_ = this->create_wall_timer(
    100ms, std::bind(&SwarmMonitorNode::publish_swarm_state, this));

  RCLCPP_INFO(this->get_logger(), "TF2 Swarm Monitor Active. Tracking 8 units in Global Map.");
}

void SwarmMonitorNode::publish_swarm_state()
{
  geometry_msgs::msg::PoseArray msg;
  msg.header.stamp = this->get_clock()->now();
  msg.header.frame_id = "map";

  // The frames we want to track
  std::vector<std::string> robot_frames = {
    "base_footprint",       // Leader (SH_01)
    "robot_2/base_footprint",    // Hub UGV
    "robot_3/base_footprint",    // Deputy
    "robot_4/base_footprint",    
    "robot_5/base_footprint",
    "robot_6/base_footprint",
    "robot_7/base_footprint",
    "robot_8/base_footprint",
  };

  for (const auto& frame : robot_frames)
  {
    try {
      // Ask TF2: "Where is this robot exactly in the MAP right now?"
      auto tf = tf_buffer_->lookupTransform("map", frame, tf2::TimePointZero);
      
      geometry_msgs::msg::Pose p;
      p.position.x = tf.transform.translation.x;
      p.position.y = tf.transform.translation.y;
      p.position.z = tf.transform.translation.z;
      p.orientation = tf.transform.rotation;
      
      msg.poses.push_back(p);
    } catch (...) {
      // Robot not spawned yet or TF tree not ready, ignore quietly
    }
  }

  swarm_pub_->publish(msg);
}

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SwarmMonitorNode>());
  rclcpp::shutdown();
  return 0;
}