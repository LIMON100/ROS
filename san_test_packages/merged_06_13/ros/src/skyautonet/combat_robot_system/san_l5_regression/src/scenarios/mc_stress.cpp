// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-020] McStressScenario implementation.

#include "san_l5_regression/scenarios/mc_stress.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>

#include <boost/crc.hpp>

namespace san_l5_regression
{

using namespace std::chrono_literals;

McStressScenario::McStressScenario(
  rclcpp::Node & host, const McStressConfig & cfg)
: host_(host), cfg_(cfg)
{
  // Audit B15 (P2) — cfg.rng_seed != 0 → deterministic replay;
  // 0 (default) keeps the non-reproducible random_device behavior
  // for production stress runs.
  if (cfg_.rng_seed != 0) {
    rng_.seed(cfg_.rng_seed);
  } else {
    std::random_device rd;
    rng_.seed(rd());
  }

  cmd_pub_ = host_.create_publisher<combat_robot_msgs::msg::MCMessage>(
    "/mc/command", rclcpp::QoS(1000).reliable());
  ack_sub_ = host_.create_subscription<combat_robot_msgs::msg::MCAck>(
    "/mc/ack", rclcpp::QoS(1000).reliable(),
    std::bind(
      &McStressScenario::onAck, this,
      std::placeholders::_1));
}

ScenarioReport McStressScenario::run()
{
  ScenarioReport rep;
  rep.id = "DCN-2026-020";
  rep.description = "MC stress — 1 kHz with drop/dup/reorder injection";
  rep.deadline_ms = cfg_.duration_sec * 1000;

  const auto period =
    std::chrono::microseconds(1000000 / cfg_.rate_hz);
  pub_timer_ = host_.create_wall_timer(
    period, std::bind(&McStressScenario::publishNext, this));

  deadline_timer_ = host_.create_wall_timer(
    std::chrono::seconds(cfg_.duration_sec),
    [this]() {finished_ = true;});

  const auto start = std::chrono::steady_clock::now();
  const auto hard_deadline = start +
    std::chrono::seconds(cfg_.duration_sec + 5);
  while (!finished_ && rclcpp::ok()) {
    rclcpp::spin_some(host_.get_node_base_interface());
    if (std::chrono::steady_clock::now() > hard_deadline) {
      rep.recordFail("hard deadline exceeded (duration + 5s)");
      return rep;
    }
  }

  // Audit B14 (P3): drain any messages still parked in the reorder
  // queue at suite end. Without this the last ~5 reorder samples
  // never publish + never get RTT measured → small bias in p99.
  for (const auto & m : reorder_queue_) {
    cmd_pub_->publish(m);
  }
  reorder_queue_.clear();

  // Brief settling spin so the freshly-published reorder tail's
  // acks have a chance to arrive before summarize() runs.
  const auto settle = std::chrono::steady_clock::now() + 500ms;
  while (std::chrono::steady_clock::now() < settle && rclcpp::ok()) {
    rclcpp::spin_some(host_.get_node_base_interface());
  }

  flushCsv();
  summarize(rep);
  return rep;
}

void McStressScenario::publishNext()
{
  std::uniform_real_distribution<> roll(0.0, 1.0);
  const double r = roll(rng_);

  combat_robot_msgs::msg::MCMessage msg;
  msg.stamp = host_.now();
  msg.seq = next_seq_++;
  msg.command = "STRESS";
  msg.payload = "{}";
  msg.crc32_checksum = computeCrc32(msg.command + msg.payload);

  ++sent_count_;

  if (r < cfg_.drop_pct) {
    // Simulated drop — record sent_count but skip publish so no ack
    // arrives → exercises the receiver's lossy-link tolerance.
    return;
  } else if (r < cfg_.drop_pct + cfg_.dup_pct) {
    cmd_pub_->publish(msg);
    cmd_pub_->publish(msg);       // duplicate → receiver should reply DUPLICATE
  } else if (r < cfg_.drop_pct + cfg_.dup_pct + cfg_.reorder_pct) {
    reorder_queue_.push_back(msg);
    if (reorder_queue_.size() > 5) {
      cmd_pub_->publish(reorder_queue_.front());
      reorder_queue_.erase(reorder_queue_.begin());
    }
  } else {
    cmd_pub_->publish(msg);
  }
  sent_times_[msg.seq] = msg.stamp;
}

void McStressScenario::onAck(
  combat_robot_msgs::msg::MCAck::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  ++outcomes_[msg->outcome];
  const auto it = sent_times_.find(msg->seq);
  if (it != sent_times_.end()) {
    const int64_t rtt_us =
      (host_.now() - it->second).nanoseconds() / 1000;
    rtt_samples_us_.push_back(rtt_us);
  }
}

void McStressScenario::flushCsv()
{
  std::ofstream csv(cfg_.csv_path);
  if (!csv.is_open()) {return;}
  csv << "rtt_us\n";
  for (auto x : rtt_samples_us_) {
    csv << x << "\n";
  }
}

void McStressScenario::summarize(ScenarioReport & rep)
{
  rep.attributes["sent_count"] = std::to_string(sent_count_);
  rep.attributes["ack_count"] = std::to_string(rtt_samples_us_.size());
  for (const auto & [outcome, n] : outcomes_) {
    rep.attributes["outcome_" + outcome] = std::to_string(n);
  }

  if (rtt_samples_us_.empty()) {
    rep.recordFail("No ACKs received");
    return;
  }

  std::sort(rtt_samples_us_.begin(), rtt_samples_us_.end());
  auto pct = [this](double p) {
      const size_t idx = std::min<size_t>(
        rtt_samples_us_.size() - 1,
        static_cast<size_t>(rtt_samples_us_.size() * p));
      return rtt_samples_us_[idx];
    };
  const int64_t p50 = pct(0.50);
  const int64_t p95 = pct(0.95);
  const int64_t p99 = pct(0.99);

  rep.attributes["p50_us"] = std::to_string(p50);
  rep.attributes["p95_us"] = std::to_string(p95);
  rep.attributes["p99_us"] = std::to_string(p99);

  char buf[256];
  snprintf(
    buf, sizeof(buf),
    "sent=%u acks=%zu p50=%ld p95=%ld p99=%ld (target=%ld) us",
    sent_count_, rtt_samples_us_.size(),
    static_cast<long>(p50), static_cast<long>(p95),
    static_cast<long>(p99), static_cast<long>(cfg_.p99_target_us));

  if (p99 < cfg_.p99_target_us) {
    rep.recordPass(0);
    rep.fail_reason = buf;       // re-use as info string
  } else {
    rep.recordFail(
      std::string("p99 ") + std::to_string(p99) +
      " us exceeds target " +
      std::to_string(cfg_.p99_target_us) + " us [" +
      buf + "]");
  }
}

uint32_t McStressScenario::computeCrc32(const std::string & s)
{
  boost::crc_32_type crc;
  crc.process_bytes(s.data(), s.size());
  return crc.checksum();
}

}  // namespace san_l5_regression
