// SAN v1.5 Phase 2-E Turn 6 — IMX678 4K H.265 camera node.
//
// Publishes sensor_msgs/CompressedImage on ~/image_compressed.
// Replaces adapters/payload_sensors.py::IMX678Adapter per
// DCN-2026-002 D-007/D-008 (ShmPool → ROS 2 intra-process zero-copy).

#include "san_cameras/camera_node_base.hpp"

#include <sensor_msgs/msg/compressed_image.hpp>
#include <rclcpp/rclcpp.hpp>

namespace san_cameras {

class Imx678Node : public CameraNodeBase {
public:
  explicit Imx678Node(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions())
      : Imx678Node(opts, makeRealV4l2()) {}

  Imx678Node(const rclcpp::NodeOptions& opts,
              std::unique_ptr<V4l2CaptureInterface> v4l2)
      : CameraNodeBase("imx678_camera_node", opts, std::move(v4l2)) {
    // Override defaults via parameter overrides AFTER base ctor declared them
    // (set_parameter does an atomic update with the new value).
    declareDefaultsForSubclass();
    // Compressed image at SensorData QoS depth 5 (intra-process zero-copy)
    image_pub_ = create_publisher<sensor_msgs::msg::CompressedImage>(
        "~/image_compressed", rclcpp::SensorDataQoS().keep_last(5));
    startCapture();
  }

protected:
  // `final` silences cppcheck virtualCallInConstructor and signals
  // that this class is a leaf in the camera-node hierarchy. Safe
  // because the ctor calls this method only once, after the base
  // ctor has fully constructed the rclcpp::Node state.
  void declareDefaultsForSubclass() override final {
    // Update common-param defaults for 4K H.265
    set_parameter(rclcpp::Parameter("width",     3840));
    set_parameter(rclcpp::Parameter("height",    2160));
    set_parameter(rclcpp::Parameter("encoding",  std::string("h265")));
    set_parameter(rclcpp::Parameter("fps",       30.0));
    set_parameter(rclcpp::Parameter("frame_id",  std::string("imx678")));
    set_parameter(rclcpp::Parameter("device",    std::string("/dev/video0")));
  }

  std::vector<uint8_t> generateStubFrame() override {
    // Minimal H.265 NAL-unit-shaped placeholder (matches Python stub).
    // Real H.265 frames are highly variable; 2 KB is plausible for an
    // I-frame's NAL prefix.
    std::vector<uint8_t> v(2048, 0);
    v[0] = 0x00; v[1] = 0x00; v[2] = 0x00; v[3] = 0x01;     // NAL start
    v[4] = 0x40; v[5] = 0x01;                                  // HEVC VPS hint
    return v;
  }

  void publishFrame(std::vector<uint8_t>&& data,
                     uint64_t timestamp_ns) override {
    auto msg = std::make_unique<sensor_msgs::msg::CompressedImage>();
    msg->header.stamp    = rclcpp::Time(
        static_cast<int64_t>(timestamp_ns), RCL_ROS_TIME);
    msg->header.frame_id = frame_id_;
    msg->format          = encoding_;        // "h265"
    msg->data            = std::move(data);  // move — zero-copy intra-proc
    image_pub_->publish(std::move(msg));
  }

private:
  rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr image_pub_;
};

}  // namespace san_cameras

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_cameras::Imx678Node>();
    rclcpp::spin(node);
  } catch (const std::exception& e) {
    RCLCPP_FATAL(rclcpp::get_logger("imx678_main"),
                 "Imx678Node aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
