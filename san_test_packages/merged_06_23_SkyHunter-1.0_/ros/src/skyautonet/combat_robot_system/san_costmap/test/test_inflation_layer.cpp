// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.3 — Phase-7 safety-critical audit P0: InflationLayerV13.
//
// inflation_layer_v13.cpp implements the Nav2-compatible exponential
// decay around lethal cells:
//   cost(d) = (LETHAL - 1) * exp(-cost_scaling_factor * d)
//
// The previous test suite (test_cost_thresholds.cpp + traversability)
// covered ObstacleLayerV13 + TraversabilityLayer thresholds well but
// left InflationLayerV13 entirely unverified despite it being the
// driver of Nav2 path planning decisions around obstacles. A 1-line
// off-by-one in the radius_cells calculation, the dist_m bound, or
// the clamp would silently route the robot through inflated zones.
//
// This test file pins the inflation invariants:
//
//   I1  Empty grid (no lethal) leaves grid untouched
//   I2  Single lethal cell produces decreasing cost band outward
//       (monotonicity: cost(d1) >= cost(d2) when d1 < d2)
//   I3  Cost at distance > inflation_radius_m stays at original value
//       (no leakage outside radius)
//   I4  Existing higher cost in non-lethal cells is preserved
//       (max-pool semantics — we don't lower an existing warn cell)
//   I5  Lethal cells are never lowered by inflation (LETHAL is the
//       hard boundary)
//   I6  Inflated values fit in [0, LETHAL - 1] — never exceed 253
//       (clamp invariant)
//   I7  Radius scaling — at d == 0 (adjacent to lethal), cost equals
//       (LETHAL - 1) before clamp, so the closest non-lethal cells
//       see the highest inflation
//   I8  Geometry boundary — lethal cell at edge of grid does NOT
//       wrap or crash; out-of-bounds neighbors are skipped
//
// Pure-logic, no rclcpp, runs in <5 ms.

#include <gtest/gtest.h>

#include "san_costmap/cost_constants.hpp"
#include "san_costmap/inflation_layer_v13.hpp"

#include <cmath>
#include <vector>

