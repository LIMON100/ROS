#include "san_hub_comm/gstreamer_relay_node.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <utility>

namespace san_hub_comm {

using Req = combat_robot_msgs::msg::VideoStreamRequest;
using Handle = combat_robot_msgs::msg::VideoStreamHandle;

namespace {

uint64_t nowMs(rclcpp::Clock& clk) {
    // rclcpp::Clock::now() is non-const (it touches internal time-source
    // state on first call to lazily attach a sub-clock). Taking the
    // clock by const& discards the qualifier and fails on GCC 11 strict
    // mode. Hardening guard: also map negative nanoseconds (pre-init or
    // backward sim-time step) to 0 instead of letting the unsigned cast
    // wrap to ~2^64.
    const auto ns = clk.now().nanoseconds();
    return ns > 0 ? static_cast<uint64_t>(ns / 1'000'000ll) : 0ull;
}

}  // namespace

GStreamerRelayNode::GStreamerRelayNode()
    : GStreamerRelayNode(rclcpp::NodeOptions())
{}

GStreamerRelayNode::GStreamerRelayNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("gstreamer_relay_node", options),
      passphrase_gen_(PassphraseGenerator::kDefaultLen),
      thumbnail_mode_(false),
      handle_seq_(0)
{
    declareParameters();
    readParameters();

    AutoRateController::Config cfg;
    cfg.promote_hold_ticks = auto_rate_promote_hold_ticks_;
    auto_rate_ = std::make_unique<AutoRateController>(
        [this](uint8_t q) { applyAutoRateTarget(q); }, cfg);

    wireInterfaces();

    RCLCPP_INFO(get_logger(),
        "GStreamerRelay started: hub_ip=%s srt_port=%d udp_base=%d encryption=%s "
        "auto_rate=%s",
        hub_ip_.c_str(), srt_port_, udp_base_port_,
        encryption_default_ ? "on" : "off",
        auto_rate_enabled_ ? "on" : "off");
}

void GStreamerRelayNode::declareParameters() {
    declare_parameter<std::string>("hub_ip", "10.0.0.2");
    declare_parameter<int>("srt_port", 8888);
    declare_parameter<int>("udp_base_port", 5000);
    declare_parameter<int>("srt_latency_ms", 120);
    declare_parameter<bool>("encryption_default", true);
    declare_parameter<bool>("auto_rate_enabled", true);
    declare_parameter<int>("auto_rate_promote_hold_ticks", 5);
}

void GStreamerRelayNode::readParameters() {
    hub_ip_ = get_parameter("hub_ip").as_string();
    srt_port_ = get_parameter("srt_port").as_int();
    udp_base_port_ = get_parameter("udp_base_port").as_int();
    srt_latency_ms_ = get_parameter("srt_latency_ms").as_int();
    encryption_default_ = get_parameter("encryption_default").as_bool();
    auto_rate_enabled_ = get_parameter("auto_rate_enabled").as_bool();
    auto_rate_promote_hold_ticks_ =
        get_parameter("auto_rate_promote_hold_ticks").as_int();
    if (auto_rate_promote_hold_ticks_ < 1) auto_rate_promote_hold_ticks_ = 1;
}

void GStreamerRelayNode::wireInterfaces() {
    // [C-1 fix v1.5.1] Single MutuallyExclusive callback group binds
    // onRequest + onLinkQuality. Even though relay_main.cpp uses
    // MultiThreadedExecutor (so this node can run in parallel with
    // other nodes hosted on the same SBC#2), within THIS node the
    // request / link-quality callbacks are now serialized — so
    // active_streams_, thumbnail_mode_, and handle_seq_ are safe
    // without explicit locking.
    cb_group_ = create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions sub_opts;
    sub_opts.callback_group = cb_group_;

    rclcpp::QoS qos(10);
    qos.reliable();

    request_sub_ = create_subscription<Req>(
        "/video/request", qos,
        std::bind(&GStreamerRelayNode::onRequest, this,
                  std::placeholders::_1),
        sub_opts);
    handle_pub_ = create_publisher<Handle>("/video/handle", qos);

    if (auto_rate_enabled_) {
        // BestEffort is intentional - LTE samples are 1 Hz and we
        // care about the latest reading, not lossless delivery.
        rclcpp::QoS lq_qos(10);
        lq_qos.best_effort();
        link_quality_sub_ =
            create_subscription<combat_robot_msgs::msg::LteLinkQuality>(
                "/lte/link_quality", lq_qos,
                std::bind(&GStreamerRelayNode::onLinkQuality, this,
                          std::placeholders::_1),
                sub_opts);
    }
}

int GStreamerRelayNode::qualityBitrateKbps(uint8_t quality) {
    // v1.5.1 (DCN-2026-003 D-001, 2026-05-13):
    //   ★ HD downsizing — operator preference: HD@2Mbps as default.
    //   기존: HD = 1500 kbps  →  변경: HD = 2000 kbps (2 Mbps, 720p)
    //   FHD 는 동시 3-stream 시 LTE bandwidth budget 초과 우려로 유지.
    //   THUMBNAIL/LOW 도 ratio 유지 위해 미세 조정 없음.
    //   default 도 HD 와 일치하도록 2000 kbps 로 변경.
    switch (quality) {
        case Req::QUALITY_THUMBNAIL: return 100;
        case Req::QUALITY_LOW:       return 500;
        case Req::QUALITY_HD:        return 2000;   // ★ 1500 → 2000 (HD 2 Mbps)
        case Req::QUALITY_FHD:       return 4000;
        default:                     return 2000;   // ★ HD default
    }
}

std::string GStreamerRelayNode::buildSrtUri(const std::string& passphrase) const
{
    std::ostringstream o;
    o << "srt://" << hub_ip_ << ":" << srt_port_
      << "?mode=listener&latency=" << srt_latency_ms_;
    if (!passphrase.empty()) {
        o << "&passphrase=" << passphrase
          << "&pbkeylen=16";    // AES-128
    }
    return o.str();
}

std::string GStreamerRelayNode::buildRelayPipelineDescription(
    uint32_t robot_id, uint8_t /*quality*/,
    const std::string& operator_ip,
    const std::string& passphrase) const
{
    // Follower → Hub leg: RTP/MPEG-TS over UDP, port = udp_base + robot_id.
    // Hub → Operator leg: SRT listener on srt_port_.
    // The relay re-encapsulates without re-encoding to keep latency low.
    const int udp_port = udp_base_port_ + static_cast<int>(robot_id);

    std::ostringstream o;
    o << "udpsrc port=" << udp_port
      << " caps=\"application/x-rtp, media=video, clock-rate=90000, "
      << "encoding-name=MP2T\" ! "
      << "rtpmp2tdepay ! tsdemux ! queue ! "
      << "srtsink uri=\"srt://" << operator_ip << ":" << srt_port_
      << "?mode=caller&latency=" << srt_latency_ms_;
    if (!passphrase.empty()) {
        o << "&passphrase=" << passphrase
          << "&pbkeylen=16";
    }
    o << "\"";
    return o.str();
}

void GStreamerRelayNode::onRequest(Req::SharedPtr msg) {
    if (msg == nullptr) return;
    Handle handle;
    switch (msg->action) {
        case Req::ACTION_START:
            handle = handleStart(*msg);
            break;
        case Req::ACTION_STOP:
            handle = handleStop(*msg);
            break;
        case Req::ACTION_CHANGE_QUALITY:
            handle = handleChangeQuality(*msg);
            break;
        default:
            handle.sequence = msg->sequence;
            handle.target_robot_id = msg->target_robot_id;
            handle.status = Handle::STATUS_ERROR;
            handle.error_msg = "unknown action";
            break;
    }
    publishHandle(handle);
}

Handle GStreamerRelayNode::handleStart(const Req& req) {
    Handle h;
    h.sequence = req.sequence;
    h.target_robot_id = req.target_robot_id;
    h.codec = req.codec;

    // Effective quality - the relay may force thumbnail when the
    // concurrent stream budget is saturated.
    uint8_t effective_quality = req.quality;
    if (active_streams_.size() + 1 >= kThumbnailThreshold) {
        effective_quality = Req::QUALITY_THUMBNAIL;
        thumbnail_mode_ = true;
    }
    h.quality = effective_quality;

    const std::string passphrase =
        (req.encryption || encryption_default_)
            ? passphrase_gen_.generate() : "";
    h.passphrase = passphrase;
    h.srt_uri = buildSrtUri(passphrase);
    h.actual_bitrate_kbps = qualityBitrateKbps(effective_quality);

    const std::string desc = buildRelayPipelineDescription(
        req.target_robot_id, effective_quality,
        req.operator_ip.empty() ? hub_ip_ : req.operator_ip,
        passphrase);

    auto pipe = std::make_unique<GStreamerPipeline>(get_logger(), desc);
    const uint32_t robot_id = req.target_robot_id;
    pipe->setEventCallback(
        [this, robot_id](PipelineEvent ev, const std::string& detail) {
            if (ev == PipelineEvent::ERROR_FATAL || ev == PipelineEvent::EOS) {
                RCLCPP_ERROR(get_logger(),
                    "pipeline event for robot %u: %s",
                    robot_id, detail.c_str());
            }
        });

    if (!pipe->play()) {
        h.status = Handle::STATUS_ERROR;
        h.error_msg = "gst_parse_launch / play failed";
        return h;
    }

    StreamState state;
    state.pipeline = std::move(pipe);
    state.passphrase = passphrase;
    state.codec = req.codec;
    state.quality = effective_quality;
    state.operator_ip = req.operator_ip.empty() ? hub_ip_ : req.operator_ip;
    state.start_ms = nowMs(*get_clock());
    active_streams_[req.target_robot_id] = std::move(state);

    // If we just crossed the threshold, downgrade everything.
    if (active_streams_.size() >= kThumbnailThreshold && !thumbnail_mode_) {
        downgradeAllToThumbnail();
    }

    h.status = Handle::STATUS_ACTIVE;
    h.stream_start_ms = active_streams_[req.target_robot_id].start_ms;
    h.active_stream_count =
        static_cast<uint32_t>(active_streams_.size());
    h.timestamp_ms = nowMs(*get_clock());
    return h;
}

Handle GStreamerRelayNode::handleStop(const Req& req) {
    Handle h;
    h.sequence = req.sequence;
    h.target_robot_id = req.target_robot_id;
    h.status = Handle::STATUS_STOPPED;

    auto it = active_streams_.find(req.target_robot_id);
    if (it != active_streams_.end()) {
        it->second.pipeline->stop();
        active_streams_.erase(it);
    }
    if (active_streams_.size() < kThumbnailThreshold && thumbnail_mode_) {
        restoreFromThumbnail();
    }
    h.active_stream_count =
        static_cast<uint32_t>(active_streams_.size());
    h.timestamp_ms = nowMs(*get_clock());
    return h;
}

Handle GStreamerRelayNode::handleChangeQuality(const Req& req) {
    Handle h;
    h.sequence = req.sequence;
    h.target_robot_id = req.target_robot_id;

    auto it = active_streams_.find(req.target_robot_id);
    if (it == active_streams_.end()) {
        h.status = Handle::STATUS_ERROR;
        h.error_msg = "no active stream for this robot_id";
        return h;
    }

    // Force thumbnail if we are already in thumbnail mode regardless
    // of what the operator requested.
    uint8_t effective_quality =
        thumbnail_mode_ ? Req::QUALITY_THUMBNAIL : req.quality;

    auto& st = it->second;
    st.quality = effective_quality;
    const std::string desc = buildRelayPipelineDescription(
        req.target_robot_id, effective_quality,
        st.operator_ip, st.passphrase);
    if (!st.pipeline->rebuild(desc)) {
        h.status = Handle::STATUS_ERROR;
        h.error_msg = "pipeline rebuild failed";
        return h;
    }
    h.codec = st.codec;
    h.quality = effective_quality;
    h.passphrase = st.passphrase;
    h.srt_uri = buildSrtUri(st.passphrase);
    h.actual_bitrate_kbps = qualityBitrateKbps(effective_quality);
    h.status = Handle::STATUS_ACTIVE;
    h.stream_start_ms = st.start_ms;
    h.active_stream_count =
        static_cast<uint32_t>(active_streams_.size());
    h.timestamp_ms = nowMs(*get_clock());
    return h;
}

void GStreamerRelayNode::downgradeAllToThumbnail() {
    thumbnail_mode_ = true;
    if (auto_rate_) auto_rate_->setExternalThumbnailLock(true);
    for (auto& [robot_id, st] : active_streams_) {
        if (st.quality == Req::QUALITY_THUMBNAIL) continue;
        st.quality = Req::QUALITY_THUMBNAIL;
        const std::string desc = buildRelayPipelineDescription(
            robot_id, Req::QUALITY_THUMBNAIL,
            st.operator_ip, st.passphrase);
        st.pipeline->rebuild(desc);
    }
    RCLCPP_WARN(get_logger(),
        "downgraded %zu streams to THUMBNAIL (>= %zu concurrent)",
        active_streams_.size(), kThumbnailThreshold);
}

void GStreamerRelayNode::restoreFromThumbnail() {
    thumbnail_mode_ = false;
    if (auto_rate_) auto_rate_->setExternalThumbnailLock(false);
    // Leave currently-running streams alone - the operator can issue
    // a CHANGE_QUALITY request to bring HD back up if they want. This
    // matches the operator UX of "thumbnail mode is sticky until the
    // operator explicitly upgrades."
    RCLCPP_INFO(get_logger(),
        "concurrent stream count dropped below %zu; ready to honor "
        "CHANGE_QUALITY requests",
        kThumbnailThreshold);
}

void GStreamerRelayNode::onLinkQuality(
    combat_robot_msgs::msg::LteLinkQuality::SharedPtr msg)
{
    if (msg == nullptr || !auto_rate_) return;
    auto_rate_->onLinkQuality(*msg);
}

void GStreamerRelayNode::applyAutoRateTarget(uint8_t target_quality) {
    // The concurrency lock should also short-circuit here, but the
    // controller already holds the lock state. Guard anyway in case
    // the lock changes mid-call.
    if (thumbnail_mode_) return;
    if (active_streams_.empty()) return;

    for (auto& [robot_id, st] : active_streams_) {
        if (st.quality == target_quality) continue;
        st.quality = target_quality;
        const std::string desc = buildRelayPipelineDescription(
            robot_id, target_quality, st.operator_ip, st.passphrase);
        st.pipeline->rebuild(desc);
    }
    RCLCPP_INFO(get_logger(),
        "auto-rate: applied target_quality=%u to %zu streams",
        static_cast<unsigned>(target_quality), active_streams_.size());
}

void GStreamerRelayNode::processLinkQualityForTest(
    const combat_robot_msgs::msg::LteLinkQuality& msg)
{
    if (auto_rate_) auto_rate_->onLinkQuality(msg);
}

uint8_t GStreamerRelayNode::currentAutoRateTarget() const {
    return auto_rate_ ? auto_rate_->currentTargetQuality()
                      : static_cast<uint8_t>(Req::QUALITY_HD);
}

Handle GStreamerRelayNode::processRequestForTest(const Req& req) {
    switch (req.action) {
        case Req::ACTION_START:          return handleStart(req);
        case Req::ACTION_STOP:           return handleStop(req);
        case Req::ACTION_CHANGE_QUALITY: return handleChangeQuality(req);
        default: {
            Handle h;
            h.sequence = req.sequence;
            h.target_robot_id = req.target_robot_id;
            h.status = Handle::STATUS_ERROR;
            h.error_msg = "unknown action";
            return h;
        }
    }
}

void GStreamerRelayNode::publishHandle(Handle& handle) {
    handle.header.stamp = now();
    handle.header.frame_id = "hub";
    handle.timestamp_ms = nowMs(*get_clock());
    handle.active_stream_count =
        static_cast<uint32_t>(active_streams_.size());
    if (handle.sequence == 0) {
        handle.sequence = ++handle_seq_;
    }
    if (handle_pub_) handle_pub_->publish(handle);
}

}  // namespace san_hub_comm
