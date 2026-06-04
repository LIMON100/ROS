// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — TierNode implementation (patched 2026-05-13).

#include "san_follower_tier/tier_node.hpp"

#include <chrono>
#include <cmath>

namespace san_follower_tier
{

using namespace std::chrono_literals;

namespace
{

uint64_t toMs(const rclcpp::Time & t)
{
  // Static-analysis hardening: nanoseconds() is int64_t; before clock
  // init or sim-time without /clock, the value can be 0 or negative.
  // Unsigned cast would wrap a negative to ~2^64 and propagate the
  // pollution downstream (tier dwell, age computation).
  const auto ns = t.nanoseconds();
  return ns > 0 ? static_cast<uint64_t>(ns / 1'000'000LL) : 0ULL;
}

}  // namespace

TierNode::TierNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("tier_node", opts),
  comm_health_(30000u)
{
  declareParameters();
  loadParameters();

  fsm_ = std::make_unique<TierFsm>(fsm_cfg_);

  // [DCN-2026-006 EXT D-025] FollowerTarget QoS burst tolerance.
  //
  // Formation publishes FollowerTargetMessage at 10 Hz (P0 contract).
  // Under WiFi 6 reconnect or DDS discovery storms (observed in
  // v1.5.1 Gate-3 dry-run) ~150 ms bursts of >5 backlogged messages
  // were dropped by the previous depth=20 reliable queue once the
  // publisher caught up. Symptom: follower 1-2 sample stall (~200 ms)
  // → slot drift visible in KPP-1 alignment measurement.
  //
  // Fix:
  //   - depth 20 → 50           (250 ms of headroom at 200 ms cycle)
  //   - keep_last + reliable    (unchanged)
  //   - history flush on reconnect handled by RMW; no app-side dedup
  //     (msg.target_sequence_id already monotonic — onFollowerTarget
  //      drops out-of-order in-band).
  //
  // Trade-off: ~2 KB extra heap per subscriber. Acceptable.
  target_sub_ = create_subscription<combat_robot_msgs::msg::FollowerTargetMessage>(
    "/swarm/formation/follower_target",
    rclcpp::QoS(rclcpp::KeepLast(50)).reliable(),
    std::bind(&TierNode::onFollowerTarget, this, std::placeholders::_1));

  // Phase 6: subscribe to the fleet's RobotStatus topic.
  // Pre-patch this was `~/robot_status` (private topic =
  // `/tier_node/robot_status`) with no producer in the system →
  // current_x_/current_y_ never updated → delta_m always 0 → FSM
  // stuck at T0/T1 → KPP-2 evidence invalid.
  // The fleet (leader_role_manager, hub_health_monitor, etc.) all
  // use `/swarm/robot_status`, so subscribe there and filter by
  // robot_id in onRobotStatus().
  status_sub_ = create_subscription<combat_robot_msgs::msg::RobotStatus>(
    "/swarm/robot_status",
    rclcpp::QoS(20).best_effort(),
    std::bind(&TierNode::onRobotStatus, this, std::placeholders::_1));

  slot_sub_ = create_subscription<combat_robot_msgs::msg::SlotAssignment>(
    "/swarm/formation/slot_assignment",
    rclcpp::QoS(5).reliable().transient_local(),
    std::bind(&TierNode::onSlotAssignment, this, std::placeholders::_1));

  obstacle_sub_ = create_subscription<std_msgs::msg::Bool>(
    "~/obstacle_on_path",
    rclcpp::QoS(5).reliable(),
    std::bind(&TierNode::onObstacleOnPath, this, std::placeholders::_1));

  // PATCH 2026-05-13: real subscriptions for comm + breadcrumb.
  comm_link_sub_ = create_subscription<std_msgs::msg::Bool>(
    "~/comm_link_status",
    rclcpp::QoS(5).reliable(),
    std::bind(&TierNode::onCommLinkStatus, this, std::placeholders::_1));

  breadcrumb_sub_ = create_subscription<std_msgs::msg::Bool>(
    "~/breadcrumb",
    rclcpp::QoS(5).reliable(),
    std::bind(&TierNode::onBreadcrumb, this, std::placeholders::_1));

  tier_pub_ = create_publisher<combat_robot_msgs::msg::TierStatusChange>(
    "~/tier_status_change",
    rclcpp::QoS(20).reliable());

  tick_timer_ = create_wall_timer(
    std::chrono::milliseconds(tick_period_ms_),
    std::bind(&TierNode::onTick, this));

  RCLCPP_INFO(
    get_logger(),
    "TierNode UP: robot_id=%u tick=%ums d0=%.1fm "
    "catchup=%.2f hard=%.2f breadcrumb=%.2f comm_timeout=%ums",
    robot_id_, tick_period_ms_, base_distance_d0_m_,
    fsm_cfg_.catch_up_ratio, fsm_cfg_.hard_catch_up_ratio,
    fsm_cfg_.breadcrumb_ratio, fsm_cfg_.comm_timeout_ms);
}

void TierNode::declareParameters()
{
  declare_parameter<int>("robot_id", 1);
  declare_parameter<int>("tick_period_ms", 100);
  declare_parameter<double>("catch_up_ratio", 1.5);
  declare_parameter<double>("hard_catch_up_ratio", 2.0);
  declare_parameter<double>("breadcrumb_ratio", 4.0);
  declare_parameter<double>("hysteresis_ratio", 0.1);
  declare_parameter<int>("comm_timeout_ms", 60000);
  declare_parameter<int>("auto_reroute_min_dwell_ms", 100);
  declare_parameter<int>("catch_up_min_dwell_ms", 200);
  declare_parameter<int>("hard_catch_up_min_dwell_ms", 200);
  declare_parameter<int>("breadcrumb_min_dwell_ms", 500);
  declare_parameter<double>("base_distance_d0_m", 5.0);
}

void TierNode::loadParameters()
{
  robot_id_ = static_cast<uint32_t>(get_parameter("robot_id").as_int());
  tick_period_ms_ =
    static_cast<uint32_t>(get_parameter("tick_period_ms").as_int());
  fsm_cfg_.catch_up_ratio =
    static_cast<float>(get_parameter("catch_up_ratio").as_double());
  fsm_cfg_.hard_catch_up_ratio =
    static_cast<float>(get_parameter("hard_catch_up_ratio").as_double());
  fsm_cfg_.breadcrumb_ratio =
    static_cast<float>(get_parameter("breadcrumb_ratio").as_double());
  fsm_cfg_.hysteresis_ratio =
    static_cast<float>(get_parameter("hysteresis_ratio").as_double());
  fsm_cfg_.comm_timeout_ms =
    static_cast<uint32_t>(get_parameter("comm_timeout_ms").as_int());
  fsm_cfg_.auto_reroute_min_dwell_ms =
    static_cast<uint32_t>(get_parameter("auto_reroute_min_dwell_ms").as_int());
  fsm_cfg_.catch_up_min_dwell_ms =
    static_cast<uint32_t>(get_parameter("catch_up_min_dwell_ms").as_int());
  fsm_cfg_.hard_catch_up_min_dwell_ms =
    static_cast<uint32_t>(get_parameter("hard_catch_up_min_dwell_ms").as_int());
  fsm_cfg_.breadcrumb_min_dwell_ms =
    static_cast<uint32_t>(get_parameter("breadcrumb_min_dwell_ms").as_int());
  base_distance_d0_m_ =
    static_cast<float>(get_parameter("base_distance_d0_m").as_double());
}

// ─── Subscription callbacks ────────────────────────────────────────────

void TierNode::onFollowerTarget(
  const combat_robot_msgs::msg::FollowerTargetMessage::SharedPtr msg)
{
  if (msg->target_robot_id != robot_id_) {return;}
  std::lock_guard<std::mutex> g(state_mu_);
  last_target_ms_ = toMs(now());
  target_x_ = static_cast<float>(msg->target_pose_now.position.x);
  target_y_ = static_cast<float>(msg->target_pose_now.position.y);
}

void TierNode::onRobotStatus(
  const combat_robot_msgs::msg::RobotStatus::SharedPtr msg)
{
  if (msg->robot_id != robot_id_) {return;}
  std::lock_guard<std::mutex> g(state_mu_);
  current_x_ = static_cast<float>(msg->pose.position.x);
  current_y_ = static_cast<float>(msg->pose.position.y);
}

void TierNode::onSlotAssignment(
  const combat_robot_msgs::msg::SlotAssignment::SharedPtr msg)
{
  std::lock_guard<std::mutex> g(state_mu_);
  base_distance_d0_m_ = msg->spacing_d;
  // R-6 deep-dive (M10): SlotAssignment establishes the AUTHORITATIVE
  // target. FollowerTarget messages (which arrive at 10 Hz) overwrite
  // it with predicted-future pose. The latest message wins regardless
  // of source — this is intentional, since FollowerTarget carries the
  // velocity-extrapolated target_pose_now while SlotAssignment carries
  // the static slot center. The tick loop uses whichever is freshest.
  //
  // Static-analysis hardening (merged from main): parallel-array indexing
  // on three vectors (robot_ids / slot_x_m / slot_y_m). A malformed
  // publisher could send mismatched lengths, in which case slot_x_m[i]
  // / slot_y_m[i] would be OOB on an i that's valid for robot_ids.
  // Verify all three sizes agree before using any of them.
  const size_t n = msg->robot_ids.size();
  if (msg->slot_x_m.size() != n || msg->slot_y_m.size() != n) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "SlotAssignment size mismatch: robot_ids=%zu slot_x_m=%zu "
      "slot_y_m=%zu — dropping",
      n, msg->slot_x_m.size(), msg->slot_y_m.size());
    return;
  }
  for (size_t i = 0; i < n; ++i) {
    if (msg->robot_ids[i] == robot_id_) {
      target_x_ = msg->slot_x_m[i];
      target_y_ = msg->slot_y_m[i];
      break;
    }
  }
}

