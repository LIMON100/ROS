#pragma once
#include "human_detector/inference_backend.hpp"
#include <memory>

class YoloEngine;  // fwd-decl

namespace human_detector {
class OnnxBackend : public InferenceBackend {
public:
    OnnxBackend();
    ~OnnxBackend() override;            // <-- out-of-line; fixes incomplete-type
    bool initialize(const std::string & model_path) override;
    std::vector<Detection> infer(const cv::Mat & frame) override;
    double getInferenceLatencyMs() const override { return latency_ms_; }
    std::string getName() const override { return "onnx"; }
    bool isReady() const override { return ready_; }

private:
    std::unique_ptr<YoloEngine> engine_;
    bool ready_ = false;
    double latency_ms_ = 0.0;
};
}  // namespace human_detector