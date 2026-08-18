#include "rclcpp/rclcpp.hpp"
#include "camera_driver.hpp"

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::NodeOptions option; 
    auto node = std::make_shared<camera_interface::CameraDriver>(option);
    rclcpp::spin(node->get_node_base_interface());

    rclcpp::shutdown();
    return 0;
}