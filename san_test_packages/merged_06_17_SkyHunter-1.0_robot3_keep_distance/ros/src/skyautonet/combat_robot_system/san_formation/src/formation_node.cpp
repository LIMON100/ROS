// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — FormationNode implementation (patched 2026-05-13).
//
// PATCH 2026-05-13 (Formation deep-dive):
//   * Velocity finite-difference now actually runs (per-robot estimator).
//   * Cost matrix uses formation_planner::buildCostMatrix() with proper
//     frame transform (leader-local → world).
//   * leader_robot_id_ anchors the slot frame; slots stored in
//     leader-local so we can re-transform every 10 Hz tick.
//   * Publishers invoked OUTSIDE state_mu_ (snapshot under lock, publish
//     after release) — eliminates hot-path lock contention.
//   * v1.5 9-formation IDs (1..9) accepted directly; v1.3 0..4 still
//     supported as legacy via a separate `legacy_formation` field check.
//   * Hungarian failure → FormationStatus.replan_failed flag (was silent).
//   * 1-second prediction (SDD §6.3) uses leader velocity + slot
//     prediction in formation_planner::predictSlotAhead.

#include "san_formation/formation_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#include "san_formation/hungarian.hpp"

namespace san_formation
{

using namespace std::chrono_literals;
using SlotMsg = combat_robot_msgs::msg::SlotAssignment;
using TargetMsg = combat_robot_msgs::msg::FollowerTargetMessage;
using StatusMsg = combat_robot_msgs::msg::FormationStatus;
using CmdMsg = combat_robot_msgs::msg::FormationCommand;
using StatusSubMsg = combat_robot_msgs::msg::RobotStatus;

namespace
{

// Convert yaw to quaternion (z-axis only).
void yawToQuat(float yaw, double & qx, double & qy, double & qz, double & qw)
{
  const float half = 0.5f * yaw;
  qx = 0.0;
  qy = 0.0;
  qz = std::sin(half);
  qw = std::cos(half);
}

// Extract yaw (rad) from a quaternion (z-axis only).
float quatToYaw(const geometry_msgs::msg::Quaternion & q)
{
  // siny_cosp / cosy_cosp
  const double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
  const double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
  return static_cast<float>(std::atan2(siny_cosp, cosy_cosp));
}

}  // namespace

FormationNode::FormationNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("formation_node", opts)
{
  declareParameters();
  loadParameters();

  formation_cmd_sub_ = create_subscription<CmdMsg>(
    "~/formation_command", rclcpp::QoS(5).reliable(),
    std::bind(
      &FormationNode::onFormationCommand, this,
      std::placeholders::_1));

  robot_status_sub_ = create_subscription<StatusSubMsg>(
    "/swarm/robot_status",
    rclcpp::QoS(20).best_effort(),
    std::bind(
      &FormationNode::onRobotStatus, this,
      std::placeholders::_1));

  // DCN-2026-026 C-2 — Encircle combat trigger: hub-aggregated threat
  // alerts + operator confirm (App 1-tap over rosbridge).
  threat_sub_ = create_subscription<combat_robot_msgs::msg::ThreatAlert>(
    get_parameter("threat_alert_topic").as_string(),
    rclcpp::QoS(20).reliable(),
    std::bind(
      &FormationNode::onThreatAlert, this,
      std::placeholders::_1));
  encircle_confirm_sub_ = create_subscription<std_msgs::msg::Bool>(
    get_parameter("encircle_confirm_topic").as_string(),
    rclcpp::QoS(5).reliable(),
    std::bind(
      &FormationNode::onEncircleConfirm, this,
      std::placeholders::_1));

  slot_pub_ = create_publisher<SlotMsg>(
    "~/slot_assignment", rclcpp::QoS(5).reliable().transient_local());
  // [DCN-2026-006 EXT D-025] depth 20 → 50 — match subscriber side
  // (san_follower_tier) so reliable transport keeps burst headroom
  // symmetric. 10 Hz cadence × 0.25 s = 2.5 messages buffered worst-
  // case; depth=50 gives ample margin for WiFi 6 reconnect bursts.
  target_pub_ = create_publisher<TargetMsg>(
    "~/follower_target", rclcpp::QoS(rclcpp::KeepLast(50)).reliable());
  status_pub_ = create_publisher<StatusMsg>(
    "~/formation_status", rclcpp::QoS(5).reliable());

  // Re-plan check at 1 Hz (cheap), broadcast targets at 10 Hz (P0).
  assign_timer_ = create_wall_timer(
    1s, std::bind(&FormationNode::onAssignTick, this));
  target_timer_ = create_wall_timer(
    100ms, std::bind(&FormationNode::onFollowerTargetTick, this));
  status_timer_ = create_wall_timer(
    1s, std::bind(&FormationNode::onStatusTick, this));

  RCLCPP_INFO(
    get_logger(),
    "FormationNode UP: form=%d preset=%u d=%.1fm θ=%.1f° leader=%u "
    "realign_thr=%.2fm pred_horizon=%.1fs",
    static_cast<int>(current_formation_), current_preset_,
    spacing_d_m_, spread_theta_deg_, leader_robot_id_,
    realign_threshold_m_, prediction_horizon_s_);
}

// ─── Parameters ─────────────────────────────────────────────────────────

void FormationNode::declareParameters()
{
  declare_parameter<int>("initial_formation_id", 3);    // VShape
  declare_parameter<int>("initial_preset_id", 2);       // recon_defence
  declare_parameter<int>("leader_robot_id", 1);
  declare_parameter<double>("realign_threshold_m", 2.0);
  declare_parameter<double>("max_speed_recon_mps", 1.3);
  declare_parameter<double>("lead_bias_s", 0.1);
  declare_parameter<double>("prediction_horizon_s", 1.0);    // SDD §6.3

  // DCN-2026-026 C-2 — Encircle combat (ratified 2026-06-10:
  // operator confirm is the DEFAULT; auto engage is explicit opt-in).
  declare_parameter<double>("encircle_radius_m", 7.0);
  declare_parameter<bool>("encircle_auto", false);
  declare_parameter<double>("encircle_min_confidence", 0.9);
  declare_parameter<double>("encircle_ttl_s", 10.0);
  declare_parameter<double>("encircle_reentry_s", 5.0);
  declare_parameter<std::string>("threat_alert_topic", "/hub/threat_alert");
  declare_parameter<std::string>(
    "encircle_confirm_topic", "/operator/encircle_confirm");
}

void FormationNode::loadParameters()
{
  const auto fid = static_cast<uint8_t>(
    get_parameter("initial_formation_id").as_int());
  current_formation_ = fromMessageId(fid);

  current_preset_ = static_cast<uint8_t>(
    get_parameter("initial_preset_id").as_int());
  auto preset = getPreset(current_preset_);
  if (!preset.name.empty()) {
    spacing_d_m_ = preset.spacing_d_m;
    spread_theta_deg_ = preset.spread_theta_deg;
  }

  leader_robot_id_ = static_cast<uint32_t>(
    get_parameter("leader_robot_id").as_int());
  realign_threshold_m_ =
    static_cast<float>(get_parameter("realign_threshold_m").as_double());
  max_speed_recon_ =
    static_cast<float>(get_parameter("max_speed_recon_mps").as_double());
  lead_bias_s_ =
    static_cast<float>(get_parameter("lead_bias_s").as_double());
  prediction_horizon_s_ =
    static_cast<float>(get_parameter("prediction_horizon_s").as_double());

  EncircleConfig ec;
  ec.min_confidence =
    static_cast<float>(get_parameter("encircle_min_confidence").as_double());
  ec.auto_engage = get_parameter("encircle_auto").as_bool();
  ec.ttl_ms = static_cast<uint32_t>(
    get_parameter("encircle_ttl_s").as_double() * 1000.0);
  ec.reentry_block_ms = static_cast<uint32_t>(
    get_parameter("encircle_reentry_s").as_double() * 1000.0);
  encircle_ = EncircleCombat(ec);
  encircle_radius_m_ =
    static_cast<float>(get_parameter("encircle_radius_m").as_double());
}

// ─── Subscriptions ──────────────────────────────────────────────────────

void FormationNode::onFormationCommand(const CmdMsg::SharedPtr msg)
{
  // Snapshot work + plan; publish after lock release (PATCH 2026-05-13).
  PlanResult plan;
  {
    std::lock_guard<std::mutex> g(state_mu_);

    // FormationCommand.msg uses v1.5 IDs (1..9), aligned with
    // FormationStatus.msg. Pre-PDR v1.3 IDs (0..4) are no longer
    // accepted (see FormationCommand.msg header).
    const uint8_t new_fid = static_cast<uint8_t>(msg->formation);
    if (new_fid < 1 || new_fid > 9) {
      RCLCPP_WARN(
        get_logger(),
        "Ignoring FormationCommand with out-of-range formation=%u "
        "(valid: 1..9 per v1.5 IDS)", new_fid);
      return;
    }
    current_formation_ = fromMessageId(new_fid);
    auto preset = getPreset(current_preset_);
    if (!preset.name.empty()) {
      spacing_d_m_ = preset.spacing_d_m;
      spread_theta_deg_ = preset.spread_theta_deg;
    }
    RCLCPP_INFO(
      get_logger(),
      "Formation change: form=%u preset=%u d=%.1f θ=%.1f",
      new_fid, current_preset_, spacing_d_m_, spread_theta_deg_);

    plan = recomputeAssignmentLocked();
  }
  // ─── Lock released — publish outside ─────────────────────────────
  if (plan.success) {
    slot_pub_->publish(plan.assignment_msg);
  } else if (!plan.error_reason.empty()) {
    RCLCPP_ERROR(
      get_logger(),
      "Replan after formation change failed: %s",
      plan.error_reason.c_str());
  }
}

void FormationNode::onRobotStatus(const StatusSubMsg::SharedPtr msg)
{
  std::lock_guard<std::mutex> g(state_mu_);
  // Static-analysis hardening (merged from main PATCH 2026-05-13):
  // stamp.sec is int32, so negative stamps from a malformed publisher
  // or pre-/clock sim time would wrap when cast to uint64 and poison
  // the dt-based VelocityEstimator. rclcpp::Time normalizes unset
  // stamps; reject non-positive nanoseconds before mutating snapshot.
  const int64_t stamp_ns = rclcpp::Time(msg->header.stamp).nanoseconds();
  if (stamp_ns <= 0) {return;}

  // [Tier2 audit 2026-05-24 P2-1] Reject non-finite poses at the source.
  // A single robot publishing NaN/Inf poisons the cost matrix → Hungarian
  // throws → replan permanently fails until that publisher recovers.
  // Worse, VelocityEstimator's LPF state has no NaN reset path, so a
  // single bad sample contaminates subsequent velocity estimates.
  const double px = msg->pose.position.x;
  const double py = msg->pose.position.y;
  const float yaw = quatToYaw(msg->pose.orientation);
  if (!std::isfinite(px) || !std::isfinite(py) || !std::isfinite(yaw)) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "robot_id=%u sent non-finite pose (x=%f y=%f yaw=%f) — "
      "snapshot skipped",
      msg->robot_id, px, py, static_cast<double>(yaw));
    return;
  }

  auto & snap = robot_poses_[msg->robot_id];
  snap.pose.x = static_cast<float>(px);
  snap.pose.y = static_cast<float>(py);
  snap.pose.yaw = yaw;
  snap.timestamp_ms = static_cast<uint64_t>(stamp_ns / 1'000'000LL);
  // R-4 deep-dive: per-robot VelocityEstimator (finite-diff + LPF) lives
  // inside the PoseSnap; one call per msg.
  snap.vel_est.update(snap.timestamp_ms, snap.pose);
}

