// #include "skyhunter_perception/yolo_detector_node.hpp"

// int main(int argc, char** argv) {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<YoloDetectorNode>());
//     rclcpp::shutdown();
//     return 0;
// }


#include "skyhunter_perception/yolo_detector_node.hpp"
#include <rclcpp/executors/multi_threaded_executor.hpp> // ADD THIS

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<YoloDetectorNode>();
    
    // Use Multi-Threaded Executor to parallelize Camera and LiDAR callbacks
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();
    
    rclcpp::shutdown();
    return 0;
}
