// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 3 — OverlapMatcher (SLAM-1 follow-up) unit tests.
//
// Validates the inter-robot loop-closure detector on SYNTHETIC submaps
// with a known ground-truth relative transform:
//   M1  recovers a pure translation bias
//   M2  recovers a pure rotation bias
//   M3  disjoint world bounds → no overlap, never confident
//   M4  overlapping bounds but non-matching structure → not confident
//   M5  featureless (free-only) overlap → not confident (no occupied IoU)
//
// Field-noise robustness (dynamic obstacles, sensor noise) needs real
// multi-robot data and is why the hub keeps active correction behind a
// default-off flag — these tests pin the geometric correctness only.

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "san_hub_slam/aggregator.hpp"
#include "san_hub_slam/overlap_matcher.hpp"

using namespace san_hub_slam;

namespace
{

// Test-tuned params: small synthetic grids need a lower occupied-support
// floor than the production default (60).
OverlapMatchParams testParams()
{
  OverlapMatchParams p;
  p.min_overlap_cells = 15;
  p.max_samples = 0;     // no subsampling for deterministic tests
  return p;
}

Submap makeSubmap(int w, int h, float res)
{
  Submap s;
  s.width = w;
  s.height = h;
  s.resolution_m = res;
  s.grid.assign(static_cast<std::size_t>(w) * h, GLOBAL_FREE);
  return s;
}

void setBlock(Submap & s, int x0, int x1, int y0, int y1, uint8_t v)
{
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      s.grid[static_cast<std::size_t>(y) * s.width + x] = v;
    }
  }
}

}  // namespace

TEST(OverlapMatcher, M1_RecoversTranslationBias) {
  // Both robots observe the same world: a FREE field with one OCCUPIED
  // block. Robot b's reported origin is biased by (+0.2, -0.1) m, so the
  // corrected origin (b.origin + correction) must land back on a's origin.
  //
  // Note: with 0.1 m cells the alignment is only observable to within one
  // cell, and ties are broken toward the minimal correction — so the exact
  // sub-cell correction is not unique. We therefore assert on the corrected
  // RESIDUAL (must be within one cell), not on a specific Δ value.
  constexpr double kRes = 0.1;
  Submap a = makeSubmap(30, 30, static_cast<float>(kRes));
  // Asymmetric rectangle (wide, short) so the alignment is not rotationally
  // degenerate.
  setBlock(a, 10, 21, 12, 15, GLOBAL_OCCUPIED);
  a.origin_x = 0.0; a.origin_y = 0.0; a.origin_theta = 0.0;

  Submap b = a;                 // identical occupancy
  b.origin_x = 0.2; b.origin_y = -0.1; b.origin_theta = 0.0;

  OverlapMatcher m(testParams());
  const OverlapMatch r = m.match(a, b);

  EXPECT_TRUE(r.has_overlap);
  EXPECT_GT(r.best_score, 0.95);
  EXPECT_GT(r.best_score, r.score_identity);
  EXPECT_TRUE(r.confident);
  // Corrected origin lands on a's origin to within one cell.
  EXPECT_LE(std::abs(b.origin_x + r.dx - a.origin_x), kRes + 1e-6);
  EXPECT_LE(std::abs(b.origin_y + r.dy - a.origin_y), kRes + 1e-6);
  EXPECT_NEAR(r.dtheta, 0.0, 0.02);
}

TEST(OverlapMatcher, M2_RecoversRotationBias) {
  // Same L-shaped structure; robot b's origin orientation is biased by
  // +10°, so the matcher must recover ~-10° about the shared origin.
  Submap a = makeSubmap(30, 30, 0.1f);
  setBlock(a, 5, 20, 5, 5, GLOBAL_OCCUPIED);     // horizontal arm
  setBlock(a, 5, 5, 5, 12, GLOBAL_OCCUPIED);     // vertical arm (shorter)
  a.origin_x = 0.0; a.origin_y = 0.0; a.origin_theta = 0.0;

  Submap b = a;
  b.origin_theta = 10.0 * M_PI / 180.0;          // +10° bias

  OverlapMatcher m(testParams());
  const OverlapMatch r = m.match(a, b);

  EXPECT_TRUE(r.has_overlap);
  EXPECT_GT(r.best_score, 0.9);
  EXPECT_TRUE(r.confident);
  EXPECT_NEAR(r.dtheta, -10.0 * M_PI / 180.0, 0.05);
  EXPECT_NEAR(r.dx, 0.0, 0.06);
  EXPECT_NEAR(r.dy, 0.0, 0.06);
}

TEST(OverlapMatcher, M3_DisjointBoundsNoOverlap) {
  Submap a = makeSubmap(30, 30, 0.1f);
  setBlock(a, 12, 17, 10, 15, GLOBAL_OCCUPIED);

  Submap b = a;
  b.origin_x = 50.0; b.origin_y = 50.0;          // far away

  OverlapMatcher m(testParams());
  const OverlapMatch r = m.match(a, b);

  EXPECT_FALSE(r.has_overlap);
  EXPECT_FALSE(r.confident);
}

TEST(OverlapMatcher, M4_OverlapButWrongStructureNotConfident) {
  // Same bounds, but the occupied blocks are > search-window apart, so no
  // in-window correction can align them.
  Submap a = makeSubmap(30, 30, 0.1f);
  setBlock(a, 18, 23, 18, 23, GLOBAL_OCCUPIED);

  Submap b = makeSubmap(30, 30, 0.1f);
  setBlock(b, 2, 7, 2, 7, GLOBAL_OCCUPIED);      // ~1.6 m away, window 0.5
  b.origin_theta = 0.0;

  OverlapMatcher m(testParams());
  const OverlapMatch r = m.match(a, b);

  EXPECT_TRUE(r.has_overlap);
  EXPECT_LT(r.best_score, 0.5);
  EXPECT_FALSE(r.confident);
}

TEST(OverlapMatcher, M5_FeaturelessOverlapNotConfident) {
  // Two fully-FREE overlapping submaps: no occupied structure to match on.
  // The occupied-IoU has zero support, so the matcher must NOT claim a
  // confident loop closure on featureless free space.
  Submap a = makeSubmap(30, 30, 0.1f);
  Submap b = makeSubmap(30, 30, 0.1f);

  OverlapMatcher m(testParams());
  const OverlapMatch r = m.match(a, b);

  EXPECT_TRUE(r.has_overlap);
  EXPECT_EQ(r.overlap_cells, 0);
  EXPECT_FALSE(r.confident);
}

TEST(OverlapMatcher, M6_DegenerateInputsRejected) {
  OverlapMatcher m(testParams());
  Submap empty;                       // 0×0
  Submap a = makeSubmap(10, 10, 0.1f);
  EXPECT_FALSE(m.match(a, empty).confident);
  EXPECT_FALSE(m.match(empty, a).confident);

  Submap bad_res = makeSubmap(10, 10, 0.0f);
  EXPECT_FALSE(m.match(a, bad_res).has_overlap);
}
