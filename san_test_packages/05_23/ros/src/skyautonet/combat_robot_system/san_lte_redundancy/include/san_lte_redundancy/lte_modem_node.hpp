// SAN v1.5 Phase 2-E Turn 3 — LteModemNode rclcpp Node.
//
// Polls the modem control port (AT commands) at ~0.5 Hz and publishes
// LteModemStatus. Replaces adapters/lte_modem.py per DCN-2026-002 D-007
// (Tier 1 C++ for HW drivers) + D-008 (ROS 2 IPC).
//
// Test ctor accepts an injected AtCommandInterface so gtest can use
// MockAtCommand without a real serial port (same pattern as Turn 2
// UnitreeGo2Node + Go2SdkInterface).

#ifndef SAN_LTE_REDUNDANCY__LTE_MODEM_NODE_HPP_
#define SAN_LTE_REDUNDANCY__LTE_MODEM_NODE_HPP_

#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/lte_modem_status.hpp>

#include "san_lte_redundancy/at_command_interface.hpp"

namespace san_lte_redundancy {

class LteModemNode : public rclcpp::Node {
public:
  /// Production ctor — auto-instantiates RealAtCommand.
  explicit LteModemNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());

  /// Test ctor — caller injects a mock AT command port.
  LteModemNode(
      const rclcpp::NodeOptions& opts,
      std::unique_ptr<AtCommandInterface> at);

private:
  void declareParameters();
  void loadParameters();
  void initializeAtPort();
  void onPollTick();

  void publishCurrent();
  void publishStub();
  void resetState();

  // Members
  std::unique_ptr<AtCommandInterface> at_;
  rclcpp::Publisher<combat_robot_msgs::msg::LteModemStatus>::SharedPtr
      status_pub_;
  rclcpp::TimerBase::SharedPtr poll_timer_;

  // Parameters
  std::string at_device_;
  int         baud_;
  double      poll_hz_;
  bool        stub_on_no_modem_;

  // State carried across polls — not every AT response arrives every cycle.
  combat_robot_msgs::msg::LteModemStatus state_;
  uint32_t  seq_           = 0;
  bool      stub_mode_     = false;
  uint32_t  dropped_count_ = 0;
};

}  // namespace san_lte_redundancy

#endif  // SAN_LTE_REDUNDANCY__LTE_MODEM_NODE_HPP_
