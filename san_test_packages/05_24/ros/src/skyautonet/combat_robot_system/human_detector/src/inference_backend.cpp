// SAN v1.5.1 (was v1.3 PHASE 6 — see DCN-2026-004 D-011) - InferenceBackend factory.

#include "human_detector/inference_backend.hpp"
#include "human_detector/rk3588_npu_backend.hpp"
#include "human_detector/hailo8_backend.hpp"
#include "human_detector/stub_backend.hpp"

#include <algorithm>
#include <cctype>

namespace human_detector {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

}  // namespace

std::unique_ptr<InferenceBackend> createBackend(const std::string& name) {
    const std::string n = toLower(name);
    if (n == "rk3588" || n == "rk3588_npu" || n == "rknn") {
        return std::make_unique<RK3588NPUBackend>();
    }
    if (n == "hailo8" || n == "hailo" || n == "hailort") {
        return std::make_unique<Hailo8Backend>();
    }
    if (n == "stub" || n == "none" || n == "cpu") {
        return std::make_unique<StubBackend>();
    }
    // Unknown name - return nullptr so the caller logs the misconfig
    // and falls back to its own default policy.
    return nullptr;
}

}  // namespace human_detector
