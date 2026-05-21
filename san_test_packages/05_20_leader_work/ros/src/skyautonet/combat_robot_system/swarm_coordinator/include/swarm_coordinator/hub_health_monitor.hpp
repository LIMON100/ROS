// SAN v1.3 PHASE 4 — Hub UGV dual-SBC health monitor.
//
// Subscribes to /swarm/robot_status, tracks the most recent
// sbc1_healthy / sbc2_healthy flags from the Hub UGV (robot_id == 2),
// and exposes:
//   * hubExcludedFromLeaderChain() — true when BOTH SBCs are down
//   * isHubSlamSbcAvailable() / isHubCommSbcAvailable() — per-SBC flags
//
// Test seam: injectStatusForTest() so unit tests can drive the three
// failure scenarios from SAN-TST-INT-001 v1.3 §S15-3:
//   Case A — SBC #1 down only (SLAM 정전, comm OK)
//   Case B — SBC #2 down only (LTE backup 활성, SLAM OK)
//   Case C — 양쪽 SBC down (Hub UGV 전체 손상; deputy_chain 에서 제외)

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>

namespace swarm_coordinator {

enum class HubHealthCase : uint8_t {
    NORMAL     = 0,   // both SBCs healthy
    CASE_A     = 1,   // SBC #1 down (SLAM 정전)
    CASE_B     = 2,   // SBC #2 down (LTE backup needed)
    CASE_C     = 3,   // 양쪽 down (excluded from leader chain)
    UNKNOWN    = 4,   // no recent heartbeat
};

class HubHealthMonitor {
public:
    explicit HubHealthMonitor(uint32_t hub_robot_id = 2,
                               int stale_threshold_ms = 3000);

    // Update from a RobotStatus broadcast.
    void update(const combat_robot_msgs::msg::RobotStatus& status,
                 uint64_t now_ms);

    // Accessors.
    bool isHubSlamSbcAvailable(uint64_t now_ms) const;
    bool isHubCommSbcAvailable(uint64_t now_ms) const;
    bool hubExcludedFromLeaderChain(uint64_t now_ms) const;
    HubHealthCase classify(uint64_t now_ms) const;

    // Test entry point - inject without a real ROS subscription.
    void injectStatusForTest(bool sbc1, bool sbc2, uint64_t now_ms);

private:
    uint32_t hub_robot_id_;
    int stale_threshold_ms_;
    mutable std::mutex mutex_;
    bool sbc1_healthy_ = false;
    bool sbc2_healthy_ = false;
    uint64_t last_heartbeat_ms_ = 0;

    bool isFresh(uint64_t now_ms) const;
};

}  // namespace swarm_coordinator
