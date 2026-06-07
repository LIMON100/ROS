// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.1 PHASE 6 — HumanDetector rclcpp node (full pipeline).
//
// DCN-2026-003 D-003 (2026-05-13): completes the C++ wiring promised
// in v1.3 PHASE 6. Before this change the node only selected a backend
// but never produced detections; the Python `san_perception` path was
// the production trigger. This version subscribes to the IMX678 H.265
// stream, drives the InferenceBackend, and publishes DetectionArray
// on `~/detections` — the exact same topic shape the san_perception
// node used, so downstream consumers (mission BT, threat aggregator)
// are unchanged.
//
// Pipeline:
//   /imx678_camera_node/image_compressed (CompressedImage, H.265 or jpeg)
//     -> cv_bridge / cv::imdecode
//     -> backend->infer(cv::Mat)              [Airys V6.13.5 port]
//     -> COCO-to-SAN class id remap
//     -> ~/detections (combat_robot_msgs/DetectionArray)
//
// Backend selection (yaml `inference_backend`):
//   "rk3588"  -> RKNN librknnrt + YOLOv5 (Airys-derived NMS + decode)
//   "hailo8"  -> HailoRT (M.2 module)
//   "stub"    -> no-op, returns empty (CI / desktop)
//
// Test seam: `detectOnFrameForTest(cv::Mat)` runs the full convert
// + infer + remap pipeline without ROS spinning.

#pragma once

#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include <opencv2/core.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <combat_robot_msgs/msg/detection_array.hpp>

#include "human_detector/inference_backend.hpp"
#include "ByteTrack/BYTETracker.h"

namespace human_detector
{

class HumanDetectorNode : public rclcpp::Node
{
public:
  HumanDetectorNode();
  explicit HumanDetectorNode(const rclcpp::NodeOptions & options);

  // ─── Test/inspection accessors ─────────────────────────────────
  std::string activeBackendName() const
  {
    return backend_ ? backend_->getName() : std::string("none");
  }
  bool backendIsReady() const
  {
    return backend_ && backend_->isReady();
  }
  double inferenceLatencyMs() const
  {
    return backend_ ? backend_->getInferenceLatencyMs() : 0.0;
  }

  // PATCH v1.5.1: drive one frame through the full pipeline.
  // Returns the published DetectionArray (also broadcast on the
  // topic when det_pub_ is wired). Used by gtest.
  combat_robot_msgs::msg::DetectionArray
  detectOnFrameForTest(const cv::Mat & bgr);

private:
  // ─── Parameters ────────────────────────────────────────────────
  std::string requested_backend_;
  std::string model_path_;
  std::string rk3588_fallback_model_path_;

  // PATCH v1.5.1: pipeline params.
  std::string camera_topic_;         // compressed input (legacy / H.265)
  std::string decoded_topic_;        // raw BGR8 input from san_video_decoder
  std::string detections_topic_;     // default: "~/detections"
  int max_inference_hz_ = 15;             // throttle when camera > infer rate
  bool drop_when_busy_ = true;            // skip frame if backend busy

  // ─── v1.5.1 (DCN-2026-003 D-003 / I-15 fix) — image source policy ─
  //
  // The v1.5 build attempted to cv::imdecode() the H.265-encoded
  // CompressedImage published by imx678_camera_node, which silently
  // failed (cv::imdecode only handles still-image formats — see
  // I-15 review). The fix introduces san_video_decoder, which
  // wraps the RK3588 MPP HW H.265 decoder and republishes raw
  // sensor_msgs/Image. human_detector now subscribes to that
  // decoded stream by default and treats the CompressedImage path
  // as legacy / fallback (useful when the upstream camera is a
  // stub that publishes JPEG directly).
  //
  //   "raw"        - subscribe ONLY to decoded raw Image (production)
  //   "compressed" - subscribe ONLY to CompressedImage (legacy / JPEG stub)
  //   "auto"       - subscribe to BOTH; useful during migration
  std::string image_mode_ = "raw";
  std::unique_ptr<byte_track::BYTETracker> tracker_;

  // ─── Backend ───────────────────────────────────────────────────
  std::unique_ptr<InferenceBackend> backend_;
  std::mutex infer_mutex_;
  std::atomic<bool> infer_busy_{false};

  // ─── ROS interfaces ────────────────────────────────────────────
  rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr
    compressed_sub_;
  // Optional: subscribe to raw Image when the source is a stub or
  // a non-compressed sensor. Both paths route into the same infer
  // method via cv::Mat.
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::DetectionArray>::SharedPtr
    det_pub_;

  // ─── Throttle state ────────────────────────────────────────────
  rclcpp::Time last_infer_at_;

  // ─── Health stats ──────────────────────────────────────────────
  std::atomic<uint64_t> frames_in_{0};        // received
  std::atomic<uint64_t> frames_dropped_{0};   // throttled or busy
  std::atomic<uint64_t> frames_inferred_{0};
  std::atomic<uint64_t> publishes_{0};
  rclcpp::TimerBase::SharedPtr health_timer_;

  // ─── Internal ──────────────────────────────────────────────────
  void declareParameters();
  void readParameters();
  void selectBackend();
  void wireInterfaces();

  void onCompressedImage(sensor_msgs::msg::CompressedImage::SharedPtr msg);
  void onImage(sensor_msgs::msg::Image::SharedPtr msg);
  void onHealthTick();

  // Decode + clock guard + infer + publish. Header is propagated.
  void processFrame(
    const cv::Mat & bgr,
    const std_msgs::msg::Header & src_header);

  // Map backend Detection (COCO class_id + label) into the SAN
  // class_id enum from combat_robot_msgs/Detection.msg.
  static uint8_t cocoToSanClassId(int coco_id, const std::string & label);
};

}  // namespace human_detector
