// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.1 PHASE 8 — Hub-Deputy redundancy manager.
//
// DCN-2026-003 D-005 (2026-05-13) concurrency hardening:
//   [C-1 fix] MutuallyExclusive callback group binds every endpoint
//             of this node so onHubAnnouncement / onRobotStatus /
//             watchdogTick / lifecycle response callbacks cannot race
//             on role_ / hub_term_ / *_active_ under MTE.
//   [C-3 fix] promoteToHub() rewritten as async state machine. The
//             old synchronous version held the watchdog thread inside
//             future.wait_for(3s) for each of 3 lifecycle services
//             — up to 15 s of starvation and deadlock-prone under
//             single-threaded executor. The new path uses cached
//             clients + async_send_request with completion callbacks.
//             A separate promoteToHubSync() is kept for the gtest
//             forcePromoteForTest() API.

#include "san_role_management/hub_role_manager.hpp"

#include <algorithm>
#include <chrono>

#include <lifecycle_msgs/msg/transition.hpp>
#include <lifecycle_msgs/srv/change_state.hpp>

#include "san_lte_redundancy/mwan3_uci_controller.hpp"

namespace san_role_management
{

HubRoleManager::HubRoleManager()
: HubRoleManager(rclcpp::NodeOptions())
{}

// Out-of-line destructor — see header note on Mwan3UciController fwd-decl.
HubRoleManager::~HubRoleManager() = default;

HubRoleManager::HubRoleManager(const rclcpp::NodeOptions & options)
: rclcpp::Node("hub_role_manager", options),
  hub_term_(0)
{
  declareParameters();
  readParameters();
  wireInterfaces();
  RCLCPP_INFO(
    get_logger(),
    "HubRoleManager started: robot_id=%u hub=%s deputy=%s "
    "(v1.5.1 async takeover + MEC callback group)",
    robot_id_,
    is_hub_ugv_ ? "yes" : "no",
    is_deputy_ugv_ ? "yes" : "no");
}

void HubRoleManager::declareParameters()
{
  declare_parameter<int>("robot_id", 0);
  declare_parameter<int>("hub_robot_id", 2);
  declare_parameter<int>("deputy_robot_id", 3);
  declare_parameter<int>(
    "hub_heartbeat_timeout_ms",
    HUB_HEARTBEAT_TIMEOUT_MS);
  declare_parameter<int>("watchdog_period_ms", 500);
  declare_parameter<int>("lifecycle_service_wait_ms", 1000);
}

void HubRoleManager::readParameters()
{
  robot_id_ = static_cast<uint32_t>(get_parameter("robot_id").as_int());
  hub_robot_id_ = static_cast<uint32_t>(
    get_parameter("hub_robot_id").as_int());
  deputy_robot_id_ = static_cast<uint32_t>(
    get_parameter("deputy_robot_id").as_int());
  hub_heartbeat_timeout_ms_ =
    get_parameter("hub_heartbeat_timeout_ms").as_int();
  watchdog_period_ms_ = get_parameter("watchdog_period_ms").as_int();
  // [Sanitizer-hardening] A negative wait_ms flows straight into
  // chrono::milliseconds and then into rmw_wait, which casts to an
  // unsigned timeout downstream — UB on most RMWs. Clamp at read
  // time so the rest of the file can keep its tight signature.
  lifecycle_service_wait_ms_ = std::max(
    0,
    static_cast<int>(get_parameter("lifecycle_service_wait_ms").as_int()));

  is_hub_ugv_ = (robot_id_ == hub_robot_id_);
  is_deputy_ugv_ = (robot_id_ == deputy_robot_id_);
}

void HubRoleManager::wireInterfaces()
{
  // [C-1 fix v1.5.1] Single MutuallyExclusive callback group.
  cb_group_ = create_callback_group(
    rclcpp::CallbackGroupType::MutuallyExclusive);

  rclcpp::SubscriptionOptions sub_opts;
  sub_opts.callback_group = cb_group_;

  rclcpp::QoS qos(10);
  qos.reliable().transient_local();

  announce_pub_ = create_publisher<HubAnn>(
    "/swarm/hub/role_announce", qos);
  announce_sub_ = create_subscription<HubAnn>(
    "/swarm/hub/role_announce", qos,
    std::bind(
      &HubRoleManager::onHubAnnouncement, this,
      std::placeholders::_1),
    sub_opts);
  robot_status_sub_ = create_subscription<Status>(
    "/swarm/robot_status", 10,
    std::bind(
      &HubRoleManager::onRobotStatus, this,
      std::placeholders::_1),
    sub_opts);

  watchdog_timer_ = create_wall_timer(
    std::chrono::milliseconds(watchdog_period_ms_),
    std::bind(&HubRoleManager::watchdogTick, this),
    cb_group_);

  // [C-3 fix v1.5.1] Cache lifecycle clients. The clients live for
  // the entire process lifetime and are reused across every
  // promotion / rollback attempt. They share cb_group_ so that
  // response callbacks (fired when the remote node completes
  // lifecycle transition) cannot race against the watchdog timer
  // that scheduled them — MutuallyExclusive serializes the pair.
  slam_lifecycle_client_ = create_client<ChangeStateSrv>(
    "/hub_slam_aggregator/change_state",
    rmw_qos_profile_services_default,
    cb_group_);
  video_lifecycle_client_ = create_client<ChangeStateSrv>(
    "/gstreamer_relay/change_state",
    rmw_qos_profile_services_default,
    cb_group_);

  // [DCN-2026-006 EXT D-024 hotfix + DCN-2026-011 D-032 follow-up]
  // RobotStatus audit — 1 Hz aggregate. HubRoleManager is instantiated
  // on every robot (so a Deputy can promote into Hub on failover), but
  // the audit publisher must run on the *active* Hub only.
  //
  // Two-tier gate:
  //   (1) robot_role == "hub" — excludes leader / deputy / follower
  //       (5 → 2 publishers; the original P0 hotfix from C-1 §3).
  //   (2) sbc_id != 2 — silences the standby SBC of the dual-SBC Hub
  //       (2 → 1 publisher; matches SW Operation doc §2 which marks
  //       SBC #2 as "standby / lifecycle inactive"). sbc_id is the
  //       DCN-2026-011 D-032 parameter passed via squadron.launch.py;
  //       its default -1 / 0 keeps the SBC#1 (and any pre-D-032
  //       deployment) publishing — only sbc_id=2 is gated out.
  //
  // Failover path is currently manual: if SBC#1 dies, operations
  // re-provision SBC#2 via infra/systemd/install.sh which writes
  // sbc_id=1. A future PR can wire the gate to HubRoleManager's
  // promotion state for automatic transfer.
  const std::string role = has_parameter("robot_role") ?
    get_parameter("robot_role").as_string() :
    declare_parameter<std::string>("robot_role", "");
  const int sbc_id = has_parameter("sbc_id") ?
    static_cast<int>(get_parameter("sbc_id").as_int()) :
    declare_parameter<int>("sbc_id", -1);
  const bool is_hub_active_sbc = (role == "hub") && (sbc_id != 2);
  if (is_hub_active_sbc) {
    status_audit_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics/robot_status_audit",
      rclcpp::QoS(rclcpp::KeepLast(5)).reliable());
    status_audit_timer_ = create_wall_timer(
      std::chrono::milliseconds(1000),
      std::bind(&HubRoleManager::publishStatusAudit, this),
      cb_group_);
  }
}

