// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "human_detector/hailo8_backend.hpp"

#include <chrono>
#include <cstdio>

#ifdef HAVE_HAILORT
#include <opencv2/opencv.hpp>
#endif

namespace human_detector
{

Hailo8Backend::Hailo8Backend()
: ready_(false), latency_ema_ms_(0.0)
{}

Hailo8Backend::~Hailo8Backend() = default;

#ifdef HAVE_HAILORT

namespace
{

double updateEma(double prev, double sample, double alpha)
{
  if (prev <= 0.0) {return sample;}
  return alpha * sample + (1.0 - alpha) * prev;
}

}  // namespace


bool Hailo8Backend::initialize(const std::string & model_path)
{
  auto vdev_exp = hailort::VDevice::create();
  if (!vdev_exp) {
    std::fprintf(
      stderr, "[hailo8] VDevice::create failed: %d\n",
      static_cast<int>(vdev_exp.status()));
    return false;
  }
  vdevice_ = std::move(vdev_exp.release());

  auto hef = hailort::Hef::create(model_path);
  if (!hef) {
    std::fprintf(
      stderr, "[hailo8] Hef::create(%s) failed: %d\n",
      model_path.c_str(), static_cast<int>(hef.status()));
    return false;
  }
  // auto cfg = vdevice_->configure(hef.release());
  auto hef_persistent = hef.release();
  auto cfg = vdevice_->configure(hef_persistent);
  if (!cfg || cfg->empty()) {
    std::fprintf(stderr, "[hailo8] configure failed\n");
    return false;
  }
  network_group_ = std::move(cfg->at(0));
  ready_.store(true);
  return true;
}

std::vector<Detection> Hailo8Backend::infer(const cv::Mat & frame)
{
  if (!ready_.load() || frame.empty()) {return {};}
  std::lock_guard<std::mutex> lock(infer_mutex_);

  const auto t0 = std::chrono::steady_clock::now();
  std::vector<Detection> detections;
  // TODO(SAN v1.6 successor — v1.5.1 baselined Airys port): wire HailoRT input/output vstreams and
  // post-process. The interface and latency tracking are exercised
  // by the unit tests regardless of the model wiring.
  const auto t1 = std::chrono::steady_clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  latency_ema_ms_.store(
    updateEma(latency_ema_ms_.load(), ms, kLatencyEmaAlpha));
  return detections;
}

#else  // HAVE_HAILORT

bool Hailo8Backend::initialize(const std::string & /*model_path*/)
{
  return false;
}

std::vector<Detection> Hailo8Backend::infer(const cv::Mat &)
{
  return {};
}

#endif  // HAVE_HAILORT

}  // namespace human_detector
