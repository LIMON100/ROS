// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 2-E - LTE 게이트웨이 역할 관리자.
//
// State machine: SAN-SDD-SWARM-001 v1.3 §5.5
//   * Hub (S2) :    NORMAL → FAILING → DEMOTED → RECOVERING → NORMAL
//   * Backup(S3):   STANDBY → ACTIVATING → ACTIVE → DEACTIVATING → STANDBY
//
// Split-brain prevention: every state-changing announcement carries a
// monotonically-increasing lte_term. The receiving node clamps its
// local term to max(local, received). A node only emits a new
// PROMOTED with lte_term + 1 (never reuses a term it has seen),
// so two simultaneous promotions cannot end in a tie - the higher
// term wins and the loser must demote.
//
// Promotion budget: mwan3 detect (~8 s) + UCI weight + service reload
// + PPP wait (≤ 2 s) ≤ 10 s. Enforced by hub_lte_down_timer_.
//
// PATCH 2026-05-13 (san_lte_redundancy deep-dive):
//   * LR1 — promote() no longer blocks the rclcpp executor with
//     std::this_thread::sleep_for. The wait-for-PPP loop is now a
//     rclcpp wall_timer ticking every 100 ms with a deadline.
//   * LR2 — role_, hub_lte_down_detected_at_, and the activation FSM
//     are protected by state_mu_ (recursive_mutex so a callback that
//     already holds the lock can re-enter when calling broadcastRole
//     → publishStatus → broadcastRole transitively).
//   * LR3 — split-brain check uses `>=` with a robot-id tiebreak for
//     equal terms (same pattern as the san_role_management Phase 1
//     Leader election fix).
//   * LR4 — chain-fight: on equal-term competing promotion the lower
//     robot_id wins; the loser yields immediately. This prevents
//     two backups simultaneously promoting when announcements arrive
//     in different orders at different nodes.
//   * LR8 — announce_seq_ is std::atomic now (was raw uint32_t).
//
// 권원:
//   * DCN-2026-001 D-005 (4-Tier leader succession), §LTE chain
//   * SDD-SWARM v1.5 §5.5

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/lte_role_announcement.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "san_lte_redundancy/mwan3_uci_controller.hpp"
#include "san_lte_redundancy/mwan3_ubus_monitor.hpp"

namespace san_lte_redundancy
{

enum class LTERole
{
  NONE,
  BACKUP_STANDBY,
  BACKUP_ACTIVATING,
  LTE_ACTIVE,
  LTE_DEACTIVATING,
  HUB_NORMAL,
  HUB_FAILING,
  HUB_DEMOTED,
  HUB_RECOVERING,
};

const char * roleToString(LTERole r);

class LTERoleManager : public rclcpp::Node
{
public:
  LTERoleManager();
  explicit LTERoleManager(const rclcpp::NodeOptions & options);

  // Test hooks - allow injecting a custom UCI/UBUS pair.
  LTERoleManager(
    const rclcpp::NodeOptions & options,
    std::unique_ptr<Mwan3UciController> uci,
    std::unique_ptr<Mwan3UbusMonitor> ubus);

  LTERole getRole() const
  {
    std::lock_guard<std::recursive_mutex> lock(state_mu_);
    return role_;
  }
  uint32_t getLteTerm() const {return lte_term_.load();}
  bool isLteActive() const;

  // Allow tests / launch scripts to seed term across reboots.
  void setInitialTerm(uint32_t term) {lte_term_.store(term);}

  // Direct test entry point - simulate receiving an announcement
  // from another robot.
  void injectAnnouncementForTest(
    const combat_robot_msgs::msg::LTERoleAnnouncement & msg);

  // Direct test entry point - simulate the local mwan3 hotplug
  // signaling the local LTE interface went down/up.
  void injectLocalLteStatusForTest(bool is_up);

