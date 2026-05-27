// SAN v1.5 Phase 2-E Turn 8 — CommUplinkNode.
//
// Replaces comm/comm_process.py per DCN-2026-002 D-007/D-008.
//
// Responsibilities:
//   1. Probe WiFi6 reachability periodically
//   2. Subscribe to LTE status from /lte_modem_node/modem_status
//   3. Run LinkSelector → publish current active link on ~/active_link
//   4. Subscribe to anomaly/telemetry topics and forward via the
//      current link (HTTP POST — actual network IO abstracted)
//
// HTTP backend is abstracted (HttpUploaderInterface). Tests inject a
// mock; CI build uses StubHttpUploader (logs only).

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

#include <combat_robot_msgs/msg/lte_modem_status.hpp>

#include "san_comm/link_selector.hpp"

namespace san_comm {

/// HTTP upload abstraction. Implementations: real (libcurl/urllib),
/// stub (logging only), mock (test).
class HttpUploaderInterface {
public:
  virtual ~HttpUploaderInterface() = default;
  /// POST `body` (UTF-8 JSON or bytes) to `url`. Returns HTTP status
  /// code on success, -1 on network error.
  virtual int post(const std::string& url,
                    const std::vector<uint8_t>& body) = 0;
};
std::unique_ptr<HttpUploaderInterface> makeRealHttpUploader();

/// WiFi6 reachability probe. Implementations: TCP connect, or stub.
class ReachabilityProbeInterface {
public:
  virtual ~ReachabilityProbeInterface() = default;
  virtual bool probe(const std::string& host, int port,
                      int timeout_ms) = 0;
};
std::unique_ptr<ReachabilityProbeInterface> makeRealProbe();

class CommUplinkNode : public rclcpp::Node {
public:
  explicit CommUplinkNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());
  CommUplinkNode(
      const rclcpp::NodeOptions& opts,
      std::unique_ptr<HttpUploaderInterface> uploader,
      std::unique_ptr<ReachabilityProbeInterface> probe);

private:
  void declareParameters();
  void loadParameters();
  void onProbeTick();
  void onHealthTick();
  void onLteStatus(combat_robot_msgs::msg::LteModemStatus::SharedPtr msg);
  void onTelemetry(std_msgs::msg::String::SharedPtr msg);

  std::unique_ptr<HttpUploaderInterface>       uploader_;
  std::unique_ptr<ReachabilityProbeInterface>  probe_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr active_link_pub_;
  rclcpp::Subscription<combat_robot_msgs::msg::LteModemStatus>::SharedPtr
      lte_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr telemetry_sub_;
  rclcpp::TimerBase::SharedPtr probe_timer_;
  rclcpp::TimerBase::SharedPtr health_timer_;

  // Params
  std::string server_url_;
  std::string probe_host_;
  int         probe_port_;
  int         probe_timeout_ms_;
  double      probe_rate_hz_;
  uint32_t    wifi_recovery_threshold_;

  // Latest probe inputs (thread-safe; mutex protects all three fields)
  std::mutex   state_mutex_;
  bool         last_wifi6_reachable_ = false;
  bool         last_lte_registered_  = false;
  bool         last_lte_pdp_active_  = false;

  LinkSelector selector_;

  // Stats
  std::atomic<uint32_t> upload_ok_count_{0};
  std::atomic<uint32_t> upload_fail_count_{0};
};

}  // namespace san_comm

#endif  // SAN_COMM__COMM_UPLINK_NODE_HPP_
