// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — CommLinkNode (PATCHED 2026-05-13).
//
// Wraps LinkHealthMonitor with a WiFi6 probe + LTE status subscription
// and publishes CommLinkStatus at 1 Hz. SINGLE SOURCE OF TRUTH for
// active link decision per SDD-SWARM v1.5 §10.
//
// PATCH 2026-05-13 (comm deep-dive review):
//   * makeRealWifi6Probe() now does a REAL TCP probe (was stub).
//     The probe opens a non-blocking TCP socket to the configured
//     host:port and uses select() with timeout. No external libraries.
//   * Stub probe (now `StubWifi6Probe`, still available for tests)
//     is no longer used as the production fallback — that hid the
//     "no real network" warning behind RCLCPP_INFO and made every
//     deployment silently pick LTE.
//   * Last-switch metadata (timestamp + reason) protected by mutex
//     so the publish path and accessors are coherent.

#ifndef SAN_COMM_LINK__COMM_LINK_NODE_HPP_
#define SAN_COMM_LINK__COMM_LINK_NODE_HPP_

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/comm_link_status.hpp>
#include <combat_robot_msgs/msg/lte_modem_status.hpp>

#include "san_comm_link/link_health_monitor.hpp"

namespace san_comm_link
{

class Wifi6ProbeInterface
{
public:
  virtual ~Wifi6ProbeInterface() = default;
  /// Try a TCP probe to (host, port). Returns true on success.
  virtual bool probe(
    const std::string & host, int port,
    std::chrono::milliseconds timeout) = 0;
};

/// ★ PATCH 2026-05-13 (C4): real TCP probe.
/// Performs a non-blocking ::connect with select() timeout. No
/// external dependency (POSIX sockets only).
std::unique_ptr<Wifi6ProbeInterface> makeRealWifi6Probe();

/// Stub for tests + simulators. Always returns false.
std::unique_ptr<Wifi6ProbeInterface> makeStubWifi6Probe();

class CommLinkNode : public rclcpp::Node
{
public:
  explicit CommLinkNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());
  CommLinkNode(
    const rclcpp::NodeOptions & opts,
    std::unique_ptr<Wifi6ProbeInterface> probe);

private:
  void declareParameters();
  void loadParameters();
  void onTick();
  void onLteStatus(
    const combat_robot_msgs::msg::LteModemStatus::SharedPtr msg);

  std::unique_ptr<Wifi6ProbeInterface> probe_;
  LinkHealthMonitor monitor_;

  rclcpp::Publisher<combat_robot_msgs::msg::CommLinkStatus>::SharedPtr status_pub_;
  rclcpp::Subscription<combat_robot_msgs::msg::LteModemStatus>::SharedPtr lte_sub_;
  rclcpp::TimerBase::SharedPtr tick_timer_;

  std::string probe_host_;
  int probe_port_;
  int probe_timeout_ms_;
  bool use_stub_probe_ = false;               // ★ PATCH 2026-05-13

  std::atomic<bool> last_lte_ok_{false};

  // ★ PATCH 2026-05-13 (M8): protected by switch_mu_
  mutable std::mutex switch_mu_;
  uint64_t last_switch_ms_ = 0;
  std::string last_switch_reason_;
};

}  // namespace san_comm_link

#endif  // SAN_COMM_LINK__COMM_LINK_NODE_HPP_
