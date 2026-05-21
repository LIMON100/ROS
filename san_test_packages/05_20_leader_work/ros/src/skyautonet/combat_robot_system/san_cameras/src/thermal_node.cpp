// SAN v1.5 Phase 2-E Turn 6 — Thermal camera node.
//
// Publishes sensor_msgs/Image (encoding=mono16) on ~/image.
// Replaces adapters/payload_sensors.py::ThermalCameraAdapter.

#include "san_cameras/camera_node_base.hpp"

#include <sensor_msgs/msg/image.hpp>
#include <rclcpp/rclcpp.hpp>

#include "san_cameras/frame_metadata.hpp"

namespace san_cameras {

class ThermalNode : public CameraNodeBase {
public:
  explicit ThermalNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions())
      : ThermalNode(opts, makeRealV4l2()) {}

  ThermalNode(const rclcpp::NodeOptions& opts,
               std::unique_ptr<V4l2CaptureInterface> v4l2)
      : CameraNodeBase("thermal_camera_node", opts, std::move(v4l2)) {
    declareDefaultsForSubclass();
    image_pub_ = create_publisher<sensor_msgs::msg::Image>(
        "~/image", rclcpp::SensorDataQoS().keep_last(5));
    startCapture();
  }

protected:
  // `final` silences cppcheck virtualCallInConstructor — leaf class.
  void declareDefaultsForSubclass() override final {
    set_parameter(rclcpp::Parameter("width",     640));
    set_parameter(rclcpp::Parameter("height",    512));
    set_parameter(rclcpp::Parameter("encoding",  std::string("mono16")));
    set_parameter(rclcpp::Parameter("fps",       9.0));
    set_parameter(rclcpp::Parameter("frame_id",  std::string("thermal")));
    set_parameter(rclcpp::Parameter("device",    std::string("/dev/video2")));
  }

  std::vector<uint8_t> generateStubFrame() override {
    // 640 × 512 × 2 = 655360 bytes; mid-range thermal pattern.
    // Use a single non-zero byte every 256 entries so it's visible
    // in `ros2 topic echo` truncated dumps yet stays cheap to build.
    std::vector<uint8_t> v(640 * 512 * 2, 0);
    for (size_t i = 0; i < v.size(); i += 256) v[i] = 0x80;
    return v;
  }

  void publishFrame(std::vector<uint8_t>&& data,
                     uint64_t timestamp_ns) override {
    auto step = computeRowStep(encoding_, width_);
    if (!step) return;   // unknown encoding — caller should have caught

    auto msg = std::make_unique<sensor_msgs::msg::Image>();
    msg->header.stamp    = rclcpp::Time(
        static_cast<int64_t>(timestamp_ns), RCL_ROS_TIME);
    msg->header.frame_id = frame_id_;
    msg->width           = width_;
    msg->height          = height_;
    msg->encoding        = encoding_;
    msg->is_bigendian    = 0;
    msg->step            = static_cast<uint32_t>(*step);
    msg->data            = std::move(data);   // zero-copy intra-proc
    image_pub_->publish(std::move(msg));
  }

private:
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
};

}  // namespace san_cameras

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<san_cameras::ThermalNode>();
    rclcpp::spin(node);
  } catch (const std::exception& e) {
    RCLCPP_FATAL(rclcpp::get_logger("thermal_main"),
                 "ThermalNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
