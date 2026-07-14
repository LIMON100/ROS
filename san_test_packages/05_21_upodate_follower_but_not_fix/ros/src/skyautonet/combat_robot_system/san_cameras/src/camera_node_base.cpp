// SAN v1.5 Phase 2-E Turn 6 — CameraNodeBase implementation.

#include "san_cameras/camera_node_base.hpp"

#include <chrono>
#include <stdexcept>

#include "san_cameras/frame_metadata.hpp"

namespace san_cameras {

using namespace std::chrono_literals;

CameraNodeBase::CameraNodeBase(
    const std::string& node_name,
    const rclcpp::NodeOptions& opts,
    std::unique_ptr<V4l2CaptureInterface> v4l2)
    : rclcpp::Node(node_name, opts), v4l2_(std::move(v4l2)) {
  if (!v4l2_) {
    throw std::runtime_error("CameraNodeBase: null V4L2 backend");
  }

  // Common parameters — subclass adds its own defaults afterwards
  declare_parameter<std::string>("device",   "/dev/video0");
  declare_parameter<int>("width",            640);
  declare_parameter<int>("height",           480);
  declare_parameter<std::string>("encoding", "yuv422");
  declare_parameter<double>("fps",           30.0);
  declare_parameter<std::string>("frame_id", "camera");
  declare_parameter<bool>("stub_on_no_device", true);

  health_timer_ = create_wall_timer(
      1s, std::bind(&CameraNodeBase::onHealthTick, this));
}

CameraNodeBase::~CameraNodeBase() {
  running_ = false;
  if (reader_thread_.joinable()) reader_thread_.join();
  if (v4l2_) v4l2_->close();
}

void CameraNodeBase::loadCommonParameters() {
  device_            = get_parameter("device").as_string();
  width_             = static_cast<uint32_t>(get_parameter("width").as_int());
  height_            = static_cast<uint32_t>(get_parameter("height").as_int());
  encoding_          = get_parameter("encoding").as_string();
  fps_               = get_parameter("fps").as_double();
  frame_id_          = get_parameter("frame_id").as_string();
  stub_on_no_device_ = get_parameter("stub_on_no_device").as_bool();
  if (fps_ <= 0.0 || fps_ > 240.0) {
    throw std::runtime_error("CameraNodeBase: fps out of range");
  }
  if (!bytesPerPixel(encoding_).has_value()) {
    throw std::runtime_error(
        "CameraNodeBase: unknown encoding: " + encoding_);
  }
}

void CameraNodeBase::startCapture() {
  loadCommonParameters();

  CaptureConfig cfg{device_, width_, height_, encoding_, fps_};
  if (!v4l2_->open(cfg)) {
    if (stub_on_no_device_) {
      RCLCPP_WARN(get_logger(),
          "V4L2 device %s unavailable — STUB mode @ %.1f fps",
          device_.c_str(), fps_);
      stub_mode_ = true;
      const auto period_ms = std::chrono::milliseconds(
          static_cast<int64_t>(1000.0 / fps_));
      stub_timer_ = create_wall_timer(
          period_ms, std::bind(&CameraNodeBase::stubTick, this));
    } else {
      throw std::runtime_error(
          "CameraNodeBase: cannot open " + device_);
    }
  } else {
    running_ = true;
    reader_thread_ = std::thread(&CameraNodeBase::readerLoop, this);
  }

  RCLCPP_INFO(get_logger(),
      "%s UP: device=%s %dx%d %s @ %.1f fps stub=%d",
      get_name(), device_.c_str(), width_, height_,
      encoding_.c_str(), fps_,
      static_cast<int>(stub_mode_));
}

void CameraNodeBase::readerLoop() {
  while (running_) {
    uint64_t ts_ns = 0;
    auto data = v4l2_->dequeueFrame(100ms, &ts_ns);
    if (data.empty()) continue;
    if (!isPlausibleBuffer(encoding_, width_, height_, data.size())) {
      ++drop_count_;
      continue;
    }
    if (ts_ns == 0) {
      ts_ns = static_cast<uint64_t>(now().nanoseconds());
    }
    publishFrame(std::move(data), ts_ns);
    ++frame_count_;
    ++seq_;
  }
}

void CameraNodeBase::stubTick() {
  auto data = generateStubFrame();
  if (data.empty()) return;
  publishFrame(std::move(data),
                static_cast<uint64_t>(now().nanoseconds()));
  ++frame_count_;
  ++seq_;
}

void CameraNodeBase::onHealthTick() {
  RCLCPP_INFO(get_logger(),
      "%s mode=%s frames=%u drops=%u",
      get_name(), stub_mode_ ? "STUB" : "REAL",
      frame_count_.load(), drop_count_.load());
}

}  // namespace san_cameras
