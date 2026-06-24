// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Reroute Planner standalone tests.
//
// Coverage:
//   V1   CostMapView basic accessors
//   V2   CostMapView out-of-bounds → COST_UNKNOWN
//   C1   checkPath free path → no obstacle, max_cost=0
//   C2   checkPath with lethal cell → obstacle detected
//   C3   checkPath with inflated cell → inflated detected
//   C4   checkPath obstacle_distance accurate
//   C5   checkPath single-point degenerate
//   E1   findBestEvasion straight clear → midpoint waypoint
//   E2   findBestEvasion central lethal → ±2m candidate found
//   E3   findBestEvasion all blocked → nullopt
//   E4   findBestEvasion prefers smaller |offset| at equal cost
//   E5   findBestEvasion respects map bounds
//   K1   ★ KPP-2 critical-path timing — checkPath + findBestEvasion
//        on 280×280 grid must run ≤ 10 ms (FSM-side budget)

#include "san_reroute_planner/cost_map_view.hpp"
#include "san_reroute_planner/cost_path_checker.hpp"
#include "san_reroute_planner/lateral_evasion.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>

namespace san_reroute_planner
{
namespace
{

/// Build a square cost map of given size, all FREE.
CostMapView makeFreeMap(
  uint32_t w, uint32_t h,
  float res = 0.05f,
  float ox = 0.0f, float oy = 0.0f)
{
  CostMapView m;
  m.width = w;
  m.height = h;
  m.resolution_m = res;
  m.origin_x_m = ox;
  m.origin_y_m = oy;
  m.grid.assign(static_cast<size_t>(w) * h, COST_FREE);
  return m;
}

/// Mark a rectangular region with given cost.
void paintRect(
  CostMapView & m,
  float x_lo, float y_lo, float x_hi, float y_hi,
  uint8_t cost)
{
  const int gx_lo = static_cast<int>((x_lo - m.origin_x_m) / m.resolution_m);
  const int gy_lo = static_cast<int>((y_lo - m.origin_y_m) / m.resolution_m);
  const int gx_hi = static_cast<int>((x_hi - m.origin_x_m) / m.resolution_m);
  const int gy_hi = static_cast<int>((y_hi - m.origin_y_m) / m.resolution_m);
  for (int y = std::max(0, gy_lo);
    y <= std::min(static_cast<int>(m.height) - 1, gy_hi); ++y)
  {
    for (int x = std::max(0, gx_lo);
      x <= std::min(static_cast<int>(m.width) - 1, gx_hi); ++x)
    {
      m.grid[static_cast<size_t>(y) * m.width + x] = cost;
    }
  }
}

// ─── CostMapView ───────────────────────────────────────────────────────

TEST(CostMapView, V1_BasicAccessors) {
  CostMapView m = makeFreeMap(10, 10);
  EXPECT_TRUE(m.valid());
  EXPECT_EQ(m.costAt(0.05f, 0.05f), COST_FREE);
}

TEST(CostMapView, V2_OutOfBoundsReturnsUnknown) {
  CostMapView m = makeFreeMap(10, 10);
  EXPECT_EQ(m.costAt(-1.0f, 0.5f), COST_UNKNOWN);
  EXPECT_EQ(m.costAt(100.0f, 0.5f), COST_UNKNOWN);
  EXPECT_FALSE(m.inBounds(-1.0f, 0.5f));
}

// ─── CostPathChecker ───────────────────────────────────────────────────

TEST(CostPathChecker, C1_FreePath) {
  CostMapView m = makeFreeMap(100, 100);
  auto r = checkPath(m, 1.0f, 1.0f, 4.0f, 4.0f);
  EXPECT_FALSE(r.obstacle_detected);
  EXPECT_FALSE(r.inflated_detected);
  EXPECT_EQ(r.max_cost, 0u);
  EXPECT_GT(r.cells_checked, 50u);
}

TEST(CostPathChecker, C2_LethalCellDetected) {
  CostMapView m = makeFreeMap(100, 100);
  paintRect(m, 2.0f, 2.0f, 2.5f, 2.5f, COST_LETHAL);
  auto r = checkPath(m, 1.0f, 1.0f, 4.0f, 4.0f);
  EXPECT_TRUE(r.obstacle_detected);
  EXPECT_EQ(r.max_cost, COST_LETHAL);
  EXPECT_GT(r.obstacle_distance_m, 0.5f);
  EXPECT_LT(r.obstacle_distance_m, 3.0f);
}

TEST(CostPathChecker, C3_InflatedCellDetected) {
  CostMapView m = makeFreeMap(100, 100);
  paintRect(m, 2.0f, 2.0f, 2.5f, 2.5f, 150);   // inflated band
  auto r = checkPath(m, 1.0f, 1.0f, 4.0f, 4.0f);
  EXPECT_FALSE(r.obstacle_detected);            // no lethal
  EXPECT_TRUE(r.inflated_detected);             // but inflated
  EXPECT_EQ(r.max_cost, 150u);
}

TEST(CostPathChecker, C4_ObstacleDistanceAccurate) {
  CostMapView m = makeFreeMap(100, 100);
  // Lethal cell at world (3.0, 1.0). Path from (1.0, 1.0) → (4.0, 1.0)
  // — straight along x. Obstacle distance should be ~2.0 m.
  paintRect(m, 3.0f, 0.95f, 3.1f, 1.05f, COST_LETHAL);
  auto r = checkPath(m, 1.0f, 1.0f, 4.0f, 1.0f);
  EXPECT_TRUE(r.obstacle_detected);
  EXPECT_NEAR(r.obstacle_distance_m, 2.0f, 0.1f);
}

TEST(CostPathChecker, C5_DegeneratePath) {
  CostMapView m = makeFreeMap(20, 20);
  paintRect(m, 0.4f, 0.4f, 0.6f, 0.6f, COST_LETHAL);
  auto r = checkPath(m, 0.5f, 0.5f, 0.5f, 0.5f);  // length ≈ 0
  EXPECT_TRUE(r.obstacle_detected);
  EXPECT_EQ(r.cells_checked, 1u);
}

// ─── LateralEvasion ─────────────────────────────────────────────────────

TEST(LateralEvasion, E1_StraightFreePathReturnsMidpoint) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  auto r = findBestEvasion(m, 1.0f, 5.0f, 9.0f, 5.0f);
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(r->feasible);
  // All offsets feasible → smallest |offset| (0.5m) preferred at zero cost
  EXPECT_EQ(r->max_cost_along_path, 0u);
  EXPECT_NEAR(std::fabs(r->lateral_offset_m), 0.5f, 1e-3);
}

TEST(LateralEvasion, E2_CentralLethalReroutes) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  // Block the direct path (along y=5) with a vertical wall at x=5.
  paintRect(m, 4.8f, 4.7f, 5.2f, 5.3f, COST_LETHAL);
  auto r = findBestEvasion(m, 1.0f, 5.0f, 9.0f, 5.0f);
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(r->feasible);
  // Waypoint must be off the centre line — |lateral_offset| > 0
  EXPECT_GT(std::fabs(r->lateral_offset_m), 0.2f);
  // Direct path cost was lethal but evasion path < lethal
  EXPECT_LT(r->max_cost_along_path, COST_LETHAL);
}

