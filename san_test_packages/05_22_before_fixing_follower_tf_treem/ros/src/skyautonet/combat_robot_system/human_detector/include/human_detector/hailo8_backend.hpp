// SAN v1.5.1 (was v1.3 PHASE 6 — see DCN-2026-004 D-011) - Hailo-8 M.2 backend (HailoRT SDK).
//
// 26 TOPS, YOLO11s, ~30 fps target. The HailoRT runtime is only
// present when the M.2 module is fitted; on a non-Hailo host the
// HAVE_HAILORT macro is undefined and initialize() returns false so
// the factory falls back to the RK3588 backend (and then the stub).

#pragma once

#include <atomic>
#include <memory>
#include <mutex>

#include "human_detector/inference_backend.hpp"

#ifdef HAVE_HAILORT
#include <hailo/hailort.hpp>
#endif

namespace human_detector {

class Hailo8Backend : public InferenceBackend {
public:
    Hailo8Backend();
    ~Hailo8Backend() override;

    bool initialize(const std::string& model_path) override;
    std::vector<Detection> infer(const cv::Mat& frame) override;
    double getInferenceLatencyMs() const override { return latency_ema_ms_.load(); }
    std::string getName() const override { return "hailo8"; }
    bool isReady() const override { return ready_.load(); }

    void setLatencyForTest(double ms) { latency_ema_ms_.store(ms); }

private:
    std::atomic<bool>   ready_;
    std::atomic<double> latency_ema_ms_;
    std::mutex          infer_mutex_;

#ifdef HAVE_HAILORT
    std::unique_ptr<hailort::VDevice> vdevice_;
    std::shared_ptr<hailort::ConfiguredNetworkGroup> network_group_;
#endif

    static constexpr double kLatencyEmaAlpha = 0.2;
};

}  // namespace human_detector
