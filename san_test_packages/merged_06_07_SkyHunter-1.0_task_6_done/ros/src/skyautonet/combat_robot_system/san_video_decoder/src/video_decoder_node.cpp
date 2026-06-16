// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.1 — H.265 video decoder node implementation.
// See video_decoder_node.hpp for design + rationale.

#include "san_video_decoder/video_decoder_node.hpp"

#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>   // for std::this_thread::sleep_for in pipeline warmup

extern "C" {
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
}

namespace san_video_decoder
{

const char * backendToString(DecoderBackend b)
{
  switch (b) {
    case DecoderBackend::MPP_HARDWARE:   return "mpp_hardware";
    case DecoderBackend::AVDEC_SOFTWARE: return "avdec_software";
    case DecoderBackend::UNINITIALIZED:  return "uninitialized";
  }
  return "unknown";
}

namespace
{

// Detect whether a GStreamer element factory is available on this host.
// Used to pick mppvideodec (HW) vs avdec_h265 (SW) at startup.
bool hasFactory(const char * factory_name)
{
  GstElementFactory * f = gst_element_factory_find(factory_name);
  if (!f) {return false;}
  gst_object_unref(f);
  return true;
}

}  // namespace

// ─── ctors / dtor ──────────────────────────────────────────────────────

VideoDecoderNode::VideoDecoderNode()
: VideoDecoderNode(rclcpp::NodeOptions())
{}

VideoDecoderNode::VideoDecoderNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("video_decoder_node", options)
{
  // GStreamer is initialized in decoder_main; if a host embeds
  // VideoDecoderNode directly (e.g. component_container), they
  // are responsible for gst_init() before construction.
  if (!gst_is_initialized()) {
    gst_init(nullptr, nullptr);
  }

  declareParameters();
  readParameters();

  if (!buildPipeline()) {
    RCLCPP_ERROR(
      get_logger(),
      "buildPipeline() failed — VideoDecoderNode will publish no frames. "
      "Verify gstreamer1.0-rockchip (board) or gstreamer1.0-libav (host) "
      "is installed.");
  }

  wireInterfaces();

  RCLCPP_INFO(
    get_logger(),
    "VideoDecoderNode UP: backend=%s in=%s out=%s",
    backendToString(backend_.load()),
    compressed_topic_.c_str(), decoded_topic_.c_str());
}

VideoDecoderNode::~VideoDecoderNode()
{
  tearDownPipeline();
}

// ─── parameters ────────────────────────────────────────────────────────

void VideoDecoderNode::declareParameters()
{
  declare_parameter<std::string>("compressed_topic", "~/compressed_in");
  declare_parameter<std::string>("decoded_topic", "~/decoded_out");
  declare_parameter<std::string>("decoded_frame_id", "imx678_decoded");
  declare_parameter<bool>("prefer_hw", true);
  declare_parameter<int>("appsrc_max_bytes", 8 * 1024 * 1024);            // 8 MB
  declare_parameter<int>("pipeline_warmup_ms", 200);
}

void VideoDecoderNode::readParameters()
{
  compressed_topic_ = get_parameter("compressed_topic").as_string();
  decoded_topic_ = get_parameter("decoded_topic").as_string();
  decoded_frame_id_ = get_parameter("decoded_frame_id").as_string();
  prefer_hw_ = get_parameter("prefer_hw").as_bool();
  appsrc_max_bytes_ = get_parameter("appsrc_max_bytes").as_int();
  pipeline_warmup_ms_ = get_parameter("pipeline_warmup_ms").as_int();
}

// ─── pipeline construction ─────────────────────────────────────────────

bool VideoDecoderNode::buildPipeline()
{
  // Choose backend based on availability + operator preference.
  const bool have_mpp = hasFactory("mppvideodec");
  const bool have_avdec = hasFactory("avdec_h265");

  std::string desc;
  if (prefer_hw_ && have_mpp) {
    // RK3588 hardware path.
    // is-live=true tells appsrc the source is wall-clock paced
    // (camera at 30 fps); without it the pipeline buffers
    // aggressively and adds latency.
    // do-timestamp=false: onCompressedImage stamps each buffer's PTS
    // with the ROS header time and onNewSample recovers it to set the
    // decoded Image's header.stamp. appsrc must NOT overwrite that PTS
    // with its running clock (do-timestamp=true did, breaking the
    // capture-time round-trip and downstream time sync).
    desc =
      "appsrc name=src is-live=true do-timestamp=false format=time "
      "caps=video/x-h265,stream-format=byte-stream,alignment=au "
      "! h265parse "
      "! mppvideodec "
      "! videoconvert "
      "! video/x-raw,format=BGR "
      "! appsink name=sink emit-signals=true sync=false max-buffers=2 drop=true";
    backend_.store(DecoderBackend::MPP_HARDWARE);
  } else if (have_avdec) {
    // Host fallback (libavcodec / FFmpeg).
    desc =
      "appsrc name=src is-live=true do-timestamp=false format=time "
      "caps=video/x-h265,stream-format=byte-stream,alignment=au "
      "! h265parse "
      "! avdec_h265 "
      "! videoconvert "
      "! video/x-raw,format=BGR "
      "! appsink name=sink emit-signals=true sync=false max-buffers=2 drop=true";
    backend_.store(DecoderBackend::AVDEC_SOFTWARE);
  } else {
    RCLCPP_ERROR(
      get_logger(),
      "No H.265 decoder element available "
      "(neither mppvideodec nor avdec_h265). Install "
      "gstreamer1.0-rockchip (board) or gstreamer1.0-libav (host).");
    return false;
  }

  RCLCPP_INFO(get_logger(), "GStreamer pipeline: %s", desc.c_str());

  GError * err = nullptr;
  pipeline_ = gst_parse_launch(desc.c_str(), &err);
  if (!pipeline_) {
    RCLCPP_ERROR(
      get_logger(),
      "gst_parse_launch failed: %s",
      err ? err->message : "(unknown)");
    if (err) {g_error_free(err);}
    backend_.store(DecoderBackend::UNINITIALIZED);
    return false;
  }
  if (err) {
    // Non-fatal warning during parse.
    RCLCPP_WARN(
      get_logger(),
      "gst_parse_launch warning: %s", err->message);
    g_error_free(err);
  }

  appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), "src");
  appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
  if (!appsrc_ || !appsink_) {
    RCLCPP_ERROR(
      get_logger(),
      "Pipeline built but appsrc/appsink could not be retrieved");
    tearDownPipeline();
    return false;
  }

  // Backpressure: cap appsrc internal queue so a stalled decoder
  // doesn't grow memory unbounded.
  g_object_set(
    appsrc_,
    "max-bytes", static_cast<guint64>(appsrc_max_bytes_),
    "block", FALSE,
    nullptr);

  // Wire appsink new-sample signal — fires every decoded frame.
  g_signal_connect(
    appsink_, "new-sample",
    G_CALLBACK(&VideoDecoderNode::onNewSampleStatic),
    this);

  // Start the pipeline. Wait a moment for state change to commit so
  // the first incoming H.265 packet is actually accepted.
  GstStateChangeReturn rc = gst_element_set_state(
    pipeline_,
    GST_STATE_PLAYING);
  if (rc == GST_STATE_CHANGE_FAILURE) {
    RCLCPP_ERROR(
      get_logger(),
      "gst_element_set_state(PLAYING) failed");
    tearDownPipeline();
    return false;
  }
  if (pipeline_warmup_ms_ > 0) {
    std::this_thread::sleep_for(
      std::chrono::milliseconds(pipeline_warmup_ms_));
  }
  return true;
}

