// SAN v1.5 — TierNode implementation.

#include "san_follower_tier/tier_node.hpp"

#include <chrono>
#include <cmath>

namespace san_follower_tier {

using namespace std::chrono_literals;

namespace {

uint64_t toMs(const rclcpp::Time& t) {
  // Static-analysis hardening: nanoseconds() is int64_t; before clock
  // init or sim-time without /clock, the value can be 0 or negative.
  // Unsigned cast would wrap a negative to ~2^64 and propagate the
  // pollution downstream (tier dwell, age computation).
  const auto ns = t.nanoseconds();
  return ns > 0 ? static_cast<uint64_t>(ns / 1'000'000LL) : 0ULL;
}

}  // namespace

TierNode::TierNode(const rclcpp::NodeOptions& opts)
    : rclcpp::Node("tier_node", opts) {
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

  status_sub_ = create_subscription<combat_robot_msgs::msg::RobotStatus>(
      "~/robot_status",
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

  tier_pub_ = create_publisher<combat_robot_msgs::msg::TierStatusChange>(
      "~/tier_status_change",
      rclcpp::QoS(20).reliable());

  tick_timer_ = create_wall_timer(
      std::chrono::milliseconds(tick_period_ms_),
      std::bind(&TierNode::onTick, this));

  RCLCPP_INFO(get_logger(),
      "TierNode UP: robot_id=%u tick=%ums d0=%.1fm "
      "catchup=%.2f hard=%.2f breadcrumb=%.2f comm_timeout=%ums",
      robot_id_, tick_period_ms_, base_distance_d0_m_,
      fsm_cfg_.catch_up_ratio, fsm_cfg_.hard_catch_up_ratio,
      fsm_cfg_.breadcrumb_ratio, fsm_cfg_.comm_timeout_ms);
}

void TierNode::declareParameters() {
  declare_parameter<int>("robot_id", 1);
  declare_parameter<int>("tick_period_ms", 100);
  declare_parameter<double>("catch_up_ratio", 1.5);
  declare_parameter<double>("hard_catch_up_ratio", 2.0);
  declare_parameter<double>("breadcrumb_ratio", 4.0);
  declare_parameter<double>("hysteresis_ratio", 0.1);
  declare_parameter<int>("comm_timeout_ms", 60000);
  declare_parameter<int>("auto_reroute_min_dwell_ms", 100);
  declare_parameter<double>("base_distance_d0_m", 5.0);
}

void TierNode::loadParameters() {
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
      static_cast<uint32_t>(
          get_parameter("auto_reroute_min_dwell_ms").as_int());
  base_distance_d0_m_ =
      static_cast<float>(get_parameter("base_distance_d0_m").as_double());
}

// ─── Subscription callbacks ────────────────────────────────────────────

void TierNode::onFollowerTarget(
    const combat_robot_msgs::msg::FollowerTargetMessage::SharedPtr msg) {
  if (msg->target_robot_id != robot_id_) return;  // not for us
  std::lock_guard<std::mutex> g(state_mu_);
  last_target_ms_ = toMs(now());
  target_x_       = static_cast<float>(msg->target_pose_now.position.x);
  target_y_       = static_cast<float>(msg->target_pose_now.position.y);
}

void TierNode::onRobotStatus(
    const combat_robot_msgs::msg::RobotStatus::SharedPtr msg) {
  if (msg->robot_id != robot_id_) return;
  std::lock_guard<std::mutex> g(state_mu_);
  current_x_ = static_cast<float>(msg->pose.position.x);
  current_y_ = static_cast<float>(msg->pose.position.y);
}

void TierNode::onSlotAssignment(
    const combat_robot_msgs::msg::SlotAssignment::SharedPtr msg) {
  std::lock_guard<std::mutex> g(state_mu_);
  base_distance_d0_m_ = msg->spacing_d;

  // Static-analysis hardening: parallel-array indexing on three vectors
  // (robot_ids / slot_x_m / slot_y_m). A malformed publisher could send
  // mismatched lengths, in which case msg->slot_x_m[i] / slot_y_m[i]
  // would be OOB on an i that's valid for robot_ids. Verify all three
  // sizes agree before using any of them.
  const size_t n = msg->robot_ids.size();
  if (msg->slot_x_m.size() != n || msg->slot_y_m.size() != n) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
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

void TierNode::onObstacleOnPath(
    const std_msgs::msg::Bool::SharedPtr msg) {
  std::lock_guard<std::mutex> g(state_mu_);
  obstacle_on_path_ = msg->data;
}

// ─── 100ms tick ────────────────────────────────────────────────────────

void TierNode::onTick() {
  TierInput in;
  {
    std::lock_guard<std::mutex> g(state_mu_);
    const uint64_t now_ms = toMs(now());

    // Prediction age — anything fresher than 500ms (5× tick) counts as
    // "received". Older = lost.
    //
    // Static-analysis hardening: now_ms - last_target_ms_ is unsigned;
    // if a sim-time reset or NTP step makes last_target_ms_ > now_ms,
    // the subtraction wraps to ~2^64, mis-classifying the predictor as
    // ancient and forcing T4. Treat any "future" timestamp as fresh
    // (age 0) rather than catastrophically stale.
    uint64_t age_ms;
    if (last_target_ms_ == 0) {
      age_ms = UINT64_MAX;          // never received
    } else if (now_ms >= last_target_ms_) {
      age_ms = now_ms - last_target_ms_;
    } else {
      age_ms = 0;                   // clock stepped backwards — assume fresh
    }
    in.prediction_received = (age_ms <= 500);
    in.prediction_loss_ms  = (age_ms == UINT64_MAX) ? 0 :
                              static_cast<uint32_t>(age_ms);

    in.comm_link_alive      = true;  // PDR-5: subscribe to /comm_link/status
    in.obstacle_on_path     = obstacle_on_path_;
    in.base_distance_d0_m   = base_distance_d0_m_;
    in.breadcrumb_available = true;  // PDR-5: track from breadcrumb sub

    // δ = current to target
    const float dx = current_x_ - target_x_;
    const float dy = current_y_ - target_y_;
    in.delta_m = std::sqrt(dx * dx + dy * dy);
  }

  const Tier prev = fsm_->currentTier();
  // Phase 7: pass the actual tick period so anti-flap dwell uses
  // real wall time (was hardcoded 100ms inside fsm step).
  auto changed = fsm_->step(in, tick_period_ms_);
  if (changed) {
    publishTransition(prev, *changed, fsm_->lastReason(), in);
  }
}

void TierNode::publishTransition(Tier prev, Tier curr,
                                  const std::string& reason,
                                  const TierInput&   in) {
  combat_robot_msgs::msg::TierStatusChange msg;
  msg.header.stamp        = now();
  msg.header.frame_id     = "world";
  msg.robot_id            = robot_id_;
  msg.previous_tier       = static_cast<uint8_t>(prev);
  msg.current_tier        = static_cast<uint8_t>(curr);
  msg.reason              = reason;
  msg.delta_m             = in.delta_m;
  msg.base_distance_d0_m  = in.base_distance_d0_m;
  msg.obstacle_detected   = in.obstacle_on_path;
  msg.prediction_loss_ms  = in.prediction_loss_ms;
  msg.timestamp_ms        = toMs(now());

  tier_pub_->publish(msg);

  RCLCPP_INFO(get_logger(),
      "Tier %s → %s (reason='%s' delta=%.2fm d0=%.2fm)",
      tierName(prev), tierName(curr), reason.c_str(),
      in.delta_m, in.base_distance_d0_m);
}

}  // namespace san_follower_tier
