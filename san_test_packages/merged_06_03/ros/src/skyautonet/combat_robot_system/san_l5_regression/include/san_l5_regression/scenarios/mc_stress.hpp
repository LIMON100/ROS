// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-020] MC stress scenario — drives /mc/command at 1 kHz with
// synthetic drop / dup / reorder noise and measures round-trip latency
// against the live mc_protocol_node receiver.
//
// Runs as a standalone host node (not the full ScenarioRunner) so it
// can be dispatched via regression_main --scenario mc_stress without
// touching the S18 baseline machinery. Acceptance: p99 RTT < 50 ms
// (Gate-1 KPP).
//
// Depends on DCN-2026-019 (combat_robot_msgs/MCMessage + MCAck +
// mc_protocol_node).

#pragma once

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/mc_ack.hpp>
#include <combat_robot_msgs/msg/mc_message.hpp>

#include "san_l5_regression/scenario_report.hpp"

namespace san_l5_regression
{

struct McStressConfig
{
  int duration_sec = 30;
  int rate_hz = 1000;
  double drop_pct = 0.05;
  double dup_pct = 0.02;
  double reorder_pct = 0.03;
  int64_t p99_target_us = 50000;                 // Gate-1 KPP
  std::string csv_path = "/tmp/mc_stress.csv";
  // ─── Audit B15 (P2) — reproducible rng seed ─────────────────────
  // 0 (default) = use std::random_device → non-reproducible (real
  // stress run). Non-zero = use that as the mt19937 seed → exact
  // replay possible for debug / regression / CI determinism.
  uint32_t rng_seed = 0;
};

class McStressScenario
{
public:
  McStressScenario(rclcpp::Node & host, const McStressConfig & cfg);

  /// Runs the publish/measure loop for `duration_sec`, returns a
  /// fully populated ScenarioReport (id = "DCN-2026-020").
  ScenarioReport run();

  // ─── Test seams — drive the deterministic helpers without spinning
  // the ROS publish/sub side. summarize() lets unit tests verify the
  // p50/p95/p99 + pass/fail logic against synthetic RTT vectors.
  void setRttSamplesForTest(std::vector<int64_t> samples)
  {
    rtt_samples_us_ = std::move(samples);
  }
  void summarizeForTest(ScenarioReport & rep) {summarize(rep);}
  const std::vector<int64_t> & rttSamples() const {return rtt_samples_us_;}
  uint32_t sentCount() const {return sent_count_;}

private:
  void publishNext();
  void onAck(combat_robot_msgs::msg::MCAck::SharedPtr msg);
  void flushCsv();
  void summarize(ScenarioReport & rep);

  /// Compute crc32 (boost::crc_32_type) so the receiver
  /// (mc_protocol_node) emits OK for stamped messages — without it,
  /// every msg lands as INVALID_CHECKSUM and we measure nothing.
  static uint32_t computeCrc32(const std::string & s);

  rclcpp::Node & host_;
  McStressConfig cfg_;

  rclcpp::Publisher<combat_robot_msgs::msg::MCMessage>::SharedPtr cmd_pub_;
  rclcpp::Subscription<combat_robot_msgs::msg::MCAck>::SharedPtr ack_sub_;
  rclcpp::TimerBase::SharedPtr pub_timer_;
  rclcpp::TimerBase::SharedPtr deadline_timer_;

  std::map<uint32_t, rclcpp::Time> sent_times_;
  std::vector<int64_t> rtt_samples_us_;
  std::map<std::string, uint32_t> outcomes_;
  std::vector<combat_robot_msgs::msg::MCMessage> reorder_queue_;

  uint32_t next_seq_ = 1;
  uint32_t sent_count_ = 0;
  std::mt19937 rng_;
  bool finished_ = false;
};

}  // namespace san_l5_regression
