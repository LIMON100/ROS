// SAN v1.3 PHASE 2 v2 - main entry for the one-shot mwan3 init node.
//
// Configures mwan3.wan_lte on boot then exits. systemd should run this
// as a Type=oneshot service that ExecStartPre-blocks the LTE role
// manager service.

#include <rclcpp/rclcpp.hpp>
#include "san_lte_redundancy/mwan3_init_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<san_lte_redundancy::Mwan3InitNode>();
    const int exit_code = node->isConfigured() ? 0 : 1;
    rclcpp::shutdown();
    return exit_code;
}