// ─── DCN-2026-026 C-2 — Encircle combat inputs ─────────────────────────

void FormationNode::onThreatAlert(
  const combat_robot_msgs::msg::ThreatAlert::SharedPtr msg)
{
  PlanResult plan;
  bool entered_pending = false;
  {
    std::lock_guard<std::mutex> g(state_mu_);

    // Gate #1/#2/#3 — non-qualifying alerts are ignored entirely (in
    // particular they can never CLEAR an active combat: release is
    // TTL / operator only, DCN-2026-026 C-2).
    const auto confidence = parseConfidenceFromDetail(msg->detail);
    if (!passesEncircleGate(
        msg->severity, msg->threat_type, confidence,
        msg->has_position, msg->range_m,
        encircle_.config().min_confidence))
    {
      return;
    }

    // World point from the REPORTING robot's pose — never the
    // leader's (the original defbb64 defect encircled the wrong
    // point whenever a non-leader robot detected the threat).
    const auto reporter_id = parseRobotIdString(msg->source_robot_id);
    if (!reporter_id) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "Encircle-qualifying alert from non-robot reporter '%s' — "
        "cannot localize, ignored", msg->source_robot_id.c_str());
      return;
    }
    const auto it = robot_poses_.find(*reporter_id);
    if (it == robot_poses_.end()) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "Encircle-qualifying alert from robot %u with no known pose — "
        "ignored", *reporter_id);
      return;
    }
    const auto [tx, ty] = threatWorldXY(
      it->second.pose.x, it->second.pose.y,
      msg->bearing_deg, msg->range_m);

    const uint64_t now_ms =
      static_cast<uint64_t>(now().nanoseconds() / 1'000'000ULL);
    const EncirclePhase before = encircle_.phase();
    encircle_.onQualifiedThreat(tx, ty, now_ms);
    const EncirclePhase after = encircle_.phase();

    if (after == EncirclePhase::Active && before != EncirclePhase::Active) {
      plan = recomputeAssignmentLocked();   // auto_engage path
    }
    entered_pending =
      (after == EncirclePhase::PendingConfirm &&
      before == EncirclePhase::Idle);
  }
  if (entered_pending) {
    RCLCPP_WARN(
      get_logger(),
      "Encircle trigger armed — 운용자 승인 대기 (publish true on "
      "the encircle confirm topic to engage)");
  }
  if (plan.success) {
    RCLCPP_WARN(get_logger(), "ENCIRCLE ENGAGED (auto) — combat slots active");
    slot_pub_->publish(plan.assignment_msg);
  }
}