void HubRoleManager::onRobotStatus(Status::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  if (msg->robot_id == hub_robot_id_) {
    last_hub_heartbeat_ = now();
    last_hub_sbc_healthy_ =
      msg->sbc1_healthy || msg->sbc2_healthy;
  }

  // [DCN-2026-006 EXT D-024] Per-robot audit tracker. Records every
  // RobotStatus arrival for every robot (not just hub_id) so the
  // 1 Hz audit timer can compute incoming rate + freshness.
  auto & slot = robot_audit_[msg->robot_id];
  slot.last_received_at = now();
  slot.samples_this_window += 1;
  slot.samples_total += 1;
  slot.last_sbc1_healthy = msg->sbc1_healthy;
  slot.last_sbc2_healthy = msg->sbc2_healthy;
}

// [DCN-2026-006 EXT D-024] 1 Hz audit publisher.
//
// Emits one DiagnosticStatus per known robot_id with keys:
//   - rate_hz                   samples this 1 s window
//   - rate_total                lifetime sample count
//   - stale_ms                  ms since last sample
//   - sbc1_healthy / sbc2_healthy
//   - level                     OK / WARN / ERROR / STALE
//
// Level rules (cumulative — worst wins):
//   STALE  if stale_ms >= stale_threshold_ms_ (default 3000)
//   ERROR  if rate_hz == 0 in this window
//   WARN   if rate_hz < expected_rate_hz_ / 2 (default expected = 5 Hz)
//   OK     otherwise
//
// Audit drains samples_this_window at every tick — so each row
// represents instantaneous (last 1 s) rate, not lifetime average.
diagnostic_msgs::msg::DiagnosticArray HubRoleManager::computeAudit(
  std::unordered_map<uint32_t, RobotStatusAudit> & robot_audit,
  const rclcpp::Time & now_t,
  int64_t stale_threshold_ms,
  uint32_t expected_rate_hz)
{
  diagnostic_msgs::msg::DiagnosticArray msg;
  msg.header.stamp = now_t;
  msg.header.frame_id = "hub_role_manager";

  using diagnostic_msgs::msg::DiagnosticStatus;
  using diagnostic_msgs::msg::KeyValue;

  for (auto & [robot_id, slot] : robot_audit) {
    DiagnosticStatus ds;
    ds.name = "robot_" + std::to_string(robot_id) + "/robot_status";
    ds.hardware_id = std::to_string(robot_id);

    const auto stale_ns = (now_t - slot.last_received_at).nanoseconds();
    const auto stale_ms = stale_ns / 1'000'000;
    const uint32_t rate = slot.samples_this_window;       // 1 Hz drain → samples = Hz

    uint8_t level = DiagnosticStatus::OK;
    std::string summary = "ok";
    if (stale_ms >= stale_threshold_ms) {
      level = DiagnosticStatus::STALE;
      summary = "stale " + std::to_string(stale_ms) + " ms";
    } else if (rate == 0) {
      level = DiagnosticStatus::ERROR;
      summary = "no samples in last second";
    } else if (rate * 2 < expected_rate_hz) {
      level = DiagnosticStatus::WARN;
      summary = "rate " + std::to_string(rate) + " Hz < expected/2";
    }
    ds.level = level;
    ds.message = summary;

    KeyValue kv;
    kv.key = "rate_hz";        kv.value = std::to_string(rate);
    ds.values.push_back(kv);
    kv.key = "rate_total";     kv.value = std::to_string(slot.samples_total);
    ds.values.push_back(kv);
    kv.key = "stale_ms";       kv.value = std::to_string(stale_ms);
    ds.values.push_back(kv);
    kv.key = "sbc1_healthy";
    kv.value = slot.last_sbc1_healthy ? "true" : "false";
    ds.values.push_back(kv);
    kv.key = "sbc2_healthy";
    kv.value = slot.last_sbc2_healthy ? "true" : "false";
    ds.values.push_back(kv);

    msg.status.push_back(ds);

    // Drain the window counter so the next tick measures the
    // next second's arrivals only.
    slot.samples_this_window = 0;
  }
  return msg;
}