  // ★ PATCH 2026-05-13 (LR1): tests need to step the async
  // promotion FSM without spinning a real executor.
  void tickActivationForTest();

private:
  // ------------ parameters ------------
  uint32_t robot_id_ = 0;
  uint32_t hub_robot_id_ = 2;
  std::vector<int64_t> lte_backup_chain_;       // ROS param: vector<int64>
  bool is_hub_ = false;
  bool is_backup_designated_ = false;
  double hub_lte_down_timeout_s_ = 8.0;
  double ppp_activation_timeout_s_ = 2.0;
  double watchdog_period_s_ = 1.0;

  // ★ PATCH 2026-05-13 (LR2): state mutex protecting role_ and
  // the activation FSM.
  mutable std::recursive_mutex state_mu_;

  // ------------ state (guarded by state_mu_) ------------
  LTERole role_ = LTERole::NONE;
  std::atomic<uint32_t> lte_term_;
  std::atomic<uint32_t> announce_seq_{0};         // ★ PATCH (LR8)
  std::optional<rclcpp::Time> hub_lte_down_detected_at_;

  // ★ PATCH 2026-05-13 (LR1): async promotion FSM state.
  struct PromotionState
  {
    std::string reason;
    rclcpp::Time deadline;
    bool in_flight = false;
  };
  PromotionState promotion_;
  rclcpp::TimerBase::SharedPtr activation_timer_;

  // ------------ ROS interfaces ------------
  rclcpp::Publisher<combat_robot_msgs::msg::LTERoleAnnouncement>::SharedPtr
    announce_pub_;
  rclcpp::Subscription<combat_robot_msgs::msg::LTERoleAnnouncement>::SharedPtr
    announce_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::RobotStatus>::SharedPtr
    status_pub_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;

  // [C-1 fix v1.5.1 — DCN-2026-003 D-005] Mutually-exclusive callback
  // group binds onAnnouncement + watchdogTick to a single virtual
  // queue, eliminating races on role_, lte_term_, announce_seq_,
  // hub_lte_down_detected_at_. The ubus uloop callback runs OFF this
  // executor (in its own libubus thread) and forwards to
  // onLocalLteStatusChange — see wireUbusCallback() for the
  // serialization strategy there.
  rclcpp::CallbackGroup::SharedPtr cb_group_;

  // ------------ OpenWrt integration ------------
  std::unique_ptr<Mwan3UciController> uci_;
  std::unique_ptr<Mwan3UbusMonitor> ubus_;

  // ------------ lifecycle ------------
  void declareParameters();
  void readParameters();
  void wireInterfaces();
  void wireUbusCallback();

  // ------------ event handlers ------------
  void onAnnouncement(
    combat_robot_msgs::msg::LTERoleAnnouncement::SharedPtr msg);
  void onLocalLteStatusChange(const std::string & iface, bool is_up);
  void watchdogTick();
  // ★ PATCH 2026-05-13 (LR1): async PPP activation tick.
  void activationTick();

  // ------------ actions ------------
  // ★ PATCH 2026-05-13 (LR1): promote is now async — schedules an
  // activation timer instead of blocking.
  void promoteAsync(const std::string & reason);
  void demote(const std::string & reason);
  void broadcastRole(
    uint8_t announcement_role,
    const std::string & reason);
  void publishStatus();

  // ------------ helpers ------------
  bool amFirstInBackupChain() const;
  bool isInBackupChain() const;
  void handleHubMessage(
    const combat_robot_msgs::msg::LTERoleAnnouncement & msg);
  void handleBackupMessage(
    const combat_robot_msgs::msg::LTERoleAnnouncement & msg);

  // ★ PATCH 2026-05-13 (LR3, LR4): equal-term tiebreak helper.
  // Returns true iff the incoming msg should preempt our current
  // (LTE_ACTIVE) role. Lower robot_id wins on equal term.
  bool incomingPromotionPreempts(
    const combat_robot_msgs::msg::LTERoleAnnouncement & msg) const;
};

}  // namespace san_lte_redundancy
