// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Cost-path traversal checker (pure C++17, ROS-agnostic).
//
// PATCH 2026-05-13 (Reroute deep-dive review):
//   This header was missing from the v1.5.0 release. The .cpp file
//   and 2 consumers (lateral_evasion.cpp, reroute_node.cpp,
//   test_reroute.cpp) all include it. API reconstructed from
//   existing call sites + test assertions.
//
// Algorithm:
//   1. Sample the line (sx,sy)→(ex,ey) at step_resolution_m intervals
//   2. For each sample, look up cost in CostMapView
//   3. Record max_cost, first lethal hit (distance), inflated flag
//   4. Return a PathCheckResult summarising the traversal
//
// Used by:
//   * lateral_evasion.cpp — score evasion candidates
//   * reroute_node.cpp    — emit obstacle_on_path / KPP-2 hot path

#ifndef SAN_REROUTE_PLANNER__COST_PATH_CHECKER_HPP_
#define SAN_REROUTE_PLANNER__COST_PATH_CHECKER_HPP_

#include <cstdint>
#include <cstddef>

#include "san_reroute_planner/cost_map_view.hpp"

namespace san_reroute_planner
{

/// Result of a line-of-sight cost-map scan.
struct PathCheckResult
{
  uint8_t max_cost = 0;                 // worst cost along the path
  bool obstacle_detected = false;       // any cell at COST_LETHAL+
  bool inflated_detected = false;       // any cell at inflated_threshold..lethal-1
  float obstacle_distance_m = 0.0f;     // metres from (sx,sy) to first lethal cell
  std::size_t cells_checked = 0;        // diagnostic — number of samples evaluated
};

/// Check the cost along a straight world-frame segment.
///
/// Args:
///   map                 — cost-grid view (valid() must be true)
///   sx, sy              — start point (world frame, metres)
///   ex, ey              — end   point (world frame, metres)
///   lethal_threshold    — cells >= this count as obstacle (default 254)
///   inflated_threshold  — cells >= this and < lethal count as inflated (default 50)
///   step_resolution_m   — sample spacing; 0 → use map.resolution_m
///
/// Returns:
///   PathCheckResult. If !map.valid() → all zero-default + cells_checked=0.
///
/// Semantics:
///   * `obstacle_distance_m` is the distance from (sx, sy) to the FIRST
///     lethal cell sampled (0.0 if obstacle is at the start sample).
///   * `inflated_detected` is true only if the path contains an
///     inflated-but-not-lethal cell; lethal cells don't also set this.
///   * COST_UNKNOWN samples count as "inflated" (so prediction
///     uncertainty is treated as soft caution, not as free space).
PathCheckResult checkPath(
  const CostMapView & map,
  float sx, float sy,
  float ex, float ey,
  uint8_t lethal_threshold = COST_LETHAL,
  uint8_t inflated_threshold = COST_INFLATED_LOW,
  float step_resolution_m = 0.0f);

}  // namespace san_reroute_planner

#endif  // SAN_REROUTE_PLANNER__COST_PATH_CHECKER_HPP_