void HubRoleManager::publishStatusAudit()
{
  if (!status_audit_pub_) {return;}
  status_audit_pub_->publish(computeAudit(robot_audit_, now()));
}

// [DCN-2026-006 EXT D-024] Synthetic audit-state injection — lets tests
// drive computeAudit() without the rclcpp subscription pipeline.
// last_received is set to (now() - ms_ago) so STALE / OK branches can
// both be exercised deterministically.
void HubRoleManager::injectAuditSampleForTest(
  uint32_t robot_id,
  int64_t ms_ago,
  bool sbc1, bool sbc2,
  uint32_t samples_in_window)
{
  auto & slot = robot_audit_[robot_id];
  slot.last_received_at = now() - rclcpp::Duration(
    std::chrono::milliseconds(ms_ago));
  slot.samples_this_window = samples_in_window;
  slot.samples_total += samples_in_window;
  slot.last_sbc1_healthy = sbc1;
  slot.last_sbc2_healthy = sbc2;
}

void HubRoleManager::injectHubStatusForTest(
  uint32_t hub_id, uint64_t /*now_ms*/,
  bool sbc1, bool sbc2)
{
  if (hub_id != hub_robot_id_) {return;}
  last_hub_heartbeat_ = now();
  last_hub_sbc_healthy_ = sbc1 || sbc2;
}

