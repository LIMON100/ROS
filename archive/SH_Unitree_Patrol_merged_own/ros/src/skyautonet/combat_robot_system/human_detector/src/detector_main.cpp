// SAN v1.5.1 (was v1.3 PHASE 6 — see DCN-2026-004 D-011) - main entry for the human detector node.

#include <rclcpp/rclcpp.hpp>
#include "human_detector/human_detector_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<human_detector::HumanDetectorNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
