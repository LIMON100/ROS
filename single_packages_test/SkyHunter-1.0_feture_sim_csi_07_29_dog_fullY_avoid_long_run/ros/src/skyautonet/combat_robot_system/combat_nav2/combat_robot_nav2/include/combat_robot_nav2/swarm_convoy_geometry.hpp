#ifndef COMBAT_ROBOT_NAV2__SWARM_CONVOY_GEOMETRY_HPP_
#define COMBAT_ROBOT_NAV2__SWARM_CONVOY_GEOMETRY_HPP_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace combat_robot_nav2
{

// Perpendicular distance from point t_p to segment [t_a, t_b] (all map-frame x,y).
inline double pointSegDist(const std::pair<double, double> &t_p,
                           const std::pair<double, double> &t_a,
                           const std::pair<double, double> &t_b)
{
  const double dx = t_b.first - t_a.first;
  const double dy = t_b.second - t_a.second;
  const double len2 = (dx * dx) + (dy * dy);
  double t = 0.0;
  if (len2 > 1e-9) {
    t = (((t_p.first - t_a.first) * dx) + ((t_p.second - t_a.second) * dy)) / len2;
    t = std::max(0.0, std::min(1.0, t));
  }
  const double px = t_a.first + (t * dx);
  const double py = t_a.second + (t * dy);
  return std::hypot(t_p.first - px, t_p.second - py);
}

// Nearest segment of polyline t_ref to point t_p → {segment index, perpendicular
// distance}. Index is the near-vertex index (i of segment [i, i+1]).
inline std::pair<std::size_t, double> nearestSegment(
  const std::pair<double, double> &t_p,
  const std::vector<std::pair<double, double>> &t_ref)
{
  double best = std::numeric_limits<double>::max();
  std::size_t best_i = 0;
  for (std::size_t i = 0; (i + 1) < t_ref.size(); ++i) {
    const double d = pointSegDist(t_p, t_ref[i], t_ref[i + 1]);
    if (d < best) { best = d; best_i = i; }
  }
  return {best_i, best};
}

// Along-track distance (metres) of point t_p projected onto the reference path's
// overall start→end axis. This is a PATH-INDEPENDENT, monotonic progress measure
// in real units — unlike a FollowPath dist-to-goal fraction it does not jump when
// the tracked path is swapped, and unlike a nearest-segment INDEX it does not
// quantise or collapse to 0 when the point sits before the path start (it simply
// goes negative). Safe to gate on.
inline double alongTrack(
  const std::pair<double, double> &t_p,
  const std::vector<std::pair<double, double>> &t_ref)
{
  if (t_ref.size() < 2) { return 0.0; }
  double dx = t_ref.back().first - t_ref.front().first;
  double dy = t_ref.back().second - t_ref.front().second;
  const double len = std::hypot(dx, dy);
  if (len < 1e-6) { return 0.0; }
  dx /= len;
  dy /= len;
  return ((t_p.first - t_ref.front().first) * dx) +
         ((t_p.second - t_ref.front().second) * dy);
}

// Along-track distance (metres) of the FAR EDGE of the leader's obstacle detour:
// the largest along-track coordinate among trace poses whose perpendicular
// distance to the reference path exceeds t_lat_thresh metres. Returns
// -std::numeric_limits<double>::max() when the trace never deviates (no detour).
inline double detourFarAlongTrack(
  const std::vector<std::pair<double, double>> &t_trace,
  const std::vector<std::pair<double, double>> &t_ref,
  double t_lat_thresh)
{
  const double kNone = -std::numeric_limits<double>::max();
  if (t_ref.size() < 2 || t_trace.empty()) { return kNone; }
  double far_s = kNone;
  for (const auto &tp : t_trace) {
    if (nearestSegment(tp, t_ref).second > t_lat_thresh) {
      const double s = alongTrack(tp, t_ref);
      if (s > far_s) { far_s = s; }
    }
  }
  return far_s;
}

}  // namespace combat_robot_nav2

#endif  // COMBAT_ROBOT_NAV2__SWARM_CONVOY_GEOMETRY_HPP_
