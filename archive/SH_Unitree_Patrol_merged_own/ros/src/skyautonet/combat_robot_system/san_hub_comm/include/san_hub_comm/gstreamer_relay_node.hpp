// SAN v1.3 PHASE 5 - Hub SBC#2 GStreamer relay node.
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

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/video_stream_request.hpp>
#include <combat_robot_msgs/msg/video_stream_handle.hpp>
#include <combat_robot_msgs/msg/lte_link_quality.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "san_hub_comm/auto_rate_controller.hpp"
#include "san_hub_comm/gstreamer_pipeline.hpp"
#include "san_hub_comm/passphrase_generator.hpp"

namespace san_hub_comm {

class GStreamerRelayNode : public rclcpp::Node {
public:
    static constexpr std::size_t kThumbnailThreshold = 4;

    GStreamerRelayNode();
    explicit GStreamerRelayNode(const rclcpp::NodeOptions& options);

    // Lifecycle and test accessors.
    std::size_t activeStreamCount() const { return active_streams_.size(); }
    bool isStreamActive(uint32_t robot_id) const {
        return active_streams_.find(robot_id) != active_streams_.end();
    }
    bool isThumbnailMode() const { return thumbnail_mode_; }

    // Test entry point: synchronously process a request without going
    // through a publisher round-trip.
    combat_robot_msgs::msg::VideoStreamHandle processRequestForTest(
        const combat_robot_msgs::msg::VideoStreamRequest& req);

    // Test entry point: feed a synthetic LinkQuality reading.
    void processLinkQualityForTest(
        const combat_robot_msgs::msg::LteLinkQuality& msg);

    uint8_t currentAutoRateTarget() const;

private:
    struct StreamState {
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

    PassphraseGenerator passphrase_gen_;

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
        const combat_robot_msgs::msg::VideoStreamRequest& req);
    combat_robot_msgs::msg::VideoStreamHandle handleStop(
        const combat_robot_msgs::msg::VideoStreamRequest& req);
    combat_robot_msgs::msg::VideoStreamHandle handleChangeQuality(
        const combat_robot_msgs::msg::VideoStreamRequest& req);

    void downgradeAllToThumbnail();
    void restoreFromThumbnail();   // when active count drops below threshold

    std::string buildRelayPipelineDescription(uint32_t robot_id,
                                              uint8_t quality,
                                              const std::string& operator_ip,
                                              const std::string& passphrase) const;
    std::string buildSrtUri(const std::string& passphrase) const;

    int udpPortFor(uint32_t robot_id) const { return udp_base_port_ + robot_id; }
    static int qualityBitrateKbps(uint8_t quality);

    void publishHandle(combat_robot_msgs::msg::VideoStreamHandle& handle);
};

}  // namespace san_hub_comm
