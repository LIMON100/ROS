// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Lateral Evasion per SDD-SWARM §6.4.1 step 4.
//
//   4. T1.5 진입 시:
//      - DWB local planner: goal = 1초 예측 위치
//      - costmap = static + obstacle + traversability + inflation
//      - planner trajectory 후보 평가 → 최저 cost 선택
//      - 통상 ±2m 측방 우회 trajectory 출력
//
// PATCH 2026-05-13 (Reroute deep-dive review):
//   * Start-point lethal check (C5) — if (cur_x, cur_y) is inside a
//     lethal cell, the algorithm previously silent-failed (all
//     candidates would max() out at lethal because both segments
//     share the start). Now returns nullopt EXPLICITLY so the node
//     can emergency-stop instead of falsely reporting "no evasion".
//   * Heading-aware preference (M9) — optional current_yaw allows
//     tie-break to prefer offsets on the side the robot is already
//     facing, reducing rotation time during KPP-2's 300 ms budget.
//   * Offsets list now config-driven (M8) — existing default
//     {±0.5, ±1.0, ±1.5, ±2.0} unchanged.

#ifndef SAN_REROUTE_PLANNER__LATERAL_EVASION_HPP_
#define SAN_REROUTE_PLANNER__LATERAL_EVASION_HPP_

#include <optional>
#include <vector>

#include "san_reroute_planner/cost_map_view.hpp"

namespace san_reroute_planner
{

/// Candidate evasion waypoint with cost.
struct EvasionCandidate
{
  float waypoint_x_m{0.0f};
  float waypoint_y_m{0.0f};
  uint8_t max_cost_along_path{0};
  float lateral_offset_m{0.0f};
  bool feasible{false};
};

/// Search parameters.
struct EvasionConfig
{
  std::vector<float> offsets_m;

  uint8_t lethal_threshold{COST_LETHAL};
  uint8_t inflated_threshold{COST_INFLATED_LOW};

  /// PATCH 2026-05-13 (M9): heading-aware tie-break.
  /// When true, ties in cost (and |offset|) are broken in favour of
  /// the side the robot is already facing (smaller required rotation).
  bool heading_aware{false};

  EvasionConfig()
  {
    offsets_m = {0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f, 2.0f, -2.0f};
  }
};

/// Reason returned with the candidate (for diagnostic + audit).
enum class EvasionStatus : uint8_t
{
  Ok               = 0,
  StartCellLethal  = 1,   // ★ PATCH C5
  EndCellLethal    = 2,   // start→end same cell is lethal
  DegeneratePath   = 3,   // length < 1mm
  InvalidMap       = 4,
  AllBlocked       = 5,
};

/// Find the best lateral evasion waypoint, or nullopt if all candidates
/// are blocked.
///
/// PATCH 2026-05-13: explicit status output via out_status so callers
/// can distinguish "no evasion possible (emergency)" from "robot
/// already inside obstacle (emergency-stop)".
std::optional<EvasionCandidate> findBestEvasion(
  const CostMapView & map,
  float cur_x, float cur_y,
  float tgt_x, float tgt_y,
  const EvasionConfig & cfg = EvasionConfig{},
  EvasionStatus * out_status = nullptr,
  float current_yaw_rad = 0.0f);

}  // namespace san_reroute_planner

#endif  // SAN_REROUTE_PLANNER__LATERAL_EVASION_HPP_
