// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_hub_slam/overlap_matcher.hpp"

#include <algorithm>
#include <cmath>

#include "san_hub_slam/aggregator.hpp"   // GLOBAL_FREE / UNKNOWN / OCCUPIED

namespace san_hub_slam
{

namespace
{

struct Aabb
{
  double minx, miny, maxx, maxy;
};

// World-frame axis-aligned bounding box over the four (possibly rotated)
// corners of a submap.
Aabb worldAabb(const Submap & s)
{
  const double ext_x = s.width * static_cast<double>(s.resolution_m);
  const double ext_y = s.height * static_cast<double>(s.resolution_m);
  const double c = std::cos(s.origin_theta);
  const double sn = std::sin(s.origin_theta);
  const double lx[4] = {0.0, ext_x, 0.0, ext_x};
  const double ly[4] = {0.0, 0.0, ext_y, ext_y};
  Aabb box{1e300, 1e300, -1e300, -1e300};
  for (int i = 0; i < 4; ++i) {
    const double wx = s.origin_x + c * lx[i] - sn * ly[i];
    const double wy = s.origin_y + sn * lx[i] + c * ly[i];
    box.minx = std::min(box.minx, wx);
    box.miny = std::min(box.miny, wy);
    box.maxx = std::max(box.maxx, wx);
    box.maxy = std::max(box.maxy, wy);
  }
  return box;
}

// Intersection area of two AABBs (0 if disjoint).
double intersectArea(const Aabb & a, const Aabb & b)
{
  const double w = std::min(a.maxx, b.maxx) - std::max(a.minx, b.minx);
  const double h = std::min(a.maxy, b.maxy) - std::max(a.miny, b.miny);
  if (w <= 0.0 || h <= 0.0) {return 0.0;}
  return w * h;
}

// A known (FREE/OCCUPIED) cell of submap b, in cell coordinates.
struct KnownCell
{
  int cx;
  int cy;
  bool occupied;
};

}  // namespace

OverlapMatcher::OverlapMatcher(const OverlapMatchParams & params)
: params_(params)
{}

OverlapMatch OverlapMatcher::match(const Submap & a, const Submap & b) const
{
  OverlapMatch out;

  if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) {
    return out;
  }
  if (a.resolution_m <= 0.0f || b.resolution_m <= 0.0f) {return out;}
  if (a.grid.size() !=
    static_cast<std::size_t>(a.width) * static_cast<std::size_t>(a.height))
  {
    return out;
  }
  if (b.grid.size() !=
    static_cast<std::size_t>(b.width) * static_cast<std::size_t>(b.height))
  {
    return out;
  }

  // AABB pre-check: expand b's box by the translation search window so a
  // pair that only overlaps after correction is still considered.
  const Aabb abox = worldAabb(a);
  Aabb bbox = worldAabb(b);
  out.overlap_area_m2 = intersectArea(abox, bbox);
  bbox.minx -= params_.search_xy_m;
  bbox.miny -= params_.search_xy_m;
  bbox.maxx += params_.search_xy_m;
  bbox.maxy += params_.search_xy_m;
  if (intersectArea(abox, bbox) <= 0.0) {
    return out;     // has_overlap stays false
  }
  out.has_overlap = true;

  // Collect b's known cells, subsampled to bound the inner loop cost.
  std::vector<KnownCell> known;
  known.reserve(b.grid.size() / 4 + 1);
  for (int cy = 0; cy < b.height; ++cy) {
    for (int cx = 0; cx < b.width; ++cx) {
      const uint8_t v = b.grid[static_cast<std::size_t>(cy) * b.width + cx];
      if (v == GLOBAL_FREE) {
        known.push_back({cx, cy, false});
      } else if (v == GLOBAL_OCCUPIED) {
        known.push_back({cx, cy, true});
      }
    }
  }
  if (known.empty()) {return out;}
  if (params_.max_samples > 0 &&
    static_cast<int>(known.size()) > params_.max_samples)
  {
    const std::size_t stride = known.size() / params_.max_samples;
    std::vector<KnownCell> sampled;
    sampled.reserve(params_.max_samples + 1);
    for (std::size_t i = 0; i < known.size(); i += stride) {
      sampled.push_back(known[i]);
    }
    known.swap(sampled);
  }

