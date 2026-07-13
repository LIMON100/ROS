#include "human_detector/onnx_backend.hpp"

#ifdef HAVE_ONNX
#include "human_detector/yolo_engine.hpp" // full YoloEngine type here
#include <opencv2/opencv.hpp>
#include <chrono>
#else
// ONNX Runtime not compiled in (HAVE_ONNX undefined). Provide a complete
// (empty) YoloEngine so the std::unique_ptr<YoloEngine> member's deleter is
// well-formed; engine_ is never instantiated in stub mode.
class YoloEngine {};
#endif

namespace human_detector {

// Constructor and Destructor
// Note: Destructor must be defined here because YoloEngine is forward-declared in the header
OnnxBackend::OnnxBackend() = default;
OnnxBackend::~OnnxBackend() = default;

#ifdef HAVE_ONNX

bool OnnxBackend::initialize(const std::string &model_path) {
    try {
        engine_ = std::make_unique<YoloEngine>(model_path, /*use_gpu=*/true);
        ready_ = true;
    } catch (...) {
        ready_ = false;
    }
    return ready_;
}

std::vector<Detection> OnnxBackend::infer(const cv::Mat &frame) {
    std::vector<Detection> out;
    if (!ready_) {
        return out;
    }

    // run_inference takes cv::Mat&, providing a local copy if necessary
    cv::Mat f = frame; 

    // Timing start
    const auto t0 = std::chrono::steady_clock::now();

    // Execute Inference
    auto boxes = engine_->run_inference(f);

    // Timing end and Latency calculation (Exponential Moving Average)
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
    
    latency_ms_ = (latency_ms_ <= 0.0) ? ms : (latency_ms_ * 0.9 + ms * 0.1);

    // Convert internal YOLOBox to InferenceBackend Detection format
    for (const auto &b : boxes) {
        std::string label;
        if (b.class_id >= 0 && b.class_id < static_cast<int>(engine_->class_names.size())) {
            label = engine_->class_names[b.class_id];
        }

        out.emplace_back(
            label,
            b.confidence,
            b.class_id,
            static_cast<float>(b.box.x),
            static_cast<float>(b.box.y),
            static_cast<float>(b.box.x + b.box.width),
            static_cast<float>(b.box.y + b.box.height)
        );
    }

    return out;
}

#else  // HAVE_ONNX

// Stub: ONNX Runtime absent. initialize() reports failure so the factory
// caller falls back to another backend (mirrors the hailo8/rk3588 stub policy).
bool OnnxBackend::initialize(const std::string & /*model_path*/) {
    return false;
}

std::vector<Detection> OnnxBackend::infer(const cv::Mat & /*frame*/) {
    return {};
}

#endif  // HAVE_ONNX

}  // namespace human_detector