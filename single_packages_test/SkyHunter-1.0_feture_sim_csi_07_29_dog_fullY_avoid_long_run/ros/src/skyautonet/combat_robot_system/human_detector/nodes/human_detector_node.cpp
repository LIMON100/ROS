#include "rclcpp/rclcpp.hpp"

#include "human_detector.hpp"

int main(int argc, char * argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::NodeOptions option; 
  rclcpp::spin(std::make_shared<human_detector::HumanDetectorComponent>(option));

  rclcpp::shutdown();
  return 0;
}