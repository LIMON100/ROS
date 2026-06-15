// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5 - main entry for the follower video sender node.

#include <rclcpp/rclcpp.hpp>

extern "C" {
#include <gst/gst.h>
}

#include "san_video_sender/video_sender_node.hpp"

int main(int argc, char ** argv)
{
  gst_init(&argc, &argv);
  rclcpp::init(argc, argv);
  auto node = std::make_shared<san_video_sender::VideoSenderNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
