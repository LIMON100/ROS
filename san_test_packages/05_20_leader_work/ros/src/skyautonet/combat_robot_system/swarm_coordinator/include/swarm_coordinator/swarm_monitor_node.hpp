#ifndef SKYHUNTER_CONTROL__SWARM_MONITOR_NODE_HPP_
#define SKYHUNTER_CONTROL__SWARM_MONITOR_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <memory>
#include <vector>
#include <string>

class SwarmMonitorNode : public rclcpp::Node
{
public:
  explicit SwarmMonitorNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~SwarmMonitorNode() override = default;

private:
  void publish_swarm_state();

  // TF2 for getting global coordinates
  std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

  // Publisher
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr swarm_pub_;

  // Timer
  rclcpp::TimerBase::SharedPtr timer_;
};

#endif  // SKYHUNTER_CONTROL__SWARM_MONITOR_NODE_HPP_