void HubRoleManager::injectAnnouncementForTest(const HubAnn & msg)
{
  auto p = std::make_shared<HubAnn>(msg);
  onHubAnnouncement(p);
}

void HubRoleManager::watchdogTick()
{
  // Only Deputy watches Hub heartbeat for promotion. Hub itself and
  // followers just observe.
  if (!is_deputy_ugv_) {return;}
  if (role_ == HubRole::PROMOTED) {return;}
  if (!last_hub_heartbeat_.has_value()) {return;}

  // [C-3 fix v1.5.1] Don't start a new promotion if one is already
  // mid-flight (state machine non-IDLE). Without this guard, the
  // watchdog could re-trigger startHubPromotionAsync every 500 ms
  // while waiting for SLAM/VIDEO lifecycle responses, queueing up
  // duplicate requests.
  if (promote_state_.load() != PromoteState::IDLE) {return;}

  const auto elapsed_ms =
    (now() - *last_hub_heartbeat_).nanoseconds() / 1'000'000;
  const bool hub_silent =
    elapsed_ms > hub_heartbeat_timeout_ms_;
  const bool hub_unhealthy = !last_hub_sbc_healthy_;

  if (hub_silent || hub_unhealthy) {
    RCLCPP_WARN(
      get_logger(),
      "Hub UGV failure detected (elapsed=%ld ms, sbc_healthy=%d). "
      "Deputy starting async promotion to Hub.",
      elapsed_ms, last_hub_sbc_healthy_);
    startHubPromotionAsync(
      hub_silent ?
      "hub_heartbeat_timeout" :
      "hub_sbc_failed");
  }
}

// ─── v1.5.1 async promotion state machine ──────────────────────────────

