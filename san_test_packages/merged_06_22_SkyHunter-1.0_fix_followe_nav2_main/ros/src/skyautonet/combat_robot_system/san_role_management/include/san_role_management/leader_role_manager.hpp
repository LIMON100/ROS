// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 8 - 4-tier Leader succession manager (PATCHED 2026-05-13).
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
//
// PATCH 2026-05-13 (Leader succession deep-dive):
//   * C1 ★★★ Non-blocking grace period — replaced
//     std::this_thread::sleep_for inside watchdog timer callback (which
//     blocked the entire executor thread, including announce_sub_,
//     making the "yield on peer promotion" path inoperative) with a
//     one-shot rclcpp::Timer scheduled at end-of-grace. While the grace
//     timer is pending, announce callbacks CAN now fire and cancel us.
//   * C2/C3/C10 State mutex `state_mu_` — protects role_,
//     grace_in_progress_, last_priority_, last_leader_heartbeat_, and
//     the announce-loopback guard.
//   * C4 Promote ordering — fetch_add(1) result CAPTURED into local
//     before publish, and the announce subscribe-loopback (M10) now
//     rejects our own published messages (impersonation check).
//   * C5 Re-arming — DEMOTED automatically returns to NORMAL after a
//     short cool-down so subsequent successions can use this candidate
//     again. Without re-arming a robot that demoted once becomes
//     permanently ineligible — latent fault accumulation.
//   * M6/M7/M11 Tuple tiebreaker — (term, robot_id) lexicographic
//     compare in onLeaderAnnouncement. Same-term different-robot_id
//     no longer accepts blindly.
//   * M9 Status freshness — recordStatus rejects samples older than
//     status_max_age_ms_.
//   * Schedule a CANDIDATE announce at grace start (replaces silent
//     waiting) so other candidates see our intent.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/leader_role_announcement.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "san_role_management/battery_monitor.hpp"
#include "san_role_management/role_types.hpp"

namespace san_role_management
{

class LeaderRoleManager : public rclcpp::Node
{
public:
  LeaderRoleManager();
  explicit LeaderRoleManager(const rclcpp::NodeOptions & options);

  // ★ PATCH 2026-05-13: thread-safe accessors.
  LeaderRole getRole() const;
  uint32_t getLeaderTerm() const {return leader_term_.load();}
  SuccessionPriority getSuccessionPriority() const;
  BatteryMonitor & batteryMonitor() {return battery_monitor_;}

  // Test entry points.
  void injectStatusForTest(const combat_robot_msgs::msg::RobotStatus & s);
  void injectAnnouncementForTest(
    const combat_robot_msgs::msg::LeaderRoleAnnouncement & msg);
  void simulateLeaderHeartbeatForTest();
  void simulateLeaderHeartbeatLossForTest();
  SuccessionPriority evaluateSuccessionForTest();
  void promoteForTest(SuccessionPriority p) {promoteToLeader(p);}

  // ★ PATCH 2026-05-13: synchronous watchdog body for unit tests.
  // Equivalent to one watchdogTick() under simulated timeout, but
  // returns the role transition that would occur — without
  // scheduling the grace-end timer (tests drive the grace
  // completion explicitly via finishGraceForTest).
  void watchdogTickForTest();
  void finishGraceForTest();
  bool isGraceInProgress() const;

private:
  // Parameters.
  uint32_t robot_id_ = 0;
  uint32_t leader_robot_id_ = 1;
  uint32_t hub_robot_id_ = 2;
  uint32_t deputy_robot_id_ = 3;
  int leader_heartbeat_timeout_ms_ = LEADER_HEARTBEAT_TIMEOUT_MS;
  int watchdog_period_ms_ = 100;
  int grace_step_ms_ = SUCCESSION_GRACE_STEP_MS;
  float min_battery_for_leader_ = MIN_BATTERY_FOR_LEADER;
  float min_battery_follower_ = MIN_BATTERY_FOLLOWER;
  // ★ PATCH 2026-05-13:
  int demote_cooldown_ms_ = 2000;        // C5: re-arm after this many ms
  int status_max_age_ms_ = 5000;         // M9: reject older RobotStatus
  bool reject_unknown_clock_ = false;    // M9: optional strict mode

  bool is_deputy_ugv_ = false;
  bool is_hub_ugv_ = false;
  bool is_leader_ = false;

  // ★ PATCH 2026-05-13: state_mu_ protects role_, grace_in_progress_,
  // last_priority_, last_leader_heartbeat_, demoted_at_ms_.
  mutable std::mutex state_mu_;
  LeaderRole role_ = LeaderRole::NORMAL;
  SuccessionPriority last_priority_ = SuccessionPriority::LIMP_MODE;
  std::optional<rclcpp::Time> last_leader_heartbeat_;
  bool grace_in_progress_ = false;
  SuccessionPriority pending_grace_priority_ = SuccessionPriority::LIMP_MODE;
  uint64_t demoted_at_ms_ = 0;

  std::atomic<uint32_t> leader_term_;
  BatteryMonitor battery_monitor_;

  using LeaderAnn = combat_robot_msgs::msg::LeaderRoleAnnouncement;
  using Status = combat_robot_msgs::msg::RobotStatus;

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
  // [R-10 C1 + main C-2 fix] One-shot non-blocking grace timer. The
  // old code blocked the watchdog thread with sleep_for during the
  // grace period — under single-threaded executor that froze the
  // entire role_management process; under MTE the sleep prevented
  // the same thread from servicing the LEADER_PROMOTED announcement
  // it was supposed to be listening for. This one-shot timer
  // schedules onGraceComplete() after grace_ms via the executor,
  // so onLeaderAnnouncement remains responsive throughout the window.
  rclcpp::TimerBase::SharedPtr grace_timer_;

  void declareParameters();
  void readParameters();
  void wireInterfaces();

  void onLeaderAnnouncement(LeaderAnn::SharedPtr msg);
  void onRobotStatus(Status::SharedPtr msg);
  void watchdogTick();

  SuccessionPriority determineMyPriority() const;
  void promoteToLeader(SuccessionPriority priority);
  void demoteToFollower(const std::string & reason);
  void recordStatus(const Status & s);

  // ★ PATCH 2026-05-13: scheduled at end-of-grace; replaces sleep.
  void onGraceComplete();
  // ★ PATCH 2026-05-13: announce intent without promoting yet.
  void announceCandidate(SuccessionPriority priority);
  // ★ PATCH 2026-05-13 (C5): clear DEMOTED back to NORMAL after cool-down.
  void rearmIfCooldownElapsed_locked();

  uint64_t nowMs() const;
};

}  // namespace san_role_management
