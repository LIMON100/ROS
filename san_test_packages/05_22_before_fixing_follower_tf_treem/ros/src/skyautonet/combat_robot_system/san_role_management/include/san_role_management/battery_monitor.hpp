// SAN v1.4 PHASE 8 - battery monitor for 3순위 (battery-max) selection.
//
// Aggregates the most-recent battery_percent from each robot's
// RobotStatus broadcast. LeaderRoleManager queries
// pickMaxBatteryFollower() to compute the 3rd-tier candidate.

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace san_role_management {

struct BatterySnapshot {
    uint32_t robot_id = 0;
    float    battery_percent = 0.0f;
    bool     sbc1_healthy = false;
    bool     sbc2_healthy = false;
    bool     is_deputy_ugv = false;
    bool     is_hub_ugv = false;
    uint64_t timestamp_ms = 0;
};

class BatteryMonitor {
public:
    BatteryMonitor() = default;

    // Update or insert a robot's latest snapshot.
    void update(const BatterySnapshot& snap);

    // Returns the snapshot for robot_id if present.
    BatterySnapshot get(uint32_t robot_id) const;
    bool has(uint32_t robot_id) const;

    // Lifecycle helpers.
    void clear();
    std::size_t size() const;

    // Returns the follower robot_id with the maximum battery_percent
    // above MIN_BATTERY_FOLLOWER. Excludes Leader, Hub, Deputy. On a
    // tie, the lower robot_id wins (deterministic).
    // Returns 0 when nobody qualifies.
    uint32_t pickMaxBatteryFollower(
        uint32_t leader_id, uint32_t hub_id, uint32_t deputy_id,
        float min_battery) const;

    // True when Hub UGV (robot_id == hub_id) is unhealthy (missing or
    // both SBCs failed).
    bool isHubFailed(uint32_t hub_id) const;

    // True when Deputy UGV (robot_id == deputy_id) is unhealthy.
    bool isDeputyFailed(uint32_t deputy_id) const;

private:
    mutable std::mutex mutex_;
    std::map<uint32_t, BatterySnapshot> snapshots_;
};

}  // namespace san_role_management
