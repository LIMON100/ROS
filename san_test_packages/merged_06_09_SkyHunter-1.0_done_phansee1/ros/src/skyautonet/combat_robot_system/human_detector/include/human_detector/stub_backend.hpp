// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.1 (was v1.3 PHASE 6 — see DCN-2026-004 D-011) - always-available stub backend.
//
// Used in two situations:
//   1. CI builds with no vendor SDK present
//   2. Selected explicitly via `inference_backend: stub` for end-to-end
//      pipeline tests where the rest of the system needs to run but
//      the model output is not under test.
//
// initialize() always succeeds. infer() returns an empty vector and a
// fixed 1 ms latency so downstream code that depends on
// getInferenceLatencyMs() observes a non-zero value.

#pragma once

#include "human_detector/inference_backend.hpp"

namespace human_detector
{

class StubBackend : public InferenceBackend
{
public:
  bool initialize(const std::string & /*model_path*/) override
  {
    ready_ = true;
    return true;
  }
  std::vector<Detection> infer(const cv::Mat & /*frame*/) override
  {
    latency_ms_ = 1.0;
    return {};
  }
  double getInferenceLatencyMs() const override {return latency_ms_;}
  std::string getName() const override {return "stub";}
  bool isReady() const override {return ready_;}

private:
  bool ready_ = false;
  double latency_ms_ = 0.0;
};

}  // namespace human_detector
