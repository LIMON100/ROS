// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "swarm_coordinator/hub_health_monitor.hpp"

namespace swarm_coordinator
{

HubHealthMonitor::HubHealthMonitor(
  uint32_t hub_robot_id,
  int stale_threshold_ms,
  FlappingPolicy policy)
: hub_robot_id_(hub_robot_id),
  stale_threshold_ms_(stale_threshold_ms),
  policy_(policy)
{}

bool HubHealthMonitor::applyHysteresis(
  bool current, bool incoming,
  uint32_t & bad_count,
  uint32_t & good_count) const
{
  if (incoming) {
    bad_count = 0;
    if (good_count < policy_.good_threshold) {++good_count;}
    // Flip false → true only after good_threshold consecutive trues.
    if (!current && good_count >= policy_.good_threshold) {
      return true;
    }
    return current;
  } else {
    good_count = 0;
    if (bad_count < policy_.bad_threshold) {++bad_count;}
    // Flip true → false only after bad_threshold consecutive falses.
    if (current && bad_count >= policy_.bad_threshold) {
      return false;
    }
    return current;
  }
}

void HubHealthMonitor::update(
  const combat_robot_msgs::msg::RobotStatus & status,
  uint64_t now_ms)
{
  if (status.robot_id != hub_robot_id_) {return;}
  std::lock_guard<std::mutex> lock(mutex_);

  // Phase 5: warm-start semantics. The very first heartbeat
  // establishes the baseline (no hysteresis lag at startup).
  // Subsequent heartbeats go through the flapping filter.
  if (!ever_seen_) {
    sbc1_healthy_ = status.sbc1_healthy;
    sbc2_healthy_ = status.sbc2_healthy;
  } else {
    sbc1_healthy_ = applyHysteresis(
      sbc1_healthy_, status.sbc1_healthy,
      sbc1_bad_count_, sbc1_good_count_);
    sbc2_healthy_ = applyHysteresis(
      sbc2_healthy_, status.sbc2_healthy,
      sbc2_bad_count_, sbc2_good_count_);
  }
  last_heartbeat_ms_ = now_ms;
  ever_seen_ = true;
}

void HubHealthMonitor::injectStatusForTest(
  bool sbc1, bool sbc2,
  uint64_t now_ms)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (!ever_seen_) {
    sbc1_healthy_ = sbc1;
    sbc2_healthy_ = sbc2;
  } else {
    sbc1_healthy_ = applyHysteresis(
      sbc1_healthy_, sbc1,
      sbc1_bad_count_, sbc1_good_count_);
    sbc2_healthy_ = applyHysteresis(
      sbc2_healthy_, sbc2,
      sbc2_bad_count_, sbc2_good_count_);
  }
  last_heartbeat_ms_ = now_ms;
  ever_seen_ = true;
}

bool HubHealthMonitor::isFresh(uint64_t now_ms) const
{
  if (last_heartbeat_ms_ == 0) {return false;}
  // Phase 5: a future-stamped heartbeat (clock skew / bag replay /
  // adversarial timestamp) used to be treated as fresh. Cap skew at
  // kHubMaxSkewMs; beyond that, treat as stale.
  if (now_ms < last_heartbeat_ms_) {
    const uint64_t future_skew = last_heartbeat_ms_ - now_ms;
    return future_skew <= static_cast<uint64_t>(kHubMaxSkewMs);
  }
  return (now_ms - last_heartbeat_ms_) <
         static_cast<uint64_t>(stale_threshold_ms_);
}

bool HubHealthMonitor::isHubSlamSbcAvailable(uint64_t now_ms) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (!isFresh(now_ms)) {return false;}
  return sbc1_healthy_;
}

bool HubHealthMonitor::isHubCommSbcAvailable(uint64_t now_ms) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (!isFresh(now_ms)) {return false;}
  return sbc2_healthy_;
}

bool HubHealthMonitor::hubExcludedFromLeaderChain(uint64_t now_ms) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (!isFresh(now_ms)) {
    // No heartbeat from Hub means it's effectively offline -
    // exclude from chain.
    return true;
  }
  return !sbc1_healthy_ && !sbc2_healthy_;
}

HubHealthCase HubHealthMonitor::classify(uint64_t now_ms) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (!isFresh(now_ms)) {return HubHealthCase::UNKNOWN;}
  if (sbc1_healthy_ && sbc2_healthy_) {return HubHealthCase::NORMAL;}
  if (!sbc1_healthy_ && sbc2_healthy_) {return HubHealthCase::CASE_A;}
  if (sbc1_healthy_ && !sbc2_healthy_) {return HubHealthCase::CASE_B;}
  return HubHealthCase::CASE_C;
}

bool HubHealthMonitor::hasEverSeenHub() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return ever_seen_;
}

}  // namespace swarm_coordinator