void HubRoleManager::startHubPromotionAsync(const std::string & reason)
{
  // v1.5 IDS §5.16.1 Full Takeover Semantics (DCN-2026-001 D-005)
  // applied step-by-step over async lifecycle responses.

  promote_reason_ = reason;
  partial_lte_ok_ = false;
  partial_slam_ok_ = false;
  partial_video_ok_ = false;

  // Step 1: LTE — libuci is local + synchronous (~10 ms). Done inline.
  partial_lte_ok_ = activateLte();
  if (!partial_lte_ok_) {
    RCLCPP_ERROR(
      get_logger(),
      "Hub promotion ABORTED at LTE step (reason=%s)", reason.c_str());
    // Nothing to roll back; reset state machine.
    promote_state_.store(PromoteState::IDLE);
    return;
  }

  // Step 2: SLAM lifecycle — ASYNC.
  if (!slam_lifecycle_client_->service_is_ready()) {
    // wait_for_service is OK here (called from MEC callback group;
    // 1 s budget). On the production target the lifecycle node
    // is up by the time Deputy considers a takeover.
    if (!slam_lifecycle_client_->wait_for_service(
        std::chrono::milliseconds(lifecycle_service_wait_ms_)))
    {
      RCLCPP_ERROR(
        get_logger(),
        "Hub promotion: /hub_slam_aggregator/change_state "
        "not available within %d ms; rolling back LTE.",
        lifecycle_service_wait_ms_);
      deactivateLte();
      promote_state_.store(PromoteState::IDLE);
      return;
    }
  }

  promote_state_.store(PromoteState::ACTIVATING_SLAM);

  auto req = std::make_shared<ChangeStateSrv::Request>();
  req->transition.id =
    lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE;

  // Async dispatch. Response callback fires on cb_group_ (MEC), so
  // it is serialized with the watchdog + subs + announcement
  // handlers. No wait_for() — the calling thread returns immediately.
  //
  // [Sanitizer-hardening] Capture a weak handle to the node so the
  // response lambda can detect a dead node before touching members.
  // The cached client is a member; if the node is destroyed mid-
  // promotion (Deputy shut down, executor torn down) and the response
  // still happens to arrive, the bare `this` capture would deref a
  // freed object.
  auto weak_self = std::weak_ptr<rclcpp::Node>(shared_from_this());
  slam_lifecycle_client_->async_send_request(
    req,
    [this, weak_self](rclcpp::Client<ChangeStateSrv>::SharedFuture future) {
      if (weak_self.expired()) {return;}
      bool ok = false;
      try {
        auto resp = future.get();
        ok = (resp != nullptr) && resp->success;
      } catch (const std::exception & e) {
        RCLCPP_ERROR(
          get_logger(),
          "SLAM lifecycle response exception: %s", e.what());
        ok = false;
      }
      onSlamLifecycleResult(ok);
    });
}

void HubRoleManager::onSlamLifecycleResult(bool ok)
{
  if (promote_state_.load() != PromoteState::ACTIVATING_SLAM) {
    // Stale callback (rollback already in progress, or test
    // injected a state change). Discard.
    return;
  }
  partial_slam_ok_ = ok;
  if (!ok) {
    RCLCPP_ERROR(
      get_logger(),
      "Hub promotion: SLAM lifecycle activate FAILED. Rolling back.");
    rollbackPromotion();
    return;
  }

  // Step 3: VIDEO lifecycle — ASYNC.
  if (!video_lifecycle_client_->service_is_ready()) {
    if (!video_lifecycle_client_->wait_for_service(
        std::chrono::milliseconds(lifecycle_service_wait_ms_)))
    {
      RCLCPP_ERROR(
        get_logger(),
        "Hub promotion: /gstreamer_relay/change_state "
        "not available within %d ms; rolling back SLAM + LTE.",
        lifecycle_service_wait_ms_);
      rollbackPromotion();
      return;
    }
  }

  promote_state_.store(PromoteState::ACTIVATING_VIDEO);

  auto req = std::make_shared<ChangeStateSrv::Request>();
  req->transition.id =
    lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE;

  // [Sanitizer-hardening] Same weak_self pattern as the SLAM step
  // — guard against node destruction before the response arrives.
  auto weak_self = std::weak_ptr<rclcpp::Node>(shared_from_this());
  video_lifecycle_client_->async_send_request(
    req,
    [this, weak_self](rclcpp::Client<ChangeStateSrv>::SharedFuture future) {
      if (weak_self.expired()) {return;}
      bool ok = false;
      try {
        auto resp = future.get();
        ok = (resp != nullptr) && resp->success;
      } catch (const std::exception & e) {
        RCLCPP_ERROR(
          get_logger(),
          "VIDEO lifecycle response exception: %s", e.what());
        ok = false;
      }
      onVideoLifecycleResult(ok);
    });
}

void HubRoleManager::onVideoLifecycleResult(bool ok)
{
  if (promote_state_.load() != PromoteState::ACTIVATING_VIDEO) {
    return;      // stale
  }
  partial_video_ok_ = ok;
  if (!ok) {
    RCLCPP_ERROR(
      get_logger(),
      "Hub promotion: VIDEO lifecycle activate FAILED. Rolling back.");
    rollbackPromotion();
    return;
  }
  commitPromotion();
}

