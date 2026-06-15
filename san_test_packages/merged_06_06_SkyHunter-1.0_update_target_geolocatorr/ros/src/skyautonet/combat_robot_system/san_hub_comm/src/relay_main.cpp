// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5 - main entry for the GStreamer relay node.

#include <rclcpp/rclcpp.hpp>

extern "C" {
#include <gst/gst.h>
}

#include "san_hub_comm/gstreamer_relay_node.hpp"

int main(int argc, char ** argv)
{
  gst_init(&argc, &argv);
  rclcpp::init(argc, argv);
  auto node = std::make_shared<san_hub_comm::GStreamerRelayNode>();
  rclcpp::executors::MultiThreadedExecutor exec;
  exec.add_node(node);
  exec.spin();
  rclcpp::shutdown();
  return 0;
}
