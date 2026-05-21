// SAN v1.5 Phase 2-E Turn 8 — CommLinkNode.
//
// Wraps LinkHealthMonitor with a WiFi6 probe + LTE status subscription
// and publishes CommLinkStatus at 1 Hz. Replaces comm/comm_process.py
// link-selection logic.
//
// WiFi6 probe abstraction (Wifi6ProbeInterface) is injected so tests
// can drive the state machine without real network.

#ifndef SAN_COMM_LINK__COMM_LINK_NODE_HPP_
#define SAN_COMM_LINK__COMM_LINK_NODE_HPP_

#include <atomic>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/comm_link_status.hpp>
#include <combat_robot_msgs/msg/lte_modem_status.hpp>

#include "san_comm_link/link_health_monitor.hpp"

namespace san_comm_link {

class Wifi6ProbeInterface {
public:
  virtual ~Wifi6ProbeInterface() = default;
  /// Try a TCP probe to (host, port). Returns true on success.
  virtual bool probe(const std::string& host, int port,
                      std::chrono::milliseconds timeout) = 0;
};
std::unique_ptr<Wifi6ProbeInterface> makeRealWifi6Probe();

class CommLinkNode : public rclcpp::Node {
public:
  explicit CommLinkNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());
  CommLinkNode(const rclcpp::NodeOptions& opts,
                std::unique_ptr<Wifi6ProbeInterface> probe);

private:
  void declareParameters();
  void loadParameters();
  void onTick();
  void onLteStatus(
      const combat_robot_msgs::msg::LteModemStatus::SharedPtr msg);

  std::unique_ptr<Wifi6ProbeInterface> probe_;
  LinkHealthMonitor                     monitor_;

  rclcpp::Publisher<combat_robot_msgs::msg::CommLinkStatus>::SharedPtr status_pub_;
  rclcpp::Subscription<combat_robot_msgs::msg::LteModemStatus>::SharedPtr lte_sub_;
  rclcpp::TimerBase::SharedPtr tick_timer_;

  std::string  probe_host_;
  int          probe_port_;
  int          probe_timeout_ms_;
  std::atomic<bool> last_lte_ok_{false};
  uint64_t     last_switch_ms_ = 0;
  std::string  last_switch_reason_;
};

}  // namespace san_comm_link

#endif  // SAN_COMM_LINK__COMM_LINK_NODE_HPP_