void VideoDecoderNode::tearDownPipeline()
{
  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
  }
  if (appsrc_) {gst_object_unref(appsrc_);  appsrc_ = nullptr;}
  if (appsink_) {gst_object_unref(appsink_); appsink_ = nullptr;}
  backend_.store(DecoderBackend::UNINITIALIZED);
}

// ─── ROS wiring ────────────────────────────────────────────────────────

void VideoDecoderNode::wireInterfaces()
{
  cb_group_ = create_callback_group(
    rclcpp::CallbackGroupType::MutuallyExclusive);
  rclcpp::SubscriptionOptions sub_opts;
  sub_opts.callback_group = cb_group_;

  // Match the camera's QoS (best-effort SensorDataQoS, depth 2).
  compressed_sub_ = create_subscription<sensor_msgs::msg::CompressedImage>(
    compressed_topic_, rclcpp::SensorDataQoS().keep_last(2),
    std::bind(
      &VideoDecoderNode::onCompressedImage, this,
      std::placeholders::_1),
    sub_opts);

  // Decoded frames are produced at the source rate; downstream
  // (human_detector) throttles further. Best-effort matches the
  // capture QoS.
  decoded_pub_ = create_publisher<sensor_msgs::msg::Image>(
    decoded_topic_, rclcpp::SensorDataQoS().keep_last(2));

  health_timer_ = create_wall_timer(
    std::chrono::seconds(1),
    std::bind(&VideoDecoderNode::onHealthTick, this),
    cb_group_);
}

