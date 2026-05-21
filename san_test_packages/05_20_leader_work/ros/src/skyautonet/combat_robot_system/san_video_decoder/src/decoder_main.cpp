// SAN v1.5.1 — VideoDecoderNode main entry.

#include <rclcpp/rclcpp.hpp>

extern "C" {
#include <gst/gst.h>
}

#include "san_video_decoder/video_decoder_node.hpp"

int main(int argc, char** argv) {
    gst_init(&argc, &argv);
    rclcpp::init(argc, argv);
    auto node = std::make_shared<san_video_decoder::VideoDecoderNode>();
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    exec.spin();
    rclcpp::shutdown();
    return 0;
}