void TierNode::onObstacleOnPath(const std_msgs::msg::Bool::SharedPtr msg)
{
  std::lock_guard<std::mutex> g(state_mu_);
  obstacle_on_path_ = msg->data;
}

// PATCH 2026-05-13.
void TierNode::onCommLinkStatus(const std_msgs::msg::Bool::SharedPtr msg)
{
  comm_health_.observeCommLink(toMs(now()), msg->data);
}

void TierNode::onBreadcrumb(const std_msgs::msg::Bool::SharedPtr msg)
{
  if (msg->data) {
    comm_health_.observeBreadcrumb(toMs(now()));
  }
}

// ─── Tick ──────────────────────────────────────────────────────────────

void TierNode::onTick()
{
  PublishSnapshot snap;
  {
    std::lock_guard<std::mutex> g(state_mu_);

    // PATCH 2026-05-13: real elapsed dt (was nominal 100ms hardcode).
    const auto now_steady = std::chrono::steady_clock::now();
    uint32_t dt_ms = tick_period_ms_;
    if (last_tick_) {
      const auto delta = now_steady - *last_tick_;
      dt_ms = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(delta).count());
      if (dt_ms == 0) {
        dt_ms = 1;                           // guard against same-instant ticks
      }
      if (dt_ms > 1000) {
        dt_ms = 1000;                         // huge dt cap (sched stall)
      }
    }
    last_tick_ = now_steady;

    // Build TierInput.
    TierInput in;
    const uint64_t now_ms = toMs(now());

    // R-6 deep-dive (C3): freshness threshold derives from tick rate.
    // 5× tick period covers transient delivery jitter without
    // over-classifying late predictions as fresh.
    const uint32_t fresh_threshold_ms = tick_period_ms_ * 5;

    // Static-analysis hardening (merged from main): now_ms -
    // last_target_ms_ is unsigned; if a sim-time reset or NTP step
    // makes last_target_ms_ > now_ms, the subtraction wraps to ~2^64,
    // mis-classifying the predictor as ancient and forcing T4. Treat
    // any "future" timestamp as fresh (age 0) rather than
    // catastrophically stale.
    uint64_t age_ms;
    if (last_target_ms_ == 0) {
      age_ms = UINT64_MAX;          // never received
    } else if (now_ms >= last_target_ms_) {
      age_ms = now_ms - last_target_ms_;
    } else {
      age_ms = 0;                   // clock stepped backwards — assume fresh
    }
    in.prediction_received = (age_ms <= fresh_threshold_ms);
    in.prediction_loss_ms = (age_ms == UINT64_MAX) ? 0u :
      static_cast<uint32_t>(
      std::min<uint64_t>(age_ms, UINT32_MAX));

    // R-6 deep-dive (C4 + C5): comm + breadcrumb from real source.
    const auto health = comm_health_.snapshot(now_ms);
    in.comm_link_alive = health.comm_link_alive;
    in.breadcrumb_available = health.breadcrumb_available;

    // R-6 deep-dive (M7): when no FollowerTarget has ever been
    // received, count the boot-anchored comm_loss instead of treating
    // it as 0 (which would make 60s comm timeout unreachable).
    if (last_target_ms_ == 0 && health.comm_loss_ms > in.prediction_loss_ms) {
      in.prediction_loss_ms = health.comm_loss_ms;
    }

    in.obstacle_on_path = obstacle_on_path_;
    in.base_distance_d0_m = base_distance_d0_m_;

    // δ = current to target
    const float dx = current_x_ - target_x_;
    const float dy = current_y_ - target_y_;
    in.delta_m = std::sqrt(dx * dx + dy * dy);

    // R-6 deep-dive (M9): step FSM INSIDE lock — TierFsm is not
    // thread-safe and a MultiThreadedExecutor would race with subs
    // otherwise. (Subsumes main's Phase 7 fix: dt_ms is now the
    // real measured wall-clock dt instead of nominal tick period.)
    const Tier prev = fsm_->currentTier();
    auto changed = fsm_->step(in, dt_ms);
    if (changed) {
      snap.emit = true;
      snap.previous = prev;
      snap.current = *changed;
      snap.reason = fsm_->lastReason();
      snap.input = in;

      // R-6 deep-dive (L11): KPP-2 latency probe.
      // If we are entering T1.5 and a trigger stamp is pending,
      // compute the obstacle-arrival → publish-time latency.
      if (*changed == Tier::T1_5) {
        const auto stamp = fsm_->pendingObstacleTriggerStamp();
        if (stamp) {
          const auto latency_ns = std::chrono::steady_clock::now() - *stamp;
          snap.obstacle_trigger_latency_ms = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
              latency_ns).count());
          fsm_->clearObstacleTriggerStamp();
        }
      }
    }
  }
  // ─── Lock released — publish outside ─────────────────────────────
  if (snap.emit) {
    publishTransition(snap);
  }
}

