#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "pan_tilt_controller.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::NodeOptions options;
  auto node = std::make_shared<combat_robot_system::PanTiltController>(options);
  
  // Use get_node_base_interface() for LifecycleNode
  rclcpp::spin(node->get_node_base_interface());
  
  rclcpp::shutdown();
  return 0;
}
