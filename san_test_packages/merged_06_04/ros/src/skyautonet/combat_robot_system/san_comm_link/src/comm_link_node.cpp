// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — CommLinkNode (PATCHED 2026-05-13).

#include "san_comm_link/comm_link_node.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace san_comm_link
{

using namespace std::chrono_literals;
using LinkStatusMsg = combat_robot_msgs::msg::CommLinkStatus;
using LteStatusMsg = combat_robot_msgs::msg::LteModemStatus;

// ─── PATCH 2026-05-13: REAL TCP probe ──────────────────────────────────

namespace
{

class RealTcpWifi6Probe : public Wifi6ProbeInterface
{
public:
  bool probe(
    const std::string & host, int port,
    std::chrono::milliseconds timeout) override
  {
    // Resolve host. getaddrinfo handles both IPv4 literal and hostname.
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
        // Immediate success (rare for non-loopback).
        ::close(fd);
        success = true;
        break;
      }
      if (errno != EINPROGRESS) {
        ::close(fd);
        continue;
      }

      // Non-blocking connect in progress. select() with timeout.
      fd_set wfds;
      FD_ZERO(&wfds);
      FD_SET(fd, &wfds);
      timeval tv;
      tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
      tv.tv_usec = static_cast<suseconds_t>(
        (timeout.count() % 1000) * 1000);
      const int sel = ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
      if (sel > 0) {
        // Check SO_ERROR — connect may have completed with an error.
        int so_err = 0;
        socklen_t so_len = sizeof(so_err);
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

class StubWifi6Probe : public Wifi6ProbeInterface
{
public:
  bool probe(
    const std::string & /*host*/, int /*port*/,
    std::chrono::milliseconds /*to*/) override
  {
    return false;
  }
};

}  // namespace

std::unique_ptr<Wifi6ProbeInterface> makeRealWifi6Probe()
{
  return std::make_unique<RealTcpWifi6Probe>();
}

std::unique_ptr<Wifi6ProbeInterface> makeStubWifi6Probe()
{
  return std::make_unique<StubWifi6Probe>();
}

// ─── ctors ──────────────────────────────────────────────────────────────

CommLinkNode::CommLinkNode(const rclcpp::NodeOptions & opts)
: CommLinkNode(opts, nullptr) {}

CommLinkNode::CommLinkNode(
  const rclcpp::NodeOptions & opts,
  std::unique_ptr<Wifi6ProbeInterface> probe)
: rclcpp::Node("comm_link_node", opts)
{
  declareParameters();
  loadParameters();

  // PATCH 2026-05-13 (C4): only fall back to stub on explicit opt-in.
  if (probe) {
    probe_ = std::move(probe);
  } else if (use_stub_probe_) {
    RCLCPP_WARN(
      get_logger(),
      "CommLinkNode: use_stub_probe=true → WiFi6 always false "
      "(integration test mode)");
    probe_ = makeStubWifi6Probe();
  } else {
    probe_ = makeRealWifi6Probe();
  }
  if (!probe_) {
    throw std::runtime_error("CommLinkNode: null probe");
  }

  status_pub_ = create_publisher<LinkStatusMsg>(
    "~/status", rclcpp::QoS(1).reliable().transient_local());

  // ★ PATCH 2026-05-13 (C2): canonical LTE topic name.
  lte_sub_ = create_subscription<LteStatusMsg>(
    "/lte/modem_status",
    rclcpp::QoS(1).best_effort(),
    std::bind(
      &CommLinkNode::onLteStatus, this,
      std::placeholders::_1));

  tick_timer_ = create_wall_timer(
    1s, std::bind(&CommLinkNode::onTick, this));

  RCLCPP_INFO(
    get_logger(),
    "CommLinkNode UP: probe=%s:%d timeout=%dms (real=%d)",
    probe_host_.c_str(), probe_port_, probe_timeout_ms_,
    use_stub_probe_ ? 0 : 1);
}

void CommLinkNode::declareParameters()
{
  declare_parameter<std::string>("wifi6_probe_host", "1.1.1.1");
  declare_parameter<int>("wifi6_probe_port", 443);
  declare_parameter<int>("wifi6_probe_timeout_ms", 1500);
  declare_parameter<int>("consec_ok_to_upgrade", 5);
  declare_parameter<int>("consec_fail_to_downgrade", 3);
  // ★ PATCH 2026-05-13:
  declare_parameter<int>("lte_consec_ok_to_stabilize", 1);
  declare_parameter<bool>("use_stub_probe", false);
}

void CommLinkNode::loadParameters()
{
  probe_host_ = get_parameter("wifi6_probe_host").as_string();
  probe_port_ = static_cast<int>(get_parameter("wifi6_probe_port").as_int());
  probe_timeout_ms_ = static_cast<int>(get_parameter("wifi6_probe_timeout_ms").as_int());
  use_stub_probe_ = get_parameter("use_stub_probe").as_bool();

  LinkHysteresisConfig cfg;
  cfg.consec_ok_to_upgrade =
    static_cast<uint16_t>(get_parameter("consec_ok_to_upgrade").as_int());
  cfg.consec_fail_to_downgrade =
    static_cast<uint16_t>(get_parameter("consec_fail_to_downgrade").as_int());
  cfg.lte_consec_ok_to_stabilize =
    static_cast<uint16_t>(get_parameter("lte_consec_ok_to_stabilize").as_int());
  monitor_.reconfigure(cfg);
}

void CommLinkNode::onLteStatus(const LteStatusMsg::SharedPtr msg)
{
  const bool registered =
    (msg->registered == LteStatusMsg::REG_HOME ||
    msg->registered == LteStatusMsg::REG_ROAMING);
  last_lte_ok_ = registered && msg->pdp_active;
}

void CommLinkNode::onTick()
{
  const bool wifi_ok = probe_->probe(
    probe_host_, probe_port_,
    std::chrono::milliseconds(probe_timeout_ms_));
  const bool lte_ok = last_lte_ok_.load();
  auto decision = monitor_.update({wifi_ok, lte_ok});

  // PATCH 2026-05-13 (M8): protect last_switch_* under lock.
  std::string published_reason;
  uint64_t published_ts_ms = 0;
  {
    std::lock_guard<std::mutex> g(switch_mu_);
    if (decision.switch_event) {
      last_switch_ms_ =
        static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
      last_switch_reason_ = decision.reason;
      RCLCPP_INFO(
        get_logger(), "link switch: %s",
        decision.reason.c_str());
    }
    published_reason = last_switch_reason_;
    published_ts_ms = last_switch_ms_;
  }

  LinkStatusMsg out;
  out.header.stamp = now();
  out.header.frame_id = "comm_link";
  out.active_link =
    static_cast<uint8_t>(decision.active_link);
  out.wifi6_ok = wifi_ok;
  out.lte_ok = lte_ok;
  out.wifi6_consec_ok_count = monitor_.consecOk();
  out.wifi6_consec_fail_count = monitor_.consecFail();
  out.switch_count = monitor_.switchCount();
  out.last_switch_timestamp_ms = published_ts_ms;
  out.last_switch_reason = published_reason;
  status_pub_->publish(out);
}

}  // namespace san_comm_link
