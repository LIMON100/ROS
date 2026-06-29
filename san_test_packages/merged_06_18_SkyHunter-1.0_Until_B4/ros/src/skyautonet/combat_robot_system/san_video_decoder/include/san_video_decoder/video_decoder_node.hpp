// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.1 — H.265 video decoder node (RK3588 MPP HW accelerated).
//
// DCN-2026-003 D-003 / I-15 BLOCKER fix (2026-05-13):
//   The v1.5 human_detector pipeline cv::imdecode()-ed H.265 frames,
//   which silently failed because OpenCV's imdecode only handles
//   still-image formats. This node sits between the camera publisher
//   and any raw-frame consumer (human_detector, perception fusion,
//   recording, snapshot OSD) and produces sensor_msgs/Image (BGR8).
//
// Pipeline (production, board):
//   appsrc ! h265parse ! mppvideodec ! videoconvert
//          ! video/x-raw,format=BGR ! appsink
//
//   * mppvideodec is the gst-plugin-rockchip H.265 element. It binds
//     to the RK3588 VPU and decodes via librockchip-mpp (zero-CPU).
//   * On RK3588J, 4K@30fps H.265 decode is ~0% A55, ~12% NPU PCIE,
//     ~3-5 ms wall latency per frame (Rockchip MPP user manual §4.3).
//
// Pipeline (host CI, fallback):
//   appsrc ! h265parse ! avdec_h265 ! videoconvert
//          ! video/x-raw,format=BGR ! appsink
//
//   Software decode via libavcodec. Slower (50-80 ms per 1080p frame
//   on x86-64) but lets the package build + smoke-test off-target.
//
// Auto-fallback policy:
//   1. Probe mppvideodec via gst_element_factory_find("mppvideodec")
//   2. If present, build the HW pipeline.
//   3. Otherwise build the SW pipeline (avdec_h265 must be installed
//      via gstreamer1.0-libav).
//
// Topics:
//   IN  : ~/compressed_in   (sensor_msgs/CompressedImage, format=h265)
//         default remap: /imx678_camera_node/image_compressed
//   OUT : ~/decoded_out     (sensor_msgs/Image, encoding=bgr8)
//         default remap: /imx678_camera_node/image_decoded
//
// Test seam: decodeOnPacketForTest(buffer) bypasses ROS pub/sub.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <sensor_msgs/msg/image.hpp>

// GStreamer types are opaque to consumers; we forward-declare so the
// header stays lightweight.
typedef struct _GstElement GstElement;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstSample GstSample;

namespace san_video_decoder
{

enum class DecoderBackend : uint8_t
{
  UNINITIALIZED  = 0,
  MPP_HARDWARE   = 1,     // RK3588 VPU via mppvideodec
  AVDEC_SOFTWARE = 2,     // libavcodec via avdec_h265
};

const char * backendToString(DecoderBackend b);

class VideoDecoderNode : public rclcpp::Node
{
public:
  VideoDecoderNode();
  explicit VideoDecoderNode(const rclcpp::NodeOptions & options);
  ~VideoDecoderNode() override;

  // ─── Test / inspection accessors ──────────────────────────────────
  DecoderBackend backend() const {return backend_.load();}
  uint64_t framesIn() const {return frames_in_.load();}
  uint64_t framesOut() const {return frames_out_.load();}
  uint64_t framesDropped() const {return frames_dropped_.load();}
  double   lastDecodeMs() const {return last_decode_ms_.load();}

  // Test seam — push one H.265 NAL packet through the pipeline and
  // wait (bounded) for the decoded frame. Returns the BGR Mat as
  // sensor_msgs/Image::SharedPtr or nullptr on timeout / error.
  sensor_msgs::msg::Image::SharedPtr
  decodeOnPacketForTest(
    const std::vector<uint8_t> & h265_packet,
    int timeout_ms = 200);

private:
  // ─── Parameters ───────────────────────────────────────────────────
  std::string compressed_topic_;     // default: "~/compressed_in"
  std::string decoded_topic_;        // default: "~/decoded_out"
  std::string decoded_frame_id_;     // default: "imx678_decoded"
  bool prefer_hw_;                   // try mppvideodec first; default true
  int appsrc_max_bytes_;             // backpressure limit
  int pipeline_warmup_ms_;           // settle time after PLAY

  // ─── GStreamer state ──────────────────────────────────────────────
  // The pipeline lives for the entire node lifetime. appsrc receives
  // H.265 packets via gst_app_src_push_buffer; appsink emits decoded
  // BGR samples via the new-sample signal.
  GstElement * pipeline_ = nullptr;
  GstElement * appsrc_ = nullptr;
  GstElement * appsink_ = nullptr;
  std::atomic<DecoderBackend> backend_{DecoderBackend::UNINITIALIZED};

  // ─── ROS interfaces ───────────────────────────────────────────────
  rclcpp::CallbackGroup::SharedPtr cb_group_;
  rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr
    compressed_sub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr decoded_pub_;
  rclcpp::TimerBase::SharedPtr health_timer_;

  // ─── Health / metrics ─────────────────────────────────────────────
  std::atomic<uint64_t> frames_in_{0};
  std::atomic<uint64_t> frames_out_{0};
  std::atomic<uint64_t> frames_dropped_{0};
  std::atomic<uint64_t> appsrc_push_errors_{0};
  std::atomic<double> last_decode_ms_{0.0};

  // For test seam — protects pending_decoded_for_test_. The waiter
  // count gates production stash: when 0 (production default),
  // onDecodedSample skips the test-seam copy entirely.
  // decodeOnPacketForTest increments before submitting the buffer
  // and decrements on every return path via an RAII guard.
  // (DCN-2026-005 D-012.)
  std::mutex test_mutex_;
  std::atomic<int> test_waiter_count_{0};
  sensor_msgs::msg::Image::SharedPtr pending_decoded_for_test_;

  // ─── Internal ─────────────────────────────────────────────────────
  void declareParameters();
  void readParameters();
  void wireInterfaces();
  bool buildPipeline();
  void tearDownPipeline();

  void onCompressedImage(
    sensor_msgs::msg::CompressedImage::SharedPtr msg);
  void onHealthTick();

  // appsink "new-sample" handler — static (C callback) trampoline
  // that pulls the decoded frame, builds a sensor_msgs/Image, and
  // either publishes it or stashes it for the test seam.
  void onDecodedSample(
    GstSample * sample,
    const std_msgs::msg::Header & src_hdr);

  static int onNewSampleStatic(GstElement * sink, void * user_data);
};

}  // namespace san_video_decoder
