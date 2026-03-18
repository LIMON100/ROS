#ifndef SKYHUNTER_CONTROL__SWARM_MONITOR_NODE_HPP_
#define SKYHUNTER_CONTROL__SWARM_MONITOR_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/pose_array.hpp>

#include <vector>
#include <string>
#include <map>
#include <chrono>

class SwarmMonitorNode : public rclcpp::Node
{
public:
  explicit SwarmMonitorNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  ~SwarmMonitorNode() override = default;

private:
  void publish_swarm_state();

  // State
  std::map<std::string, geometry_msgs::msg::Pose> swarm_poses_;

  // Subscriptions (we keep them alive)
  std::vector<rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr> subscriptions_;

  // Publisher
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr swarm_pub_;

  // Timer
  rclcpp::TimerBase::SharedPtr timer_;
};

#endif  // SKYHUNTER_CONTROL__SWARM_MONITOR_NODE_HPP_