void FormationNode::onEncircleConfirm(const std_msgs::msg::Bool::SharedPtr msg)
{
  PlanResult plan;
  bool changed = false;
  {
    std::lock_guard<std::mutex> g(state_mu_);
    const uint64_t now_ms =
      static_cast<uint64_t>(now().nanoseconds() / 1'000'000ULL);
    changed = msg->data ?
      encircle_.onOperatorConfirm(now_ms) :
      encircle_.onOperatorRelease(now_ms);
    if (changed) {
      plan = recomputeAssignmentLocked();   // engage OR return-to-formation
    }
  }
  if (changed) {
    RCLCPP_WARN(
      get_logger(), "ENCIRCLE %s by operator",
      msg->data ? "ENGAGED" : "RELEASED");
  }
  if (plan.success) {
    slot_pub_->publish(plan.assignment_msg);
  }
}

// ─── Periodic ───────────────────────────────────────────────────────────

void FormationNode::onAssignTick()
{
  PlanResult plan;
  {
    std::lock_guard<std::mutex> g(state_mu_);
    if (robot_poses_.size() < 2) {return;}

    // DCN-2026-026 C-2: time-driven combat decay (TTL → Cooldown,
    // Cooldown → Idle). A phase change re-plans immediately so the
    // followers leave/return without waiting for drift to exceed the
    // realign threshold.
    const uint64_t now_ms =
      static_cast<uint64_t>(now().nanoseconds() / 1'000'000ULL);
    if (encircle_.tick(now_ms)) {
      RCLCPP_WARN(
        get_logger(), "Encircle phase → %u — replanning",
        static_cast<unsigned>(encircle_.phase()));
      plan = recomputeAssignmentLocked();
    } else if (current_assignment_.empty()) {
      // If no current assignment yet, force one
      plan = recomputeAssignmentLocked();
    } else {
      // Check alignment error — if avg > threshold, re-plan.
      // Anchor = combat anchor while encircle is Active, else leader
      // (DCN-2026-026 C-2 anchor unification).
      if (encircle_.phase() != EncirclePhase::Active &&
        robot_poses_.find(leader_robot_id_) == robot_poses_.end())
      {
        return;
      }
      const PoseXY anchor = slotAnchorLocked();

      float total_err = 0.0f;
      uint32_t n = 0;
      for (const auto & slot : current_assignment_) {
        auto it = robot_poses_.find(slot.robot_id);
        if (it == robot_poses_.end()) {continue;}
        // Transform slot local → world for fair distance comparison.
        float sx_w, sy_w;
        slotLocalToWorld(
          slot.slot_local_x, slot.slot_local_y,
          anchor, sx_w, sy_w);
        const float dx = it->second.pose.x - sx_w;
        const float dy = it->second.pose.y - sy_w;
        total_err += std::sqrt(dx * dx + dy * dy);
        ++n;
      }
      if (n == 0) {return;}
      const float avg_err = total_err / static_cast<float>(n);
      if (avg_err > realign_threshold_m_) {
        RCLCPP_INFO(
          get_logger(),
          "Avg alignment error %.2fm > %.2fm — replanning",
          avg_err, realign_threshold_m_);
        plan = recomputeAssignmentLocked();
      }
    }
  }
  if (plan.success) {
    slot_pub_->publish(plan.assignment_msg);
  }
}

