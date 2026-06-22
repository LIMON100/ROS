// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — CommUplinkNode (PATCHED 2026-05-13).
//
// Replaces comm/comm_process.py per DCN-2026-002 D-007/D-008.
//
// PATCH 2026-05-13 (comm deep-dive review):
//   * `use_external_link_status` parameter (default true) makes the
//     node SUBSCRIBE to san_comm_link's CommLinkStatus instead of
//     running its own LinkSelector. This is the canonical mode per
//     SDD-SWARM v1.5 §10. Legacy self-selecting mode preserved for
//     backwards compatibility.
//   * Stub probe / uploader still available but require explicit
//     opt-in via `use_stub_backend` parameter; production default is
//     real TCP probe + libcurl-ready HTTP path (HTTP impl deferred
//     to CDR — see PATCH_NOTES).

#ifndef SAN_COMM__COMM_UPLINK_NODE_HPP_
#define SAN_COMM__COMM_UPLINK_NODE_HPP_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include <combat_robot_msgs/msg/comm_link_status.hpp>
#include <combat_robot_msgs/msg/lte_modem_status.hpp>

#include "san_comm/link_selector.hpp"

namespace san_comm
{

class HttpUploaderInterface
{
public:
  virtual ~HttpUploaderInterface() = default;
  virtual int post(
    const std::string & url,
    const std::vector<uint8_t> & body) = 0;
};
std::unique_ptr<HttpUploaderInterface> makeRealHttpUploader();
std::unique_ptr<HttpUploaderInterface> makeStubHttpUploader();

class ReachabilityProbeInterface
{
public:
  virtual ~ReachabilityProbeInterface() = default;
  virtual bool probe(
    const std::string & host, int port,
    int timeout_ms) = 0;
};
std::unique_ptr<ReachabilityProbeInterface> makeRealProbe();
std::unique_ptr<ReachabilityProbeInterface> makeStubProbe();

class CommUplinkNode : public rclcpp::Node
{
public:
  explicit CommUplinkNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());
  CommUplinkNode(
    const rclcpp::NodeOptions & opts,
    std::unique_ptr<HttpUploaderInterface> uploader,
    std::unique_ptr<ReachabilityProbeInterface> probe);

private:
  void declareParameters();
  void loadParameters();
  void onProbeTick();
  void onHealthTick();
  void onLteStatus(combat_robot_msgs::msg::LteModemStatus::SharedPtr msg);
  void onTelemetry(std_msgs::msg::String::SharedPtr msg);
  // ★ PATCH 2026-05-13: CommLinkStatus consumer mode.
  void onCommLinkStatus(
    combat_robot_msgs::msg::CommLinkStatus::SharedPtr msg);

  std::unique_ptr<HttpUploaderInterface> uploader_;
  std::unique_ptr<ReachabilityProbeInterface> probe_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr active_link_pub_;
  rclcpp::Subscription<combat_robot_msgs::msg::LteModemStatus>::SharedPtr
    lte_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::CommLinkStatus>::SharedPtr
    comm_link_sub_;     // ★ PATCH 2026-05-13
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr telemetry_sub_;
  rclcpp::TimerBase::SharedPtr probe_timer_;
  rclcpp::TimerBase::SharedPtr health_timer_;

  // Params
  std::string server_url_;
  std::string probe_host_;
  int probe_port_;
  int probe_timeout_ms_;
  double probe_rate_hz_;
  uint32_t wifi_recovery_threshold_;
  bool use_external_link_status_;          // ★ PATCH 2026-05-13
  bool use_stub_backend_;                  // ★ PATCH 2026-05-13

  // Latest probe inputs (legacy mode only).
  std::mutex state_mutex_;
  bool last_wifi6_reachable_ = false;
  bool last_lte_registered_ = false;
  bool last_lte_pdp_active_ = false;

  // ★ PATCH 2026-05-13: external CommLinkStatus snapshot (mode = true).
  std::atomic<uint8_t> external_active_link_{0};   // 0=None/1=Wifi6/2=Lte
  std::atomic<uint64_t> external_last_update_ms_{0};

  // Legacy self-selecting selector (kept for backward-compat).
  // Wrapped via deprecated typedef to suppress the warning here only.
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  LinkSelector selector_;
  #pragma GCC diagnostic pop

  // Stats
  std::atomic<uint32_t> upload_ok_count_{0};
  std::atomic<uint32_t> upload_fail_count_{0};
};

}  // namespace san_comm

#endif  // SAN_COMM__COMM_UPLINK_NODE_HPP_
