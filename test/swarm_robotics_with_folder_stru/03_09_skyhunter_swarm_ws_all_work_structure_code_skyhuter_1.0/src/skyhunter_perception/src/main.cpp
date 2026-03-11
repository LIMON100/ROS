#include "skyhunter_perception/yolo_detector_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<YoloDetectorNode>());
    rclcpp::shutdown();
    return 0;
}