TEST(LateralEvasion, E3_AllBlockedReturnsNullopt) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  // Wall covering all reasonable lateral offsets at x=5
  paintRect(m, 4.8f, 0.0f, 5.2f, 20.0f, COST_LETHAL);
  auto r = findBestEvasion(m, 1.0f, 5.0f, 9.0f, 5.0f);
  EXPECT_FALSE(r.has_value());
}

TEST(LateralEvasion, E4_PrefersSmallerOffsetAtEqualCost) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  auto r = findBestEvasion(m, 1.0f, 5.0f, 9.0f, 5.0f);
  ASSERT_TRUE(r.has_value());
  // All offsets are zero cost on a fully free map; the search should
  // prefer the smallest |offset| (0.5m, the first in the list).
  EXPECT_NEAR(std::fabs(r->lateral_offset_m), 0.5f, 1e-3);
}

TEST(LateralEvasion, E5_RespectsMapBounds) {
  // Tiny 5×5 map (0.5m × 0.5m); cur/tgt near edge so +2m offset is out.
  CostMapView m = makeFreeMap(5, 5, 0.1f);
  // checkPath against COST_UNKNOWN OOB treats as inflated; the function
  // returns a result regardless. We just verify the call doesn't crash.
  auto r = findBestEvasion(m, 0.05f, 0.25f, 0.45f, 0.25f);
  // Tiny map — most offsets land OOB → effective cost 50 (inflated)
  // but no lethal → still feasible.
  if (r.has_value()) {
    EXPECT_TRUE(r->feasible);
  }
}

// ─── KPP-2 Critical Path Timing ────────────────────────────────────────

TEST(KPP2Timing, K1_FullCriticalPathUnder10ms) {
  // Realistic grid: 280×280 cells × 0.05 m = 14m × 14m (SDD default).
  CostMapView m = makeFreeMap(280, 280, 0.05f);
  // Plant 3 obstacles to make the search non-trivial
  paintRect(m, 5.0f, 5.0f, 5.3f, 5.3f, COST_LETHAL);
  paintRect(m, 8.0f, 4.0f, 8.3f, 4.3f, COST_LETHAL);
  paintRect(m, 6.5f, 8.0f, 6.8f, 8.3f, 200);

  const auto t0 = std::chrono::high_resolution_clock::now();

  // Step 1: cost path check (current → 1s predicted target)
  auto check = checkPath(m, 2.0f, 5.0f, 12.0f, 5.0f);
  ASSERT_TRUE(check.obstacle_detected);

  // Step 2: lateral evasion search
  auto cand = findBestEvasion(m, 2.0f, 5.0f, 12.0f, 5.0f);
  ASSERT_TRUE(cand.has_value());

  const auto t1 = std::chrono::high_resolution_clock::now();
  const auto elapsed_us =
    std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

  // KPP-2 system budget: 300 ms
  //   FSM step:           < 1 ms
  //   Cost map traversal: target ≤ 10 ms  ← this test
  //   Controller_server:  ~50 ms
  //   Actuator:           ~100 ms
  EXPECT_LT(elapsed_us, 10000)
    << "KPP-2 critical path " << elapsed_us << "us > 10ms";

  // Report for visibility
  std::printf(
    "[KPP-2] critical path elapsed: %ldus (budget 10000us)\n",
    static_cast<long>(elapsed_us));
}

