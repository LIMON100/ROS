// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 6 — V4L2 capture abstraction.
//
// Both IMX678 and Thermal nodes use this abstraction so:
//   * Real V4L2 backend (production) — opens /dev/videoN, ioctl
//   * Stub backend (CI/dev) — returns empty captures
//   * Mock backend (tests, optional Turn 6.5) — canned frames
//
// Frames are returned as captured byte buffers; encoding/dimension
// validation is done by the caller via frame_metadata utilities.

#ifndef SAN_CAMERAS__V4L2_INTERFACE_HPP_
#define SAN_CAMERAS__V4L2_INTERFACE_HPP_

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace san_cameras
{

struct CaptureConfig
{
  std::string device;        // /dev/videoN
  uint32_t width;
  uint32_t height;
  std::string encoding;      // "h265", "mono16", "yuyv", ...
  double fps;
};

struct CapturedFrame
{
  uint64_t timestamp_ns;
  uint32_t seq;
  std::vector<uint8_t> data;
};

class V4l2CaptureInterface
{
public:
  virtual ~V4l2CaptureInterface() = default;

  /// Configure + start streaming. Returns false on any failure
  /// (device missing, format unsupported, mmap fail, etc.).
  virtual bool open(const CaptureConfig & cfg) = 0;

  /// Stop streaming + close FD.
  virtual void close() = 0;

  /// Block up to `timeout` for the next frame. Returns std::nullopt
  /// on timeout. Frame data is move-able (uses internal buffer pool).
  virtual std::vector<uint8_t> dequeueFrame(
    std::chrono::milliseconds timeout,
    uint64_t * timestamp_ns_out) = 0;

  virtual bool isOpen() const = 0;

  /// True if this implementation is a stub (no real V4L2 hardware
  /// access). Nodes use this to refuse-to-start in production unless
  /// the user has explicitly opted in via parameter.
  virtual bool isStub() const {return false;}
};

/// Real V4L2 backend — fail-loud unless SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK
/// is defined at build time (in which case it returns a stub).
std::unique_ptr<V4l2CaptureInterface> makeRealV4l2();

/// Explicit stub backend. Only safe for bringup tests / CI.
/// Nodes must check `isStub()` and the runtime `allow_stub_v4l2`
/// parameter before accepting a stub-returning implementation.
std::unique_ptr<V4l2CaptureInterface> makeStubV4l2();

}  // namespace san_cameras

#endif  // SAN_CAMERAS__V4L2_INTERFACE_HPP_