void HubRoleManager::commitPromotion()
{
  role_ = HubRole::PROMOTED;
  hub_term_.fetch_add(1);
  lte_active_ = true;
  slam_active_ = true;
  video_active_ = true;
  promote_state_.store(PromoteState::COMMITTED);

  broadcast(HubRole::PROMOTED, promote_reason_);

  RCLCPP_INFO(
    get_logger(),
    "Deputy -> Hub PROMOTED (Full Takeover async) "
    "(term=%u, lte=1 slam=1 video=1, reason=%s)",
    hub_term_.load(), promote_reason_.c_str());

  // Settle back to IDLE so subsequent demotion/repromotion cycles
  // can run.
  promote_state_.store(PromoteState::IDLE);
}

void HubRoleManager::rollbackPromotion()
{
  // Best-effort: deactivate whatever succeeded so far. Each
  // deactivate* call is still synchronous because rollback is rare
  // and we want it deterministic.
  if (partial_video_ok_) {
    promote_state_.store(PromoteState::ROLLING_BACK_VIDEO);
    deactivateVideoRelay();
  }
  if (partial_slam_ok_) {
    promote_state_.store(PromoteState::ROLLING_BACK_SLAM);
    deactivateSlamAggregation();
  }
  if (partial_lte_ok_) {
    deactivateLte();
  }
  lte_active_ = false;
  slam_active_ = false;
  video_active_ = false;
  // role_ and hub_term_ left unchanged (NOT promoted).
  RCLCPP_ERROR(
    get_logger(),
    "Hub promotion ABORTED: partial takeover not permitted by "
    "IDS v1.5 §5.16.1 (lte=%d slam=%d video=%d, reason=%s). "
    "Rolled back; LimpModeManager should escalate.",
    partial_lte_ok_, partial_slam_ok_, partial_video_ok_,
    promote_reason_.c_str());
  promote_state_.store(PromoteState::IDLE);
}

// ─── Test-only state-machine commit ────────────────────────────────────

void HubRoleManager::commitPromotionForTest(const std::string & reason)
{
  // Bypasses activateLte / activateSlamAggregation / activateVideoRelay
  // because those depend on vendor SDKs and a live lifecycle service
  // bus that the CI host does not provide. test_hub_deputy_takeover
  // documents this contract explicitly (file header §1-7): exercise
  // the role-state machine; SDK plumbing belongs to L4 HIL.
  //
  // We do NOT set lte_active_/slam_active_/video_active_ — they stay
  // false to reflect that no hook actually ran. The S18_3 test
  // doesn't assert on them; production callers should use
  // startHubPromotionAsync (which goes through the real hooks).
  role_ = HubRole::PROMOTED;
  hub_term_.fetch_add(1);
  broadcast(HubRole::PROMOTED, reason);
  RCLCPP_INFO(
    get_logger(),
    "[TEST PATH] Deputy -> Hub PROMOTED via commitPromotionForTest "
    "(term=%u, reason=%s, activation hooks SKIPPED)",
    hub_term_.load(), reason.c_str());
}

// ─── v1.5.1 sync orchestrator (TEST ONLY) ──────────────────────────────

void HubRoleManager::promoteToHubSync(const std::string & reason)
{
  // Test-only path — preserves the v1.4 / pre-v1.5.1 observable
  // semantics expected by test_hub_deputy_takeover.cpp.
  // Production code path is startHubPromotionAsync().
  const bool lte_ok = activateLte();
  const bool slam_ok = activateSlamAggregation();
  const bool video_ok = activateVideoRelay();

  if (!lte_ok || !slam_ok || !video_ok) {
    if (lte_ok) {deactivateLte();}
    if (slam_ok) {deactivateSlamAggregation();}
    if (video_ok) {deactivateVideoRelay();}
    lte_active_ = false;
    slam_active_ = false;
    video_active_ = false;
    RCLCPP_ERROR(
      get_logger(),
      "[SYNC TEST PATH] Hub promotion ABORTED "
      "(lte=%d slam=%d video=%d, reason=%s)",
      lte_ok, slam_ok, video_ok, reason.c_str());
    return;
  }

  role_ = HubRole::PROMOTED;
  hub_term_.fetch_add(1);
  lte_active_ = true;
  slam_active_ = true;
  video_active_ = true;
  broadcast(HubRole::PROMOTED, reason);
  RCLCPP_INFO(
    get_logger(),
    "[SYNC TEST PATH] Deputy -> Hub PROMOTED "
    "(term=%u, reason=%s)",
    hub_term_.load(), reason.c_str());
}

