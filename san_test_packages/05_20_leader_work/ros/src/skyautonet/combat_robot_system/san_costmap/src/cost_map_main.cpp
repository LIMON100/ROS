// SAN v1.3 PHASE 1 - san_costmap main entry.

#include <rclcpp/rclcpp.hpp>
#include "san_costmap/cost_map_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<san_costmap::CostMapNode>();
    rclcpp::executors::MultiThreadedExecutor exec;
    exec.add_node(node);
    exec.spin();
    rclcpp::shutdown();
    return 0;
}
