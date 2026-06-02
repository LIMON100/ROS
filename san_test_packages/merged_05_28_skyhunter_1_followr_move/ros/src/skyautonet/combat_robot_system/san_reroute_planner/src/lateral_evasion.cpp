// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Lateral Evasion implementation (PATCHED 2026-05-13).

#include "san_reroute_planner/lateral_evasion.hpp"

#include <algorithm>
#include <cmath>

#include "san_reroute_planner/cost_path_checker.hpp"

namespace san_reroute_planner
{

namespace
{

/// Wrap angle to (-π, π].
float wrapPi(float a)
{
  while (a > static_cast<float>(M_PI)) {a -= 2.0f * static_cast<float>(M_PI);}
  while (a <= -static_cast<float>(M_PI)) {a += 2.0f * static_cast<float>(M_PI);}
  return a;
}

}  // namespace

std::optional<EvasionCandidate> findBestEvasion(
  const CostMapView & map,
  float cur_x, float cur_y,
  float tgt_x, float tgt_y,
  const EvasionConfig & cfg,
  EvasionStatus * out_status,
  float current_yaw_rad)
{
  auto set_status = [&out_status](EvasionStatus s) {
      if (out_status) {*out_status = s;}
    };

  if (!map.valid()) {
    set_status(EvasionStatus::InvalidMap);
    return std::nullopt;
  }

  // ★ PATCH 2026-05-13 (C5): start-point lethal check.
  // If the robot is already inside a lethal cell, no lateral offset
  // can produce a feasible (cur → wp) segment (every segment starts
  // there). Return explicit StartCellLethal so the caller can
  // emergency-stop instead of trying to evade.
  const uint8_t start_cost = map.costAt(cur_x, cur_y);
  if (start_cost >= cfg.lethal_threshold && start_cost != COST_UNKNOWN) {
    set_status(EvasionStatus::StartCellLethal);
    return std::nullopt;
  }

  const float dx = tgt_x - cur_x;
  const float dy = tgt_y - cur_y;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-3f) {
    set_status(EvasionStatus::DegeneratePath);
    return std::nullopt;
  }

  // Unit normal (left of travel direction = +90° CCW from path).
  const float nx_unit = -dy / len;
  const float ny_unit = dx / len;

  // Midpoint between current and target — apply lateral offset here.
  const float mid_x = 0.5f * (cur_x + tgt_x);
  const float mid_y = 0.5f * (cur_y + tgt_y);

  // ★ PATCH 2026-05-13 (M9): heading-aware preference.
  // Compute the "natural side" the robot is currently facing relative
  // to the path direction. If heading_aware is on, ties get broken in
  // favour of offsets on that side (less rotation = faster KPP-2).
  // Path-direction angle, then signed angle from path to current yaw.
  const float path_yaw = std::atan2(dy, dx);
  const float yaw_to_path = wrapPi(current_yaw_rad - path_yaw);
  const bool facing_left = (yaw_to_path > 0.0f);     // CCW = left side

  EvasionCandidate best;
  bool any_feasible = false;

  for (float off : cfg.offsets_m) {
    EvasionCandidate cand;
    cand.lateral_offset_m = off;
    cand.waypoint_x_m = mid_x + off * nx_unit;
    cand.waypoint_y_m = mid_y + off * ny_unit;

    // Score = max cost on (cur → wp) and (wp → tgt).
    auto seg1 = checkPath(
      map, cur_x, cur_y,
      cand.waypoint_x_m, cand.waypoint_y_m,
      cfg.lethal_threshold,
      cfg.inflated_threshold);
    auto seg2 = checkPath(
      map,
      cand.waypoint_x_m, cand.waypoint_y_m,
      tgt_x, tgt_y,
      cfg.lethal_threshold,
      cfg.inflated_threshold);

    cand.max_cost_along_path = std::max(seg1.max_cost, seg2.max_cost);
    cand.feasible = !(seg1.obstacle_detected || seg2.obstacle_detected);

    if (!cand.feasible) {continue;}

    if (!any_feasible) {
      best = cand;
      any_feasible = true;
      continue;
    }

    // Selection rule (priority highest first):
    //   1. lower max_cost
    //   2. smaller |offset|
    //   3. (heading_aware) same side as facing
    const bool lower_cost =
      cand.max_cost_along_path < best.max_cost_along_path;
    const bool same_cost_smaller_off =
      (cand.max_cost_along_path == best.max_cost_along_path) &&
      (std::fabs(cand.lateral_offset_m) <
      std::fabs(best.lateral_offset_m));

    bool heading_tiebreak = false;
    if (cfg.heading_aware &&
      cand.max_cost_along_path == best.max_cost_along_path &&
      std::fabs(cand.lateral_offset_m) ==
      std::fabs(best.lateral_offset_m))
    {
      const bool cand_left = (cand.lateral_offset_m > 0.0f);
      const bool best_left = (best.lateral_offset_m > 0.0f);
      // Pick the one on the side we're facing; if best is already on
      // that side, no change. If neither is, leave it.
      heading_tiebreak = (cand_left == facing_left) &&
        (best_left != facing_left);
    }

    if (lower_cost || same_cost_smaller_off || heading_tiebreak) {
      best = cand;
    }
  }

  if (!any_feasible) {
    set_status(EvasionStatus::AllBlocked);
    return std::nullopt;
  }
  set_status(EvasionStatus::Ok);
  return best;
}

}  // namespace san_reroute_planner
