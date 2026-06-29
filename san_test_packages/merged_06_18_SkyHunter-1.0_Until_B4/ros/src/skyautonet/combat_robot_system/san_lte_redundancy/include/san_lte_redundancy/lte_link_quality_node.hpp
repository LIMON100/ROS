// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5b - LTE link-quality publisher.
//
// Reads a hardware-agnostic key=value status file (written by an
// external ModemManager / qmicli / mmcli poller running outside this
// process), grades the RSRP, and publishes /lte/link_quality at 1 Hz.
//
// Why a status file instead of a direct D-Bus client here? The dual
// SBC layout (PHASE 4) already runs ModemManager as the OS-level
// modem owner; duplicating its bus client inside this node would
// race against systemd-modemmanager.service for the same modem lock.
// The status file is the cleanest hand-off that keeps this ROS2
// process out of the modem control loop.
//
// Status file format (one key=value per line):
//   rsrp_dbm=-95
//   rsrq_db=-10
//   sinr_db=15
//
// Missing file or unparseable contents → LTE_GRADE_UNKNOWN.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/lte_link_quality.hpp>

#include <atomic>
#include <mutex>
#include <string>

#include "san_lte_redundancy/lte_link_quality_grader.hpp"

namespace san_lte_redundancy
{

class LteLinkQualityNode : public rclcpp::Node
{
public:
  LteLinkQualityNode();
  explicit LteLinkQualityNode(const rclcpp::NodeOptions & options);

  // Test seam: pre-populate the next tick's signal reading without
  // touching the filesystem. Subsequent ticks reuse the injected value
  // until clearInjected() is called.
  void injectForTest(const LteSignalRaw & s);
  void clearInjected();

  // Test entry point: run one publish cycle synchronously.
  combat_robot_msgs::msg::LteLinkQuality tickForTest();

  // Parse a status-file blob (used by tests and by the timer body).
  static LteSignalRaw parseStatusBlob(const std::string & blob);

private:
  void declareParameters();
  void readParameters();
  void wireInterfaces();

  void onTimer();

  LteSignalRaw readStatusFile() const;
  combat_robot_msgs::msg::LteLinkQuality buildMessage(const LteSignalRaw & raw);

  // Parameters.
  // [DCN-2026-006 EXT cleanup §5.1] Explicit `{}` is visually
  // unambiguous about default-empty intent; std::string already
  // default-constructs to empty so behaviour is unchanged.
  std::string status_file_path_{};
  std::string source_iface_{};
  int publish_period_ms_ = 1000;

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<combat_robot_msgs::msg::LteLinkQuality>::SharedPtr pub_;

  mutable std::mutex inject_mu_;
  bool has_injected_ = false;
  LteSignalRaw injected_{};

  // [DCN-2026-006 EXT D-020] Stateful grader — applies 2 dB
  // hysteresis on upgrades to suppress GOOD↔FAIR chatter at the
  // -100 dBm cliff. State is one byte; safe on the timer thread.
  StatefulLteLinkQualityGrader stateful_grader_;
};

}  // namespace san_lte_redundancy
