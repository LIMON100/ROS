// SAN v1.3 PHASE 3 - san_hub_slam main entry.

#include <rclcpp/rclcpp.hpp>
#include "san_hub_slam/hub_slam_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<san_hub_slam::HubSlamNode>();
    rclcpp::executors::MultiThreadedExecutor exec;
    exec.add_node(node);
    exec.spin();
    rclcpp::shutdown();
    return 0;
}
