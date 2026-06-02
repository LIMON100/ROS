// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 6 — CameraNodeBase implementation.

#include "san_cameras/camera_node_base.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <stdexcept>

#include <std_msgs/msg/bool.hpp>

#include "san_cameras/frame_metadata.hpp"

namespace san_cameras
{

using namespace std::chrono_literals;

CameraNodeBase::CameraNodeBase(
  const std::string & node_name,
  const rclcpp::NodeOptions & opts,
  std::unique_ptr<V4l2CaptureInterface> v4l2,
  const SubclassDefaults & defaults)
: rclcpp::Node(node_name, opts), v4l2_(std::move(v4l2))
{
  if (!v4l2_) {
    throw std::runtime_error("CameraNodeBase: null V4L2 backend");
  }

  // ★ PATCH 2026-05-13 (CM1/CM7): declare with subclass-provided
  // defaults. Any user override via NodeOptions.parameter_overrides()
  // is applied INSIDE declare_parameter and is respected — we never
  // clobber it with a later set_parameter call.
  declare_parameter<std::string>("device", defaults.device);
  declare_parameter<int>("width", static_cast<int>(defaults.width));
  declare_parameter<int>("height", static_cast<int>(defaults.height));
  declare_parameter<std::string>("encoding", defaults.encoding);
  declare_parameter<double>("fps", defaults.fps);
  declare_parameter<std::string>("frame_id", defaults.frame_id);
  declare_parameter<bool>("stub_on_no_device", true);
  // Phase 0 PR-B: explicit opt-in gate when the linked V4L2 backend
  // is a stub (e.g. SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK=ON build).
  // Production launch MUST leave this false; the node refuses to
  // start with a stub backend otherwise.
  declare_parameter<bool>("allow_stub_v4l2", false);

  health_timer_ = create_wall_timer(
    1s, std::bind(&CameraNodeBase::onHealthTick, this));

  // Phase 1: latched stub-status publisher. Late-joining subscribers
  // immediately see whether this camera is producing real frames.
  stub_status_pub_ = create_publisher<std_msgs::msg::Bool>(
    "~/stub_status",
    rclcpp::QoS(1).transient_local().reliable());
}

CameraNodeBase::~CameraNodeBase()
{
  // ★ PATCH 2026-05-13: shutdown order matters.
  //   1. clear running flag — readerLoop exits its next iteration
  //   2. close V4L2 — wakes any blocking dequeue (real backend)
  //   3. join thread — only AFTER the above (was: join then close,
  //      which could leave dequeue blocked indefinitely on real V4L2)
  running_.store(false);
  if (v4l2_) {v4l2_->close();}
  if (reader_thread_.joinable()) {reader_thread_.join();}
}

bool CameraNodeBase::loadCommonParameters()
{
  device_ = get_parameter("device").as_string();
  width_ = static_cast<uint32_t>(get_parameter("width").as_int());
  height_ = static_cast<uint32_t>(get_parameter("height").as_int());
  encoding_ = get_parameter("encoding").as_string();
  fps_ = get_parameter("fps").as_double();
  frame_id_ = get_parameter("frame_id").as_string();
  stub_on_no_device_ = get_parameter("stub_on_no_device").as_bool();
  if (fps_ <= 0.0 || fps_ > 240.0) {
    RCLCPP_ERROR(
      get_logger(),
      "CameraNodeBase: fps=%.2f out of (0, 240] range", fps_);
    return false;
  }
  if (!bytesPerPixel(encoding_).has_value()) {
    RCLCPP_ERROR(
      get_logger(),
      "CameraNodeBase: unknown encoding '%s'", encoding_.c_str());
    return false;
  }
  if (width_ == 0 || height_ == 0) {
    RCLCPP_ERROR(
      get_logger(),
      "CameraNodeBase: width/height must be > 0 (got %ux%u)",
      width_, height_);
    return false;
  }
  return true;
}

bool CameraNodeBase::start()
{
  // ★ PATCH 2026-05-13 (CM6): explicit start.
  if (running_.load() || stub_timer_) {
    RCLCPP_WARN(get_logger(), "start() called twice — ignoring");
    return true;
  }
  if (!loadCommonParameters()) {return false;}

  // Phase 0 PR-B: reject stub backend unless caller explicitly opted
  // in via allow_stub_v4l2. This prevents "build green, looks running,
  // never publishes" production deployments where SAN_CAMERAS_ALLOW_
  // STUB_V4L2_FALLBACK is ON but launch forgot to install a real
  // backend.
  const bool allow_stub = get_parameter("allow_stub_v4l2").as_bool();
  if (v4l2_->isStub() && !allow_stub) {
    throw std::runtime_error(
            std::string("CameraNodeBase: linked V4L2 backend is a STUB ") +
            "(no real hardware access). Refusing to start. Either link " +
            "a real V4L2 backend (build with " +
            "-DSAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK=OFF and supply a " +
            "real implementation), or set the launch parameter " +
            "allow_stub_v4l2:=true (bringup/CI ONLY).");
  }

  CaptureConfig cfg{device_, width_, height_, encoding_, fps_};
  if (!v4l2_->open(cfg)) {
    if (stub_on_no_device_) {
      RCLCPP_WARN(
        get_logger(),
        "V4L2 device %s unavailable — STUB mode @ %.1f fps",
        device_.c_str(), fps_);
      stub_mode_.store(true);
      const auto period_ms = std::chrono::milliseconds(
        std::max<int64_t>(1, static_cast<int64_t>(1000.0 / fps_)));
      stub_timer_ = create_wall_timer(
        period_ms, std::bind(&CameraNodeBase::stubTick, this));
    } else {
      RCLCPP_ERROR(
        get_logger(),
        "CameraNodeBase: cannot open '%s' and stub_on_no_device=false",
        device_.c_str());
      return false;
    }
  } else {
    running_.store(true);
    reader_thread_ = std::thread(&CameraNodeBase::readerLoop, this);
  }

  // Phase 1: latch the stub status now that the mode is settled.
  // (Publisher was created in the ctor with transient_local QoS.)
  if (stub_status_pub_) {
    std_msgs::msg::Bool m;
    m.data = stub_mode_;
    stub_status_pub_->publish(m);
  }

  RCLCPP_INFO(
    get_logger(),
    "%s UP: device=%s %dx%d %s @ %.1f fps stub=%d",
    get_name(), device_.c_str(), width_, height_,
    encoding_.c_str(), fps_,
    static_cast<int>(stub_mode_.load()));
  return true;
}

void CameraNodeBase::readerLoop()
{
  while (running_.load()) {
    // ★ PATCH 2026-05-13 (CM14): catch escaped exceptions so the
    // process doesn't terminate on a transient publishFrame error.
    try {
      uint64_t ts_ns = 0;
      auto data = v4l2_->dequeueFrame(100ms, &ts_ns);
      if (data.empty()) {continue;}
      if (!isPlausibleBuffer(encoding_, width_, height_, data.size())) {
        logDropThrottled("size_mismatch", data.size());
        ++drop_count_;
        continue;
      }
      ts_ns = validateOrReplaceTimestamp(ts_ns);
      publishFrame(std::move(data), ts_ns);
      ++frame_count_;
      ++seq_;
    } catch (const std::exception & e) {
      RCLCPP_ERROR_THROTTLE(
        get_logger(), *get_clock(), 1000,
        "readerLoop exception (continuing): %s", e.what());
      ++drop_count_;
    } catch (...) {
      RCLCPP_ERROR_THROTTLE(
        get_logger(), *get_clock(), 1000,
        "readerLoop unknown exception (continuing)");
      ++drop_count_;
    }
  }
}

void CameraNodeBase::stubTick()
{
  try {
    auto data = generateStubFrame();
    if (data.empty()) {return;}
    const uint64_t ts_ns =
      validateOrReplaceTimestamp(
      static_cast<uint64_t>(
        now().nanoseconds()));
    publishFrame(std::move(data), ts_ns);
    ++frame_count_;
    ++seq_;
  } catch (const std::exception & e) {
    RCLCPP_ERROR_THROTTLE(
      get_logger(), *get_clock(), 1000,
      "stubTick exception (continuing): %s", e.what());
  }
}

void CameraNodeBase::onHealthTick()
{
  RCLCPP_INFO(
    get_logger(),
    "%s mode=%s frames=%lu drops=%lu",
    get_name(), stub_mode_.load() ? "STUB" : "REAL",
    static_cast<unsigned long>(frame_count_.load()),
    static_cast<unsigned long>(drop_count_.load()));
}

uint64_t CameraNodeBase::validateOrReplaceTimestamp(uint64_t ts_ns)
{
  // ★ PATCH 2026-05-13 (CM4): treat 0 / out-of-band stamps as bad.
  const int64_t now_ns = now().nanoseconds();
  if (ts_ns == 0) {
    return static_cast<uint64_t>(now_ns);
  }
  const int64_t ts_signed = static_cast<int64_t>(ts_ns);
  const int64_t drift =
    ts_signed > now_ns ? ts_signed - now_ns : now_ns - ts_signed;
  if (drift > kMaxStampDriftNs) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "frame timestamp drift %.1fs > %.1fs limit — using local clock",
      drift / 1e9, kMaxStampDriftNs / 1e9);
    return static_cast<uint64_t>(now_ns);
  }
  return ts_ns;
}

void CameraNodeBase::logDropThrottled(
  const char * reason,
  std::size_t buffer_size)
{
  RCLCPP_WARN_THROTTLE(
    get_logger(), *get_clock(), 1000,
    "dropped frame: reason=%s buffer_size=%zu encoding=%s expected=%ux%u",
    reason, buffer_size, encoding_.c_str(), width_, height_);
}

}  // namespace san_cameras
