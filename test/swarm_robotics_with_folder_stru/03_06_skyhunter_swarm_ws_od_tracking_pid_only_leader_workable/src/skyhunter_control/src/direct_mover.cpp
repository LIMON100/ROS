#include <chrono>
#include <cmath>
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "tf2/utils.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

using namespace std::chrono_literals;

class DirectMover : public rclcpp::Node {
public:
  DirectMover() : Node("direct_mover") {
    // Hardcoded Target: 10 meters forward from 0,0
    target_x_ = 10.0;
    target_y_ = 0.0;

    auto qos = rclcpp::SensorDataQoS();
    sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(
        "odom", qos, std::bind(&DirectMover::odom_cb, this, std::placeholders::_1));
    
    pub_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    
    // Timer for the control loop (20Hz)
    timer_ = this->create_wall_timer(50ms, std::bind(&DirectMover::control_loop, this));
    
    start_time_ = this->get_clock()->now();
    RCLCPP_INFO(this->get_logger(), "Direct Mover Started. Waiting 5s before moving to (%.1f, %.1f)", target_x_, target_y_);
  }

private:
  void odom_cb(const nav_msgs::msg::Odometry::SharedPtr msg) {
    current_x_ = msg->pose.pose.position.x;
    current_y_ = msg->pose.pose.position.y;
    current_yaw_ = tf2::getYaw(msg->pose.pose.orientation);
    has_odom_ = true;
  }

  void control_loop() {
    // 1. Initial 5-second wait
    if ((this->get_clock()->now() - start_time_).seconds() < 5.0) return;
    if (!has_odom_) return;

    // 2. Calculate Distance and Angle Error
    double dx = target_x_ - current_x_;
    double dy = target_y_ - current_y_;
    double distance = std::hypot(dx, dy);
    double target_yaw = std::atan2(dy, dx);
    double angle_error = target_yaw - current_yaw_;

    // Normalize angle error to [-PI, PI]
    while (angle_error > M_PI) angle_error -= 2.0 * M_PI;
    while (angle_error < -M_PI) angle_error += 2.0 * M_PI;

    geometry_msgs::msg::Twist cmd;

    // 3. Arrival Logic
    if (distance < 0.3) {
      RCLCPP_INFO_ONCE(this->get_logger(), "GOAL REACHED!");
      pub_cmd_->publish(cmd); // Stop
      return;
    }

    // 4. P-Controller Logic
    // If angle error is large, rotate in place first
    if (std::abs(angle_error) > 0.5) {
        cmd.linear.x = 0.0;
        cmd.angular.z = (angle_error > 0) ? 0.6 : -0.6;
    } else {
        // Smoothly drive and steer
        cmd.linear.x = std::min(0.8, 0.4 * distance); // Cap speed at 0.8 m/s
        cmd.angular.z = 1.2 * angle_error;
    }

    pub_cmd_->publish(cmd);

    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
        "Moving... Dist: %.2fm | Angle Err: %.2f rad", distance, angle_error);
  }

  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time start_time_;
  
  double current_x_, current_y_, current_yaw_;
  double target_x_, target_y_;
  bool has_odom_ = false;
};

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DirectMover>());
  rclcpp::shutdown();
  return 0;
}