namespace san_costmap
{
namespace
{

// Build a master grid of given size pre-filled with COST_FREE.
std::vector<uint8_t> makeFreeGrid(int w, int h)
{
  return std::vector<uint8_t>(
    static_cast<std::size_t>(w) * static_cast<std::size_t>(h),
    COST_FREE);
}

}  // namespace

// ─── I1: no lethal cells → grid unchanged ─────────────────────────────
TEST(InflationLayer, I1_EmptyGridUnchanged) {
  InflationLayerV13 inf(20, 20, 0.05f);     // 1 m × 1 m
  auto grid = makeFreeGrid(20, 20);
  const auto orig = grid;
  inf.inflate(grid);
  EXPECT_EQ(grid, orig)
    << "no lethal cells means inflation has no work; output must "
    "be byte-equivalent to input";
}

// ─── I2: monotonic decay outward from a single lethal cell ───────────
TEST(InflationLayer, I2_MonotonicDecayOutward) {
  InflationLayerV13 inf(40, 40, 0.05f);      // 2 m × 2 m
  inf.setInflationRadiusM(0.5f);             // 10 cells radius
  auto grid = makeFreeGrid(40, 40);

  // Lethal cell at (20, 20)
  const int cx = 20, cy = 20;
  grid[cellIndex(cx, cy, 40)] = COST_LETHAL;
  inf.inflate(grid);

  // Sample cells along +X axis at 1, 2, 3, ..., 9 cells.
  uint8_t prev_cost = COST_LETHAL;     // start "below" the inflation cap
  for (int d = 1; d <= 9; ++d) {
    const uint8_t c = grid[cellIndex(cx + d, cy, 40)];
    EXPECT_LE(c, prev_cost)
      << "monotonicity violated at d=" << d
      << " — cost should not increase as distance grows from "
      "lethal cell. prev=" << static_cast<int>(prev_cost)
      << " now=" << static_cast<int>(c);
    prev_cost = c;
  }
  // Sanity: closest neighbor (d=1, 0.05 m) is at least non-zero.
  EXPECT_GT(grid[cellIndex(cx + 1, cy, 40)], 0u)
    << "cell adjacent to lethal must receive non-zero inflation";
}

// ─── I3: no leakage outside inflation_radius_m ────────────────────────
TEST(InflationLayer, I3_NoLeakageOutsideRadius) {
  InflationLayerV13 inf(50, 50, 0.05f);       // 2.5 m × 2.5 m
  inf.setInflationRadiusM(0.5f);              // 10 cells radius

  auto grid = makeFreeGrid(50, 50);
  grid[cellIndex(25, 25, 50)] = COST_LETHAL;
  inf.inflate(grid);

  // Cell at d=20 cells (1.0 m) — well outside 0.5 m radius
  EXPECT_EQ(grid[cellIndex(25 + 20, 25, 50)], COST_FREE)
    << "cell far outside inflation_radius_m must stay at the "
    "original cost (COST_FREE here) — no leakage";
  // Cell at d=11 cells (0.55 m) — just outside radius
  EXPECT_EQ(grid[cellIndex(25 + 11, 25, 50)], COST_FREE)
    << "cell just outside inflation_radius_m must stay at "
    "original cost (boundary correctness)";
}

// ─── I4: existing higher cost preserved (max-pool semantics) ─────────
TEST(InflationLayer, I4_ExistingHigherCostPreserved) {
  InflationLayerV13 inf(20, 20, 0.05f);
  inf.setInflationRadiusM(0.5f);

  auto grid = makeFreeGrid(20, 20);
  // Lethal cell at (10, 10) and a pre-existing WARN_HIGH at (12, 10)
  grid[cellIndex(10, 10, 20)] = COST_LETHAL;
  grid[cellIndex(12, 10, 20)] = COST_WARN_HIGH;    // 200, very high
  inf.inflate(grid);

  // The WARN_HIGH cell (200) should remain at 200 because the
  // inflation at d=2 cells (0.1 m) computes
  //   cost = 253 * exp(-3.0 * 0.1) ≈ 187, which is < 200
  // → max-pool keeps 200, doesn't lower it. Critical invariant.
  EXPECT_EQ(grid[cellIndex(12, 10, 20)], COST_WARN_HIGH)
    << "pre-existing warn cells must NOT be lowered by an "
    "inflation pass that computes a smaller value";
}

// ─── I5: lethal cells never lowered ──────────────────────────────────
TEST(InflationLayer, I5_LethalCellsNeverLowered) {
  InflationLayerV13 inf(20, 20, 0.05f);
  inf.setInflationRadiusM(0.5f);

  auto grid = makeFreeGrid(20, 20);
  grid[cellIndex(10, 10, 20)] = COST_LETHAL;
  grid[cellIndex(11, 10, 20)] = COST_LETHAL;     // adjacent lethal
  inf.inflate(grid);

  EXPECT_EQ(grid[cellIndex(10, 10, 20)], COST_LETHAL)
    << "lethal cells must not be overwritten by inflation";
  EXPECT_EQ(grid[cellIndex(11, 10, 20)], COST_LETHAL);
}

// ─── I6: inflated values stay strictly below COST_LETHAL ─────────────
TEST(InflationLayer, I6_InflatedValuesClampedBelowLethal) {
  InflationLayerV13 inf(40, 40, 0.05f);
  inf.setInflationRadiusM(0.5f);

  auto grid = makeFreeGrid(40, 40);
  grid[cellIndex(20, 20, 40)] = COST_LETHAL;
  inf.inflate(grid);

  // Sweep entire grid; ANY non-original-lethal cell that was
  // touched by inflation must be ≤ LETHAL - 1.
  for (int y = 0; y < 40; ++y) {
    for (int x = 0; x < 40; ++x) {
      if (x == 20 && y == 20) {
        continue;                               // the lethal origin
      }
      const uint8_t c = grid[cellIndex(x, y, 40)];
      ASSERT_LT(c, COST_LETHAL)
        << "cell (" << x << ", " << y << ") inflated to "
        << static_cast<int>(c)
        << " — must be strictly < COST_LETHAL ("
        << static_cast<int>(COST_LETHAL) << ")";
    }
  }
}

// ─── I7: cell immediately adjacent to lethal gets near-max inflation ─
TEST(InflationLayer, I7_AdjacentCellsGetHighestInflation) {
  InflationLayerV13 inf(20, 20, 0.05f);
  inf.setInflationRadiusM(1.0f);

  auto grid = makeFreeGrid(20, 20);
  grid[cellIndex(10, 10, 20)] = COST_LETHAL;
  inf.inflate(grid);

  // Adjacent (d=1 cell = 0.05 m): cost = 253 * exp(-3.0 * 0.05)
  //                               ≈ 253 * 0.861 ≈ 218
  const uint8_t c_adj = grid[cellIndex(11, 10, 20)];
  EXPECT_GE(c_adj, 200u)
    << "cell directly adjacent to lethal at radius=1.0 m, k=3.0, "
    "0.05m resolution should get cost ≈ 218 (got "
    << static_cast<int>(c_adj) << ")";
  EXPECT_LT(c_adj, COST_LETHAL);

  // Diagonal adjacent (d=sqrt(2) cells = 0.0707 m): cost slightly
  // lower than horizontal adjacent. Verifies sqrt distance handling.
  const uint8_t c_diag = grid[cellIndex(11, 11, 20)];
  EXPECT_LT(c_diag, c_adj)
    << "diagonal neighbor should have lower cost than horizontal "
    "(sqrt(2)·res > 1·res); confirms dist_m uses Euclidean.";
}

// ─── I8: lethal at grid edge — no wrap, no crash ─────────────────────
TEST(InflationLayer, I8_LethalAtEdgeDoesNotWrap) {
  InflationLayerV13 inf(10, 10, 0.05f);
  inf.setInflationRadiusM(0.3f);     // 6-cell radius

  auto grid = makeFreeGrid(10, 10);
  grid[cellIndex(0, 0, 10)] = COST_LETHAL;     // corner cell

  EXPECT_NO_THROW(inf.inflate(grid));

  // Cell (9, 9) is the opposite corner — far outside any
  // wrap-around radius. Must remain COST_FREE.
  EXPECT_EQ(grid[cellIndex(9, 9, 10)], COST_FREE)
    << "corner lethal must not wrap to opposite corner via "
    "index arithmetic overflow";
  // Confirm in-bounds inflation still happened at (1, 0).
  EXPECT_GT(grid[cellIndex(1, 0, 10)], 0u)
    << "in-bounds neighbor of corner lethal should be inflated";
}

}  // namespace san_costmap
