// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 3 — inter-robot SLAM overlap matcher (SLAM-1 follow-up).
//
// Detects whether two robots' local occupancy submaps cover overlapping
// physical ground and, if so, estimates the small corrective transform
// (Δx, Δy, Δθ) to add to one robot's self-reported grid origin so the two
// occupancy patterns align. The correction is the measurement of a
// pose-graph loop-closure edge.
//
// SAFETY: a bad loop-closure constraint corrupts the whole pose graph, so
// the matcher is deliberately conservative and structural:
//   * the agreement score is an IoU over OCCUPIED cells only — featureless
//     free space cannot trigger a match (free/free overlap is ignored), so
//     the score is not inflated by the large free majority;
//   * a candidate is only "confident" when its score clears an absolute
//     threshold AND beats the zero-correction (identity) score by a margin
//     AND has enough occupied-cell support.
// The hub only turns a confident match into an actual edge when the
// operator opts in via the loop_closure_enabled parameter (default off);
// otherwise the result is published as a diagnostic only.
//
// Pure C++ (no rclcpp) so it is unit-tested directly with synthetic grids.

#pragma once

#include <cstdint>
#include <vector>

namespace san_hub_slam
{

// A robot's local occupancy submap placed in the world frame. grid is
// row-major with the GLOBAL_FREE / GLOBAL_UNKNOWN / GLOBAL_OCCUPIED
// sentinels from aggregator.hpp. (origin_x, origin_y, origin_theta) is the
// world pose of cell (0,0) — i.e. the robot's current best origin estimate
// (its self-reported origin, refined by the pose graph).
struct Submap
{
  std::vector<uint8_t> grid;
  int width = 0;
  int height = 0;
  float resolution_m = 0.0f;
  double origin_x = 0.0;
  double origin_y = 0.0;
  double origin_theta = 0.0;
};

struct OverlapMatchParams
{
  double search_xy_m = 0.5;            // ± translation search window
  double step_xy_m = 0.05;             // translation search step
  double search_theta_rad = 0.2618;    // ± rotation window (~15°)
  double step_theta_rad = 0.0873;      // rotation step (~5°)
  int min_overlap_cells = 60;          // min occupied-union support
  int max_samples = 800;               // subsample of b's known cells
  double confident_score = 0.70;       // absolute IoU gate
  double min_score_gain = 0.05;        // best must beat identity by this
};

struct OverlapMatch
{
  bool has_overlap = false;        // world AABBs (b expanded by window) meet
  int overlap_cells = 0;           // occupied-union cells at best candidate
  double overlap_area_m2 = 0.0;    // world AABB intersection area
  double score_identity = 0.0;     // occupied IoU at zero correction
  double best_score = 0.0;         // occupied IoU at best correction
  double dx = 0.0;                 // additive correction to b.origin_x
  double dy = 0.0;                 // additive correction to b.origin_y
  double dtheta = 0.0;             // additive correction to b.origin_theta
  bool confident = false;          // passes all gates → safe loop closure
};

class OverlapMatcher
{
public:
  explicit OverlapMatcher(const OverlapMatchParams & params = {});

  // Estimate the additive correction to submap b's origin that best aligns
  // b's occupancy with reference submap a. The reference a is held fixed.
  OverlapMatch match(const Submap & a, const Submap & b) const;

  const OverlapMatchParams & params() const {return params_;}

private:
  OverlapMatchParams params_;
};

}  // namespace san_hub_slam
