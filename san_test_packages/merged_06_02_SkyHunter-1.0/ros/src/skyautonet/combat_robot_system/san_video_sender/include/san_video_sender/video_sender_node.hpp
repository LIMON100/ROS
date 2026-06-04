// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5 - Follower-side video sender.
//
// Builds a v4l2src → encoder → mpegtsmux → udpsink pipeline using
// the GStreamer C API (gst_parse_launch). No `gst-launch-1.0` shell,
// no `system()`. Emits to (hub_ip, udp_base_port + robot_id), where
// the Hub's gstreamer_relay_node consumes the RTP/MPEG-TS feed and
// re-encapsulates as SRT for the operator.
//
// Quality switching: the node subscribes to /video/handle so it can
// rebuild the pipeline whenever the Hub forces a thumbnail downgrade
// (>= 4 concurrent streams) or the operator issues CHANGE_QUALITY.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/video_stream_handle.hpp>

#include <memory>
#include <string>

extern "C" {
#include <gst/gst.h>
}

namespace san_video_sender
{

class VideoSenderNode : public rclcpp::Node
{
public:
  VideoSenderNode();
  explicit VideoSenderNode(const rclcpp::NodeOptions & options);
  ~VideoSenderNode() override;

  // Test accessors.
  bool isPipelinePlaying() const {return pipeline_ != nullptr;}
  std::string currentDescription() const {return current_description_;}
  int currentBitrateKbps() const {return current_bitrate_kbps_;}

  // P1-5 part 2: exposed for direct gtest coverage of the quality
  // table — the mapping is a contract shared with the Hub-side
  // gstreamer_relay_node (which echos actual_bitrate_kbps back to
  // the sender), so drifting these values silently would manifest
  // as a sender↔relay bitrate mismatch in production. Static, no
  // internal state — safe to test outside any instance.
  static void qualityToResolution(
    uint8_t quality,
    int & w, int & h, int & bitrate_kbps);

private:
  // Parameters.
  std::string hub_ip_;
  int robot_id_ = 3;
  std::string video_device_ = "/dev/video0";
  int default_bitrate_kbps_ = 1500;
  int udp_base_port_ = 5000;
  int width_ = 1280;
  int height_ = 720;
  int framerate_ = 30;
  std::string codec_ = "h265";     // "h264" | "h265"

  GstElement * pipeline_ = nullptr;
  std::string current_description_;
  int current_bitrate_kbps_ = 0;
  int current_width_ = 0;
  int current_height_ = 0;

  rclcpp::Subscription<combat_robot_msgs::msg::VideoStreamHandle>::SharedPtr
    handle_sub_;

  void declareParameters();
  void readParameters();
  void wireInterfaces();

  void onHandle(combat_robot_msgs::msg::VideoStreamHandle::SharedPtr msg);

  // Pipeline lifecycle.
  bool startPipeline(int bitrate_kbps, int width, int height);
  void stopPipeline();
  bool rebuildPipeline(int bitrate_kbps, int width, int height);

  std::string buildDescription(int bitrate_kbps, int width, int height) const;
};

}  // namespace san_video_sender
