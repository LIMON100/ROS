// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_video_sender/video_sender_node.hpp"

#include <combat_robot_msgs/msg/video_stream_handle.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

namespace san_video_sender
{

using Handle = combat_robot_msgs::msg::VideoStreamHandle;

VideoSenderNode::VideoSenderNode()
: VideoSenderNode(rclcpp::NodeOptions())
{}

VideoSenderNode::VideoSenderNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("video_sender_node", options)
{
  if (!gst_is_initialized()) {
    gst_init(nullptr, nullptr);
  }
  declareParameters();
  readParameters();
  wireInterfaces();
  if (!startPipeline(default_bitrate_kbps_, width_, height_)) {
    RCLCPP_ERROR(
      get_logger(),
      "Initial pipeline failed; node will wait for VideoStreamHandle");
  }
}

VideoSenderNode::~VideoSenderNode()
{
  stopPipeline();
}

void VideoSenderNode::declareParameters()
{
  declare_parameter<std::string>("hub_ip", "10.0.0.2");
  declare_parameter<int>("robot_id", 3);
  declare_parameter<std::string>("video_device", "/dev/video0");
  declare_parameter<int>("default_bitrate_kbps", 2000);     // ★ v1.5.1: HD 2 Mbps
  declare_parameter<int>("udp_base_port", 5000);
  declare_parameter<int>("width", 1280);
  declare_parameter<int>("height", 720);
  declare_parameter<int>("framerate", 30);
  declare_parameter<std::string>("codec", "h265");
}

void VideoSenderNode::readParameters()
{
  hub_ip_ = get_parameter("hub_ip").as_string();
  robot_id_ = get_parameter("robot_id").as_int();
  video_device_ = get_parameter("video_device").as_string();
  default_bitrate_kbps_ = get_parameter("default_bitrate_kbps").as_int();
  udp_base_port_ = get_parameter("udp_base_port").as_int();
  width_ = get_parameter("width").as_int();
  height_ = get_parameter("height").as_int();
  framerate_ = get_parameter("framerate").as_int();
  codec_ = get_parameter("codec").as_string();
}

void VideoSenderNode::wireInterfaces()
{
  rclcpp::QoS qos(10);
  qos.reliable();
  handle_sub_ = create_subscription<Handle>(
    "/video/handle", qos,
    std::bind(
      &VideoSenderNode::onHandle, this,
      std::placeholders::_1));
}

void VideoSenderNode::qualityToResolution(
  uint8_t quality, int & w, int & h,
  int & bitrate_kbps)
{
  // v1.5.1 (DCN-2026-003 D-001, 2026-05-13):
  //   ★ HD = 1280x720 @ 2 Mbps (operator preference).
  //   기존: HD = 1500 kbps  →  변경: HD = 2000 kbps
  //   relay 측 qualityBitrateKbps() 와 동일 값 유지 필수
  //   (relay 가 actual_bitrate_kbps 로 sender 에 echo).
  switch (quality) {
    case Handle::QUALITY_THUMBNAIL:
      w = 320; h = 240; bitrate_kbps = 100; break;
    case Handle::QUALITY_LOW:
      w = 640; h = 480; bitrate_kbps = 500; break;
    case Handle::QUALITY_HD:
      w = 1280; h = 720; bitrate_kbps = 2000; break;         // ★ 1500 → 2000
    case Handle::QUALITY_FHD:
      w = 1920; h = 1080; bitrate_kbps = 4000; break;
    default:
      w = 1280; h = 720; bitrate_kbps = 2000; break;         // ★ HD default
  }
}

void VideoSenderNode::onHandle(Handle::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  // Ignore handles for other robots.
  if (msg->target_robot_id != static_cast<uint32_t>(robot_id_)) {return;}

  if (msg->status == Handle::STATUS_STOPPED) {
    stopPipeline();
    return;
  }
  if (msg->status == Handle::STATUS_ERROR) {
    RCLCPP_ERROR(
      get_logger(), "Handle reported ERROR: %s",
      msg->error_msg.c_str());
    return;
  }

  int w = width_, h = height_;
  int bitrate = default_bitrate_kbps_;
  qualityToResolution(msg->quality, w, h, bitrate);

  // Only rebuild if anything actually changed.
  if (pipeline_ != nullptr &&
    bitrate == current_bitrate_kbps_ &&
    w == current_width_ && h == current_height_)
  {
    return;
  }
  rebuildPipeline(bitrate, w, h);
}

std::string VideoSenderNode::buildDescription(
  int bitrate_kbps,
  int width, int height) const
{
  // Choose encoder by codec parameter. The pipeline shape stays
  // identical so the Hub's relay does not care which codec the
  // follower used.
  const std::string encoder_part =
    (codec_ == "h264") ?
    std::string("x264enc bitrate=") + std::to_string(bitrate_kbps) +
    " tune=zerolatency speed-preset=ultrafast ! " +
    "video/x-h264, stream-format=byte-stream ! h264parse" :
    std::string("x265enc bitrate=") + std::to_string(bitrate_kbps) +
    " tune=zerolatency speed-preset=ultrafast ! " +
    "video/x-h265, stream-format=byte-stream ! h265parse";

  std::ostringstream o;
  o << "v4l2src device=" << video_device_ << " ! "
    << "videoconvert ! "
    << "video/x-raw,width=" << width << ",height=" << height
    << ",framerate=" << framerate_ << "/1 ! "
    << encoder_part << " ! "
    << "mpegtsmux alignment=7 ! rtpmp2tpay ! "
    << "udpsink host=" << hub_ip_
    << " port=" << (udp_base_port_ + robot_id_)
    << " sync=false";
  return o.str();
}

bool VideoSenderNode::startPipeline(int bitrate_kbps, int width, int height)
{
  if (pipeline_ != nullptr) {
    return rebuildPipeline(bitrate_kbps, width, height);
  }
  const std::string desc = buildDescription(bitrate_kbps, width, height);
  GError * err = nullptr;
  pipeline_ = gst_parse_launch(desc.c_str(), &err);
  if (pipeline_ == nullptr) {
    RCLCPP_ERROR(
      get_logger(), "gst_parse_launch failed: %s",
      err ? err->message : "(no detail)");
    if (err) {g_error_free(err);}
    return false;
  }
  if (err) {
    RCLCPP_WARN(
      get_logger(), "gst_parse_launch warning: %s",
      err->message);
    g_error_free(err);
  }
  if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) ==
    GST_STATE_CHANGE_FAILURE)
  {
    RCLCPP_ERROR(get_logger(), "set_state(PLAYING) failed");
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
    return false;
  }
  current_description_ = desc;
  current_bitrate_kbps_ = bitrate_kbps;
  current_width_ = width;
  current_height_ = height;
  RCLCPP_INFO(
    get_logger(),
    "VideoSender pipeline up: %dx%d @ %d kbps -> %s:%d",
    width, height, bitrate_kbps, hub_ip_.c_str(),
    udp_base_port_ + robot_id_);
  return true;
}

void VideoSenderNode::stopPipeline()
{
  if (pipeline_ == nullptr) {return;}
  gst_element_set_state(pipeline_, GST_STATE_NULL);
  gst_element_get_state(pipeline_, nullptr, nullptr, 5 * GST_SECOND);
  gst_object_unref(pipeline_);
  pipeline_ = nullptr;
  current_description_.clear();
  current_bitrate_kbps_ = 0;
  current_width_ = 0;
  current_height_ = 0;
}

bool VideoSenderNode::rebuildPipeline(int bitrate_kbps, int width, int height)
{
  stopPipeline();
  return startPipeline(bitrate_kbps, width, height);
}

}  // namespace san_video_sender