void FormationNode::onFollowerTargetTick()
{
  // Build under lock, publish outside.
  std::vector<TargetMsg> messages;
  {
    std::lock_guard<std::mutex> g(state_mu_);
    messages = buildFollowerTargetsLocked();
  }
  for (const auto & m : messages) {
    target_pub_->publish(m);
  }
}

void FormationNode::onStatusTick()
{
  StatusMsg msg;
  {
    std::lock_guard<std::mutex> g(state_mu_);
    msg = buildStatusMsgLocked();
  }
  status_pub_->publish(msg);
}

// ─── Core algorithm (caller holds state_mu_) ────────────────────────────

FormationNode::PlanResult FormationNode::recomputeAssignmentLocked()
{
  PlanResult result;

  // DCN-2026-026 C-2: while encircle combat is Active the followers
  // take ring slots around the threat anchor; the leader keeps no
  // slot (it holds its own position — SDD §8.2 analogue for combat).
  const bool combat = encircle_.phase() == EncirclePhase::Active &&
    encircle_.anchor().has_value();

  // Build stable robot ID ordering for deterministic cost matrix.
  std::vector<uint32_t> robot_ids;
  std::vector<PoseXY> robots_world;
  robot_ids.reserve(robot_poses_.size());
  robots_world.reserve(robot_poses_.size());
  for (const auto & [id, snap] : robot_poses_) {
    if (combat && id == leader_robot_id_) {continue;}
    robot_ids.push_back(id);
    robots_world.push_back(snap.pose);
  }
  const size_t n = robot_ids.size();
  if (n == 0) {
    result.error_reason = combat ? "no followers for encircle" :
      "no robots known";
    last_plan_failed_ = true;
    return result;
  }

  PoseXY anchor_world;
  std::vector<SlotXY> slots_local;
  if (combat) {
    const auto a = *encircle_.anchor();
    anchor_world = PoseXY{a.first, a.second, 0.0f};   // world-aligned ring
    slots_local = encircleSlots(n, encircle_radius_m_);
  } else {
    // PATCH 2026-05-13: leader_robot_id_ anchors the local frame.
    // If the configured leader isn't present we cannot anchor the
    // formation safely — abort rather than re-anchoring on an arbitrary
    // robot (which would place every follower in the wrong region the
    // moment leader telemetry blips). Caller publishes replan_failed=1.
    auto leader_it = robot_poses_.find(leader_robot_id_);
    if (leader_it == robot_poses_.end()) {
      result.error_reason = "configured leader not in robot_poses_";
      last_plan_failed_ = true;
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000,
        "leader_robot_id=%u not yet reporting — skipping replan",
        leader_robot_id_);
      return result;
    }
    anchor_world = leader_it->second.pose;
    // Generate slots in LEADER LOCAL frame.
    slots_local = generateSlots(
      current_formation_, n, spacing_d_m_, spread_theta_deg_);
  }
  if (slots_local.size() != n) {
    result.error_reason = "slot count mismatch";
    last_plan_failed_ = true;
    return result;
  }

  // PATCH 2026-05-13: cost matrix with proper frame transform.
  auto cost = buildCostMatrix(robots_world, slots_local, anchor_world);

  // Solve Hungarian.
  std::vector<size_t> assignment;
  try {
    assignment = solveAssignment(cost);
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "Hungarian failed: %s", e.what());
    result.error_reason = e.what();
    last_plan_failed_ = true;
    return result;
  }

  // Update state.
  ++current_epoch_;
  current_assignment_.clear();
  for (size_t i = 0; i < n; ++i) {
    const size_t j = assignment[i];
    AssignedSlot s;
    s.robot_id = robot_ids[i];
    s.slot_index = static_cast<uint8_t>(j);
    s.slot_local_x = slots_local[j].x;
    s.slot_local_y = slots_local[j].y;
    current_assignment_.push_back(s);
  }
  last_total_cost_ = static_cast<float>(assignmentCost(cost, assignment));
  last_reassignment_ms_ =
    static_cast<uint64_t>(now().nanoseconds() / 1'000'000ULL);
  last_plan_failed_ = false;

  result.success = true;
  result.assignment_msg = buildAssignmentMsgLocked();

  RCLCPP_INFO(
    get_logger(),
    "Reassigned: epoch=%u n=%zu %s total_cost=%.2fm",
    current_epoch_, n,
    combat ? "ENCIRCLE" : "formation",
    last_total_cost_);
  return result;
}

