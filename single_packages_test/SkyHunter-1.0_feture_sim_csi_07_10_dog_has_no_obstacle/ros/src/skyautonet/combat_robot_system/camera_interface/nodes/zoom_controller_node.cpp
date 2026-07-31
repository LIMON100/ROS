#include <rclcpp/rclcpp.hpp>
#include <memory>
#include "zoom_controller.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<camera_interface::ZoomController>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