// ═══════════════════════════════════════════════════════════════════════
// PATCH 2026-05-13 — new tests covering deep-dive fixes
// ═══════════════════════════════════════════════════════════════════════

// ─── PR1 (★ C5 fix): StartCellLethal explicit handling ─────────────────
TEST(PatchEvasion, PR1_StartInsideLethalReturnsStartCellLethal) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  // Plant a lethal patch right under the start point.
  paintRect(m, 0.9f, 4.9f, 1.1f, 5.1f, COST_LETHAL);

  EvasionStatus status = EvasionStatus::Ok;
  auto r = findBestEvasion(
    m, 1.0f, 5.0f, 9.0f, 5.0f,
    EvasionConfig{}, &status);
  EXPECT_FALSE(r.has_value());
  // Critical distinction: this must NOT be reported as AllBlocked.
  EXPECT_EQ(status, EvasionStatus::StartCellLethal);
}

// ─── PR2 (★ M9 fix): heading-aware tie-break ───────────────────────────
TEST(PatchEvasion, PR2_HeadingAwarePrefersFacingSide) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  EvasionConfig cfg;
  cfg.heading_aware = true;

  // Path is +x. With current_yaw slightly left (CCW), preferred side
  // is left → positive offset. All map free so cost ties at every
  // offset; smallest |offset| (0.5) wins on cost+|offset|, then on
  // heading tie-break: +0.5 over -0.5.
  EvasionStatus status = EvasionStatus::Ok;
  auto r = findBestEvasion(
    m, 1.0f, 5.0f, 9.0f, 5.0f, cfg, &status,
    /*current_yaw_rad=*/ +0.5f);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(status, EvasionStatus::Ok);
  EXPECT_NEAR(std::fabs(r->lateral_offset_m), 0.5f, 1e-3);
  // Heading-aware → robot facing left → prefer positive offset.
  EXPECT_GT(r->lateral_offset_m, 0.0f);
}

// ─── PR3 (★ M9 fix): heading-aware DISABLED preserves default ──────────
TEST(PatchEvasion, PR3_HeadingAwareOffPreservesV1_5_0Behavior) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  EvasionConfig cfg;
  cfg.heading_aware = false;   // default — back-compat

  EvasionStatus status = EvasionStatus::Ok;
  auto r = findBestEvasion(m, 1.0f, 5.0f, 9.0f, 5.0f, cfg, &status, +0.5f);
  ASSERT_TRUE(r.has_value());
  // Without heading-awareness, search order picks first feasible at
  // each cost/|offset| tier — same as v1.5.0 (offset = +0.5 because
  // it appears first in the default offsets list).
  EXPECT_NEAR(std::fabs(r->lateral_offset_m), 0.5f, 1e-3);
}

// ─── PR4 (★ COST_UNKNOWN handling regression) ──────────────────────────
TEST(PatchEvasion, PR4_StartUnknownIsNotTreatedAsLethal) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  // Wipe a cell to UNKNOWN under the start point.
  const int gx = static_cast<int>((1.0f - m.origin_x_m) / m.resolution_m);
  const int gy = static_cast<int>((5.0f - m.origin_y_m) / m.resolution_m);
  m.grid[gy * m.width + gx] = COST_UNKNOWN;

  EvasionStatus status = EvasionStatus::Ok;
  auto r = findBestEvasion(
    m, 1.0f, 5.0f, 9.0f, 5.0f,
    EvasionConfig{}, &status);
  // UNKNOWN at start is NOT lethal — the search should proceed.
  EXPECT_NE(status, EvasionStatus::StartCellLethal);
  EXPECT_TRUE(r.has_value());
}

// ─── PR5 (★ regression): backward-compat 5-arg findBestEvasion ──────────
TEST(PatchEvasion, PR5_FiveArgOverloadStillWorks) {
  CostMapView m = makeFreeMap(200, 200, 0.1f);
  // Original (pre-patch) call signature — no status, no yaw.
  auto r = findBestEvasion(m, 1.0f, 5.0f, 9.0f, 5.0f);
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(r->feasible);
}

}  // namespace
}  // namespace san_reroute_planner
