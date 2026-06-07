// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 PHASE 8 - Hub-Deputy redundancy manager.
//
// On Hub UGV (S2) heartbeat timeout (5 s default), Deputy UGV (S3)
// takes over all Hub responsibilities:
//   (1) LTE gateway      — via PHASE 2 Mwan3UciController
//   (2) SLAM aggregation — lifecycle-activate /hub_slam_aggregator
//   (3) GStreamer SRT    — lifecycle-activate /gstreamer_relay
//
// Split-brain prevention via monotonically-increasing hub_term.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/hub_role_announcement.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <lifecycle_msgs/srv/change_state.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

#include "san_role_management/role_types.hpp"

// [DCN-2026-006 EXT D-024] DiagnosticArray for RobotStatus audit.
#include <diagnostic_msgs/msg/diagnostic_array.hpp>

// [Sanitizer-hardening] Mwan3UciController used to be a function-local
// static, capturing whichever node's get_logger() ran first. Lifted to
// a member to bind the logger to *this* node's lifetime.
namespace san_lte_redundancy {class Mwan3UciController;}

namespace san_role_management
{

class HubRoleManager : public rclcpp::Node
{
public:
  HubRoleManager();
  explicit HubRoleManager(const rclcpp::NodeOptions & options);
  // [Sanitizer-hardening] Out-of-line dtor required because uci_ is a
  // unique_ptr to a forward-declared type (Mwan3UciController). The
  // type must be complete where the unique_ptr destructor is emitted.
  ~HubRoleManager();

  // Test accessors.
  HubRole getRole() const {return role_;}
  uint32_t getHubTerm() const {return hub_term_.load();}
  bool isHubUgv() const {return is_hub_ugv_;}
  bool isDeputyUgv() const {return is_deputy_ugv_;}
  bool isLteActive() const {return lte_active_;}
  bool isSlamAggregationActive() const {return slam_active_;}
  bool isVideoRelayActive() const {return video_active_;}
  // [DCN-2026-006 EXT D-024 hotfix] Audit publisher is created only on
  // the active Hub. Tests assert this by inspecting hasAuditPublisher()
  // after instantiation with different robot_role values.
  bool hasAuditPublisher() const {return status_audit_pub_ != nullptr;}

  // Per-robot tracker for the audit. Updated on every onRobotStatus,
  // drained by the 1 Hz audit timer. Exposed publicly so the static
  // computeAudit() helper (used by tests) can read it without friend.
  struct RobotStatusAudit
  {
    rclcpp::Time last_received_at;
    uint32_t samples_this_window = 0;           // since last audit tick
    uint32_t samples_total = 0;
    bool last_sbc1_healthy = false;
    bool last_sbc2_healthy = false;
  };

  // [DCN-2026-006 EXT D-024] Pure-logic audit computation — no
  // publisher, no clock state, no node coupling. Returns the
  // DiagnosticArray that publishStatusAudit() would publish. Drains
  // samples_this_window in-place to match the production cadence.
  static diagnostic_msgs::msg::DiagnosticArray computeAudit(
    std::unordered_map<uint32_t, RobotStatusAudit> & robot_audit,
    const rclcpp::Time & now_t,
    int64_t stale_threshold_ms = 3000,
    uint32_t expected_rate_hz = 5);

  // Test entry point — inject a fake RobotStatus arrival for the
  // given robot at last_received = now_offset_ms_ago.
  void injectAuditSampleForTest(
    uint32_t robot_id,
    int64_t ms_ago,
    bool sbc1, bool sbc2,
    uint32_t samples_in_window);

  // Test entry points.
  void injectHubStatusForTest(
    uint32_t hub_id, uint64_t now_ms,
    bool sbc1, bool sbc2);
  void injectAnnouncementForTest(
    const combat_robot_msgs::msg::HubRoleAnnouncement & msg);
  // forcePromoteForTest exercises the role-state machine in isolation
  // from the activation hooks (libuci LTE / SLAM lifecycle / video
  // lifecycle), which are not available on CI hosts. The test
  // suite's docstring (test_hub_deputy_takeover.cpp:1-7) explicitly
  // states that vendor-SDK plumbing is exercised at the L4 HIL bench
  // tier; this entry point unconditionally commits the role + term
  // transition so the state-machine tests can run on CI.
  //
  // [Pre-v1.5.1 this called promoteToHubSync, which on CI failed
  //  because activateSlam/Video did wait_for_service(1s) against
  //  services that don't exist — and the v1.5.1 D-005 rewrite added
  //  strict rollback that reverted the role transition. That broke
  //  test_hub_deputy_takeover.S18_3_PromoteAdvancesTerm and
  //  HubRecoveryDemotesDeputy because the test never observed a
  //  PROMOTED state.]
  void forcePromoteForTest() {commitPromotionForTest("test");}

private:
  // Parameters.
  uint32_t robot_id_ = 0;
  uint32_t hub_robot_id_ = 2;
  uint32_t deputy_robot_id_ = 3;
  int hub_heartbeat_timeout_ms_ = HUB_HEARTBEAT_TIMEOUT_MS;
  int watchdog_period_ms_ = 500;
  // [C-3 fix v1.5.1] lifecycle service timeout, knob for L4 tuning.
  int lifecycle_service_wait_ms_ = 1000;

  bool is_hub_ugv_ = false;
  bool is_deputy_ugv_ = false;

  HubRole role_ = HubRole::NORMAL;
  std::atomic<uint32_t> hub_term_;
  std::optional<rclcpp::Time> last_hub_heartbeat_;
  bool last_hub_sbc_healthy_ = true;

