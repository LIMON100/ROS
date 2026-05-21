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

#ifndef SAN_CAMERAS__CAMERA_NODE_BASE_HPP_
#define SAN_CAMERAS__CAMERA_NODE_BASE_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>

#include "san_cameras/v4l2_interface.hpp"

namespace san_cameras {

class CameraNodeBase : public rclcpp::Node {
protected:
  CameraNodeBase(const std::string& node_name,
                  const rclcpp::NodeOptions& opts,
                  std::unique_ptr<V4l2CaptureInterface> v4l2);
  ~CameraNodeBase() override;

  /// Subclass hooks
  virtual std::vector<uint8_t> generateStubFrame() = 0;
  /// Publish a frame (real or stub). Caller passes the byte data;
  /// subclass converts to the appropriate ROS message.
  virtual void publishFrame(std::vector<uint8_t>&& data,
                             uint64_t timestamp_ns) = 0;
  /// Subclass declares ros2 parameters with appropriate defaults.
  virtual void declareDefaultsForSubclass() = 0;

  /// Subclass-readable params (populated after loadCommonParameters).
  std::string device_;
  uint32_t    width_  = 0;
  uint32_t    height_ = 0;
  std::string encoding_;
  double      fps_ = 30.0;
  std::string frame_id_;
  bool        stub_on_no_device_ = true;

  /// Stats — subclass may include in its own logs.
  std::atomic<uint32_t> frame_count_{0};
  std::atomic<uint32_t> drop_count_{0};

  /// Called by subclass ctor AFTER it has declared its parameters.
  /// Loads them into the protected fields and starts capture/stub
  /// loop.
  void startCapture();

private:
  void readerLoop();
  void stubTick();
  void onHealthTick();
  void loadCommonParameters();

  std::unique_ptr<V4l2CaptureInterface> v4l2_;
  std::thread       reader_thread_;
  std::atomic<bool> running_{false};
  rclcpp::TimerBase::SharedPtr health_timer_;
  rclcpp::TimerBase::SharedPtr stub_timer_;
  bool                          stub_mode_ = false;
  uint64_t                      seq_ = 0;
};

}  // namespace san_cameras

#endif  // SAN_CAMERAS__CAMERA_NODE_BASE_HPP_
