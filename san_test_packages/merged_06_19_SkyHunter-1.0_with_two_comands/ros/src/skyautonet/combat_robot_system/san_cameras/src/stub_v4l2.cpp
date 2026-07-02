// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 6 — Stub V4L2 backend.
//
// SAFETY NOTE (Phase 0 PR-B): `makeRealV4l2()` was previously
// unconditionally returning a stub. This is dangerous in production —
// `Imx678Node` / `ThermalNode` constructed via the default ctor would
// always be in stub mode, silently never producing real frames.
//
// Behavior now:
//   * The default factory below is wrapped in `SAN_CAMERAS_ALLOW_STUB_V4L2`
//     CMake guard. When OFF (production default), the factory FATAL-
//     ERRORs the build because no real backend is linked.
//   * When ON (bringup / CI), the stub IS linked, but `makeStubV4l2()`
//     is the explicit name. The convenience `makeRealV4l2()` shim is
//     marked deprecated and returns a stub only when both:
//       (a) the CMake gate is ON, AND
//       (b) the caller node has explicitly set
//           `allow_stub_v4l2: true` parameter (enforced by node code).

#include "san_cameras/v4l2_interface.hpp"

#include <iostream>

namespace san_cameras
{

namespace
{

class StubV4l2 : public V4l2CaptureInterface
{
public:
  bool open(const CaptureConfig & cfg) override
  {
    std::cerr << "[san_cameras][STUB-V4L2] open(" << cfg.device
              << " " << cfg.width << "x" << cfg.height
              << " " << cfg.encoding
              << " @ " << cfg.fps << " fps) — no real V4L2 backend.\n";
    return false;
  }
  void close() override {}
  std::vector<uint8_t> dequeueFrame(
    std::chrono::milliseconds, uint64_t *) override
  {
    return {};
  }
  bool isOpen() const override {return false;}
  bool isStub() const override {return true;}
};

}  // namespace

std::unique_ptr<V4l2CaptureInterface> makeStubV4l2()
{
  return std::make_unique<StubV4l2>();
}

#ifndef SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK
#  error "san_cameras: makeRealV4l2() requested but no real V4L2 backend " \
  "is linked. Build with -DSAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK=ON for " \
  "bringup/CI only — production builds MUST link a real backend. " \
  "Camera nodes will refuse to start with stub unless launch sets " \
  "allow_stub_v4l2:=true."
#endif

std::unique_ptr<V4l2CaptureInterface> makeRealV4l2()
{
  // Fall-back path enabled only when SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK
  // is defined (CMake option). Node still refuses to use this unless
  // its `allow_stub_v4l2` parameter is explicitly true.
  return makeStubV4l2();
}

}  // namespace san_cameras
