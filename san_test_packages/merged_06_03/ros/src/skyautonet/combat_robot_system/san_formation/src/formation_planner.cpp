// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Formation planner helper implementations.

#include "san_formation/formation_planner.hpp"

#include <cmath>
#include <stdexcept>

namespace san_formation
{

// ─── VelocityEstimator ──────────────────────────────────────────────────

VelocityEstimator::VelocityEstimator(float alpha)
: alpha_(alpha)
{
  if (alpha <= 0.0f || alpha > 1.0f) {
    throw std::invalid_argument(
            "VelocityEstimator: alpha must be in (0, 1]");
  }
}

std::optional<Velocity2D> VelocityEstimator::update(
  uint64_t timestamp_ms, const PoseXY & pose)
{
  if (!last_ts_ms_) {
    // First sample — record, but no velocity yet.
    last_ts_ms_ = timestamp_ms;
    last_pose_ = pose;
    return std::nullopt;
  }

  const auto dt_ms = (timestamp_ms > *last_ts_ms_) ?
    (timestamp_ms - *last_ts_ms_) :
    0ULL;
  if (dt_ms == 0) {
    // Same timestamp (or out-of-order) — keep prior estimate, no update.
    return latest_;
  }
  const float dt_s = static_cast<float>(dt_ms) / 1000.0f;

  Velocity2D raw;
  raw.vx = (pose.x - last_pose_->x) / dt_s;
  raw.vy = (pose.y - last_pose_->y) / dt_s;
  // Wrap yaw delta to (-π, π] before differentiating.
  float dyaw = pose.yaw - last_pose_->yaw;
  while (dyaw > static_cast<float>(M_PI)) {dyaw -= 2.0f * static_cast<float>(M_PI);}
  while (dyaw < -static_cast<float>(M_PI)) {dyaw += 2.0f * static_cast<float>(M_PI);}
  raw.wz = dyaw / dt_s;

  // Low-pass filter
  Velocity2D filtered;
  if (latest_) {
    filtered.vx = alpha_ * raw.vx + (1.0f - alpha_) * latest_->vx;
    filtered.vy = alpha_ * raw.vy + (1.0f - alpha_) * latest_->vy;
    filtered.wz = alpha_ * raw.wz + (1.0f - alpha_) * latest_->wz;
  } else {
    filtered = raw;
  }

  last_ts_ms_ = timestamp_ms;
  last_pose_ = pose;
  latest_ = filtered;
  return filtered;
}

void VelocityEstimator::reset()
{
  last_ts_ms_.reset();
  last_pose_.reset();
  latest_.reset();
}

// ─── Cost matrix with proper frame transform ────────────────────────────

std::vector<std::vector<double>> buildCostMatrix(
  const std::vector<PoseXY> & robots_world,
  const std::vector<SlotXY> & slots_local,
  const PoseXY & leader_world)
{
  const std::size_t n = robots_world.size();
  if (n == 0 || slots_local.size() != n) {return {};}

  // Pre-compute slots in world frame ONCE (not n times).
  std::vector<SlotXY> slots_world;
  slots_world.reserve(n);
  for (const auto & s : slots_local) {
    float wx, wy;
    slotLocalToWorld(s.x, s.y, leader_world, wx, wy);
    slots_world.push_back({wx, wy});
  }

  std::vector<std::vector<double>> cost(n, std::vector<double>(n));
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      const float dx = robots_world[i].x - slots_world[j].x;
      const float dy = robots_world[i].y - slots_world[j].y;
      cost[i][j] = std::sqrt(
        static_cast<double>(dx) * dx +
        static_cast<double>(dy) * dy);
    }
  }
  return cost;
}

// ─── Slot prediction (1 s ahead) ────────────────────────────────────────

PredictedSlot predictSlotAhead(
  const SlotXY & slot_local,
  const PoseXY & leader_now,
  const Velocity2D & leader_vel,
  float t_predict_s)
{
  // Predict leader pose at t_predict_s.
  PoseXY leader_pred;
  leader_pred.x = leader_now.x + leader_vel.vx * t_predict_s;
  leader_pred.y = leader_now.y + leader_vel.vy * t_predict_s;
  leader_pred.yaw = leader_now.yaw + leader_vel.wz * t_predict_s;

  // Transform slot offset into predicted leader frame.
  PredictedSlot out;
  slotLocalToWorld(
    slot_local.x, slot_local.y, leader_pred,
    out.world_x, out.world_y);
  // Heading alignment — follower faces leader heading.
  // PATCH 2026-05-13: previously orientation was always identity.
  out.world_yaw = leader_pred.yaw;
  return out;
}

}  // namespace san_formation
