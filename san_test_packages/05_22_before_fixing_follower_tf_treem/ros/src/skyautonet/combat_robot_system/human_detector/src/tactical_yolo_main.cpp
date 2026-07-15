#include "human_detector/yolo_detector_node.hpp"
#include <rclcpp/executors/multi_threaded_executor.hpp> 

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
