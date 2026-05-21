// SAN v1.3 PHASE 2 v2 - main entry for the LTE role manager node.
//
// Runs the LTERoleManager rclcpp node. The MultiThreadedExecutor gives
// the ubus uloop callback its own callback group, so a long
// ubus_invoke timeout cannot stall the watchdog timer.

#include <rclcpp/rclcpp.hpp>
#include "san_lte_redundancy/lte_role_manager.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<san_lte_redundancy::LTERoleManager>();
    rclcpp::executors::MultiThreadedExecutor exec;
    exec.add_node(node);
    exec.spin();
    rclcpp::shutdown();
    return 0;
}
