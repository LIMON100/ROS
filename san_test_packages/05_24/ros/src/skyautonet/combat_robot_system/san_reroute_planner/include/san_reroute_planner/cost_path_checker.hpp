// SAN v1.5 — Cost Path Checker per SDD-SWARM §6.4.1 step 3.
//
//   매 100ms BT tick:
//     ...
//     3. 예측 경로 상 cell 검사:
//        - 모든 cell cost < 50 (free)        → T0 유지
//        - cell cost >= 254 (lethal) 감지     → T1.5 진입
//        - cell cost 50~253 (inflated)        → T1.5 (감속)
//
// Pure C++17, no rclcpp. Standalone testable.

#ifndef SAN_REROUTE_PLANNER__COST_PATH_CHECKER_HPP_
#define SAN_REROUTE_PLANNER__COST_PATH_CHECKER_HPP_

#include <cstddef>
#include <cstdint>

#include "san_reroute_planner/cost_map_view.hpp"

namespace san_reroute_planner {

/// Result of a path check.
struct PathCheckResult {
  bool    obstacle_detected{false};       // any cell ≥ lethal_threshold
  bool    inflated_detected{false};       // any cell in [inflated, lethal)
  uint8_t max_cost{0};
  float   obstacle_distance_m{0.0f};      // along-path distance to first lethal cell
  size_t  cells_checked{0};
};

/// Sample cost along a straight line from (sx, sy) to (ex, ey) and
/// classify into FREE / INFLATED / LETHAL bands.
///
/// Args:
///   map                  — cost grid + world transform
///   sx, sy               — start world coord (current robot pose)
///   ex, ey               — end world coord (1s predicted target)
///   lethal_threshold     — default 254 (T1.5 mandatory reroute)
///   inflated_threshold   — default 50  (T1.5 with slowdown)
///   step_resolution_m    — sampling step (default = map.resolution)
///
/// Returns: result populated with worst-case findings.
PathCheckResult checkPath(
    const CostMapView& map,
    float sx, float sy,
    float ex, float ey,
    uint8_t lethal_threshold     = COST_LETHAL,
    uint8_t inflated_threshold   = COST_INFLATED_LOW,
    float   step_resolution_m    = 0.0f);  // 0 = use map.resolution_m

}  // namespace san_reroute_planner

#endif  // SAN_REROUTE_PLANNER__COST_PATH_CHECKER_HPP_
