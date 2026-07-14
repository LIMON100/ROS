// SAN v1.5 Phase 2-E Turn 8 — CommUplinkNode implementation.

#include "san_comm/comm_uplink_node.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace san_comm {

using namespace std::chrono_literals;

// ─── Stub HTTP backend ──────────────────────────────────────────────────

namespace {

class StubHttpUploader : public HttpUploaderInterface {
public:
  int post(const std::string& url,
            const std::vector<uint8_t>& body) override {
    std::cerr << "[san_comm][STUB-HTTP] POST " << url
              << " body=" << body.size() << " bytes (no real network)\n";
    return -1;
  }
};

class StubReachabilityProbe : public ReachabilityProbeInterface {
public:
  bool probe(const std::string& /*host*/, int /*port*/,
              int /*timeout_ms*/) override {
    return false;
  }
};

}  // namespace

std::unique_ptr<HttpUploaderInterface> makeRealHttpUploader() {
  return std::make_unique<StubHttpUploader>();
}
std::unique_ptr<ReachabilityProbeInterface> makeRealProbe() {
  return std::make_unique<StubReachabilityProbe>();
}

// ─── ctors ──────────────────────────────────────────────────────────────

CommUplinkNode::CommUplinkNode(const rclcpp::NodeOptions& opts)
    : CommUplinkNode(opts, makeRealHttpUploader(), makeRealProbe()) {}

CommUplinkNode::CommUplinkNode(
    const rclcpp::NodeOptions& opts,
    std::unique_ptr<HttpUploaderInterface> uploader,
    std::unique_ptr<ReachabilityProbeInterface> probe)
    : rclcpp::Node("comm_uplink_node", opts),
      uploader_(std::move(uploader)),
      probe_(std::move(probe)) {
  if (!uploader_ || !probe_) {
    throw std::runtime_error(
        "CommUplinkNode: null uploader/probe injected");
  }
  declareParameters();
  loadParameters();

  selector_ = LinkSelector(LinkSelectorConfig{wifi_recovery_threshold_});

  active_link_pub_ = create_publisher<std_msgs::msg::String>(
      "~/active_link", rclcpp::QoS(1).reliable().transient_local());

  lte_sub_ = create_subscription<combat_robot_msgs::msg::LteModemStatus>(
      "/robot_lte_status",   // remapped by launch
      rclcpp::QoS(1).best_effort(),
      std::bind(&CommUplinkNode::onLteStatus, this, std::placeholders::_1));

  telemetry_sub_ = create_subscription<std_msgs::msg::String>(
      "~/telemetry",
      rclcpp::QoS(20).reliable(),
      std::bind(&CommUplinkNode::onTelemetry, this, std::placeholders::_1));

  const auto period_ms = std::chrono::milliseconds(
      static_cast<int64_t>(1000.0 / probe_rate_hz_));
  probe_timer_ = create_wall_timer(
      period_ms, std::bind(&CommUplinkNode::onProbeTick, this));
  health_timer_ = create_wall_timer(
      1s, std::bind(&CommUplinkNode::onHealthTick, this));

  RCLCPP_INFO(get_logger(),
      "CommUplinkNode UP: server=%s probe=%s:%d hyst=%u",
      server_url_.c_str(), probe_host_.c_str(),
      probe_port_, wifi_recovery_threshold_);
}

// ─── params ─────────────────────────────────────────────────────────────

void CommUplinkNode::declareParameters() {
  declare_parameter<std::string>("server_url",
      "https://telemetry.example.invalid/v1/upload");
  declare_parameter<std::string>("probe_host", "10.0.0.1");
  declare_parameter<int>("probe_port",          443);
  declare_parameter<int>("probe_timeout_ms",    1500);
  declare_parameter<double>("probe_rate_hz",    1.0);
  declare_parameter<int>("wifi_recovery_threshold", 3);
}

void CommUplinkNode::loadParameters() {
  server_url_      = get_parameter("server_url").as_string();
  probe_host_      = get_parameter("probe_host").as_string();
  probe_port_      = static_cast<int>(get_parameter("probe_port").as_int());
  probe_timeout_ms_=
      static_cast<int>(get_parameter("probe_timeout_ms").as_int());
  probe_rate_hz_   = get_parameter("probe_rate_hz").as_double();
  wifi_recovery_threshold_ = static_cast<uint32_t>(
      get_parameter("wifi_recovery_threshold").as_int());
  if (probe_rate_hz_ <= 0.0 || probe_rate_hz_ > 10.0) {
    throw std::runtime_error("probe_rate_hz out of range");
  }
}

// ─── callbacks ──────────────────────────────────────────────────────────

void CommUplinkNode::onProbeTick() {
  const bool ok =
      probe_->probe(probe_host_, probe_port_, probe_timeout_ms_);

  LinkProbe lp;
  {
    std::lock_guard<std::mutex> g(state_mutex_);
    last_wifi6_reachable_ = ok;
    lp.wifi6_reachable    = ok;
    lp.lte_registered     = last_lte_registered_;
    lp.lte_pdp_active     = last_lte_pdp_active_;
  }

  const auto link = selector_.update(lp);

  std_msgs::msg::String msg;
  msg.data = toString(link);
  active_link_pub_->publish(msg);
}

void CommUplinkNode::onLteStatus(
    combat_robot_msgs::msg::LteModemStatus::SharedPtr msg) {
  std::lock_guard<std::mutex> g(state_mutex_);
  using StatusMsg = combat_robot_msgs::msg::LteModemStatus;
  last_lte_registered_ =
      (msg->registered == StatusMsg::REG_HOME ||
       msg->registered == StatusMsg::REG_ROAMING);
  last_lte_pdp_active_ = msg->pdp_active;
}

void CommUplinkNode::onTelemetry(std_msgs::msg::String::SharedPtr msg) {
  if (selector_.current() == ActiveLink::None) {
    // No link — would cache to disk. Out of scope for Turn 8.
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

void CommUplinkNode::onHealthTick() {
  RCLCPP_INFO(get_logger(),
      "comm link=%s recov=%u failovers=%u recoveries=%u "
      "upload ok=%u fail=%u",
      toString(selector_.current()),
      selector_.wifi_recovery_count(),
      selector_.failover_count(),
      selector_.recovery_count(),
      upload_ok_count_.load(), upload_fail_count_.load());
}

}  // namespace san_comm
