// SAN v1.5 — Lateral Evasion implementation.

#include "san_reroute_planner/lateral_evasion.hpp"

#include <algorithm>
#include <cmath>

#include "san_reroute_planner/cost_path_checker.hpp"

namespace san_reroute_planner {

std::optional<EvasionCandidate> findBestEvasion(
    const CostMapView& map,
    float cur_x, float cur_y,
    float tgt_x, float tgt_y,
    const EvasionConfig& cfg) {
  if (!map.valid()) return std::nullopt;

  const float dx = tgt_x - cur_x;
  const float dy = tgt_y - cur_y;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-3f) return std::nullopt;

  // Unit normal (left of travel direction = +90° CCW from path)
  const float nx_unit = -dy / len;
  const float ny_unit =  dx / len;

  // Midpoint between current and target — apply lateral offset here.
  const float mid_x = 0.5f * (cur_x + tgt_x);
  const float mid_y = 0.5f * (cur_y + tgt_y);

  EvasionCandidate best;
  bool any_feasible = false;

  for (float off : cfg.offsets_m) {
    EvasionCandidate cand;
    cand.lateral_offset_m = off;
    cand.waypoint_x_m = mid_x + off * nx_unit;
    cand.waypoint_y_m = mid_y + off * ny_unit;

    // Score = max cost on (cur → wp) and (wp → tgt)
    auto seg1 = checkPath(map, cur_x, cur_y,
                            cand.waypoint_x_m, cand.waypoint_y_m,
                            cfg.lethal_threshold,
                            cfg.inflated_threshold);
    auto seg2 = checkPath(map,
                            cand.waypoint_x_m, cand.waypoint_y_m,
                            tgt_x, tgt_y,
                            cfg.lethal_threshold,
                            cfg.inflated_threshold);

    cand.max_cost_along_path = std::max(seg1.max_cost, seg2.max_cost);
    cand.feasible = !(seg1.obstacle_detected || seg2.obstacle_detected);

    if (!cand.feasible) continue;

    if (!any_feasible) {
      best = cand;
      any_feasible = true;
    } else {
      // Prefer lower max_cost; tie-break on smaller |offset|
      const bool lower_cost = cand.max_cost_along_path < best.max_cost_along_path;
      const bool same_cost_smaller_off =
          (cand.max_cost_along_path == best.max_cost_along_path) &&
          (std::fabs(cand.lateral_offset_m) < std::fabs(best.lateral_offset_m));
      if (lower_cost || same_cost_smaller_off) {
        best = cand;
      }
    }
  }

  if (!any_feasible) return std::nullopt;
  return best;
}

}  // namespace san_reroute_planner
