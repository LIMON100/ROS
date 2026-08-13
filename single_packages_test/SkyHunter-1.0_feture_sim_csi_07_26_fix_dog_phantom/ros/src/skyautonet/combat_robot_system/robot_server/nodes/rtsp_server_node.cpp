#include "rclcpp/rclcpp.hpp"
#include "rtsp_server.hpp"

int main(int argc, char *argv[]) {
    // Initialize and run the ROS2 node.
    rclcpp::init(argc, argv);
    
    auto node = std::make_shared<rtsp_server::RTSPServerNode>(rclcpp::NodeOptions());

    // [Auto-Start] Configure and Activate the Lifecycle Node
    node->configure();
    node->activate();

    // Use an executor to spin the LifecycleNode
    rclcpp::executors::SingleThreadedExecutor exe;
    exe.add_node(node->get_node_base_interface());
    exe.spin();

    rclcpp::shutdown();
    return 0;
}
