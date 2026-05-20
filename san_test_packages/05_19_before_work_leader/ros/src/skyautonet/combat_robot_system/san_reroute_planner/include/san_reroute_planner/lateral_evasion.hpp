// SAN v1.5 — Lateral Evasion per SDD-SWARM §6.4.1 step 4.
//
//   4. T1.5 진입 시:
//      - DWB local planner: goal = 1초 예측 위치
//      - costmap = static + obstacle + traversability + inflation
//      - planner trajectory 후보 평가 → 최저 cost 선택
//      - 통상 ±2m 측방 우회 trajectory 출력
//
// Simplified candidate-based evasion (no Nav2 dependency):
//   1. Compute path normal (perpendicular to start→goal direction)
//   2. Generate N candidates at lateral offsets ±0.5m, ±1.0m, ±1.5m, ±2.0m
//   3. Score each by: max cost along (current → waypoint → target)
//   4. Pick the lowest-cost candidate that has cost < lethal threshold
//
// Pure C++17. Standalone testable.

#ifndef SAN_REROUTE_PLANNER__LATERAL_EVASION_HPP_
#define SAN_REROUTE_PLANNER__LATERAL_EVASION_HPP_

#include <optional>

#include "san_reroute_planner/cost_map_view.hpp"

namespace san_reroute_planner {

/// Candidate evasion waypoint with cost.
struct EvasionCandidate {
  float    waypoint_x_m{0.0f};
  float    waypoint_y_m{0.0f};
  uint8_t  max_cost_along_path{0};    // worst cost on (current → wp → target)
  float    lateral_offset_m{0.0f};    // signed: +left / -right
  bool     feasible{false};            // max_cost < COST_LETHAL
};

/// Search params.
struct EvasionConfig {
  /// Lateral offsets to try (positive = left, negative = right).
  /// Default per SDD §6.4: ±0.5, ±1.0, ±1.5, ±2.0 m
  std::vector<float> offsets_m;

  uint8_t lethal_threshold{COST_LETHAL};   // 254
  uint8_t inflated_threshold{COST_INFLATED_LOW};  // 50

  EvasionConfig() {
    offsets_m = {0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f, 2.0f, -2.0f};
  }
};

/// Find the best lateral evasion waypoint, or nullopt if all candidates
/// are blocked.
///
/// Args:
///   map    — cost grid
///   cur_x, cur_y — current pose (start of trajectory)
///   tgt_x, tgt_y — target pose (1s predicted)
///   cfg    — evasion parameters (defaults per SDD §6.4)
///
/// Algorithm:
///   For each offset d in cfg.offsets_m:
///     wp = midpoint(cur, tgt) + d × normal(cur → tgt)
///     score = max(cost along cur → wp, cost along wp → tgt)
///     if score < lethal_threshold: feasible candidate
///   Return lowest-score feasible candidate (preferring smaller |offset|).
///
/// Returns: best EvasionCandidate or nullopt if no feasible solution.
std::optional<EvasionCandidate> findBestEvasion(
    const CostMapView& map,
    float cur_x, float cur_y,
    float tgt_x, float tgt_y,
    const EvasionConfig& cfg = {});

}  // namespace san_reroute_planner

#endif  // SAN_REROUTE_PLANNER__LATERAL_EVASION_HPP_
