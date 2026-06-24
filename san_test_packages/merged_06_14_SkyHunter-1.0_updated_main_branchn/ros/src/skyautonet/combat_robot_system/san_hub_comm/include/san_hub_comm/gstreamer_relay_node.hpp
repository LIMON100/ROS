// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 2-E - Hub SBC#2 GStreamer relay node.
//
// Bridges Follower → Hub (UDP, port 5000 + robot_id) and Hub → Operator
// (SRT, port 8888) for each VideoStreamRequest. Pipelines are built
// via gst_parse_launch (GStreamer C API) - no shell, no gst-launch-1.0.
//
// Concurrency policy:
//   * Up to kThumbnailThreshold-1 streams keep their requested quality.
//   * Once kThumbnailThreshold streams are active simultaneously, the
//     relay rebuilds them all at QUALITY_THUMBNAIL so total egress
//     stays well under the operator's LTE budget.
//
// PATCH 2026-05-13 (san_hub_comm deep-dive):
//   * HC1 — passphrase is no longer embedded in handle.srt_uri. The
//     handle exposes srt_uri (no secrets, OK to broadcast) AND
//     passphrase (separate field, intended for the authorized
//     operator only via authenticated topic / DDS partition).
//   * HC2 — srt_uri now reflects the operator endpoint matching the
//     srtsink caller mode in the GStreamer pipeline. Pre-patch the
//     handle told the operator "listen on hub_ip" while the relay
//     was sending to operator_ip — connection never established.
//   * HC3/HC4 — streams_mutex_ guards active_streams_ AND thumbnail_mode_
//     compound updates. The atomic on thumbnail_mode_ alone wasn't
//     enough for the check-then-write pattern.
//   * HC6 — kThumbnailThreshold exposed as `thumbnail_threshold`
//     ROS parameter (still defaults to 4).

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/video_stream_request.hpp>
#include <combat_robot_msgs/msg/video_stream_handle.hpp>
#include <combat_robot_msgs/msg/lte_link_quality.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "san_hub_comm/auto_rate_controller.hpp"
#include "san_hub_comm/gstreamer_pipeline.hpp"
#include "san_hub_comm/passphrase_generator.hpp"

namespace san_hub_comm
{

class GStreamerRelayNode : public rclcpp::Node
{
public:
  // ★ PATCH 2026-05-13 (HC6): default threshold; runtime param overrides.
  static constexpr std::size_t kDefaultThumbnailThreshold = 4;

  GStreamerRelayNode();
  explicit GStreamerRelayNode(const rclcpp::NodeOptions & options);

  // Lifecycle and test accessors.
  std::size_t activeStreamCount() const
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    return active_streams_.size();
  }
  bool isStreamActive(uint32_t robot_id) const
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    return active_streams_.find(robot_id) != active_streams_.end();
  }
  bool isThumbnailMode() const {return thumbnail_mode_.load();}
  std::size_t thumbnailThreshold() const {return thumbnail_threshold_;}

  // Test entry point: synchronously process a request without going
  // through a publisher round-trip.
  combat_robot_msgs::msg::VideoStreamHandle processRequestForTest(
    const combat_robot_msgs::msg::VideoStreamRequest & req);

  // Test entry point: feed a synthetic LinkQuality reading.
  void processLinkQualityForTest(
    const combat_robot_msgs::msg::LteLinkQuality & msg);

  uint8_t currentAutoRateTarget() const;

