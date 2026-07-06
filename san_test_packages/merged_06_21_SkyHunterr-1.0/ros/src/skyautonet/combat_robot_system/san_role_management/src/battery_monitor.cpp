// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_role_management/battery_monitor.hpp"

namespace san_role_management
{

void BatteryMonitor::update(const BatterySnapshot & snap)
{
  std::lock_guard<std::mutex> lock(mutex_);
  snapshots_[snap.robot_id] = snap;
}

BatterySnapshot BatteryMonitor::get(uint32_t robot_id) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = snapshots_.find(robot_id);
  if (it == snapshots_.end()) {return BatterySnapshot{};}
  return it->second;
}

bool BatteryMonitor::has(uint32_t robot_id) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshots_.find(robot_id) != snapshots_.end();
}

void BatteryMonitor::clear()
{
  std::lock_guard<std::mutex> lock(mutex_);
  snapshots_.clear();
}

std::size_t BatteryMonitor::size() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshots_.size();
}

uint32_t BatteryMonitor::pickMaxBatteryFollower(
  uint32_t leader_id, uint32_t hub_id, uint32_t deputy_id,
  float min_battery) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  uint32_t best_id = 0;
  float best_battery = -1.0f;

  for (const auto & [id, snap] : snapshots_) {
    if (id == leader_id) {continue;}
    if (id == hub_id) {continue;}
    if (id == deputy_id) {continue;}
    if (!snap.sbc1_healthy) {continue;}
    if (snap.battery_percent < min_battery) {continue;}

    if (snap.battery_percent > best_battery ||
      (snap.battery_percent == best_battery &&
      (best_id == 0 || id < best_id)))
    {
      best_battery = snap.battery_percent;
      best_id = id;
    }
  }
  return best_id;
}

bool BatteryMonitor::isHubFailed(uint32_t hub_id) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = snapshots_.find(hub_id);
  if (it == snapshots_.end()) {return true;}
  return !it->second.sbc1_healthy && !it->second.sbc2_healthy;
}

bool BatteryMonitor::isDeputyFailed(uint32_t deputy_id) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = snapshots_.find(deputy_id);
  if (it == snapshots_.end()) {return true;}
  return !it->second.sbc1_healthy && !it->second.sbc2_healthy;
}

}  // namespace san_role_management
