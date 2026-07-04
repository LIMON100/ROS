// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 6 — Thermal camera node.
//
// Publishes sensor_msgs/Image (encoding=mono16) on ~/image.
// Replaces adapters/payload_sensors.py::ThermalCameraAdapter.
//
// PATCH 2026-05-13 (san_cameras deep-dive): see imx678_node.cpp.

#include "san_cameras/camera_node_base.hpp"

#include <sensor_msgs/msg/image.hpp>
#include <rclcpp/rclcpp.hpp>

#include "san_cameras/frame_metadata.hpp"

namespace san_cameras
{

namespace
{

const SubclassDefaults kThermalDefaults = {
  /*device  =*/ "/dev/video2",
  /*width   =*/ 640,
  /*height  =*/ 512,
  /*encoding=*/ "mono16",
  /*fps     =*/ 9.0,
  /*frame_id=*/ "thermal",
};

}  // namespace

class ThermalNode : public CameraNodeBase
{
public:
  explicit ThermalNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions())
  : ThermalNode(opts, makeRealV4l2()) {}

  ThermalNode(
    const rclcpp::NodeOptions & opts,
    std::unique_ptr<V4l2CaptureInterface> v4l2)
  : CameraNodeBase("thermal_camera_node", opts, std::move(v4l2),
      kThermalDefaults)
  {
    image_pub_ = create_publisher<sensor_msgs::msg::Image>(
      "~/image", rclcpp::SensorDataQoS().keep_last(5));
  }

protected:
  std::vector<uint8_t> generateStubFrame() override
  {
    // 640 × 512 × 2 = 655360 bytes; mid-range thermal pattern.
    // Use a single non-zero byte every 256 entries so it's visible
    // in `ros2 topic echo` truncated dumps yet stays cheap to build.
    //
    // ★ PATCH 2026-05-13: respect the runtime width/height (which
    // may have been overridden by the user) rather than baking 640×512.
    const std::size_t pixel_count =
      static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    std::vector<uint8_t> v(pixel_count * 2, 0);
    for (std::size_t i = 0; i < v.size(); i += 256) {v[i] = 0x80;}
    return v;
  }

  void publishFrame(
    std::vector<uint8_t> && data,
    uint64_t timestamp_ns) override
  {
    auto step = computeRowStep(encoding_, width_);
    if (!step) {
      return;             // unknown encoding — caller should have caught
    }
    auto msg = std::make_unique<sensor_msgs::msg::Image>();
    msg->header.stamp = rclcpp::Time(
      static_cast<int64_t>(timestamp_ns), RCL_ROS_TIME);
    msg->header.frame_id = frame_id_;
    msg->width = width_;
    msg->height = height_;
    msg->encoding = encoding_;
    msg->is_bigendian = 0;
    msg->step = static_cast<uint32_t>(*step);
    msg->data = std::move(data);              // zero-copy intra-proc
    image_pub_->publish(std::move(msg));
  }

private:
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
};

}  // namespace san_cameras

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_cameras::ThermalNode>();
    if (!node->start()) {
      RCLCPP_FATAL(
        rclcpp::get_logger("thermal_main"),
        "ThermalNode start() failed");
      rclcpp::shutdown();
      return 1;
    }
    rclcpp::spin(node);
  } catch (const std::exception & e) {
    RCLCPP_FATAL(
      rclcpp::get_logger("thermal_main"),
      "ThermalNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
