// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Formation planner helpers (pure C++17, no ROS).
//
// Separates the parts of FormationNode that are pure math (frame
// transforms, velocity estimation, slot prediction) so they can be
// gtest-verified without spinning up rclcpp.
//
// PATCH 2026-05-13 (Formation deep-dive review):
//   * Adds proper leader-local frame transform (rotation + translation).
//     The previous formation_node.cpp computed cost = ||robot.world - slot.local||
//     which only worked when the leader sat at the origin with heading=+x.
//   * Adds finite-difference leader velocity estimation. The previous
//     PoseSnap had vx/vy fields but onRobotStatus() never filled them,
//     so the "1-second prediction" (SDD §6.3 / PDR-3 KPP) was zero-velocity.
//   * Adds 1-second slot prediction using leader velocity.

#ifndef SAN_FORMATION__FORMATION_PLANNER_HPP_
#define SAN_FORMATION__FORMATION_PLANNER_HPP_

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "san_formation/formations.hpp"

namespace san_formation
{

// ─── Pose (world frame) ─────────────────────────────────────────────────

struct PoseXY
{
  float x = 0.0f;
  float y = 0.0f;
  float yaw = 0.0f;     // heading (rad), CCW from +x
};

struct Velocity2D
{
  float vx = 0.0f;
  float vy = 0.0f;
  float wz = 0.0f;     // yaw rate
};

// ─── Frame transforms ───────────────────────────────────────────────────

/// Rotate a 2D vector by yaw radians.
inline void rotate2D(
  float in_x, float in_y, float yaw,
  float & out_x, float & out_y)
{
  const float c = std::cos(yaw);
  const float s = std::sin(yaw);
  out_x = c * in_x - s * in_y;
  out_y = s * in_x + c * in_y;
}

/// Transform a slot offset (leader-local frame, +x = leader heading)
/// to world frame by applying leader pose.
///   world = R(leader.yaw) * local + leader.position
inline void slotLocalToWorld(
  float local_x, float local_y,
  const PoseXY & leader,
  float & world_x, float & world_y)
{
  float rx, ry;
  rotate2D(local_x, local_y, leader.yaw, rx, ry);
  world_x = leader.x + rx;
  world_y = leader.y + ry;
}

// ─── Velocity estimator ─────────────────────────────────────────────────

/// Per-robot finite-difference velocity estimator with low-pass filter.
/// Caller pushes timestamped (x, y, yaw) samples; estimator returns
/// the latest filtered velocity in world frame.
///
/// PATCH 2026-05-13: previously the FormationNode declared PoseSnap.vx/vy
/// but never assigned them. This class fills the gap.
class VelocityEstimator
{
public:
  /// alpha ∈ (0, 1]: low-pass weight applied to NEW samples.
  ///   alpha=1.0 → no smoothing (use raw finite diff)
  ///   alpha=0.3 → moderate smoothing (default)
  explicit VelocityEstimator(float alpha = 0.3f);

  /// Push a new (timestamped) pose sample. Returns the updated
  /// velocity estimate (world frame) — or nullopt on the very first
  /// sample (no prior point to diff against).
  std::optional<Velocity2D> update(
    uint64_t timestamp_ms,
    const PoseXY & pose);

  std::optional<Velocity2D> latest() const {return latest_;}
  bool hasEstimate() const {return latest_.has_value();}
  void reset();

private:
  float alpha_;
  std::optional<uint64_t> last_ts_ms_;
  std::optional<PoseXY> last_pose_;
  std::optional<Velocity2D> latest_;
};

// ─── Cost-matrix builder ────────────────────────────────────────────────

/// Build the N×N cost matrix robot-to-slot WITH proper frame transform.
///
/// PATCH 2026-05-13: previously formation_node.cpp computed
///   cost[i][j] = ||robot_i.world − slot_j.local||
/// which is dimensionally wrong unless the leader sits at the origin
/// with yaw=0. This function:
///   1. Transforms each slot from leader-local to world frame
///   2. Computes Euclidean distance in world frame
///
/// Args:
///   robots_world : N poses in world frame, in stable order
///   slots_local  : N slots in leader-local frame (output of generateSlots)
///   leader_world : leader's world-frame pose (anchors local frame)
///
/// Returns: N×N matrix where cost[i][j] = distance(robots_world[i], slots_world[j]).
std::vector<std::vector<double>> buildCostMatrix(
  const std::vector<PoseXY> & robots_world,
  const std::vector<SlotXY> & slots_local,
  const PoseXY & leader_world);

// ─── Slot prediction (1 second ahead) ───────────────────────────────────

/// Predict where slot j will be at time `t_predict_s` into the future,
/// assuming the leader continues with the given velocity in world frame.
///
/// PATCH 2026-05-13: this implements the "1-second prediction" required
/// by SDD §6.3 (T0 PREDICTIVE_TRACK / PDR-3 KPP). Previously the node
/// just published `target_pose_pred_1s = target_pose_now` (no prediction).
///
/// Returns world-frame (x, y, yaw) of the predicted slot.
struct PredictedSlot
{
  float world_x;
  float world_y;
  float world_yaw;   // matches leader yaw (heading alignment)
};

PredictedSlot predictSlotAhead(
  const SlotXY & slot_local,
  const PoseXY & leader_now,
  const Velocity2D & leader_vel,
  float t_predict_s);

}  // namespace san_formation

#endif  // SAN_FORMATION__FORMATION_PLANNER_HPP_
