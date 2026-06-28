// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_hub_comm/gstreamer_relay_node.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <utility>

namespace san_hub_comm
{

using Req = combat_robot_msgs::msg::VideoStreamRequest;
using Handle = combat_robot_msgs::msg::VideoStreamHandle;

namespace
{

uint64_t nowMs(rclcpp::Clock & clk)
{
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

GStreamerRelayNode::GStreamerRelayNode(const rclcpp::NodeOptions & options)
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
    [this](uint8_t q) {applyAutoRateTarget(q);}, cfg);

  wireInterfaces();

  RCLCPP_INFO(
    get_logger(),
    "GStreamerRelay started: hub_ip=%s srt_port=%d udp_base=%d encryption=%s "
    "auto_rate=%s thumbnail_threshold=%zu redact_passphrase=%s csprng=%s",
    hub_ip_.c_str(), srt_port_, udp_base_port_,
    encryption_default_ ? "on" : "off",
    auto_rate_enabled_ ? "on" : "off",
    thumbnail_threshold_,
    redact_passphrase_in_handle_ ? "yes" : "no",
    PassphraseGenerator::isCsprngAvailable() ? "ok" : "FAIL");
}

void GStreamerRelayNode::declareParameters()
{
  declare_parameter<std::string>("hub_ip", "10.0.0.2");
  declare_parameter<int>("srt_port", 8888);
  declare_parameter<int>("udp_base_port", 5000);
  declare_parameter<int>("srt_latency_ms", 120);
  declare_parameter<bool>("encryption_default", true);
  declare_parameter<bool>("auto_rate_enabled", true);
  declare_parameter<int>("auto_rate_promote_hold_ticks", 5);
  // ★ PATCH 2026-05-13 (HC6): configurable threshold.
  declare_parameter<int>(
    "thumbnail_threshold",
    static_cast<int>(kDefaultThumbnailThreshold));
  // ★ PATCH 2026-05-13 (HC1): redaction toggle.
  declare_parameter<bool>("redact_passphrase_in_handle", false);
}

void GStreamerRelayNode::readParameters()
{
  hub_ip_ = get_parameter("hub_ip").as_string();
  srt_port_ = get_parameter("srt_port").as_int();
  udp_base_port_ = get_parameter("udp_base_port").as_int();
  srt_latency_ms_ = get_parameter("srt_latency_ms").as_int();
  encryption_default_ = get_parameter("encryption_default").as_bool();
  auto_rate_enabled_ = get_parameter("auto_rate_enabled").as_bool();
  auto_rate_promote_hold_ticks_ =
    get_parameter("auto_rate_promote_hold_ticks").as_int();
  if (auto_rate_promote_hold_ticks_ < 1) {auto_rate_promote_hold_ticks_ = 1;}

  int tt = get_parameter("thumbnail_threshold").as_int();
  if (tt < 1) {
    RCLCPP_WARN(
      get_logger(),
      "thumbnail_threshold=%d invalid, clamping to 1", tt);
    tt = 1;
  }
  thumbnail_threshold_ = static_cast<std::size_t>(tt);
  redact_passphrase_in_handle_ =
    get_parameter("redact_passphrase_in_handle").as_bool();
}

void GStreamerRelayNode::wireInterfaces()
{
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
    std::bind(
      &GStreamerRelayNode::onRequest, this,
      std::placeholders::_1),
    sub_opts);
  handle_pub_ = create_publisher<Handle>("/video/handle", qos);

  if (auto_rate_enabled_) {
    rclcpp::QoS lq_qos(10);
    lq_qos.best_effort();
    link_quality_sub_ =
      create_subscription<combat_robot_msgs::msg::LteLinkQuality>(
      "/lte/link_quality", lq_qos,
      std::bind(
        &GStreamerRelayNode::onLinkQuality, this,
        std::placeholders::_1),
      sub_opts);
  }
}

