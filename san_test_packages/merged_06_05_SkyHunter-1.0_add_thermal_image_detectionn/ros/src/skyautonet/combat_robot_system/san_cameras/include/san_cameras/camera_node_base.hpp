// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 6 — Shared camera node base.
//
// Both IMX678 and Thermal nodes share ~80% of structure:
//   - V4L2 reader thread
//   - Frame buffering / metadata
//   - Stub mode fallback
//   - Health timer
//
// Concrete subclasses only differ in:
//   - Default config (device, dimensions, encoding, fps)
//   - Output message type (CompressedImage for H.265, Image for raw)
//   - Stub frame generation
//
// PATCH 2026-05-13 (san_cameras deep-dive):
//   * CM1/CM7 — Subclass passes a `SubclassDefaults` struct to the
//     base ctor. The base uses these as the default values in its
//     declare_parameter calls. User launch overrides win cleanly
//     because the override-application happens INSIDE declare_parameter,
//     not via a later set_parameter that clobbers them. The pure-
//     virtual `declareDefaultsForSubclass()` is gone — its only job
//     was to push subclass-specific defaults, and the struct approach
//     does that without virtual dispatch from derived ctor body.
//   * CM2/CM3 — seq_ and stub_mode_ promoted to std::atomic.
//   * CM4 — V4L2 timestamps are validated (must be within ±60 s of
//     local clock); out-of-band stamps are replaced with now().
//   * CM5/CM11 — drop_count_ increments are accompanied by throttled
//     RCLCPP_WARN that includes the reason (size mismatch, encoding,
//     timestamp out of band).
//   * CM6 — startCapture() is renamed to start() and is no longer
//     called from the derived ctor. main() calls node->start() after
//     constructing. Derived ctors only declare publishers/state, so
//     adding state after start() can no longer race the reader thread.
//   * CM14 — readerLoop() wraps its body in a try/catch so a stray
//     exception from publishFrame doesn't terminate the process.
//   * CM15 — drop_count_ overflow-safe via std::atomic<uint64_t>.

#ifndef SAN_CAMERAS__CAMERA_NODE_BASE_HPP_
#define SAN_CAMERAS__CAMERA_NODE_BASE_HPP_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>

#include "san_cameras/v4l2_interface.hpp"

namespace san_cameras
{

/// ★ PATCH 2026-05-13 (CM1/CM7): subclass-supplied defaults. The
/// base ctor uses these as declare_parameter() defaults so the
/// user's parameter_overrides (from launch / CLI) are respected.
struct SubclassDefaults
{
  std::string device;
  uint32_t width;
  uint32_t height;
  std::string encoding;
  double fps;
  std::string frame_id;
};

class CameraNodeBase : public rclcpp::Node
{
protected:
  CameraNodeBase(
    const std::string & node_name,
    const rclcpp::NodeOptions & opts,
    std::unique_ptr<V4l2CaptureInterface> v4l2,
    const SubclassDefaults & defaults);
  ~CameraNodeBase() override;

  /// Subclass hooks
  virtual std::vector<uint8_t> generateStubFrame() = 0;
  /// Publish a frame (real or stub). Caller passes the byte data;
  /// subclass converts to the appropriate ROS message.
  virtual void publishFrame(
    std::vector<uint8_t> && data,
    uint64_t timestamp_ns) = 0;

  /// Subclass-readable params (populated after loadCommonParameters).
  std::string device_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  std::string encoding_;
  double fps_ = 30.0;
  std::string frame_id_;
  bool stub_on_no_device_ = true;

  /// Stats — subclass may include in its own logs.
  /// ★ PATCH 2026-05-13 (CM2/CM15): atomic uint64_t (was raw u32).
  std::atomic<uint64_t> frame_count_{0};
  std::atomic<uint64_t> drop_count_{0};

public:
  /// ★ PATCH 2026-05-13 (CM6): explicit start. main() (or test
  /// harness) must call start() after constructing the node. The
  /// reader thread does not run until start() returns successfully.
  /// Returns false on a fatal failure (unknown encoding, fps out
  /// of range, device unopenable AND stub_on_no_device=false).
  bool start();

  /// Test/diagnostic accessors.
  bool isStubMode() const {return stub_mode_.load();}
  bool isRunning() const {return running_.load();}
  std::size_t framesPublished() const {return frame_count_.load();}
  std::size_t framesDropped() const {return drop_count_.load();}

  /// ★ PATCH 2026-05-13 (CM4): max allowed stamp drift from local
  /// clock before we substitute now(). Exposed for tests.
  static constexpr int64_t kMaxStampDriftNs = 60'000'000'000LL;  // 60 s

private:
  void readerLoop();
  void stubTick();
  void onHealthTick();
  bool loadCommonParameters();
  /// ★ PATCH 2026-05-13 (CM4): clamp / replace bad timestamps.
  uint64_t validateOrReplaceTimestamp(uint64_t ts_ns);
  /// ★ PATCH 2026-05-13 (CM5/CM11): throttled drop log helper.
  void logDropThrottled(const char * reason, std::size_t buffer_size);

  std::unique_ptr<V4l2CaptureInterface> v4l2_;
  std::thread reader_thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stub_mode_{false};       // ★ PATCH (CM3)
  rclcpp::TimerBase::SharedPtr health_timer_;
  rclcpp::TimerBase::SharedPtr stub_timer_;
  // PATCH (CM2) atomic seq counter — readable across reader thread + ROS
  // callbacks without a lock.
  std::atomic<uint64_t> seq_{0};

  /// Phase 1 (PR #123): latched stub-status. Published once at startup
  /// so downstream consumers can refuse to trust this camera's frames.
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stub_status_pub_;
};

}  // namespace san_cameras

#endif  // SAN_CAMERAS__CAMERA_NODE_BASE_HPP_