void TierNode::publishTransition(const PublishSnapshot & snap)
{
  combat_robot_msgs::msg::TierStatusChange msg;
  msg.header.stamp = now();
  msg.header.frame_id = "world";
  msg.robot_id = robot_id_;
  msg.previous_tier = static_cast<uint8_t>(snap.previous);
  msg.current_tier = static_cast<uint8_t>(snap.current);
  msg.reason = snap.reason;
  msg.delta_m = snap.input.delta_m;
  msg.base_distance_d0_m = snap.input.base_distance_d0_m;
  msg.obstacle_detected = snap.input.obstacle_on_path;
  msg.prediction_loss_ms = snap.input.prediction_loss_ms;
  msg.timestamp_ms = toMs(now());

  // PATCH 2026-05-13: latency field added to TierStatusChange.msg
  // (PATCH_PDR-7 patch). Gate access in case msg version is older.
  // Uncomment when the field is confirmed present in the build.
  // if (snap.obstacle_trigger_latency_ms) {
  //   msg.transition_latency_ms = *snap.obstacle_trigger_latency_ms;
  // }

  tier_pub_->publish(msg);

  if (snap.obstacle_trigger_latency_ms) {
    RCLCPP_INFO(
      get_logger(),
      "Tier %s → %s (reason='%s' delta=%.2fm d0=%.2fm) ★ KPP-2 latency=%ums",
      tierName(snap.previous), tierName(snap.current),
      snap.reason.c_str(),
      snap.input.delta_m, snap.input.base_distance_d0_m,
      *snap.obstacle_trigger_latency_ms);
  } else {
    RCLCPP_INFO(
      get_logger(),
      "Tier %s → %s (reason='%s' delta=%.2fm d0=%.2fm)",
      tierName(snap.previous), tierName(snap.current),
      snap.reason.c_str(),
      snap.input.delta_m, snap.input.base_distance_d0_m);
  }
}

Tier TierNode::currentTierForTest() const
{
  std::lock_guard<std::mutex> g(state_mu_);
  return fsm_->currentTier();
}

}  // namespace san_follower_tier
