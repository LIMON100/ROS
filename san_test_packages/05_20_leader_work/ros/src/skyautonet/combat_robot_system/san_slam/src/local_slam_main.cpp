// SAN v1.3 PHASE 3 - san_slam main entry.

#include <rclcpp/rclcpp.hpp>
#include "san_slam/local_slam_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<san_slam::LocalSlamNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