int GStreamerRelayNode::qualityBitrateKbps(uint8_t quality)
{
  // v1.5.1 (DCN-2026-003 D-001, 2026-05-13):
  //   ★ HD downsizing — operator preference: HD@2Mbps as default.
  //   기존: HD = 1500 kbps  →  변경: HD = 2000 kbps (2 Mbps, 720p)
  //   FHD 는 동시 3-stream 시 LTE bandwidth budget 초과 우려로 유지.
  //   THUMBNAIL/LOW 도 ratio 유지 위해 미세 조정 없음.
  //   default 도 HD 와 일치하도록 2000 kbps 로 변경.
  switch (quality) {
    case Req::QUALITY_THUMBNAIL: return 100;
    case Req::QUALITY_LOW:       return 500;
    case Req::QUALITY_HD:        return 2000;       // ★ 1500 → 2000 (HD 2 Mbps)
    case Req::QUALITY_FHD:       return 4000;
    default:                     return 2000;       // ★ HD default
  }
}

// ★ PATCH 2026-05-13 (HC1, HC2):
//   * URI no longer embeds the passphrase (it travels in
//     handle.passphrase, which the operator topic policy must
//     protect — DDS partition / authenticated bridge).
//   * URI now uses the operator endpoint matching the srtsink caller
//     mode actually used by the GStreamer pipeline. The pre-patch
//     handle told the operator "listen at hub_ip:srt_port" while
//     the relay was pushing to "operator_ip:srt_port" — the two
//     endpoints differed, so the connection never established.
std::string GStreamerRelayNode::buildSrtUri(const std::string & operator_ip) const
{
  std::ostringstream o;
  o << "srt://" << operator_ip << ":" << srt_port_
    << "?mode=listener&latency=" << srt_latency_ms_;
  return o.str();
}

