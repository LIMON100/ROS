// SAN v1.5 Phase 2-E Turn 6 — Stub V4L2 backend.

#include "san_cameras/v4l2_interface.hpp"

#include <iostream>

namespace san_cameras {

namespace {

class StubV4l2 : public V4l2CaptureInterface {
public:
  bool open(const CaptureConfig& cfg) override {
    std::cerr << "[san_cameras][STUB-V4L2] open(" << cfg.device
              << " " << cfg.width << "x" << cfg.height
              << " " << cfg.encoding
              << " @ " << cfg.fps << " fps) — no real V4L2 backend.\n";
    return false;
  }
  void close() override {}
  std::vector<uint8_t> dequeueFrame(
      std::chrono::milliseconds, uint64_t*) override {
    return {};
  }
  bool isOpen() const override { return false; }
};

}  // namespace

std::unique_ptr<V4l2CaptureInterface> makeRealV4l2() {
  return std::make_unique<StubV4l2>();
}

}  // namespace san_cameras
