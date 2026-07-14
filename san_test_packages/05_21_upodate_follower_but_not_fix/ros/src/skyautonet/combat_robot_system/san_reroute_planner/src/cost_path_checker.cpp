// SAN v1.5 — CostPathChecker implementation.

#include "san_reroute_planner/cost_path_checker.hpp"

#include <algorithm>
#include <cmath>

namespace san_reroute_planner {

PathCheckResult checkPath(
    const CostMapView& map,
    float sx, float sy,
    float ex, float ey,
    uint8_t lethal_threshold,
    uint8_t inflated_threshold,
    float   step_resolution_m) {
  PathCheckResult r;
  if (!map.valid()) return r;

  const float dx = ex - sx;
  const float dy = ey - sy;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length < 1e-3f) {
    // Degenerate path — just check the single point
    const uint8_t c = map.costAt(sx, sy);
    r.max_cost = c;
    r.obstacle_detected = (c >= lethal_threshold);
    r.inflated_detected = (c >= inflated_threshold
                            && c <  lethal_threshold);
    r.cells_checked = 1;
    return r;
  }

  const float step = (step_resolution_m > 0.0f)
      ? step_resolution_m : map.resolution_m;
  const size_t n_steps = std::max<size_t>(1,
      static_cast<size_t>(length / step));
  const float step_dx = dx / static_cast<float>(n_steps);
  const float step_dy = dy / static_cast<float>(n_steps);
  const float step_len = std::sqrt(step_dx * step_dx + step_dy * step_dy);

  bool first_lethal_seen = false;
  for (size_t i = 0; i <= n_steps; ++i) {
    const float x = sx + step_dx * static_cast<float>(i);
    const float y = sy + step_dy * static_cast<float>(i);
    const uint8_t c = map.costAt(x, y);
    ++r.cells_checked;
    // Treat NO_INFORMATION as "inflated" caution (not lethal but warn)
    const uint8_t effective_cost = (c == COST_UNKNOWN)
        ? inflated_threshold : c;
    if (effective_cost > r.max_cost) r.max_cost = effective_cost;
    if (c >= lethal_threshold && !first_lethal_seen) {
      r.obstacle_detected   = true;
      r.obstacle_distance_m = step_len * static_cast<float>(i);
      first_lethal_seen = true;
    }
    if (c >= inflated_threshold && c < lethal_threshold) {
      r.inflated_detected = true;
    }
  }
  return r;
}

}  // namespace san_reroute_planner
