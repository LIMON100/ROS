// SAN v1.4 L5 regression - synthetic failure injection.
//
// The L5 regression suite cannot kill containers or yank cables on a
// real 8-robot fleet, so failures are injected at the *signal* level:
// we publish synthetic RobotStatus / LteLinkQuality messages that the
// role managers and auto-rate controllers consume identically to the
// real telemetry path. This keeps the orchestrator HIL/real-robot
// agnostic and shell-free.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/lte_link_quality.hpp>

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace san_l5_regression {

struct RobotHealth {
    bool sbc1_healthy = true;
    bool sbc2_healthy = true;
    bool slam_healthy = true;
    bool perception_healthy = true;
    bool comm_healthy = true;
    bool lte_active = false;
    float battery_percent = 80.0f;
    bool is_deputy_ugv = false;
    bool is_hub_role_active = false;
    bool is_leader_role_active = false;
};

class FailureInjector {
public:
    explicit FailureInjector(rclcpp::Node* node);

    // Set the synthetic baseline for a robot. Subsequent publish() calls
    // will reflect the latest state for that robot_id.
    void setHealth(uint32_t robot_id, const RobotHealth& h);

    // Convenience mutators (preserve other fields).
    void killSbc(uint32_t robot_id, bool sbc1, bool sbc2);
    void setBattery(uint32_t robot_id, float percent);
    void markDeputy(uint32_t robot_id, bool is_deputy);
    void markLeader(uint32_t robot_id, bool is_leader);

    // Stop publishing entirely for `robot_id`. This is the correct
    // kill semantics for the watchdog-based role managers (PHASE 4
    // HubHealthMonitor::isFresh() and the PHASE 8 leader/hub watchdog
    // timers both treat missing heartbeats as failure). `restore` brings
    // the robot back into the publish set with its last known health.
    void removeRobot(uint32_t robot_id);
    void restoreRobot(uint32_t robot_id, const RobotHealth& h);

    bool isPublished(uint32_t robot_id) const;

    // One-shot publish for every tracked robot. Returns the count
    // published. Use this from the runner's tick loop.
    std::size_t publishAll();

    // Publish a single robot now (e.g. immediately after a mutator).
    void publishOne(uint32_t robot_id);

    // Push a synthetic LTE link-quality grade. The auto-rate controller
    // and S15-5 paths consume this.
    void publishLteGrade(uint8_t grade,
                          int16_t rsrp_dbm = -90,
                          const std::string& iface = "lte0");

    // Test seam: drop everything (used between scenarios).
    void reset();

    // Read-back for assertions.
    RobotHealth health(uint32_t robot_id) const;
    std::size_t trackedRobotCount() const;

    // Topic names (so tests can subscribe to verify what was sent).
    static constexpr const char* kRobotStatusTopic = "/swarm/robot_status";
    static constexpr const char* kLinkQualityTopic = "/lte/link_quality";

private:
    rclcpp::Node* node_;
    rclcpp::Publisher<combat_robot_msgs::msg::RobotStatus>::SharedPtr status_pub_;
    rclcpp::Publisher<combat_robot_msgs::msg::LteLinkQuality>::SharedPtr lq_pub_;

    mutable std::mutex mu_;
    std::unordered_map<uint32_t, RobotHealth> state_;
    std::unordered_map<uint32_t, RobotHealth> archived_;   // for restoreRobot
    std::unordered_set<uint32_t> dead_;                    // not published

    combat_robot_msgs::msg::RobotStatus buildMessage(
        uint32_t robot_id, const RobotHealth& h) const;
    uint64_t nowMs() const;
};

}  // namespace san_l5_regression
