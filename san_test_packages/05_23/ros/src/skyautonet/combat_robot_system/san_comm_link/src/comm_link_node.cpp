// SAN v1.5 Phase 2-E Turn 8 — CommLinkNode implementation.

#include "san_comm_link/comm_link_node.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace san_comm_link {

using namespace std::chrono_literals;
using LinkStatusMsg = combat_robot_msgs::msg::CommLinkStatus;
using LteStatusMsg  = combat_robot_msgs::msg::LteModemStatus;

// ─── Stub WiFi6 probe (build-time fallback) ─────────────────────────────

namespace {
class StubWifi6Probe : public Wifi6ProbeInterface {
public:
  bool probe(const std::string& host, int port,
              std::chrono::milliseconds /*to*/) override {
    std::cerr << "[san_comm_link][STUB-PROBE] probe(" << host << ":"
              << port << ") — no real net backend, returning false.\n";
    return false;
  }
};
}  // namespace

std::unique_ptr<Wifi6ProbeInterface> makeRealWifi6Probe() {
  // Production: TCP connect to host:port with timeout. For now (stub
  // build), return the stub so build succeeds.
  return std::make_unique<StubWifi6Probe>();
}

// ─── ctors ──────────────────────────────────────────────────────────────

CommLinkNode::CommLinkNode(const rclcpp::NodeOptions& opts)
    : CommLinkNode(opts, makeRealWifi6Probe()) {}

CommLinkNode::CommLinkNode(const rclcpp::NodeOptions& opts,
                            std::unique_ptr<Wifi6ProbeInterface> probe)
    : rclcpp::Node("comm_link_node", opts), probe_(std::move(probe)) {
  if (!probe_) {
    throw std::runtime_error("CommLinkNode: null probe injected");
  }
  declareParameters();
  loadParameters();

  status_pub_ = create_publisher<LinkStatusMsg>(
      "~/status", rclcpp::QoS(1).reliable().transient_local());

  lte_sub_ = create_subscription<LteStatusMsg>(
      "/lte/modem_status",
      rclcpp::QoS(1).best_effort(),
      std::bind(&CommLinkNode::onLteStatus, this,
                std::placeholders::_1));

  tick_timer_ = create_wall_timer(
      1s, std::bind(&CommLinkNode::onTick, this));

  RCLCPP_INFO(get_logger(),
      "CommLinkNode UP: probe=%s:%d timeout=%dms",
      probe_host_.c_str(), probe_port_, probe_timeout_ms_);
}

void CommLinkNode::declareParameters() {
  declare_parameter<std::string>("wifi6_probe_host",   "1.1.1.1");
  declare_parameter<int>("wifi6_probe_port",            443);
  declare_parameter<int>("wifi6_probe_timeout_ms",      1500);
  declare_parameter<int>("consec_ok_to_upgrade",        5);
  declare_parameter<int>("consec_fail_to_downgrade",    3);
}

void CommLinkNode::loadParameters() {
  probe_host_       = get_parameter("wifi6_probe_host").as_string();
  probe_port_       = static_cast<int>(get_parameter("wifi6_probe_port").as_int());
  probe_timeout_ms_ = static_cast<int>(get_parameter("wifi6_probe_timeout_ms").as_int());

  LinkHysteresisConfig cfg;
  cfg.consec_ok_to_upgrade =
      static_cast<uint16_t>(get_parameter("consec_ok_to_upgrade").as_int());
  cfg.consec_fail_to_downgrade =
      static_cast<uint16_t>(get_parameter("consec_fail_to_downgrade").as_int());
  monitor_ = LinkHealthMonitor(cfg);
}

void CommLinkNode::onLteStatus(const LteStatusMsg::SharedPtr msg) {
  // We treat the LTE link as "ok" iff the modem is registered AND has
  // a PDP context (data path).
  const bool registered =
      (msg->registered == LteStatusMsg::REG_HOME ||
       msg->registered == LteStatusMsg::REG_ROAMING);
  last_lte_ok_ = registered && msg->pdp_active;
}

void CommLinkNode::onTick() {
  const bool wifi_ok = probe_->probe(probe_host_, probe_port_,
                                       std::chrono::milliseconds(probe_timeout_ms_));
  const bool lte_ok  = last_lte_ok_.load();
  auto decision = monitor_.update({wifi_ok, lte_ok});

  if (decision.switch_event) {
    last_switch_ms_     =
        static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
    last_switch_reason_ = decision.reason;
    RCLCPP_INFO(get_logger(), "link switch: %s",
                 decision.reason.c_str());
  }

  LinkStatusMsg out;
  out.header.stamp                  = now();
  out.header.frame_id               = "comm_link";
  out.active_link                   =
      static_cast<uint8_t>(decision.active_link);
  out.wifi6_ok                      = wifi_ok;
  out.lte_ok                        = lte_ok;
  out.wifi6_consec_ok_count         = monitor_.consecOk();
  out.wifi6_consec_fail_count       = monitor_.consecFail();
  out.switch_count                  = monitor_.switchCount();
  out.last_switch_timestamp_ms      = last_switch_ms_;
  out.last_switch_reason            = last_switch_reason_;
  status_pub_->publish(out);
}

}  // namespace san_comm_link