  // Reference-frame inverse transform (world -> a cell).
  const double ca = std::cos(a.origin_theta);
  const double sa = std::sin(a.origin_theta);
  const double inv_res_a = 1.0 / static_cast<double>(a.resolution_m);
  const double res_b = static_cast<double>(b.resolution_m);

  const double step_xy = std::max(1e-6, params_.step_xy_m);
  const double step_th = std::max(1e-9, params_.step_theta_rad);
  const int n_xy = static_cast<int>(params_.search_xy_m / step_xy);
  const int n_th = static_cast<int>(params_.search_theta_rad / step_th);

  bool best_set = false;
  double best_mag = 1e300;     // squared magnitude of the chosen correction
  // Integer-indexed loops so the identity (i==j==k==0) candidate is hit
  // exactly and recorded as score_identity.
  for (int k = -n_th; k <= n_th; ++k) {
    const double dth = k * step_th;
    const double bth = b.origin_theta + dth;
    const double cb = std::cos(bth);
    const double sb = std::sin(bth);
    for (int j = -n_xy; j <= n_xy; ++j) {
      const double ddy = j * step_xy;
      const double by = b.origin_y + ddy;
      for (int i = -n_xy; i <= n_xy; ++i) {
        const double ddx = i * step_xy;
        const double bx = b.origin_x + ddx;

        // Occupied-IoU tally over b's known cells mapped into a.
        int tp = 0;     // b occupied AND a occupied
        int fp = 0;     // b occupied, a free  (conflict)
        int fn = 0;     // b free, a occupied  (conflict)
        for (const auto & kc : known) {
          const double lxb = kc.cx * res_b;
          const double lyb = kc.cy * res_b;
          const double wx = bx + cb * lxb - sb * lyb;
          const double wy = by + sb * lxb + cb * lyb;
          const double rx = wx - a.origin_x;
          const double ry = wy - a.origin_y;
          const double alx = ca * rx + sa * ry;     // R(-aθ)
          const double aly = -sa * rx + ca * ry;
          const int acx = static_cast<int>(std::floor(alx * inv_res_a));
          const int acy = static_cast<int>(std::floor(aly * inv_res_a));
          if (acx < 0 || acx >= a.width || acy < 0 || acy >= a.height) {
            continue;
          }
          const uint8_t av =
            a.grid[static_cast<std::size_t>(acy) * a.width + acx];
          if (av == GLOBAL_UNKNOWN) {continue;}
          const bool a_occ = (av == GLOBAL_OCCUPIED);
          if (kc.occupied && a_occ) {
            ++tp;
          } else if (kc.occupied && !a_occ) {
            ++fp;
          } else if (!kc.occupied && a_occ) {
            ++fn;
          }
          // free/free contributes nothing (ignored on purpose).
        }
        const int occ_union = tp + fp + fn;
        const double score = (occ_union > 0) ?
          static_cast<double>(tp) / static_cast<double>(occ_union) : 0.0;

        const bool is_identity = (i == 0 && j == 0 && k == 0);
        if (is_identity) {
          out.score_identity = (occ_union >= params_.min_overlap_cells) ?
            score : 0.0;
        }
        if (occ_union < params_.min_overlap_cells) {continue;}
        // Tie-break toward the SMALLEST correction: when two candidates
        // explain the overlap equally well (occupied IoU within eps),
        // trust the prior and prefer the minimal shift/rotation. This
        // removes the degeneracy of near-symmetric structure and is the
        // conservative choice for a loop-closure constraint.
        const double mag = ddx * ddx + ddy * ddy + dth * dth;
        constexpr double kScoreEps = 1e-6;
        if (!best_set || score > out.best_score + kScoreEps) {
          best_set = true;
          out.best_score = score;
          best_mag = mag;
          out.overlap_cells = occ_union;
          out.dx = ddx;
          out.dy = ddy;
          out.dtheta = dth;
        } else if (score > out.best_score - kScoreEps && mag < best_mag) {
          best_mag = mag;
          out.best_score = std::max(out.best_score, score);
          out.overlap_cells = occ_union;
          out.dx = ddx;
          out.dy = ddy;
          out.dtheta = dth;
        }
      }
    }
  }

  out.confident =
    best_set &&
    out.best_score >= params_.confident_score &&
    (out.best_score - out.score_identity) >= params_.min_score_gain &&
    out.overlap_cells >= params_.min_overlap_cells;
  return out;
}

}  // namespace san_hub_slam
