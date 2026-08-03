// frame_fixer_node.cpp
// C++ 포팅: script/frame_fixer.py (FrameFixer) feature-parity 재구현.
//   /odometry/gps 의 frame_id 를 'map' 으로 바꿔 /odometry/gps_map 으로 재발행.
//   navsat_transform 이 잘못 설정하는 frame_id 를 우회.
#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>

#include <chrono>
#include <string>

class FrameFixer : public rclcpp::Node
{
public:
  FrameFixer()
  : Node("frame_fixer")
  {
    in_topic_ = declare_parameter<std::string>("input_topic", "/odometry/gps");
    out_topic_ = declare_parameter<std::string>("output_topic", "/odometry/gps_map");
    target_frame_ = declare_parameter<std::string>("target_frame_id", "map");

    pub_ = create_publisher<nav_msgs::msg::Odometry>(out_topic_, 10);
    sub_ = create_subscription<nav_msgs::msg::Odometry>(
      in_topic_, 10, std::bind(&FrameFixer::cb, this, std::placeholders::_1));
    status_timer_ = create_wall_timer(
      std::chrono::seconds(5), std::bind(&FrameFixer::status_timer, this));

    RCLCPP_INFO(get_logger(),
      "✅ frame_fixer: %s → %s (frame_id forced to \"%s\")",
      in_topic_.c_str(), out_topic_.c_str(), target_frame_.c_str());
  }

private:
  void status_timer()
  {
    RCLCPP_INFO(get_logger(), "[frame_fixer] relayed %d msgs in 5s", msg_count_);
    msg_count_ = 0;
  }

  void cb(nav_msgs::msg::Odometry::SharedPtr msg)
  {
    msg->header.frame_id = target_frame_;
    pub_->publish(*msg);
    msg_count_++;
  }

  std::string in_topic_, out_topic_, target_frame_;
  int msg_count_{0};

  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr pub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr status_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FrameFixer>());
  rclcpp::shutdown();
  return 0;
}