std::string GStreamerRelayNode::buildRelayPipelineDescription(
  uint32_t robot_id, uint8_t /*quality*/,
  const std::string & operator_ip,
  const std::string & passphrase) const
{
  // Follower → Hub leg: RTP/MPEG-TS over UDP, port = udp_base + robot_id.
  // Hub → Operator leg: SRT caller pointing at operator_ip:srt_port.
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

void GStreamerRelayNode::onRequest(Req::SharedPtr msg)
{
  if (msg == nullptr) {return;}
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

Handle GStreamerRelayNode::handleStart(const Req & req)
{
  Handle h;
  h.sequence = req.sequence;
  h.target_robot_id = req.target_robot_id;
  h.codec = req.codec;

  const std::string operator_ip =
    req.operator_ip.empty() ? hub_ip_ : req.operator_ip;
  const std::string passphrase =
    (req.encryption || encryption_default_) ?
    passphrase_gen_.generate() : "";

  // ★ PATCH 2026-05-13 (HC3/HC4): single critical section for the
  // check-then-write thumbnail decision + map insertion.
  std::unique_ptr<GStreamerPipeline> pipe;
  bool pipeline_play_ok = false;
  uint8_t effective_quality = req.quality;
  uint64_t start_ms_for_handle = 0;
  std::size_t active_count_for_handle = 0;

  {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    // Decide effective quality under the lock so we see a
    // consistent active count.
    if (active_streams_.size() + 1 >= thumbnail_threshold_) {
      effective_quality = Req::QUALITY_THUMBNAIL;
      thumbnail_mode_.store(true);
    }
    h.quality = effective_quality;

    const std::string desc = buildRelayPipelineDescription(
      req.target_robot_id, effective_quality,
      operator_ip, passphrase);
    pipe = std::make_unique<GStreamerPipeline>(get_logger(), desc);
    const uint32_t robot_id = req.target_robot_id;
    pipe->setEventCallback(
      [this, robot_id](PipelineEvent ev, const std::string & detail) {
        if (ev == PipelineEvent::ERROR_FATAL || ev == PipelineEvent::EOS) {
          RCLCPP_ERROR(
            get_logger(),
            "pipeline event for robot %u: %s",
            robot_id, detail.c_str());
        }
      });

    pipeline_play_ok = pipe->play();
    if (!pipeline_play_ok) {
      h.status = Handle::STATUS_ERROR;
      h.error_msg = "gst_parse_launch / play failed";
      // [review] The stream was NOT inserted into active_streams_, but
      // thumbnail_mode_ may have been set true above on the threshold
      // check. Recompute it from the actual (unchanged) active count so a
      // failed start can't leave thumbnail_mode_ stuck true — which would
      // suppress applyAutoRateTarget() indefinitely. Drop pipe; lock
      // released by scope exit.
      thumbnail_mode_.store(active_streams_.size() >= thumbnail_threshold_);
      return h;
    }

    StreamState state;
    state.pipeline = std::move(pipe);
    state.passphrase = passphrase;
    state.codec = req.codec;
    state.quality = effective_quality;
    state.operator_ip = operator_ip;
    state.start_ms = nowMs(*get_clock());
    start_ms_for_handle = state.start_ms;
    active_streams_[req.target_robot_id] = std::move(state);

    // If we crossed the threshold AND haven't downgraded yet, do it now.
    if (active_streams_.size() >= thumbnail_threshold_) {
      // downgradeAllToThumbnailLocked is idempotent.
      downgradeAllToThumbnailLocked();
    }
    active_count_for_handle = active_streams_.size();
  }

  // ★ PATCH 2026-05-13 (HC1, HC2): srt_uri is the operator-listener
  // endpoint matching the relay's caller-mode srtsink. Passphrase
  // travels in its own field and may be redacted on publish.
  h.passphrase = redact_passphrase_in_handle_ ? std::string() : passphrase;
  h.srt_uri = buildSrtUri(operator_ip);
  h.actual_bitrate_kbps = qualityBitrateKbps(effective_quality);
  h.status = Handle::STATUS_ACTIVE;
  h.stream_start_ms = start_ms_for_handle;
  h.active_stream_count =
    static_cast<uint32_t>(active_count_for_handle);
  h.timestamp_ms = nowMs(*get_clock());
  return h;
}

Handle GStreamerRelayNode::handleStop(const Req & req)
{
  Handle h;
  h.sequence = req.sequence;
  h.target_robot_id = req.target_robot_id;
  h.status = Handle::STATUS_STOPPED;

  std::size_t active_after_stop = 0;
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = active_streams_.find(req.target_robot_id);
    if (it != active_streams_.end()) {
      it->second.pipeline->stop();
      active_streams_.erase(it);
    }
    if (active_streams_.size() < thumbnail_threshold_ &&
      thumbnail_mode_.load())
    {
      restoreFromThumbnailLocked();
    }
    active_after_stop = active_streams_.size();
  }
  h.active_stream_count = static_cast<uint32_t>(active_after_stop);
  h.timestamp_ms = nowMs(*get_clock());
  return h;
}

Handle GStreamerRelayNode::handleChangeQuality(const Req & req)
{
  Handle h;
  h.sequence = req.sequence;
  h.target_robot_id = req.target_robot_id;

  std::unique_lock<std::mutex> lock(streams_mutex_);
  auto it = active_streams_.find(req.target_robot_id);
  if (it == active_streams_.end()) {
    h.status = Handle::STATUS_ERROR;
    h.error_msg = "no active stream for this robot_id";
    return h;
  }

  uint8_t effective_quality =
    thumbnail_mode_.load() ? Req::QUALITY_THUMBNAIL : req.quality;

  auto & st = it->second;
  st.quality = effective_quality;
  const std::string desc = buildRelayPipelineDescription(
    req.target_robot_id, effective_quality,
    st.operator_ip, st.passphrase);
  if (!st.pipeline->rebuild(desc)) {
    h.status = Handle::STATUS_ERROR;
    h.error_msg = "pipeline rebuild failed";
    return h;
  }
  const std::string operator_ip = st.operator_ip;
  const std::string passphrase = st.passphrase;
  const uint64_t start_ms = st.start_ms;
  const std::size_t active_count = active_streams_.size();
  h.codec = st.codec;
  h.quality = effective_quality;
  lock.unlock();

  h.passphrase = redact_passphrase_in_handle_ ? std::string() : passphrase;
  h.srt_uri = buildSrtUri(operator_ip);
  h.actual_bitrate_kbps = qualityBitrateKbps(effective_quality);
  h.status = Handle::STATUS_ACTIVE;
  h.stream_start_ms = start_ms;
  h.active_stream_count = static_cast<uint32_t>(active_count);
  h.timestamp_ms = nowMs(*get_clock());
  return h;
}

void GStreamerRelayNode::downgradeAllToThumbnailLocked()
{
  thumbnail_mode_.store(true);
  if (auto_rate_) {auto_rate_->setExternalThumbnailLock(true);}
  std::size_t downgraded = 0;
  for (auto & [robot_id, st] : active_streams_) {
    if (st.quality == Req::QUALITY_THUMBNAIL) {continue;}
    st.quality = Req::QUALITY_THUMBNAIL;
    const std::string desc = buildRelayPipelineDescription(
      robot_id, Req::QUALITY_THUMBNAIL,
      st.operator_ip, st.passphrase);
    st.pipeline->rebuild(desc);
    ++downgraded;
  }
  if (downgraded > 0) {
    RCLCPP_WARN(
      get_logger(),
      "downgraded %zu streams to THUMBNAIL (>= %zu concurrent)",
      downgraded, thumbnail_threshold_);
  }
}

void GStreamerRelayNode::restoreFromThumbnailLocked()
{
  thumbnail_mode_.store(false);
  if (auto_rate_) {auto_rate_->setExternalThumbnailLock(false);}
  RCLCPP_INFO(
    get_logger(),
    "concurrent stream count dropped below %zu; ready to honor "
    "CHANGE_QUALITY requests",
    thumbnail_threshold_);
}

void GStreamerRelayNode::onLinkQuality(
  combat_robot_msgs::msg::LteLinkQuality::SharedPtr msg)
{
  if (msg == nullptr || !auto_rate_) {return;}
  auto_rate_->onLinkQuality(*msg);
}

void GStreamerRelayNode::applyAutoRateTarget(uint8_t target_quality)
{
  if (thumbnail_mode_.load()) {return;}

  std::lock_guard<std::mutex> lock(streams_mutex_);
  if (active_streams_.empty()) {return;}

  std::size_t changed = 0;
  for (auto & [robot_id, st] : active_streams_) {
    if (st.quality == target_quality) {continue;}
    st.quality = target_quality;
    const std::string desc = buildRelayPipelineDescription(
      robot_id, target_quality, st.operator_ip, st.passphrase);
    st.pipeline->rebuild(desc);
    ++changed;
  }
  if (changed > 0) {
    RCLCPP_INFO(
      get_logger(),
      "auto-rate: applied target_quality=%u to %zu streams",
      static_cast<unsigned>(target_quality), changed);
  }
}

void GStreamerRelayNode::processLinkQualityForTest(
  const combat_robot_msgs::msg::LteLinkQuality & msg)
{
  if (auto_rate_) {auto_rate_->onLinkQuality(msg);}
}

uint8_t GStreamerRelayNode::currentAutoRateTarget() const
{
  return auto_rate_ ? auto_rate_->currentTargetQuality() :
         static_cast<uint8_t>(Req::QUALITY_HD);
}

Handle GStreamerRelayNode::processRequestForTest(const Req & req)
{
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

void GStreamerRelayNode::publishHandle(Handle & handle)
{
  handle.header.stamp = now();
  handle.header.frame_id = "hub";
  handle.timestamp_ms = nowMs(*get_clock());
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    handle.active_stream_count =
      static_cast<uint32_t>(active_streams_.size());
  }
  if (handle.sequence == 0) {
    handle.sequence = ++handle_seq_;
  }
  if (handle_pub_) {handle_pub_->publish(handle);}
}

}  // namespace san_hub_comm
