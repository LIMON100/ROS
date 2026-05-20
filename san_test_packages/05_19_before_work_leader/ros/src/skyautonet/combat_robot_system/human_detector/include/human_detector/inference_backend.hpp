// SAN v1.5.1 (was v1.3 PHASE 6 — see DCN-2026-004 D-011) - AI inference backend abstraction.
//
// Decouples the detector node from the underlying NPU SDK so Phase 1
// (RK3588 onboard NPU, 6 TOPS) and the PoC stage (Hailo-8 M.2, 26 TOPS)
// can be swapped via yaml without touching the detector code.
// Reference: SAN-SDD-SWARM-001 v1.3 §4 (AI accelerator).
//
// Build-time guarding:
//   * RK3588NPUBackend     — compiled in when HAVE_RKNN is defined
//   * Hailo8Backend         — compiled in when HAVE_HAILORT is defined
//   * StubBackend           — always available; returns no detections
//     and a fixed latency. Lets CI / desktop dev runs proceed without
//     vendor SDKs.

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cv {
class Mat;   // forward decl - kept out of the interface header so a
             // pure-stub build doesn't need OpenCV at all.
}

namespace human_detector {

// Single inference result. Matches the v1.1 perception/rknn_inference.py
// Detection dataclass on the Python side so cross-language consumers
// can rely on the same field set.
struct Detection {
    std::string label;            // e.g. "person"
    float       confidence = 0.f; // [0, 1]
    int         class_id = -1;    // COCO id; -1 = unknown
    // bbox in image pixels: [x1, y1, x2, y2]
    std::array<float, 4> bbox{0.f, 0.f, 0.f, 0.f};

    Detection() = default;
    Detection(std::string l, float c, int cls,
              float x1, float y1, float x2, float y2)
        : label(std::move(l)), confidence(c), class_id(cls),
          bbox{x1, y1, x2, y2} {}
};

class InferenceBackend {
public:
    virtual ~InferenceBackend() = default;

    // Initialize with a model file path. Returns false on any
    // hardware/SDK failure; the caller is expected to fall back to a
    // different backend rather than retry.
    virtual bool initialize(const std::string& model_path) = 0;

    // Run one forward pass. Returns the detections in image-pixel
    // coordinates. `frame` is BGR/BGRA expected by the underlying SDK
    // (each backend converts as needed).
    virtual std::vector<Detection> infer(const cv::Mat& frame) = 0;

    // Rolling EMA of the most recent infer() wall-clock latency in
    // milliseconds. 0.0 before the first inference.
    virtual double getInferenceLatencyMs() const = 0;

    // Stable identifier used in log lines + RobotStatus telemetry.
    virtual std::string getName() const = 0;

    // True when initialize() succeeded and infer() can be called.
    virtual bool isReady() const = 0;
};

// Factory. `name` is one of: "rk3588", "hailo8", "stub" (always
// available). An unknown name returns nullptr; a request for a
// backend whose SDK was not compiled in returns the stub. Callers
// should still check isReady() after initialize().
std::unique_ptr<InferenceBackend> createBackend(const std::string& name);

}  // namespace human_detector
