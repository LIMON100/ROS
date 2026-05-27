// SAN v1.5 — FormationNode implementation.

#include "san_formation/formation_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#include "san_formation/hungarian.hpp"

namespace san_formation {

using namespace std::chrono_literals;
using SlotMsg    = combat_robot_msgs::msg::SlotAssignment;
using TargetMsg  = combat_robot_msgs::msg::FollowerTargetMessage;
using StatusMsg  = combat_robot_msgs::msg::FormationStatus;
using CmdMsg     = combat_robot_msgs::msg::FormationCommand;
using StatusSubMsg = combat_robot_msgs::msg::RobotStatus;

FormationNode::FormationNode(const rclcpp::NodeOptions& opts)
    : rclcpp::Node("formation_node", opts) {
  declareParameters();
  loadParameters();

  formation_cmd_sub_ = create_subscription<CmdMsg>(
      "~/formation_command", rclcpp::QoS(5).reliable(),
      std::bind(&FormationNode::onFormationCommand, this,
                 std::placeholders::_1));

  robot_status_sub_ = create_subscription<StatusSubMsg>(
      "/swarm/robot_status",
      rclcpp::QoS(20).best_effort(),
      std::bind(&FormationNode::onRobotStatus, this,
                 std::placeholders::_1));

  // slot_pub_    = create_publisher<SlotMsg>(
  //     "~/slot_assignment", rclcpp::QoS(5).reliable().transient_local());
  // // [DCN-2026-006 EXT D-025] depth 20 → 50 — match subscriber side
  // // (san_follower_tier) so reliable transport keeps burst headroom
  // // symmetric. 10 Hz cadence × 0.25 s = 2.5 messages buffered worst-
  // // case; depth=50 gives ample margin for WiFi 6 reconnect bursts.
  // target_pub_  = create_publisher<TargetMsg>(
  //     "~/follower_target", rclcpp::QoS(rclcpp::KeepLast(50)).reliable());

  slot_pub_    = create_publisher<SlotMsg>(
      "/swarm/formation/slot_assignment", rclcpp::QoS(5).reliable().transient_local());
  target_pub_  = create_publisher<TargetMsg>(
      "/swarm/formation/follower_target", rclcpp::QoS(rclcpp::KeepLast(50)).reliable());


  status_pub_  = create_publisher<StatusMsg>(
      "~/formation_status", rclcpp::QoS(5).reliable());

  // Re-plan check at 1 Hz (cheap), broadcast targets at 10 Hz (P0).
  assign_timer_ = create_wall_timer(
      1s,    std::bind(&FormationNode::onAssignTick, this));
  target_timer_ = create_wall_timer(
      100ms, std::bind(&FormationNode::onFollowerTargetTick, this));
  status_timer_ = create_wall_timer(
      1s,    std::bind(&FormationNode::onStatusTick, this));

  RCLCPP_INFO(get_logger(),
      "FormationNode UP: form=%d preset=%u d=%.1fm θ=%.1f° realign_thr=%.2fm",
      static_cast<int>(current_formation_), current_preset_,
      spacing_d_m_, spread_theta_deg_, realign_threshold_m_);
}

void FormationNode::declareParameters() {
  declare_parameter<int>("initial_formation_id", 3);   // VShape
  declare_parameter<int>("initial_preset_id",    2);   // recon_defence
  declare_parameter<double>("realign_threshold_m",  2.0);
  declare_parameter<double>("max_speed_recon_mps", 1.3);
  declare_parameter<double>("lead_bias_s",          0.1);
}

void FormationNode::loadParameters() {
  const auto fid = static_cast<uint8_t>(
      get_parameter("initial_formation_id").as_int());
  current_formation_ = fromMessageId(fid);

  current_preset_ = static_cast<uint8_t>(
      get_parameter("initial_preset_id").as_int());
  auto preset = getPreset(current_preset_);
  if (!preset.name.empty()) {
    spacing_d_m_       = preset.spacing_d_m;
    spread_theta_deg_  = preset.spread_theta_deg;
  }

  realign_threshold_m_ =
      static_cast<float>(get_parameter("realign_threshold_m").as_double());
  max_speed_recon_     =
      static_cast<float>(get_parameter("max_speed_recon_mps").as_double());
  lead_bias_s_         =
      static_cast<float>(get_parameter("lead_bias_s").as_double());
}

// ─── Subscriptions ──────────────────────────────────────────────────────

void FormationNode::onFormationCommand(const CmdMsg::SharedPtr msg) {
  std::lock_guard<std::mutex> g(state_mu_);

  // Translate v1.3 FormationCommand.formation (0..4) → v1.5 Formation enum (1..9).
  // COLUMN=0→Column, LINE=1→Line, WEDGE=2→VShape, DIAMOND=3→Diamond, CUSTOM=4→FreeSpread.
  uint8_t new_fid = 0;
  switch (msg->formation) {
    case 0: new_fid = toMessageId(Formation::Column);     break;
    case 1: new_fid = toMessageId(Formation::Line);       break;
    case 2: new_fid = toMessageId(Formation::VShape);     break;
    case 3: new_fid = toMessageId(Formation::Diamond);    break;
    case 4: new_fid = toMessageId(Formation::FreeSpread); break;
    default: break;
  }
  if (new_fid >= 1 && new_fid <= 9) {
    current_formation_ = fromMessageId(new_fid);
  }
  // Preset only updated if it's a valid 1..4
  // (Some legacy commands omit preset — keep current in that case.)
  // FormationCommand may not have preset_id in v1.4 — guard accordingly.
  // For now we re-apply current preset's spacing/theta.
  auto preset = getPreset(current_preset_);
  if (!preset.name.empty()) {
    spacing_d_m_      = preset.spacing_d_m;
    spread_theta_deg_ = preset.spread_theta_deg;
  }
  RCLCPP_INFO(get_logger(),
      "Formation change request: form=%u preset=%u d=%.1f θ=%.1f",
      new_fid, current_preset_, spacing_d_m_, spread_theta_deg_);
  recomputeAssignment();
}

void FormationNode::onRobotStatus(const StatusSubMsg::SharedPtr msg) {
  std::lock_guard<std::mutex> g(state_mu_);
  PoseSnap snap;
  snap.x = static_cast<float>(msg->pose.position.x);
  snap.y = static_cast<float>(msg->pose.position.y);
  // Static-analysis hardening: stamp.sec is int32. Negative stamps from
  // a malformed publisher (or pre-/clock sim time) would wrap when cast
  // to uint64 and then poison the dt-based velocity estimate below.
  // Use rclcpp::Time so the canonical conversion handles unset stamps.
  const int64_t stamp_ns = rclcpp::Time(msg->header.stamp).nanoseconds();
  if (stamp_ns <= 0) return;
  snap.timestamp_ms = static_cast<uint64_t>(stamp_ns / 1'000'000LL);

  // PDR-3: Velocity estimation by finite-difference from previous
  // snapshot. Provides leader velocity used to predict slot positions
  // 1s ahead in publishFollowerTargets() per SDD §6.3.
  auto it = robot_poses_.find(msg->robot_id);
  if (it != robot_poses_.end() &&
      snap.timestamp_ms > it->second.timestamp_ms) {
    const uint64_t dt_ms = snap.timestamp_ms - it->second.timestamp_ms;
    if (dt_ms > 0 && dt_ms < 5000) {  // ignore stale > 5s
      const float dt_s = dt_ms / 1000.0f;
      snap.vx = (snap.x - it->second.x) / dt_s;
      snap.vy = (snap.y - it->second.y) / dt_s;
      // Simple low-pass filter to smooth noise (60% old + 40% new)
      snap.vx = 0.6f * it->second.vx + 0.4f * snap.vx;
      snap.vy = 0.6f * it->second.vy + 0.4f * snap.vy;
    }
  }
  robot_poses_[msg->robot_id] = snap;
}

// ─── Periodic ───────────────────────────────────────────────────────────

void FormationNode::onAssignTick() {
  std::lock_guard<std::mutex> g(state_mu_);
  if (robot_poses_.size() < 2) return;     // need leader + at least 1
  // If no current assignment yet, force one
  if (current_assignment_.empty()) {
    recomputeAssignment();
    return;
  }
  // Check alignment error — if avg > threshold, re-plan
  float total_err = 0.0f;
  uint32_t n = 0;
  for (const auto& slot : current_assignment_) {
    auto it = robot_poses_.find(slot.robot_id);
    if (it == robot_poses_.end()) continue;
    const float dx = it->second.x - slot.target_x;
    const float dy = it->second.y - slot.target_y;
    total_err += std::sqrt(dx*dx + dy*dy);
    ++n;
  }
  if (n == 0) return;
  const float avg_err = total_err / static_cast<float>(n);
  if (avg_err > realign_threshold_m_) {
    RCLCPP_INFO(get_logger(),
        "Avg alignment error %.2fm > %.2fm — replanning",
        avg_err, realign_threshold_m_);
    recomputeAssignment();
  }
}

void FormationNode::onFollowerTargetTick() {
  publishFollowerTargets();
}

void FormationNode::onStatusTick() {
  status_pub_->publish(buildStatusMsg());
}

// ─── Core algorithm ────────────────────────────────────────────────────

void FormationNode::recomputeAssignment() {
  // Caller holds state_mu_.
  const size_t n = robot_poses_.size();
  if (n == 0) return;

  // Generate slots in leader's frame (origin = first robot in map order,
  // for simplicity; production would resolve actual leader_id).
  auto slots = generateSlots(
      current_formation_, n, spacing_d_m_, spread_theta_deg_);
  if (slots.size() != n) return;

  // Stable robot ID ordering for deterministic cost matrix
  std::vector<uint32_t> robot_ids;
  robot_ids.reserve(n);
  for (const auto& [id, _] : robot_poses_) robot_ids.push_back(id);

  // Cost matrix: distance from robot i to slot j (world frame ≈ leader local)
  std::vector<std::vector<double>> cost(n, std::vector<double>(n));
  for (size_t i = 0; i < n; ++i) {
    const auto& robot = robot_poses_.at(robot_ids[i]);
    for (size_t j = 0; j < n; ++j) {
      const float dx = robot.x - slots[j].x;
      const float dy = robot.y - slots[j].y;
      cost[i][j] = std::sqrt(dx * dx + dy * dy);
    }
  }

  // Solve Hungarian
  std::vector<size_t> assignment;
  try {
    assignment = solveAssignment(cost);
  } catch (const std::exception& e) {
    RCLCPP_ERROR(get_logger(),
        "Hungarian failed: %s", e.what());
    return;
  }

  // Update state
  ++current_epoch_;
  current_assignment_.clear();
  for (size_t i = 0; i < n; ++i) {
    const size_t j = assignment[i];
    AssignedSlot s;
    s.robot_id   = robot_ids[i];
    s.slot_index = static_cast<uint8_t>(j);
    s.target_x   = slots[j].x;
    s.target_y   = slots[j].y;
    current_assignment_.push_back(s);
  }
  last_total_cost_ = static_cast<float>(assignmentCost(cost, assignment));
  last_reassignment_ms_ =
      static_cast<uint64_t>(now().nanoseconds() / 1'000'000ULL);

  // Publish the assignment to all followers
  slot_pub_->publish(buildAssignmentMsg());

  RCLCPP_INFO(get_logger(),
      "Reassigned: epoch=%u n=%zu form=%d total_cost=%.2fm",
      current_epoch_, n, static_cast<int>(current_formation_),
      last_total_cost_);
}

SlotMsg FormationNode::buildAssignmentMsg() const {
  // Caller holds state_mu_.
  SlotMsg m;
  m.header.stamp        = now();
  m.header.frame_id     = "world";
  m.epoch               = current_epoch_;
  m.formation_id        = toMessageId(current_formation_);
  m.spacing_d           = spacing_d_m_;
  m.spread_theta_deg    = spread_theta_deg_;
  m.total_cost_m        = last_total_cost_;
  for (const auto& slot : current_assignment_) {
    m.robot_ids.push_back(slot.robot_id);
    m.slot_x_m.push_back(slot.target_x);
    m.slot_y_m.push_back(slot.target_y);
    m.slot_index.push_back(slot.slot_index);
  }
  return m;
}

void FormationNode::publishFollowerTargets() {
  std::lock_guard<std::mutex> g(state_mu_);
  if (current_assignment_.empty()) return;

  // PDR-3: Look up leader velocity for 1-second prediction.
  // SDD §6.3 — leader's velocity vector applied to slot positions
  // gives followers a 1-second lookahead target. This is the input
  // to T0 PREDICTIVE_TRACK in the follower-side TierFsm.
  float leader_vx = 0.0f;
  float leader_vy = 0.0f;
  auto lit = robot_poses_.find(leader_robot_id_);
  if (lit != robot_poses_.end()) {
    leader_vx = lit->second.vx;
    leader_vy = lit->second.vy;
  }

  for (const auto& slot : current_assignment_) {
    TargetMsg t;
    t.header.stamp                    = now();
    t.header.frame_id                 = "world";
    t.target_robot_id                 = slot.robot_id;
    t.formation_epoch                 = current_epoch_;
    t.target_pose_now.position.x      = slot.target_x;
    t.target_pose_now.position.y      = slot.target_y;
    t.target_pose_now.orientation.w   = 1.0;
    // PDR-3: 1-second prediction = current slot + leader_velocity × 1.0s.
    // Assumes formation rigidly translates with leader (acceptable for
    // straight-line motion; curved motion adds extra error absorbed by
    // T1.5 AUTO_REROUTE on the follower side).
    t.target_pose_pred_1s.position.x  = slot.target_x + leader_vx * 1.0f;
    t.target_pose_pred_1s.position.y  = slot.target_y + leader_vy * 1.0f;
    t.target_pose_pred_1s.orientation.w = 1.0;
    t.target_velocity.linear.x        = leader_vx;
    t.target_velocity.linear.y        = leader_vy;
    t.max_speed_mps                   = max_speed_recon_;
    t.lead_bias_s                     = lead_bias_s_;
    target_pub_->publish(t);
  }
}

StatusMsg FormationNode::buildStatusMsg() const {
  std::lock_guard<std::mutex> g(state_mu_);
  StatusMsg s;
  s.header.stamp           = now();
  s.header.frame_id        = "world";
  s.formation_id           = toMessageId(current_formation_);
  s.preset_id              = current_preset_;
  s.spacing_d_m            = spacing_d_m_;
  s.spread_theta_deg       = spread_theta_deg_;
  s.current_epoch          = current_epoch_;
  s.last_reassignment_ms   = last_reassignment_ms_;
  s.phase                  = phase_;

  // KPP-1 alignment metrics (real-time)
  float sum_err = 0.0f, max_err = 0.0f;
  uint8_t in_ct = 0, out_ct = 0;
  for (const auto& slot : current_assignment_) {
    auto it = robot_poses_.find(slot.robot_id);
    if (it == robot_poses_.end()) continue;
    const float dx = it->second.x - slot.target_x;
    const float dy = it->second.y - slot.target_y;
    const float e  = std::sqrt(dx * dx + dy * dy);
    sum_err += e;
    if (e > max_err) max_err = e;
    if (e < realign_threshold_m_) ++in_ct;
    else                          ++out_ct;
  }
  const uint32_t n = static_cast<uint32_t>(current_assignment_.size());
  s.avg_alignment_error_m  = n > 0 ? sum_err / n : 0.0f;
  s.max_alignment_error_m  = max_err;
  s.robots_in_formation    = in_ct;
  s.robots_out_of_formation= out_ct;
  return s;
}

float FormationNode::computeAlignmentError(uint32_t robot_id) const {
  // Caller holds state_mu_.
  auto pose_it = robot_poses_.find(robot_id);
  if (pose_it == robot_poses_.end()) return -1.0f;
  for (const auto& slot : current_assignment_) {
    if (slot.robot_id != robot_id) continue;
    const float dx = pose_it->second.x - slot.target_x;
    const float dy = pose_it->second.y - slot.target_y;
    return std::sqrt(dx * dx + dy * dy);
  }
  return -1.0f;
}

}  // namespace san_formation
