// SAN v1.5.1 PHASE 6 - RK3588 onboard NPU backend (RKNN SDK).
//
// 6 TOPS, YOLOv5n/s quantized, ~15-25 fps target. The RKNN runtime is
// only present on RK3588 hardware images; on a desktop build the
// HAVE_RKNN macro is undefined and initialize() returns false so the
// factory falls back to the stub.
//
// DCN-2026-003 D-003 (2026-05-13): full YOLO post-process ported from
// Airys V6.13.5 (src/board/rknn_detector_board.cpp). The Airys impl
// has been field-validated on RK3576/RK3588J with the same
// yolov5s-640-640_rk3588.rknn model SAN ships in models/.
//
// Implementation notes:
//   - 3-head YOLOv5 (strides 8/16/32, 3 anchors per head)
//   - sigmoid + anchor decode + class confidence × objectness
//   - NMS (IoU 0.45) with thread_local scratch (no per-frame heap)
//   - rknn_inputs_set + rknn_run + rknn_outputs_get (CPU memcpy path)
//   - want_float=1 outputs; INT8 quant handled inside librknnrt
//   - Class id 0 = "person" (COCO), label mapped via static table

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>

#include "human_detector/inference_backend.hpp"

#ifdef HAVE_RKNN
extern "C" {
#include <rknn_api.h>
}
#endif

namespace human_detector {

// COCO 80-class label table (kept short — only the labels we explicitly
// remap in HumanDetectorNode::cocoToSanClassId are used downstream).
// Anything beyond index 79 falls through to CLASS_UNKNOWN.
const std::string& cocoLabel(int class_id);

class RK3588NPUBackend : public InferenceBackend {
public:
    RK3588NPUBackend();
    ~RK3588NPUBackend() override;

    bool initialize(const std::string& model_path) override;
    std::vector<Detection> infer(const cv::Mat& frame) override;
    double getInferenceLatencyMs() const override { return latency_ema_ms_.load(); }
    std::string getName() const override { return "rk3588"; }
    bool isReady() const override { return ready_.load(); }

    // Tunables (v1.5.1)
    void setConfidenceThreshold(float t) { conf_threshold_ = t; }
    void setNmsIouThreshold(float t)     { nms_iou_threshold_ = t; }

    // Test hook: override latency directly so unit tests can verify
    // RobotStatus telemetry plumbing without a real NPU.
    void setLatencyForTest(double ms) { latency_ema_ms_.store(ms); }

private:
    std::atomic<bool>   ready_;
    std::atomic<double> latency_ema_ms_;
    std::mutex          infer_mutex_;     // RKNN context not thread-safe

    // YOLO post-process tunables (v1.5.1)
    float conf_threshold_    = 0.25f;
    float nms_iou_threshold_ = 0.45f;

#ifdef HAVE_RKNN
    rknn_context ctx_         = 0;
    int          model_in_w_  = 640;
    int          model_in_h_  = 640;
    bool         model_is_nhwc_ = true;   // detected from input_attrs[0].fmt
    std::vector<rknn_tensor_attr> input_attrs_;
    std::vector<rknn_tensor_attr> output_attrs_;
    uint32_t n_input_  = 0;
    uint32_t n_output_ = 0;

    // YOLOv5 grid decode: scratch buffer for letterboxed RGB input.
    // Resized in initialize() once the model dims are known.
    std::vector<uint8_t> rgb_input_buf_;
#endif

    // EMA smoothing for displayed latency. 0.2 = new sample is 20% weight.
    static constexpr double kLatencyEmaAlpha = 0.2;
};

}  // namespace human_detector
