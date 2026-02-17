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
    this->declare_parameter<std::string>("leader_state_topic", "leader_state");
    this->declare_parameter<std::string>("odom_topic", "odom");
    
    std::string state_topic = this->get_parameter("leader_state_topic").as_string();
    std::string odom_topic = this->get_parameter("odom_topic").as_string();

    // --- Publishers & Subscribers ---
    
    // FIX 1: Use SensorDataQoS (Best Effort) for Odometry to ensure connection with Gazebo
    auto qos = rclcpp::SensorDataQoS();
    
    publisher_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>(state_topic, 10);
    
    subscription_ = this->create_subscription<nav_msgs::msg::Odometry>(
      odom_topic, qos, std::bind(&LeaderNode::odom_callback, this, std::placeholders::_1));

    // --- State Initialization ---
    current_formation_mode_ = 0; // Default: V-Shape
    current_formation_state_ = 0; // Default: Normal

    // Timer (20Hz)
    timer_ = this->create_wall_timer(
      50ms, std::bind(&LeaderNode::timer_callback, this));

    RCLCPP_INFO(this->get_logger(), "Leader Node Started. Listening to: %s, Publishing on: %s", 
      odom_topic.c_str(), state_topic.c_str());
  }

private:
  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
  {
    latest_odom_ = *msg;
    has_odom_ = true;
  }

  void timer_callback()
  {
    if (!has_odom_) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000, "Waiting for Odometry...");
      return;
    }

    auto message = skyhunter_msgs::msg::LeaderState();
    
    // FIX 2: Use the TIMESTAMP from the Odom message, not 'now'.
    // This allows the follower to know exactly when this position was valid.
    message.header.stamp = latest_odom_.header.stamp;
    
    // Pass the frame ID (e.g., "robot_01/odom")
    // message.header.frame_id = latest_odom_.header.frame_id;
    message.header.frame_id = "map";
 

    // Pose and Velocity
    message.pose = latest_odom_.pose.pose;
    message.velocity = latest_odom_.twist.twist;

    // Formation Status
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