// ─── input path: push H.265 to appsrc ──────────────────────────────────

void VideoDecoderNode::onCompressedImage(
  sensor_msgs::msg::CompressedImage::SharedPtr msg)
{
  if (!msg || !pipeline_ || !appsrc_) {return;}
  ++frames_in_;

  // Sanity: only h265 / hevc. JPEG would route to a different decoder.
  if (msg->format != "h265" && msg->format != "hevc" &&
    msg->format != "h.265" && msg->format != "HEVC")
  {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "Received CompressedImage with format='%s' — expected h265. "
      "Skipping.", msg->format.c_str());
    ++frames_dropped_;
    return;
  }
  if (msg->data.empty()) {
    ++frames_dropped_;
    return;
  }

  // Wrap the message bytes in a GstBuffer (copy is necessary —
  // appsrc owns the buffer asynchronously, and the SharedPtr would
  // go out of scope here). For a true zero-copy path on RK3588 we
  // would import a dma_fd; that's a future optimisation.
  GstBuffer * buf = gst_buffer_new_allocate(
    nullptr, msg->data.size(),
    nullptr);
  if (!buf) {
    ++appsrc_push_errors_;
    ++frames_dropped_;
    return;
  }
  GstMapInfo map;
  if (!gst_buffer_map(buf, &map, GST_MAP_WRITE)) {
    gst_buffer_unref(buf);
    ++appsrc_push_errors_;
    ++frames_dropped_;
    return;
  }
  std::memcpy(map.data, msg->data.data(), msg->data.size());
  gst_buffer_unmap(buf, &map);

  // Stamp PTS with the ROS header timestamp so onDecodedSample can
  // recover it. We encode header.stamp.nanosec*1e9 + sec into PTS.
  const uint64_t ros_ns =
    static_cast<uint64_t>(msg->header.stamp.sec) * 1'000'000'000ULL +
    static_cast<uint64_t>(msg->header.stamp.nanosec);
  GST_BUFFER_PTS(buf) = static_cast<GstClockTime>(ros_ns);

  GstFlowReturn fr = gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buf);
  if (fr != GST_FLOW_OK) {
    ++appsrc_push_errors_;
    ++frames_dropped_;
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 2000,
      "appsrc push_buffer returned %d", fr);
  }
}

// ─── output path: pull decoded frame from appsink ─────────────────────