private:
  struct StreamState
  {
    std::unique_ptr<GStreamerPipeline> pipeline;
    std::string passphrase;
    uint8_t codec;
    uint8_t quality;
    std::string operator_ip;
    uint64_t start_ms;
  };

  // Parameters.
  std::string hub_ip_;
  int srt_port_ = 8888;
  int udp_base_port_ = 5000;
  int srt_latency_ms_ = 120;
  bool encryption_default_ = true;
  // ★ PATCH 2026-05-13 (HC6): configurable threshold.
  std::size_t thumbnail_threshold_ = kDefaultThumbnailThreshold;
  // ★ PATCH 2026-05-13 (HC1): operators wanting to scrub
  // passphrase from /video/handle (e.g. when the topic is bridged
  // outside the trust boundary) set this true; the field is set to
  // empty string when published.
  bool redact_passphrase_in_handle_ = false;

  PassphraseGenerator passphrase_gen_;

  // ★ PATCH 2026-05-13 (HC3/HC4): streams_mutex_ guards
  // active_streams_ AND the compound thumbnail-threshold logic.
  // mutable so const accessors can lock.
  mutable std::mutex streams_mutex_;
  std::unordered_map<uint32_t, StreamState> active_streams_;
  std::atomic<bool> thumbnail_mode_;
  std::atomic<uint32_t> handle_seq_;

  // PHASE 5b: LTE-driven auto-rate. Owned here so the controller's
  // hysteresis state survives across LinkQuality samples and so its
  // applier can reach into active_streams_.
  std::unique_ptr<AutoRateController> auto_rate_;
  bool auto_rate_enabled_ = true;
  int auto_rate_promote_hold_ticks_ = 5;

  rclcpp::Subscription<combat_robot_msgs::msg::VideoStreamRequest>::SharedPtr
    request_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::LteLinkQuality>::SharedPtr
    link_quality_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::VideoStreamHandle>::SharedPtr
    handle_pub_;

  // [C-1 fix v1.5.1 — DCN-2026-003 D-005] MutuallyExclusive callback
  // group binds onRequest + onLinkQuality so they cannot race on
  // active_streams_ / thumbnail_mode_ / handle_seq_ under MTE
  // (relay_main.cpp uses MultiThreadedExecutor for GStreamer thread
  // separation; without this group, onLinkQuality on thread A could
  // mutate thumbnail_mode_ while onRequest on thread B inserts /
  // erases an active_streams_ entry — both with no locking).
  rclcpp::CallbackGroup::SharedPtr cb_group_;

  void declareParameters();
  void readParameters();
  void wireInterfaces();

  void onRequest(combat_robot_msgs::msg::VideoStreamRequest::SharedPtr msg);
  void onLinkQuality(
    combat_robot_msgs::msg::LteLinkQuality::SharedPtr msg);

  // Apply a new target quality to every currently-active stream by
  // rebuilding their pipelines. No-op when the concurrency-driven
  // thumbnail mode is active.
  void applyAutoRateTarget(uint8_t target_quality);

  combat_robot_msgs::msg::VideoStreamHandle handleStart(
    const combat_robot_msgs::msg::VideoStreamRequest & req);
  combat_robot_msgs::msg::VideoStreamHandle handleStop(
    const combat_robot_msgs::msg::VideoStreamRequest & req);
  combat_robot_msgs::msg::VideoStreamHandle handleChangeQuality(
    const combat_robot_msgs::msg::VideoStreamRequest & req);

  // ★ PATCH 2026-05-13: caller MUST hold streams_mutex_ for these.
  void downgradeAllToThumbnailLocked();
  void restoreFromThumbnailLocked();

  std::string buildRelayPipelineDescription(
    uint32_t robot_id,
    uint8_t quality,
    const std::string & operator_ip,
    const std::string & passphrase) const;
  // ★ PATCH 2026-05-13 (HC1, HC2): srt_uri now reflects the
  // operator endpoint (matches srtsink caller target) and never
  // contains the passphrase. Passphrase travels in its own
  // VideoStreamHandle.passphrase field.
  std::string buildSrtUri(const std::string & operator_ip) const;

  int udpPortFor(uint32_t robot_id) const {return udp_base_port_ + robot_id;}
  static int qualityBitrateKbps(uint8_t quality);

  void publishHandle(combat_robot_msgs::msg::VideoStreamHandle & handle);
};

}  // namespace san_hub_comm
