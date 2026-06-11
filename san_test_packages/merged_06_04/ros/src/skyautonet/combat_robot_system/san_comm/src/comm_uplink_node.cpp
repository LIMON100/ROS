// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — CommUplinkNode (PATCHED 2026-05-13).

#include "san_comm/comm_uplink_node.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace san_comm
{

using namespace std::chrono_literals;

// Wrap LinkSelector usage to silence the deprecation warning in the
// implementation file (consumers still get the warning).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// ─── Stub backends ──────────────────────────────────────────────────────

namespace
{

class StubHttpUploader : public HttpUploaderInterface
{
public:
  int post(
    const std::string & url,
    const std::vector<uint8_t> & body) override
  {
    std::cerr << "[san_comm][STUB-HTTP] POST " << url
              << " body=" << body.size() << " bytes\n";
    return -1;
  }
};

class StubReachabilityProbe : public ReachabilityProbeInterface
{
public:
  bool probe(const std::string & /*h*/, int /*p*/, int /*t*/) override
  {
    return false;
  }
};

// ★ PATCH 2026-05-13: real TCP probe (mirrors san_comm_link).
class RealTcpProbe : public ReachabilityProbeInterface
{
public:
  bool probe(
    const std::string & host, int port,
    int timeout_ms) override
  {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo * res = nullptr;
    const std::string port_str = std::to_string(port);
    if (::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 ||
      res == nullptr)
    {
      return false;
    }
    bool success = false;
    for (addrinfo * ai = res; ai != nullptr; ai = ai->ai_next) {
      int fd = ::socket(
        ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK,
        ai->ai_protocol);
      if (fd < 0) {continue;}
      const int rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
      if (rc == 0) {
        ::close(fd);
        success = true;
        break;
      }
      if (errno != EINPROGRESS) {::close(fd); continue;}
      fd_set wfds; FD_ZERO(&wfds); FD_SET(fd, &wfds);
      timeval tv;
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      const int sel = ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
      if (sel > 0) {
        int so_err = 0; socklen_t so_len = sizeof(so_err);
        if (::getsockopt(
            fd, SOL_SOCKET, SO_ERROR,
            &so_err, &so_len) == 0 && so_err == 0)
        {
          success = true;
        }
      }
      ::close(fd);
      if (success) {break;}
    }
    ::freeaddrinfo(res);
    return success;
  }
};

}  // namespace

std::unique_ptr<HttpUploaderInterface> makeRealHttpUploader()
{
  // CDR: real libcurl-based uploader. For now, stub with WARN —
  // honest "production not yet wired" rather than silent grant.
  std::cerr << "[san_comm] WARNING: makeRealHttpUploader() returns stub "
    "(libcurl integration deferred to CDR).\n";
  return std::make_unique<StubHttpUploader>();
}
std::unique_ptr<HttpUploaderInterface> makeStubHttpUploader()
{
  return std::make_unique<StubHttpUploader>();
}
std::unique_ptr<ReachabilityProbeInterface> makeRealProbe()
{
  return std::make_unique<RealTcpProbe>();
}
std::unique_ptr<ReachabilityProbeInterface> makeStubProbe()
{
  return std::make_unique<StubReachabilityProbe>();
}

// ─── ctors ──────────────────────────────────────────────────────────────

CommUplinkNode::CommUplinkNode(const rclcpp::NodeOptions & opts)
: CommUplinkNode(opts, nullptr, nullptr) {}

CommUplinkNode::CommUplinkNode(
  const rclcpp::NodeOptions & opts,
  std::unique_ptr<HttpUploaderInterface> uploader,
  std::unique_ptr<ReachabilityProbeInterface> probe)
: rclcpp::Node("comm_uplink_node", opts)
{
  declareParameters();
  loadParameters();

  // PATCH 2026-05-13: explicit stub vs real selection.
  uploader_ = uploader ? std::move(uploader) :
    (use_stub_backend_ ? makeStubHttpUploader() :
    makeRealHttpUploader());
  probe_ = probe ? std::move(probe) :
    (use_stub_backend_ ? makeStubProbe() :
    makeRealProbe());
  if (!uploader_ || !probe_) {
    throw std::runtime_error("CommUplinkNode: null uploader/probe");
  }

  selector_.reconfigure(LinkSelectorConfig{wifi_recovery_threshold_});

  active_link_pub_ = create_publisher<std_msgs::msg::String>(
    "~/active_link", rclcpp::QoS(1).reliable().transient_local());

  // ★ PATCH 2026-05-13: subscribe to canonical CommLinkStatus if
  // configured to use it. Subscription is created unconditionally so
  // operators can flip the param at runtime without restart, but the
  // probe path only consumes it when use_external_link_status_=true.
  comm_link_sub_ = create_subscription<combat_robot_msgs::msg::CommLinkStatus>(
    "/comm_link_node/status",
    rclcpp::QoS(1).reliable().transient_local(),
    std::bind(
      &CommUplinkNode::onCommLinkStatus, this,
      std::placeholders::_1));

  // ★ PATCH 2026-05-13 (C2): canonical LTE topic name.
  lte_sub_ = create_subscription<combat_robot_msgs::msg::LteModemStatus>(
    "/lte/modem_status",
    rclcpp::QoS(1).best_effort(),
    std::bind(
      &CommUplinkNode::onLteStatus, this,
      std::placeholders::_1));

  telemetry_sub_ = create_subscription<std_msgs::msg::String>(
    "~/telemetry",
    rclcpp::QoS(20).reliable(),
    std::bind(
      &CommUplinkNode::onTelemetry, this,
      std::placeholders::_1));

  const auto period_ms = std::chrono::milliseconds(
    static_cast<int64_t>(1000.0 / probe_rate_hz_));
  probe_timer_ = create_wall_timer(
    period_ms, std::bind(&CommUplinkNode::onProbeTick, this));
  health_timer_ = create_wall_timer(
    1s, std::bind(&CommUplinkNode::onHealthTick, this));

  RCLCPP_INFO(
    get_logger(),
    "CommUplinkNode UP: server=%s probe=%s:%d hyst=%u "
    "external_link_status=%d stub_backend=%d",
    server_url_.c_str(), probe_host_.c_str(),
    probe_port_, wifi_recovery_threshold_,
    use_external_link_status_ ? 1 : 0,
    use_stub_backend_ ? 1 : 0);
}

// ─── params ─────────────────────────────────────────────────────────────

void CommUplinkNode::declareParameters()
{
  declare_parameter<std::string>(
    "server_url",
    "https://telemetry.example.invalid/v1/upload");
  declare_parameter<std::string>("probe_host", "10.0.0.1");
  declare_parameter<int>("probe_port", 443);
  declare_parameter<int>("probe_timeout_ms", 1500);
  declare_parameter<double>("probe_rate_hz", 1.0);
  declare_parameter<int>("wifi_recovery_threshold", 3);
  // ★ PATCH 2026-05-13:
  declare_parameter<bool>("use_external_link_status", true);
  declare_parameter<bool>("use_stub_backend", false);
}

void CommUplinkNode::loadParameters()
{
  server_url_ = get_parameter("server_url").as_string();
  probe_host_ = get_parameter("probe_host").as_string();
  probe_port_ = static_cast<int>(get_parameter("probe_port").as_int());
  probe_timeout_ms_ =
    static_cast<int>(get_parameter("probe_timeout_ms").as_int());
  probe_rate_hz_ = get_parameter("probe_rate_hz").as_double();
  wifi_recovery_threshold_ = static_cast<uint32_t>(
    get_parameter("wifi_recovery_threshold").as_int());
  use_external_link_status_ =
    get_parameter("use_external_link_status").as_bool();
  use_stub_backend_ = get_parameter("use_stub_backend").as_bool();

  // PATCH 2026-05-13 (M11): widen probe_rate_hz allowed range.
  if (probe_rate_hz_ <= 0.0 || probe_rate_hz_ > 20.0) {
    throw std::runtime_error("probe_rate_hz out of range (0..20]");
  }
}

// ─── callbacks ──────────────────────────────────────────────────────────

void CommUplinkNode::onProbeTick()
{
  if (use_external_link_status_) {
    // ★ PATCH 2026-05-13: external CommLinkStatus mode — just
    // republish the active link snapshot for any legacy String
    // consumers. No self-selection.
    std_msgs::msg::String msg;
    switch (external_active_link_.load()) {
      case 1: msg.data = "wifi6"; break;
      case 2: msg.data = "lte";   break;
      default: msg.data = "none"; break;
    }
    active_link_pub_->publish(msg);
    return;
  }

  // Legacy self-selecting mode.
  const bool ok =
    probe_->probe(probe_host_, probe_port_, probe_timeout_ms_);

  LinkProbe lp;
  {
    std::lock_guard<std::mutex> g(state_mutex_);
    last_wifi6_reachable_ = ok;
    lp.wifi6_reachable = ok;
    lp.lte_registered = last_lte_registered_;
    lp.lte_pdp_active = last_lte_pdp_active_;
  }

  const auto link = selector_.update(lp);

  std_msgs::msg::String msg;
  msg.data = toString(link);
  active_link_pub_->publish(msg);
}

void CommUplinkNode::onLteStatus(
  combat_robot_msgs::msg::LteModemStatus::SharedPtr msg)
{
  std::lock_guard<std::mutex> g(state_mutex_);
  using StatusMsg = combat_robot_msgs::msg::LteModemStatus;
  last_lte_registered_ =
    (msg->registered == StatusMsg::REG_HOME ||
    msg->registered == StatusMsg::REG_ROAMING);
  last_lte_pdp_active_ = msg->pdp_active;
}

// ★ PATCH 2026-05-13: CommLinkStatus consumer.
void CommUplinkNode::onCommLinkStatus(
  combat_robot_msgs::msg::CommLinkStatus::SharedPtr msg)
{
  external_active_link_.store(msg->active_link);
  external_last_update_ms_.store(
    static_cast<uint64_t>(now().nanoseconds() / 1'000'000));
}

void CommUplinkNode::onTelemetry(std_msgs::msg::String::SharedPtr msg)
{
  bool have_link = false;
  if (use_external_link_status_) {
    have_link = (external_active_link_.load() != 0);
  } else {
    have_link = (selector_.current() != ActiveLink::None);
  }
  if (!have_link) {
    // PATCH 2026-05-13 (M9): TODO disk cache for "no link" telemetry
    // (tracked for CDR — store-and-forward).
    ++upload_fail_count_;
    return;
  }
  std::vector<uint8_t> body(msg->data.begin(), msg->data.end());
  const int status = uploader_->post(server_url_, body);
  if (status >= 200 && status < 300) {
    ++upload_ok_count_;
  } else {
    ++upload_fail_count_;
  }
}

void CommUplinkNode::onHealthTick()
{
  if (use_external_link_status_) {
    const uint64_t now_ms =
      static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
    const uint64_t age_ms =
      now_ms - external_last_update_ms_.load();
    const char * link_str = "none";
    switch (external_active_link_.load()) {
      case 1: link_str = "wifi6"; break;
      case 2: link_str = "lte";   break;
    }
    RCLCPP_INFO(
      get_logger(),
      "comm link=%s (external, age=%lums) upload ok=%u fail=%u",
      link_str, static_cast<unsigned long>(age_ms),
      upload_ok_count_.load(), upload_fail_count_.load());
  } else {
    RCLCPP_INFO(
      get_logger(),
      "comm link=%s (self) recov=%u failovers=%u recoveries=%u "
      "upload ok=%u fail=%u",
      toString(selector_.current()),
      selector_.wifi_recovery_count(),
      selector_.failover_count(),
      selector_.recovery_count(),
      upload_ok_count_.load(), upload_fail_count_.load());
  }
}

#pragma GCC diagnostic pop

}  // namespace san_comm