int VideoDecoderNode::onNewSampleStatic(GstElement * sink, void * user_data)
{
  auto * self = static_cast<VideoDecoderNode *>(user_data);
  if (!self || !sink) {return GST_FLOW_ERROR;}

  GstSample * sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
  if (!sample) {return GST_FLOW_ERROR;}

  // We do NOT have access to the source header here directly; we
  // recover the PTS that onCompressedImage stamped into the buffer
  // and stuff it back into the Image header. If the decoder drops
  // PTS, the publisher stamps current time as a fallback.
  std_msgs::msg::Header hdr;
  GstBuffer * buf = gst_sample_get_buffer(sample);
  if (buf && GST_CLOCK_TIME_IS_VALID(GST_BUFFER_PTS(buf))) {
    const uint64_t pts_ns = GST_BUFFER_PTS(buf);
    hdr.stamp.sec = static_cast<int32_t>(pts_ns / 1'000'000'000ULL);
    hdr.stamp.nanosec = static_cast<uint32_t>(pts_ns % 1'000'000'000ULL);
  } else {
    hdr.stamp = self->now();
  }
  hdr.frame_id = self->decoded_frame_id_;

  self->onDecodedSample(sample, hdr);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

void VideoDecoderNode::onDecodedSample(
  GstSample * sample,
  const std_msgs::msg::Header & hdr)
{
  const auto t0 = std::chrono::steady_clock::now();

  GstBuffer * buf = gst_sample_get_buffer(sample);
  GstCaps * caps = gst_sample_get_caps(sample);
  if (!buf || !caps) {return;}

  GstVideoInfo vinfo;
  gst_video_info_init(&vinfo);
  if (!gst_video_info_from_caps(&vinfo, caps)) {
    return;
  }
  const int w = GST_VIDEO_INFO_WIDTH(&vinfo);
  const int h = GST_VIDEO_INFO_HEIGHT(&vinfo);

  GstMapInfo map;
  if (!gst_buffer_map(buf, &map, GST_MAP_READ)) {return;}

  auto img = std::make_unique<sensor_msgs::msg::Image>();
  img->header = hdr;
  img->height = h;
  img->width = w;
  img->encoding = "bgr8";
  img->is_bigendian = 0;
  img->step = w * 3;
  img->data.assign(map.data, map.data + (img->step * h));
  gst_buffer_unmap(buf, &map);

  // Stash for test seam ONLY when a waiter is present. Production
  // (no test attached) skips this branch entirely — no copy, no
  // mutex acquire — and the unique_ptr is moved straight into the
  // publisher. (DCN-2026-005 D-012.)
  if (test_waiter_count_.load(std::memory_order_acquire) > 0) {
    std::lock_guard<std::mutex> lock(test_mutex_);
    pending_decoded_for_test_ =
      std::make_shared<sensor_msgs::msg::Image>(*img);
  }

  // Publish on the ROS topic. Note: this fires from the GStreamer
  // streaming thread, NOT from the rclcpp executor. rclcpp Publisher
  // is documented as thread-safe so direct publish is OK. Moving the
  // unique_ptr lets rclcpp hand off to intra-process subscribers
  // without a serialization copy once D-008 enables intra-process
  // comms (~180 MB/s @ 1080p BGR8 30fps).
  if (decoded_pub_) {
    decoded_pub_->publish(std::move(img));
    ++frames_out_;
  }

  const auto t1 = std::chrono::steady_clock::now();
  last_decode_ms_.store(
    std::chrono::duration<double, std::milli>(t1 - t0).count());
}

// ─── test seam ─────────────────────────────────────────────────────────

sensor_msgs::msg::Image::SharedPtr
VideoDecoderNode::decodeOnPacketForTest(
  const std::vector<uint8_t> & h265_packet, int timeout_ms)
{
  if (!appsrc_) {return nullptr;}

  // Increment the waiter count so onDecodedSample stashes the
  // result. The guard decrements on every return path (including
  // the early returns below). (DCN-2026-005 D-012.)
  test_waiter_count_.fetch_add(1, std::memory_order_release);
  struct WaiterGuard
  {
    std::atomic<int> * c;
    ~WaiterGuard() {c->fetch_sub(1, std::memory_order_release);}
  } guard{&test_waiter_count_};

  // Clear any previous test result.
  {
    std::lock_guard<std::mutex> lock(test_mutex_);
    pending_decoded_for_test_.reset();
  }

  GstBuffer * buf = gst_buffer_new_allocate(
    nullptr, h265_packet.size(),
    nullptr);
  if (!buf) {return nullptr;}
  GstMapInfo map;
  if (!gst_buffer_map(buf, &map, GST_MAP_WRITE)) {
    gst_buffer_unref(buf);
    return nullptr;
  }
  std::memcpy(map.data, h265_packet.data(), h265_packet.size());
  gst_buffer_unmap(buf, &map);
  GST_BUFFER_PTS(buf) = GST_CLOCK_TIME_NONE;

  if (gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buf) != GST_FLOW_OK) {
    return nullptr;
  }

  // Poll for the decoded frame. WaiterGuard above decrements the
  // counter on return regardless of timeout / early break.
  const auto deadline = std::chrono::steady_clock::now() +
    std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    {
      std::lock_guard<std::mutex> lock(test_mutex_);
      if (pending_decoded_for_test_) {
        return pending_decoded_for_test_;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return nullptr;
}

// ─── health log ────────────────────────────────────────────────────────

void VideoDecoderNode::onHealthTick()
{
  RCLCPP_INFO_THROTTLE(
    get_logger(), *get_clock(), 5000,
    "video_decoder: backend=%s in=%lu out=%lu drop=%lu push_err=%lu "
    "last_decode_ms=%.2f",
    backendToString(backend_.load()),
    frames_in_.load(), frames_out_.load(),
    frames_dropped_.load(), appsrc_push_errors_.load(),
    last_decode_ms_.load());
}

}  // namespace san_video_decoder
