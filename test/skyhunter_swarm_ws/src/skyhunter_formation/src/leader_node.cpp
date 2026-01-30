#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "skyhunter_msgs/msg/leader_state.hpp"

using namespace std::chrono_literals;

class LeaderNode : public rclcpp::Node
{
public:
  LeaderNode()
  : Node("leader_node")
  {
    // --- Parameters ---
    // The topic where the leader tells followers what to do
    this->declare_parameter<std::string>("leader_state_topic", "leader_state");
    // The source of truth for the leader's position
    this->declare_parameter<std::string>("odom_topic", "odom");
    
    std::string state_topic = this->get_parameter("leader_state_topic").as_string();
    std::string odom_topic = this->get_parameter("odom_topic").as_string();

    // --- Publishers & Subscribers ---
    // QoS History 10 is standard for state data
    publisher_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>(state_topic, 10);
    
    subscription_ = this->create_subscription<nav_msgs::msg::Odometry>(
      odom_topic, 10, std::bind(&LeaderNode::odom_callback, this, std::placeholders::_1));

    // --- State Initialization ---
    current_formation_mode_ = 0; // Default: V-Shape
    current_formation_state_ = 0; // Default: Normal

    // Timer to ensure we publish even if odom is slow (Safety heartbeat)
    timer_ = this->create_wall_timer(
      50ms, std::bind(&LeaderNode::timer_callback, this)); // 20Hz

    RCLCPP_INFO(this->get_logger(), "Leader Node Started. Broadcasting on: %s", state_topic.c_str());
  }

private:
  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
  {
    // We store the latest odom to republish it as LeaderState
    latest_odom_ = *msg;
    has_odom_ = true;
  }

  void timer_callback()
  {
    if (!has_odom_) {
      // Don't publish garbage data if we haven't moved or initialized yet
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000, "Waiting for Odometry...");
      return;
    }

    auto message = skyhunter_msgs::msg::LeaderState();
    
    // Header Info
    message.header.stamp = this->get_clock()->now();
    message.header.frame_id = latest_odom_.header.frame_id; // Usually "odom" or "robot1/odom"

    // Pose and Velocity from Odometry
    message.pose = latest_odom_.pose.pose;
    message.velocity = latest_odom_.twist.twist;

    // Formation Logic
    // In the future, this can be changed by a service call or operator input
    message.formation_mode = current_formation_mode_;
    message.formation_state = current_formation_state_;

    publisher_->publish(message);
  }

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr publisher_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr subscription_;

  nav_msgs::msg::Odometry latest_odom_;
  bool has_odom_ = false;
  uint8_t current_formation_mode_;
  uint8_t current_formation_state_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LeaderNode>());
  rclcpp::shutdown();
  return 0;
}