PoseXY FormationNode::slotAnchorLocked() const
{
  // DCN-2026-026 C-2 — single anchor for cost/error/targets/status:
  // the threat anchor while combat is Active (yaw 0, world-aligned
  // ring), the leader pose otherwise.
  if (encircle_.phase() == EncirclePhase::Active) {
    if (const auto a = encircle_.anchor()) {
      return PoseXY{a->first, a->second, 0.0f};
    }
  }
  auto leader_it = robot_poses_.find(leader_robot_id_);
  if (leader_it == robot_poses_.end() && !robot_poses_.empty()) {
    leader_it = robot_poses_.begin();
  }
  return (leader_it != robot_poses_.end()) ?
         leader_it->second.pose : PoseXY{};
}

SlotMsg FormationNode::buildAssignmentMsgLocked() const
{
  SlotMsg m;
  m.header.stamp = now();
  m.header.frame_id = "world";
  m.epoch = current_epoch_;
  m.formation_id = toMessageId(current_formation_);
  m.spacing_d = spacing_d_m_;
  m.spread_theta_deg = spread_theta_deg_;
  m.total_cost_m = last_total_cost_;

  // Compose slots in world frame for the published assignment, so
  // consumers don't have to redo the transform. Anchor = combat
  // anchor while encircle is Active (DCN-2026-026 C-2).
  const PoseXY anchor = slotAnchorLocked();
  for (const auto & slot : current_assignment_) {
    float wx, wy;
    slotLocalToWorld(
      slot.slot_local_x, slot.slot_local_y,
      anchor, wx, wy);
    m.robot_ids.push_back(slot.robot_id);
    m.slot_x_m.push_back(wx);
    m.slot_y_m.push_back(wy);
    m.slot_index.push_back(slot.slot_index);
  }
  return m;
}

