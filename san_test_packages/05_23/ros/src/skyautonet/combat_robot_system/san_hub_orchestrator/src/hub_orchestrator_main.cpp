#include <rclcpp/rclcpp.hpp>
#include "san_hub_orchestrator/hub_orchestrator_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_hub_orchestrator::HubOrchestratorNode>();
    rclcpp::spin(node);
  } catch (const std::exception& e) {
    RCLCPP_FATAL(rclcpp::get_logger("hub_orchestrator_main"),
                 "HubOrchestratorNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
