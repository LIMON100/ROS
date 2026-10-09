#include "rclcpp/rclcpp.hpp"
#include "command_server.hpp"

int main(int argc, char *argv[]) {
    // Initialize and run the ROS2 node.
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<command_server::CommandServerNode>(rclcpp::NodeOptions()));
    rclcpp::shutdown();
    return 0;
}