void HubRoleManager::deactivateHubRole(const std::string & reason)
{
  if (role_ != HubRole::PROMOTED) {return;}
  deactivateLte();
  deactivateSlamAggregation();
  deactivateVideoRelay();
  lte_active_ = false;
  slam_active_ = false;
  video_active_ = false;
  role_ = HubRole::DEMOTED;
  hub_term_.fetch_add(1);
  broadcast(HubRole::DEMOTED, reason);
  RCLCPP_INFO(
    get_logger(),
    "Deputy -> Hub DEMOTED (term=%u, reason=%s)",
    hub_term_.load(), reason.c_str());
}

void HubRoleManager::onHubAnnouncement(HubAnn::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  if (msg->robot_id == robot_id_) {
    // Loopback: still ratchet term forward so a freshly-restarted
    // instance does not regress.
    if (msg->hub_term > hub_term_.load()) {
      hub_term_.store(msg->hub_term);
    }
    return;
  }

  if (msg->hub_term < hub_term_.load()) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 1000,
      "Stale HubRoleAnnouncement from robot %u (term=%u local=%u)",
      msg->robot_id, msg->hub_term, hub_term_.load());
    return;
  }
  hub_term_.store(msg->hub_term);

  // Original Hub recovered and re-asserted PROMOTED -> Deputy demotes.
  if (msg->robot_id == hub_robot_id_ &&
    msg->role == HubAnn::HUB_PROMOTED &&
    is_deputy_ugv_ &&
    role_ == HubRole::PROMOTED)
  {
    RCLCPP_INFO(
      get_logger(),
      "Original Hub recovered (term=%u). Deputy demoting.",
      msg->hub_term);
    deactivateHubRole("hub_recovered");
  }
}

void HubRoleManager::broadcast(HubRole role, const std::string & reason)
{
  if (!announce_pub_) {return;}
  HubAnn msg;
  msg.header.stamp = now();
  msg.header.frame_id = "swarm";
  msg.robot_id = robot_id_;
  msg.hub_term = hub_term_.load();
  msg.role = static_cast<uint8_t>(role);
  msg.lte_active = lte_active_;
  msg.slam_aggregation_active = slam_active_;
  msg.video_relay_active = video_active_;
  msg.reason = reason;
  msg.timestamp_ms = nowMs();
  announce_pub_->publish(msg);
}

// ─── Activation hooks ──────────────────────────────────────────────────
//
// activateLte stays sync (libuci local). The SLAM / VIDEO activation
// hooks below keep their original blocking signature ONLY for the
// sync test path (promoteToHubSync → forcePromoteForTest). Production
// uses startHubPromotionAsync + onSlamLifecycleResult /
// onVideoLifecycleResult which bypass these and call the cached
// clients directly with completion callbacks.
// Test subclasses can still override these virtual methods to mock
// lifecycle behaviour.

bool HubRoleManager::activateLte()
{
  try {
    // [Sanitizer-hardening] Lazy-init the member uci_ (was a
    // function-local static that captured the first node's logger
    // and never released it — use-after-free under intra-process
    // composition). Construction is serialized by cb_group_.
    if (!uci_) {
      uci_ = std::make_unique<san_lte_redundancy::Mwan3UciController>(
        get_logger());
    }
    return uci_->setLteWeight(100);
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "activateLte: %s", e.what());
    return false;
  }
}