  // Reported takeover state (true when Deputy successfully activated each).
  bool lte_active_ = false;
  bool slam_active_ = false;
  bool video_active_ = false;

  // ─── v1.5.1 (DCN-2026-003 D-005) — concurrency hardening ──────────
  //
  // [C-1 fix] Single MutuallyExclusive callback group serializes
  // onHubAnnouncement / onRobotStatus / watchdog tick / lifecycle
  // response callbacks within this node — eliminating races on
  // role_, hub_term_, last_hub_heartbeat_, lte_active_, slam_active_,
  // video_active_. Cross-node parallelism (Leader / Hub / Limp)
  // remains via MTE in role_management_node.cpp.
  rclcpp::CallbackGroup::SharedPtr cb_group_;

  // [C-3 fix] Async Hub promotion state machine. Replaces
  // synchronous lifecycle service calls (which used
  // create_client-per-call + future.wait_for(3s) and could deadlock
  // under MTE + cause the watchdog to stall for up to 15 s).
  enum class PromoteState : uint8_t
  {
    IDLE,
    ACTIVATING_SLAM,
    ACTIVATING_VIDEO,
    COMMITTED,
    ROLLING_BACK_SLAM,
    ROLLING_BACK_VIDEO,
  };
  std::atomic<PromoteState> promote_state_{PromoteState::IDLE};
  std::string promote_reason_{};
  bool partial_lte_ok_ = false;
  bool partial_slam_ok_ = false;
  bool partial_video_ok_ = false;

  using HubAnn = combat_robot_msgs::msg::HubRoleAnnouncement;
  using Status = combat_robot_msgs::msg::RobotStatus;
  using ChangeStateSrv = lifecycle_msgs::srv::ChangeState;

  rclcpp::Publisher<HubAnn>::SharedPtr announce_pub_;
  rclcpp::Subscription<HubAnn>::SharedPtr announce_sub_;
  rclcpp::Subscription<Status>::SharedPtr robot_status_sub_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;

  // [DCN-2026-006 EXT D-024] RobotStatus audit publisher (1 Hz).
  //
  // Hub aggregates per-robot status arrival rate + freshness and
  // publishes a DiagnosticArray for operator-side telemetry. This
  // exposes silent failures that the watchdog (5 s coarse heartbeat
  // only on hub_id) cannot see — e.g. a follower whose RobotStatus
  // stream drops from 5 Hz to 0.2 Hz without ever timing out the
  // 5 s window.
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr
    status_audit_pub_;
  rclcpp::TimerBase::SharedPtr status_audit_timer_;

  // [DCN-2026-006 EXT D-024] Per-robot audit map — declared public above
  // alongside computeAudit(). Member instance lives here.
  std::unordered_map<uint32_t, RobotStatusAudit> robot_audit_;

  // Helpers.
  void publishStatusAudit();

  rclcpp::Client<ChangeStateSrv>::SharedPtr slam_lifecycle_client_;
  rclcpp::Client<ChangeStateSrv>::SharedPtr video_lifecycle_client_;

  // [Sanitizer-hardening] Owned UCI controller — replaces the
  // function-local static in activate/deactivateLte() that captured
  // a snapshot of the *first* HubRoleManager instance's logger and
  // outlived all subsequent ones (use-after-free under composition).
  // Lazily constructed under cb_group_ serialization so no extra
  // synchronization is needed.
  std::unique_ptr<san_lte_redundancy::Mwan3UciController> uci_;

  void declareParameters();
  void readParameters();
  void wireInterfaces();

  void onHubAnnouncement(HubAnn::SharedPtr msg);
  void onRobotStatus(Status::SharedPtr msg);
  void watchdogTick();

  // [C-3 fix v1.5.1] Async state machine entry / steps.
  //   startHubPromotionAsync — kicks off LTE (sync) then SLAM (async)
  //   onSlamLifecycleResult  — fires when SLAM activate completes
  //   onVideoLifecycleResult — fires when VIDEO activate completes
  //   commitPromotion        — all three succeeded → broadcast PROMOTED
  //   rollbackPromotion      — any failure → undo successes, no broadcast
  void startHubPromotionAsync(const std::string & reason);
  void onSlamLifecycleResult(bool ok);
  void onVideoLifecycleResult(bool ok);
  void commitPromotion();
  void rollbackPromotion();

  // Synchronous orchestrator — kept for forcePromoteForTest only.
  void promoteToHubSync(const std::string & reason);

  // Test-only commit-without-hooks. Used by forcePromoteForTest so
  // CI runs (which lack the lifecycle services) can still exercise
  // the role-state transition. Does NOT call activate*; flips
  // role_ → PROMOTED, bumps hub_term_, and broadcasts.
  void commitPromotionForTest(const std::string & reason);

  void deactivateHubRole(const std::string & reason);
  void broadcast(HubRole role, const std::string & reason);

  // Activation hooks - return true on success. Each is a thin shim
  // over a vendor/library/lifecycle call so the test fixtures can
  // intercept via virtual override or parameter flag.
  //   [C-3 fix v1.5.1] activateLte stays SYNC (libuci, ~10 ms local).
  //   activateSlamAggregation / activateVideoRelay retain their
  //   blocking signature for the TEST sync path; the production
  //   async path uses the cached clients directly via
  //   startHubPromotionAsync / onSlamLifecycleResult / etc.
  virtual bool activateLte();
  virtual bool activateSlamAggregation();
  virtual bool activateVideoRelay();
  virtual bool deactivateLte();
  virtual bool deactivateSlamAggregation();
  virtual bool deactivateVideoRelay();

  uint64_t nowMs() const;
};

}  // namespace san_role_management
