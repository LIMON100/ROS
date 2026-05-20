#include "swarm_coordinator/hub_health_monitor.hpp"

namespace swarm_coordinator {

HubHealthMonitor::HubHealthMonitor(uint32_t hub_robot_id,
                                    int stale_threshold_ms)
    : hub_robot_id_(hub_robot_id),
      stale_threshold_ms_(stale_threshold_ms)
{}

void HubHealthMonitor::update(
    const combat_robot_msgs::msg::RobotStatus& status,
    uint64_t now_ms)
{
    if (status.robot_id != hub_robot_id_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    sbc1_healthy_ = status.sbc1_healthy;
    sbc2_healthy_ = status.sbc2_healthy;
    last_heartbeat_ms_ = now_ms;
}

void HubHealthMonitor::injectStatusForTest(bool sbc1, bool sbc2,
                                            uint64_t now_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sbc1_healthy_ = sbc1;
    sbc2_healthy_ = sbc2;
    last_heartbeat_ms_ = now_ms;
}

bool HubHealthMonitor::isFresh(uint64_t now_ms) const {
    if (last_heartbeat_ms_ == 0) return false;
    if (now_ms < last_heartbeat_ms_) return true;
    return (now_ms - last_heartbeat_ms_)
        < static_cast<uint64_t>(stale_threshold_ms_);
}

bool HubHealthMonitor::isHubSlamSbcAvailable(uint64_t now_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isFresh(now_ms)) return false;
    return sbc1_healthy_;
}

bool HubHealthMonitor::isHubCommSbcAvailable(uint64_t now_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isFresh(now_ms)) return false;
    return sbc2_healthy_;
}

bool HubHealthMonitor::hubExcludedFromLeaderChain(uint64_t now_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isFresh(now_ms)) {
        // No heartbeat from Hub means it's effectively offline -
        // exclude from chain.
        return true;
    }
    return !sbc1_healthy_ && !sbc2_healthy_;
}

HubHealthCase HubHealthMonitor::classify(uint64_t now_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isFresh(now_ms)) return HubHealthCase::UNKNOWN;
    if (sbc1_healthy_ && sbc2_healthy_) return HubHealthCase::NORMAL;
    if (!sbc1_healthy_ && sbc2_healthy_) return HubHealthCase::CASE_A;
    if (sbc1_healthy_ && !sbc2_healthy_) return HubHealthCase::CASE_B;
    return HubHealthCase::CASE_C;
}

}  // namespace swarm_coordinator
