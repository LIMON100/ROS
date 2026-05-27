// SAN v1.3 PHASE 7 - main entry for the operation_control node.

#include <rclcpp/rclcpp.hpp>
#include "san_operation_control/operation_control_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<
        san_operation_control::OperationControlNode>();
    rclcpp::executors::MultiThreadedExecutor exec;
    exec.add_node(node);
    exec.spin();
    rclcpp::shutdown();
    return 0;
}