bool HubRoleManager::activateSlamAggregation()
{
  // [Sync test path only — v1.5.1 D-005] Uses the cached client
  // and configurable timeout. Production code uses the async state
  // machine; this entry point is kept ONLY for S18_3 forcePromoteForTest
  // backward compat. Future production callers MUST use the async path.
  //
  // PR #129 Phase 7 fix preserved: wait_for==ready alone proves only
  // that the response arrived; a rejected ChangeState (already active,
  // wrong state, etc.) still returns ready but success=false. The
  // caller (Full Takeover guard) was previously trusting a lie.
  if (!slam_lifecycle_client_) {return false;}
  if (!slam_lifecycle_client_->wait_for_service(
      std::chrono::milliseconds(lifecycle_service_wait_ms_)))
  {
    return false;
  }
  auto req = std::make_shared<ChangeStateSrv::Request>();
  req->transition.id =
    lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE;
  auto future = slam_lifecycle_client_->async_send_request(req);
  if (future.wait_for(
      std::chrono::milliseconds(lifecycle_service_wait_ms_)) !=
    std::future_status::ready)
  {
    return false;
  }
  auto resp = future.get();
  return resp && resp->success;
}

bool HubRoleManager::activateVideoRelay()
{
  // [Sync test path only — v1.5.1 D-005] See activateSlamAggregation
  // for cached-client + Phase 7 success-check rationale.
  if (!video_lifecycle_client_) {return false;}
  if (!video_lifecycle_client_->wait_for_service(
      std::chrono::milliseconds(lifecycle_service_wait_ms_)))
  {
    return false;
  }
  auto req = std::make_shared<ChangeStateSrv::Request>();
  req->transition.id =
    lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE;
  auto future = video_lifecycle_client_->async_send_request(req);
  if (future.wait_for(
      std::chrono::milliseconds(lifecycle_service_wait_ms_)) !=
    std::future_status::ready)
  {
    return false;
  }
  auto resp = future.get();
  return resp && resp->success;
}

bool HubRoleManager::deactivateLte()
{
  try {
    // [Sanitizer-hardening] Same lazy-init pattern as activateLte().
    if (!uci_) {
      uci_ = std::make_unique<san_lte_redundancy::Mwan3UciController>(
        get_logger());
    }
    return uci_->setLteWeight(0);
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "deactivateLte: %s", e.what());
    return false;
  }
}

bool HubRoleManager::deactivateSlamAggregation()
{
  // [Sync test path only — v1.5.1 D-005] Cached client + configurable
  // timeout + PR #129 Phase 7 success-check preserved.
  if (!slam_lifecycle_client_) {return false;}
  if (!slam_lifecycle_client_->wait_for_service(
      std::chrono::milliseconds(lifecycle_service_wait_ms_)))
  {
    return false;
  }
  auto req = std::make_shared<ChangeStateSrv::Request>();
  req->transition.id =
    lifecycle_msgs::msg::Transition::TRANSITION_DEACTIVATE;
  auto future = slam_lifecycle_client_->async_send_request(req);
  if (future.wait_for(
      std::chrono::milliseconds(lifecycle_service_wait_ms_)) !=
    std::future_status::ready)
  {
    return false;
  }
  auto resp = future.get();
  return resp && resp->success;
}

bool HubRoleManager::deactivateVideoRelay()
{
  // [Sync test path only — v1.5.1 D-005]
  if (!video_lifecycle_client_) {return false;}
  if (!video_lifecycle_client_->wait_for_service(
      std::chrono::milliseconds(lifecycle_service_wait_ms_)))
  {
    return false;
  }
  auto req = std::make_shared<ChangeStateSrv::Request>();
  req->transition.id =
    lifecycle_msgs::msg::Transition::TRANSITION_DEACTIVATE;
  auto future = video_lifecycle_client_->async_send_request(req);
  if (future.wait_for(
      std::chrono::milliseconds(lifecycle_service_wait_ms_)) !=
    std::future_status::ready)
  {
    return false;
  }
  auto resp = future.get();
  return resp && resp->success;
}

uint64_t HubRoleManager::nowMs() const
{
  return static_cast<uint64_t>(now().nanoseconds() / 1'000'000ll);
}

}  // namespace san_role_management
