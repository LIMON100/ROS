// SAN v1.4 PHASE 8 - 4-tier Leader succession manager.
//
// Succession order on Leader heartbeat timeout (1.4 s default):
//   1. Deputy UGV (S3) - battery >= 20%, both SBCs healthy
//   2. Hub UGV (S2)    - battery >= 20%, Deputy failed
//   3. Battery-max follower - both Hub/Deputy failed, battery >= 10%
//   4. Limp Mode trigger    - nobody eligible
//
// Grace period of 200 ms × priority lets a higher-priority candidate
// promote first; lower-priority candidates re-check before stepping
// in. Split-brain prevention via leader_term.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/leader_role_announcement.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

#include "san_role_management/battery_monitor.hpp"
#include "san_role_management/role_types.hpp"

namespace san_role_management {

class LeaderRoleManager : public rclcpp::Node {
public:
    LeaderRoleManager();
    explicit LeaderRoleManager(const rclcpp::NodeOptions& options);

    // Test accessors.
    LeaderRole getRole() const { return role_; }
    uint32_t getLeaderTerm() const { return leader_term_.load(); }
    SuccessionPriority getSuccessionPriority() const {
        return last_priority_;
    }
    BatteryMonitor& batteryMonitor() { return battery_monitor_; }

    // Test entry points.
    void injectStatusForTest(const combat_robot_msgs::msg::RobotStatus& s);
    void injectAnnouncementForTest(
        const combat_robot_msgs::msg::LeaderRoleAnnouncement& msg);
    void simulateLeaderHeartbeatForTest() { last_leader_heartbeat_ = now(); }
    void simulateLeaderHeartbeatLossForTest();
    // Synchronous variant of the watchdog body (no sleep) for unit tests.
    SuccessionPriority evaluateSuccessionForTest();
    void promoteForTest(SuccessionPriority p) { promoteToLeader(p); }

private:
    // Parameters.
    uint32_t robot_id_ = 0;
    uint32_t leader_robot_id_ = 1;
    uint32_t hub_robot_id_ = 2;
    uint32_t deputy_robot_id_ = 3;
    int  leader_heartbeat_timeout_ms_ = LEADER_HEARTBEAT_TIMEOUT_MS;
    int  watchdog_period_ms_ = 100;
    int  grace_step_ms_ = SUCCESSION_GRACE_STEP_MS;
    float min_battery_for_leader_ = MIN_BATTERY_FOR_LEADER;
    float min_battery_follower_   = MIN_BATTERY_FOLLOWER;

    bool is_deputy_ugv_ = false;
    bool is_hub_ugv_ = false;
    bool is_leader_ = false;

    LeaderRole role_ = LeaderRole::NORMAL;
    SuccessionPriority last_priority_ = SuccessionPriority::LIMP_MODE;
    std::atomic<uint32_t> leader_term_;
    std::optional<rclcpp::Time> last_leader_heartbeat_;
    bool grace_in_progress_ = false;

    BatteryMonitor battery_monitor_;

    using LeaderAnn = combat_robot_msgs::msg::LeaderRoleAnnouncement;
    using Status    = combat_robot_msgs::msg::RobotStatus;

    // ─── v1.5.1 (DCN-2026-003 D-005) — concurrency hardening ──────────
    //
    // [C-1 fix] Mutually-exclusive callback group bundles ALL of this
    // node's subscriptions + timer onto a single virtual queue. Under
    // MultiThreadedExecutor (role_management_node.cpp) callbacks of
    // DIFFERENT nodes (Leader / Hub / Limp) still run on separate
    // threads, but within ONE node the watchdog timer cannot race
    // against onLeaderAnnouncement / onRobotStatus — eliminating the
    // role_ / last_leader_heartbeat_ / grace_in_progress_ data race.
    rclcpp::CallbackGroup::SharedPtr cb_group_;

    rclcpp::Publisher<LeaderAnn>::SharedPtr announce_pub_;
    rclcpp::Subscription<LeaderAnn>::SharedPtr announce_sub_;
    rclcpp::Subscription<Status>::SharedPtr status_sub_;
    rclcpp::TimerBase::SharedPtr watchdog_timer_;

    // [C-2 fix] One-shot deferred promotion timer. The old code blocked
    // the watchdog thread with std::this_thread::sleep_for during the
    // grace period — under single-threaded executor that froze the
    // entire role_management process; under MTE the sleep still
    // prevented the same thread from servicing the LEADER_PROMOTED
    // announcement it was supposed to be listening for. This one-shot
    // timer schedules promoteToLeader() after grace_ms via the executor,
    // so onLeaderAnnouncement remains responsive throughout the window.
    rclcpp::TimerBase::SharedPtr grace_timer_;

    void declareParameters();
    void readParameters();
    void wireInterfaces();

    void onLeaderAnnouncement(LeaderAnn::SharedPtr msg);
    void onRobotStatus(Status::SharedPtr msg);
    void watchdogTick();

    // Priority decision (no side-effects).
    SuccessionPriority determineMyPriority() const;
    void promoteToLeader(SuccessionPriority priority);
    void demoteToFollower(const std::string& reason);

    void recordStatus(const Status& s);

    uint64_t nowMs() const;
};

}  // namespace san_role_management
