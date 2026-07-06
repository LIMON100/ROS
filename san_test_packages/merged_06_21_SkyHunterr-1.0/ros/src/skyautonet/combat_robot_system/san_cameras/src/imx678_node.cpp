// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 6 — IMX678 4K H.265 camera node.
//
// Publishes sensor_msgs/CompressedImage on ~/image_compressed.
// Replaces adapters/payload_sensors.py::IMX678Adapter per
// DCN-2026-002 D-007/D-008 (ShmPool → ROS 2 intra-process zero-copy).
//
// PATCH 2026-05-13 (san_cameras deep-dive):
//   * CM1/CM7 — defaults flow into base via SubclassDefaults struct,
//     not via post-declare set_parameter. User launch overrides win.
//   * CM6 — main() calls node->start() explicitly after construction.

#include "san_cameras/camera_node_base.hpp"

#include <sensor_msgs/msg/compressed_image.hpp>
#include <rclcpp/rclcpp.hpp>

namespace san_cameras
{

namespace
{

// ★ PATCH 2026-05-13 (CM1/CM7): canonical defaults — passed to base
// as declare_parameter() defaults so user overrides take precedence.
const SubclassDefaults kImx678Defaults = {
  /*device  =*/ "/dev/video0",
  /*width   =*/ 3840,
  /*height  =*/ 2160,
  /*encoding=*/ "h265",
  /*fps     =*/ 30.0,
  /*frame_id=*/ "imx678",
};

}  // namespace

class Imx678Node : public CameraNodeBase
{
public:
  explicit Imx678Node(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions())
  : Imx678Node(opts, makeRealV4l2()) {}

  Imx678Node(
    const rclcpp::NodeOptions & opts,
    std::unique_ptr<V4l2CaptureInterface> v4l2)
  : CameraNodeBase("imx678_camera_node", opts, std::move(v4l2),
      kImx678Defaults)
  {
    // ★ PATCH 2026-05-13 (CM6): publisher only — no startCapture here.
    // main() (or test harness) is responsible for calling start().
    image_pub_ = create_publisher<sensor_msgs::msg::CompressedImage>(
      "~/image_compressed", rclcpp::SensorDataQoS().keep_last(5));
  }

protected:
  std::vector<uint8_t> generateStubFrame() override
  {
    // Minimal H.265 NAL-unit-shaped placeholder (matches Python stub).
    // Real H.265 frames are highly variable; 2 KB is plausible for an
    // I-frame's NAL prefix.
    std::vector<uint8_t> v(2048, 0);
    v[0] = 0x00; v[1] = 0x00; v[2] = 0x00; v[3] = 0x01;     // NAL start
    v[4] = 0x40; v[5] = 0x01;                                  // HEVC VPS hint
    return v;
  }

  void publishFrame(
    std::vector<uint8_t> && data,
    uint64_t timestamp_ns) override
  {
    auto msg = std::make_unique<sensor_msgs::msg::CompressedImage>();
    msg->header.stamp = rclcpp::Time(
      static_cast<int64_t>(timestamp_ns), RCL_ROS_TIME);
    msg->header.frame_id = frame_id_;
    msg->format = encoding_;                 // "h265"
    msg->data = std::move(data);             // move — zero-copy intra-proc
    image_pub_->publish(std::move(msg));
  }

private:
  rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr image_pub_;
};

}  // namespace san_cameras

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_cameras::Imx678Node>();
    // ★ PATCH 2026-05-13 (CM6): explicit start.
    if (!node->start()) {
      RCLCPP_FATAL(
        rclcpp::get_logger("imx678_main"),
        "Imx678Node start() failed");
      rclcpp::shutdown();
      return 1;
    }
    rclcpp::spin(node);
  } catch (const std::exception & e) {
    RCLCPP_FATAL(
      rclcpp::get_logger("imx678_main"),
      "Imx678Node aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