std::vector<TargetMsg> FormationNode::buildFollowerTargetsLocked()
{
  std::vector<TargetMsg> out;
  if (current_assignment_.empty()) {return out;}

  // DCN-2026-026 C-2: in encircle combat the targets sit on a STATIC
  // ring around the threat anchor — anchor velocity is zero so the
  // 1 s prediction degenerates to the same point (no leader needed).
  const bool combat = encircle_.phase() == EncirclePhase::Active;
  PoseXY anchor;
  Velocity2D anchor_vel{};
  if (combat) {
    anchor = slotAnchorLocked();
  } else {
    auto leader_it = robot_poses_.find(leader_robot_id_);
    if (leader_it == robot_poses_.end()) {return out;}
    anchor = leader_it->second.pose;
    // ★ PATCH 2026-05-13: real leader velocity (default 0 if estimator
    // hasn't seen enough samples yet).
    anchor_vel = leader_it->second.vel_est.latest().value_or(Velocity2D{});
  }
  const PoseXY leader = anchor;
  const Velocity2D leader_vel = anchor_vel;

  out.reserve(current_assignment_.size());
  for (const auto & slot : current_assignment_) {
    TargetMsg t;
    t.header.stamp = now();
    t.header.frame_id = "world";
    t.target_robot_id = slot.robot_id;
    t.formation_epoch = current_epoch_;

    const SlotXY local_slot{slot.slot_local_x, slot.slot_local_y};
    // "Now" target — transform slot into current leader frame.
    const auto now_pred = predictSlotAhead(
      local_slot, leader, leader_vel, 0.0f);
    t.target_pose_now.position.x = now_pred.world_x;
    t.target_pose_now.position.y = now_pred.world_y;
    yawToQuat(
      now_pred.world_yaw,
      t.target_pose_now.orientation.x,
      t.target_pose_now.orientation.y,
      t.target_pose_now.orientation.z,
      t.target_pose_now.orientation.w);

    // 1-second prediction — extrapolate leader pose with velocity.
    const auto fut_pred = predictSlotAhead(
      local_slot, leader, leader_vel, prediction_horizon_s_);
    t.target_pose_pred_1s.position.x = fut_pred.world_x;
    t.target_pose_pred_1s.position.y = fut_pred.world_y;
    yawToQuat(
      fut_pred.world_yaw,
      t.target_pose_pred_1s.orientation.x,
      t.target_pose_pred_1s.orientation.y,
      t.target_pose_pred_1s.orientation.z,
      t.target_pose_pred_1s.orientation.w);

    // ★ PATCH 2026-05-13: target velocity = leader velocity (constant
    // formation assumption). Previously was always 0.
    t.target_velocity.linear.x = leader_vel.vx;
    t.target_velocity.linear.y = leader_vel.vy;
    t.target_velocity.angular.z = leader_vel.wz;

    t.max_speed_mps = max_speed_recon_;
    t.lead_bias_s = lead_bias_s_;
    out.push_back(std::move(t));
  }
  return out;
}

