#include <vector>
#include <string>
#include <map>

#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "geometry_msgs/msg/pose_array.hpp"

class SwarmMonitorNode : public rclcpp::Node
{
public:
  SwarmMonitorNode() : Node("swarm_monitor_node")
  {
    // Configure for 8 robots
    int robot_count = 8;
    
    // Create subscriber for each robot
    for (int i = 1; i <= robot_count; i++) {
        std::string topic = "/robot" + std::to_string(i) + "/odom";
        
        // We use a lambda to capture the ID
        auto sub = this->create_subscription<nav_msgs::msg::Odometry>(
            topic, 10, 
            [this, i](const nav_msgs::msg::Odometry::SharedPtr msg) {
                this->odom_callback(msg, i);
            });
        
        subscriptions_.push_back(sub);
    }

    // Publisher for the combined positions
    swarm_pub_ = this->create_publisher<geometry_msgs::msg::PoseArray>("/swarm/poses", 10);

    // Timer to broadcast at 10Hz
    timer_ = this->create_wall_timer(
        std::chrono::milliseconds(100), 
        std::bind(&SwarmMonitorNode::publish_swarm_state, this));

    RCLCPP_INFO(this->get_logger(), "Swarm Monitor Active. Tracking %d robots.", robot_count);
  }

private:
  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg, int robot_id)
  {
    swarm_poses_[robot_id] = msg->pose.pose;
  }

  void publish_swarm_state()
  {
    geometry_msgs::msg::PoseArray msg;
    msg.header.stamp = this->get_clock()->now();
    msg.header.frame_id = "odom"; // Assuming global odom frame

    // Convert map to vector for publishing
    for (auto const& [id, pose] : swarm_poses_) {
        msg.poses.push_back(pose);
    }

    swarm_pub_->publish(msg);
  }

  std::vector<rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr> subscriptions_;
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr swarm_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::map<int, geometry_msgs::msg::Pose> swarm_poses_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SwarmMonitorNode>());
  rclcpp::shutdown();
  return 0;
}