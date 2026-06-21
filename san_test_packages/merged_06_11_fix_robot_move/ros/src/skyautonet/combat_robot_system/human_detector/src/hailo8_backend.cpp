// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "human_detector/hailo8_backend.hpp"

#include <chrono>
#include <cstdio>
#include <future>

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

constexpr int    kInputW     = 640;   // y5s_person_drone.hef input
constexpr int    kInputH     = 640;
constexpr size_t kClassCount = 2;     // person, drone

struct NamedBbox {
  hailo_bbox_float32_t bbox;
  size_t class_id;
};

std::vector<NamedBbox> parse_nms_data(uint8_t * data, size_t max_class_count)
{
  std::vector<NamedBbox> bboxes; 
  size_t offset = 0;
  for (size_t class_id = 0; class_id < max_class_count; ++class_id) {
    auto det_count = static_cast<uint32_t>(*reinterpret_cast<float32_t *>(data + offset));
    offset += sizeof(float32_t);
    for (size_t j = 0; j < det_count; ++j) {
      NamedBbox nb; 
      nb.bbox = *reinterpret_cast<hailo_bbox_float32_t *>(data + offset);
      offset += sizeof(hailo_bbox_float32_t);
      nb.class_id = class_id + 1;   // 1=person, 2=drone
      bboxes.push_back(nb);
    } 
  }
  return bboxes;
} 

const char * hailoClassLabel(size_t class_id)
{
  switch (class_id) {
    case 1:  return "person";
    case 2:  return "drone";
    default: return "";
  }
}


double updateEma(double prev, double sample, double alpha)
{
  if (prev <= 0.0) {return sample;}
  return alpha * sample + (1.0 - alpha) * prev;
}

}  // namespace


// bool Hailo8Backend::initialize(const std::string & model_path)
// {
//   auto vdev_exp = hailort::VDevice::create();
//   if (!vdev_exp) {
//     std::fprintf(
//       stderr, "[hailo8] VDevice::create failed: %d\n",
//       static_cast<int>(vdev_exp.status()));
//     return false;
//   }
//   vdevice_ = std::move(vdev_exp.release());

//   auto hef = hailort::Hef::create(model_path);
//   if (!hef) {
//     std::fprintf(
//       stderr, "[hailo8] Hef::create(%s) failed: %d\n",
//       model_path.c_str(), static_cast<int>(hef.status()));
//     return false;
//   }
//   // auto cfg = vdevice_->configure(hef.release());
//   auto hef_persistent = hef.release();
//   auto cfg = vdevice_->configure(hef_persistent);
//   if (!cfg || cfg->empty()) {
//     std::fprintf(stderr, "[hailo8] configure failed\n");
//     return false;
//   }
//   network_group_ = std::move(cfg->at(0));
//   ready_.store(true);
//   return true;
// }

bool Hailo8Backend::initialize(const std::string & model_path)
{
  try {
    model_ = std::make_unique<AsyncModelInfer>(model_path);
  } catch (const std::exception & e) {
    std::fprintf(stderr, "[hailo8] init failed: %s\n", e.what());
    ready_.store(false);
    return false;
  }
  ready_.store(true);
  return true;
}

// std::vector<Detection> Hailo8Backend::infer(const cv::Mat & frame)
// {
//   if (!ready_.load() || frame.empty()) {return {};}
//   std::lock_guard<std::mutex> lock(infer_mutex_);

//   const auto t0 = std::chrono::steady_clock::now();
//   std::vector<Detection> detections;
//   // TODO(SAN v1.6 successor — v1.5.1 baselined Airys port): wire HailoRT input/output vstreams and
//   // post-process. The interface and latency tracking are exercised
//   // by the unit tests regardless of the model wiring.
//   const auto t1 = std::chrono::steady_clock::now();
//   const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
//   latency_ema_ms_.store(
//     updateEma(latency_ema_ms_.load(), ms, kLatencyEmaAlpha));
//   return detections;
// }

std::vector<Detection> Hailo8Backend::infer(const cv::Mat & frame)
{
  if (!ready_.load() || frame.empty()) {return {};}
  std::lock_guard<std::mutex> lock(infer_mutex_);

  const auto t0 = std::chrono::steady_clock::now();

  // Preprocess: plain resize to 640x640, BGR (matches validated pipeline).
  auto input = std::make_shared<cv::Mat>();
  cv::resize(frame, *input, cv::Size(kInputW, kInputH));

  const int orig_w = frame.cols;
  const int orig_h = frame.rows;

  // Bridge Hailo's async callback to a synchronous return. shared_ptr so
  // the promise outlives a timeout (callback may still fire later).
  auto prom = std::make_shared<std::promise<std::vector<Detection>>>();
  auto fut = prom->get_future();

  model_->infer(
    input,
    [prom, orig_w, orig_h](
      const hailort::AsyncInferCompletionInfo & info,
      const std::vector<std::pair<uint8_t *, hailo_vstream_info_t>> & outputs,
      const std::vector<std::shared_ptr<uint8_t>> & /*guards*/)
    {
      std::vector<Detection> dets;
      if (info.status == HAILO_SUCCESS && !outputs.empty()) {
        const auto bboxes = parse_nms_data(outputs[0].first, kClassCount);
        dets.reserve(bboxes.size());
        for (const auto & b : bboxes) {
          dets.emplace_back(
            hailoClassLabel(b.class_id), b.bbox.score,
            static_cast<int>(b.class_id),
            b.bbox.x_min * orig_w, b.bbox.y_min * orig_h,
            b.bbox.x_max * orig_w, b.bbox.y_max * orig_h);
        }
      } 
      prom->set_value(std::move(dets));
    });
    
  std::vector<Detection> detections;
  if (fut.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready) {
    detections = fut.get();
  } else {
    std::fprintf(stderr, "[hailo8] infer timeout\n");
  }
  
  const auto t1 = std::chrono::steady_clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  latency_ema_ms_.store(updateEma(latency_ema_ms_.load(), ms, kLatencyEmaAlpha));
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
