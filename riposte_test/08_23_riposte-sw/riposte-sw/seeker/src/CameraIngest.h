#pragma once
#include "IDetector.h" // Frame

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "riposte/Tunables.h"

namespace riposte {

// ---- V4L2 dequeue boundary predicates (P1-05, pure — host-tested) ----------
// Driver-returned buffer metadata (index, bytesused, data_offset) is UNTRUSTED
// input: these decide whether a dequeued buffer is usable before any pointer
// arithmetic touches it. A short buffer or a wild index once meant an OOB read
// in the NV12 conversion or an uncaught .at() exception killing the process.

// Minimum payload bytes a whole frame needs. `stride` is the driver's
// bytesperline — BYTES per row: NV12's Y rows are `width`-ish bytes and the
// frame is stride x h x 3/2; packed 4:2:2 rows already carry 2 bytes/pixel,
// so the frame is stride x h.
inline std::size_t v4l2_frame_bytes_required(bool nv12, int height, std::size_t stride) {
    if (height <= 0) {
        return 0; // degenerate geometry: caller rejects via v4l2_payload_ok
    }
    const auto rows = static_cast<std::size_t>(height);
    return nv12 ? (stride * rows * 3U / 2U) : (stride * rows);
}

// The usable payload (bytesused - data_offset) must cover a whole frame —
// anything less would have the consumers reading past what the driver filled.
inline bool v4l2_payload_ok(uint32_t bytesused, uint32_t data_offset, bool nv12,
                            int height, std::size_t stride) {
    if (height <= 0 || stride == 0U || bytesused == 0U || data_offset >= bytesused) {
        return false;
    }
    return (static_cast<std::size_t>(bytesused) - data_offset) >=
           v4l2_frame_bytes_required(nv12, height, stride);
}

// NV12 (4:2:0) and the packed 4:2:2 sources both subsample chroma: odd
// negotiated dimensions would make every plane/row size computation lie, and a
// bytesperline narrower than a row cannot hold the image it claims to
// (NV12: >= width bytes; packed 4:2:2: >= 2 x width bytes).
inline bool v4l2_geometry_ok(bool nv12, int width, int height, std::size_t stride) {
    if (width <= 0 || height <= 0 || ((width % 2) != 0) || ((height % 2) != 0)) {
        return false;
    }
    const auto min_stride = static_cast<std::size_t>(width) * (nv12 ? 1U : 2U);
    return stride >= min_stride;
}

// Frame source abstraction. V4L2Camera drives the MIPI-CSI sensor on the target;
// SyntheticCamera emits blank frames at a fixed rate for SIL. Policy is
// latest-wins: the seeker always processes the newest frame and discards backlog.
class ICamera {
public:
    ICamera() = default;
    virtual ~ICamera() = default;
    ICamera(const ICamera&) = delete;
    ICamera& operator=(const ICamera&) = delete;
    ICamera(ICamera&&) = delete;
    ICamera& operator=(ICamera&&) = delete;
    virtual bool open() = 0;
    // Blocks up to timeout_ms for the next frame. `f.data` is valid until the
    // next grab() call. Returns false on timeout/error.
    virtual bool grab(Frame& f, int timeout_ms) = 0;
    // NEGOTIATED frame dimensions, valid after a successful open(). The V4L2
    // driver may substitute a different mode than requested; every consumer
    // that sizes buffers (recorder, estimator aspect) must use these, never
    // the configured values. Per-frame stride is carried in Frame.stride.
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual const char* name() const = 0;
};

// SIL camera: NV12 gray frames paced at SEEKER_FRAME_HZ. Pixel content is
// irrelevant because SyntheticDetector ignores it; it exists to drive real
// capture timing/jitter. A NON-BLOCKING caller (timeout 0) models a BUFFERED
// camera: like the V4L2 mmap ring, the most recently produced frame stays
// available (with its original timestamp) until the next one is due — without
// this, two independently-paced synthetic cameras phase-drift and the second
// channel misses every other tick, which no buffered real camera does.
class SyntheticCamera final : public ICamera {
public:
    SyntheticCamera(int w, int h)
        : w_(w), h_(h), buf_(static_cast<size_t>(w * h * 3 / 2), 16) {}
    bool open() override;
    bool grab(Frame& f, int timeout_ms) override;
    int width() const override { return w_; }
    int height() const override { return h_; }
    const char* name() const override { return "SyntheticCamera"; }

private:
    void fill(Frame& f, uint64_t mono_ns) const;

    int w_, h_;
    std::vector<uint8_t> buf_;
    uint64_t next_ns_ = 0;
    uint64_t last_ns_ = 0; // timestamp of the most recently produced frame
};

// V4L2 MIPI-CSI camera (target). Implemented in CameraIngest.cpp under
// RIPOSTE_WITH_V4L2; uses mmap streaming buffers, dequeues newest, requeues rest.
// On RK3588 the MIPI-CSI pipeline (D-PHY lanes, ISP links) is wired by the kernel
// device tree + rkisp; this userspace driver just opens the ISP capture node and
// negotiates NV12 (falling back to YUYV/UYVY + software convert for UVC sensors).
class V4L2Camera final : public ICamera {
public:
    struct Params {
        std::string device = "/dev/video0"; // ISP mainpath / UVC capture node
        int width = 1280, height = 720;
        // Requested rate. Whether the sensor mode actually delivers 60 fps is
        // a bring-up check, not an assumption.
        int fps = tun::SEEKER_FRAME_HZ; // stored; not renegotiated with the driver
        int num_buffers = 6;            // mmap streaming buffers (driver may grant fewer)
    };
    explicit V4L2Camera(const Params& params);
    ~V4L2Camera() override;

    // Owns the V4L2 fd / mmap buffers (Impl); not copyable or movable.
    V4L2Camera(const V4L2Camera&) = delete;
    V4L2Camera& operator=(const V4L2Camera&) = delete;
    V4L2Camera(V4L2Camera&&) = delete;
    V4L2Camera& operator=(V4L2Camera&&) = delete;
    bool open() override;
    bool grab(Frame& f, int timeout_ms) override;
    int width() const override;  // negotiated (may differ from Params::width)
    int height() const override; // negotiated (may differ from Params::height)
    const char* name() const override { return "V4L2Camera"; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace riposte
