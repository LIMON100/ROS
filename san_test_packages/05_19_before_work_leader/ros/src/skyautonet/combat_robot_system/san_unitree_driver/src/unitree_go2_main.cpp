// SAN v1.5.1 (DCN-2026-004 D-007) — UnitreeGo2Node main entry.
//
// Intra-process comms are enabled at the NodeOptions level so any
// downstream consumer (e.g. san_navigation, san_perception) that
// runs in the SAME process via ComposableNodeContainer (see
// DCN-2026-004 D-008) benefits from zero-copy delivery. When the
// node runs as a standalone process (current default launch), the
// option is harmless overhead (~0).

#include <rclcpp/rclcpp.hpp>

#include "san_unitree_driver/unitree_go2_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    rclcpp::NodeOptions opts;
    opts.use_intra_process_comms(true);  // [v1.5.1 C-7 fix]
    auto node = std::make_shared<san_unitree_driver::UnitreeGo2Node>(opts);
    rclcpp::spin(node);
  } catch (const std::exception& e) {
    RCLCPP_FATAL(
        rclcpp::get_logger("unitree_go2_main"),
        "UnitreeGo2Node aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