StatusMsg FormationNode::buildStatusMsgLocked() const
{
  StatusMsg s;
  s.header.stamp = now();
  s.header.frame_id = "world";
  s.formation_id = toMessageId(current_formation_);
  s.preset_id = current_preset_;
  s.spacing_d_m = spacing_d_m_;
  s.spread_theta_deg = spread_theta_deg_;
  s.current_epoch = current_epoch_;
  s.last_reassignment_ms = last_reassignment_ms_;
  s.phase = phase_;
  // ★ PATCH 2026-05-13: surface replan_failed flag so the operator
  // UI can see silent Hungarian failures (FormationStatus.msg has the
  // bool field — see combat_robot_msgs).
  s.replan_failed = last_plan_failed_;
  // DCN-2026-026 C-2: surface combat to the operator UI via the
  // existing SDD §7.3 phase field.
  if (encircle_.phase() == EncirclePhase::Active) {
    s.phase = StatusMsg::PHASE_ENGAGE;
  }

  // KPP-1 alignment metrics — use proper frame transform. Anchor =
  // combat anchor while encircle is Active (DCN-2026-026 C-2 — the
  // leader-anchored metric would report bogus error during combat).
  float sum_err = 0.0f, max_err = 0.0f;
  uint8_t in_ct = 0, out_ct = 0;
  const PoseXY anchor = slotAnchorLocked();
  for (const auto & slot : current_assignment_) {
    auto it = robot_poses_.find(slot.robot_id);
    if (it == robot_poses_.end()) {continue;}
    float sx_w, sy_w;
    slotLocalToWorld(
      slot.slot_local_x, slot.slot_local_y,
      anchor, sx_w, sy_w);
    const float dx = it->second.pose.x - sx_w;
    const float dy = it->second.pose.y - sy_w;
    const float e = std::sqrt(dx * dx + dy * dy);
    sum_err += e;
    if (e > max_err) {max_err = e;}
    if (e < realign_threshold_m_) {++in_ct;} else {++out_ct;}
  }
  const uint32_t n = static_cast<uint32_t>(current_assignment_.size());
  s.avg_alignment_error_m = n > 0 ? sum_err / n : 0.0f;
  s.max_alignment_error_m = max_err;
  s.robots_in_formation = in_ct;
  s.robots_out_of_formation = out_ct;
  return s;
}

// ─── Test accessors ─────────────────────────────────────────────────────

uint32_t FormationNode::currentEpochForTest() const
{
  std::lock_guard<std::mutex> g(state_mu_);
  return current_epoch_;
}

std::size_t FormationNode::robotCountForTest() const
{
  std::lock_guard<std::mutex> g(state_mu_);
  return robot_poses_.size();
}

}  // namespace san_formation
