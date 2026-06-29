// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 — DCN-2026-006 EXT D-021 (Bayesian voting) + D-026
// (mismatch logging) unit tests.

#include <gtest/gtest.h>
#include <vector>

#include "san_hub_slam/aggregator.hpp"

using namespace san_hub_slam;

namespace
{

// Build a uniform-value 3×3 grid for tiny test cases.
std::vector<uint8_t> grid3x3(uint8_t v)
{
  return std::vector<uint8_t>(9, v);
}

// Build a grid with one cell set to `v`, rest UNKNOWN.
std::vector<uint8_t> grid3x3OneCell(std::size_t idx, uint8_t v)
{
  auto g = grid3x3(GLOBAL_UNKNOWN);
  g[idx] = v;
  return g;
}

// PNG-encode a raw row-major grid so it can be fed to applyDeltaAt
// (which takes PNG bytes, mirroring the on-wire SLAMLocalDelta payload).
std::vector<uint8_t> png(
  const std::vector<uint8_t> & grid, int width, int height)
{
  return MultirobotAggregator::encodePng(grid, width, height);
}

}  // namespace

// ─── D-021: vote-based merge replaces last-write-wins ──────────────────

TEST(AggregatorD021, MajorityFreeWins) {
  MultirobotAggregator agg(3, 3, 0.05f);

  // Three free votes, one occupied vote → cell resolves FREE.
  agg.applyDeltaRaw("r1", grid3x3OneCell(4, GLOBAL_FREE));
  agg.applyDeltaRaw("r2", grid3x3OneCell(4, GLOBAL_FREE));
  agg.applyDeltaRaw("r3", grid3x3OneCell(4, GLOBAL_FREE));
  agg.applyDeltaRaw("r4", grid3x3OneCell(4, GLOBAL_OCCUPIED));

  agg.recomputeGlobal();
  EXPECT_EQ(agg.globalGrid()[4], GLOBAL_FREE);
}

TEST(AggregatorD021, MajorityOccupiedWins) {
  MultirobotAggregator agg(3, 3, 0.05f);

  agg.applyDeltaRaw("r1", grid3x3OneCell(0, GLOBAL_OCCUPIED));
  agg.applyDeltaRaw("r2", grid3x3OneCell(0, GLOBAL_OCCUPIED));
  agg.applyDeltaRaw("r3", grid3x3OneCell(0, GLOBAL_FREE));

  agg.recomputeGlobal();
  EXPECT_EQ(agg.globalGrid()[0], GLOBAL_OCCUPIED);
}

TEST(AggregatorD021, OrderIndependence) {
  // Critical regression check: in v1.5.1 last-write-wins, applying
  // r4's OCCUPIED last would have overwritten r1-r3's FREE.
  MultirobotAggregator agg_a(3, 3, 0.05f);
  agg_a.applyDeltaRaw("r4", grid3x3OneCell(4, GLOBAL_OCCUPIED));
  agg_a.applyDeltaRaw("r1", grid3x3OneCell(4, GLOBAL_FREE));
  agg_a.applyDeltaRaw("r2", grid3x3OneCell(4, GLOBAL_FREE));
  agg_a.applyDeltaRaw("r3", grid3x3OneCell(4, GLOBAL_FREE));

  MultirobotAggregator agg_b(3, 3, 0.05f);
  agg_b.applyDeltaRaw("r1", grid3x3OneCell(4, GLOBAL_FREE));
  agg_b.applyDeltaRaw("r2", grid3x3OneCell(4, GLOBAL_FREE));
  agg_b.applyDeltaRaw("r3", grid3x3OneCell(4, GLOBAL_FREE));
  agg_b.applyDeltaRaw("r4", grid3x3OneCell(4, GLOBAL_OCCUPIED));

  agg_a.recomputeGlobal();
  agg_b.recomputeGlobal();
  EXPECT_EQ(agg_a.globalGrid()[4], agg_b.globalGrid()[4]);
  EXPECT_EQ(agg_a.globalGrid()[4], GLOBAL_FREE);    // 3:1 free majority
}

TEST(AggregatorD021, TiePreservesPreviousMaster) {
  MultirobotAggregator agg(3, 3, 0.05f);
  // First round: cell 0 FREE.
  agg.applyDeltaRaw("r1", grid3x3OneCell(0, GLOBAL_FREE));
  agg.recomputeGlobal();
  ASSERT_EQ(agg.globalGrid()[0], GLOBAL_FREE);

  // Second round: balance the votes (1 free + 1 occupied).
  agg.applyDeltaRaw("r2", grid3x3OneCell(0, GLOBAL_OCCUPIED));
  agg.recomputeGlobal();
  // Tie → previous master value preserved (anti-flicker).
  EXPECT_EQ(agg.globalGrid()[0], GLOBAL_FREE);
}

TEST(AggregatorD021, NoVotesResolvesUnknown) {
  MultirobotAggregator agg(3, 3, 0.05f);
  agg.recomputeGlobal();
  for (auto v : agg.globalGrid()) {
    EXPECT_EQ(v, GLOBAL_UNKNOWN);
  }
}

// ─── D-026: mismatch (disagreement) tracking ───────────────────────────

TEST(AggregatorD026, NoMismatchOnAgreement) {
  MultirobotAggregator agg(3, 3, 0.05f);
  agg.applyDeltaRaw("r1", grid3x3(GLOBAL_FREE));
  agg.applyDeltaRaw("r2", grid3x3(GLOBAL_FREE));
  agg.applyDeltaRaw("r3", grid3x3(GLOBAL_FREE));
  agg.recomputeGlobal();
  EXPECT_EQ(agg.mismatchCellCount(), 0u)
    << "All three robots agreed — no cell should be mismatched";
  EXPECT_EQ(agg.contributingCellCount(), 9u);
}

TEST(AggregatorD026, MismatchCountedWhenRobotsDisagree) {
  MultirobotAggregator agg(3, 3, 0.05f);
  // r1 says cell 0 FREE; r2 says cell 0 OCCUPIED. Both > 0 → mismatch.
  agg.applyDeltaRaw("r1", grid3x3OneCell(0, GLOBAL_FREE));
  agg.applyDeltaRaw("r2", grid3x3OneCell(0, GLOBAL_OCCUPIED));
  // r1 says cell 8 FREE only — no disagreement on cell 8.
  agg.applyDeltaRaw("r1", grid3x3OneCell(8, GLOBAL_FREE));

  agg.recomputeGlobal();
  EXPECT_EQ(agg.mismatchCellCount(), 1u);
  EXPECT_EQ(agg.contributingCellCount(), 2u);
}

TEST(AggregatorD026, MismatchClearedOnClear) {
  MultirobotAggregator agg(3, 3, 0.05f);
  agg.applyDeltaRaw("r1", grid3x3OneCell(0, GLOBAL_FREE));
  agg.applyDeltaRaw("r2", grid3x3OneCell(0, GLOBAL_OCCUPIED));
  agg.recomputeGlobal();
  ASSERT_EQ(agg.mismatchCellCount(), 1u);

  agg.clear();
  agg.recomputeGlobal();
  EXPECT_EQ(agg.mismatchCellCount(), 0u);
  EXPECT_EQ(agg.contributingCellCount(), 0u);
}

TEST(AggregatorD026, SnapshotIncludesMismatchMetric) {
  MultirobotAggregator agg(3, 3, 0.05f);
  agg.applyDeltaRaw("r1", grid3x3OneCell(4, GLOBAL_FREE));
  agg.applyDeltaRaw("r2", grid3x3OneCell(4, GLOBAL_OCCUPIED));

  const auto snap = agg.snapshot();
  EXPECT_EQ(snap.mismatch_cells, 1u);
  EXPECT_EQ(snap.contributing_cells, 1u);
  EXPECT_EQ(snap.contributing_robots, 2u);
}

// ─── D-021 saturation regression — uint16 vote counter ─────────────────

TEST(AggregatorD021, VoteCounterSaturatesGracefully) {
  MultirobotAggregator agg(3, 3, 0.05f);
  // Slam the same cell 65540 times; counter saturates at UINT16_MAX.
  for (int i = 0; i < 65540; ++i) {
    agg.applyDeltaRaw("r1", grid3x3OneCell(0, GLOBAL_FREE));
  }
  agg.applyDeltaRaw("r2", grid3x3OneCell(0, GLOBAL_OCCUPIED));
  agg.recomputeGlobal();
  // 65535 free vs 1 occupied → FREE wins, NOT saturating wraparound.
  EXPECT_EQ(agg.globalGrid()[0], GLOBAL_FREE);
}

// ─── P1-5: Multi-cell non-interference across robots ──────────────────
//
// r1 votes on cells 0/1/2 (top row) as FREE; r2 votes on cells 6/7/8
// (bottom row) as OCCUPIED. The two robots touch disjoint cells, so
// every cell should resolve to the value its sole voter chose — no
// cross-contamination via the global tally. Catches a class of bugs
// where the aggregator accidentally broadcasts a robot's vote to all
// cells in its delta grid.
TEST(AggregatorD021, MultiCellDisjointVotersDontCollide) {
  MultirobotAggregator agg(3, 3, 0.05f);

  // r1: FREE on top row (cells 0, 1, 2)
  auto g1 = grid3x3(GLOBAL_UNKNOWN);
  g1[0] = GLOBAL_FREE;
  g1[1] = GLOBAL_FREE;
  g1[2] = GLOBAL_FREE;
  agg.applyDeltaRaw("r1", g1);

  // r2: OCCUPIED on bottom row (cells 6, 7, 8)
  auto g2 = grid3x3(GLOBAL_UNKNOWN);
  g2[6] = GLOBAL_OCCUPIED;
  g2[7] = GLOBAL_OCCUPIED;
  g2[8] = GLOBAL_OCCUPIED;
  agg.applyDeltaRaw("r2", g2);

  agg.recomputeGlobal();
  const auto & g = agg.globalGrid();
  EXPECT_EQ(g[0], GLOBAL_FREE);
  EXPECT_EQ(g[1], GLOBAL_FREE);
  EXPECT_EQ(g[2], GLOBAL_FREE);
  EXPECT_EQ(g[3], GLOBAL_UNKNOWN);     // middle row untouched
  EXPECT_EQ(g[4], GLOBAL_UNKNOWN);
  EXPECT_EQ(g[5], GLOBAL_UNKNOWN);
  EXPECT_EQ(g[6], GLOBAL_OCCUPIED);
  EXPECT_EQ(g[7], GLOBAL_OCCUPIED);
  EXPECT_EQ(g[8], GLOBAL_OCCUPIED);
  EXPECT_EQ(agg.contributingCellCount(), 6u);
  EXPECT_EQ(agg.mismatchCellCount(), 0u);         // disjoint → no disagreement
}

// ─── P1-5: Snapshot consistency invariant ──────────────────────────────
//
// After a sequence of votes + recomputeGlobal, snapshot() must return
// numbers consistent with the publicly-observable getters. Guards
// against snapshot() drifting from globalGrid()/mismatchCellCount()/
// contributingCellCount() if someone splits the implementation later.
TEST(AggregatorSnapshotConsistency, SnapshotMatchesGettersAfterRecompute) {
  MultirobotAggregator agg(3, 3, 0.05f);
  // 3 robots, partial overlap on cell 4 (mismatch), unique cells too.
  auto g1 = grid3x3OneCell(0, GLOBAL_FREE);     g1[4] = GLOBAL_FREE;
  auto g2 = grid3x3OneCell(1, GLOBAL_OCCUPIED); g2[4] = GLOBAL_OCCUPIED;
  auto g3 = grid3x3OneCell(2, GLOBAL_FREE);
  agg.applyDeltaRaw("r1", g1);
  agg.applyDeltaRaw("r2", g2);
  agg.applyDeltaRaw("r3", g3);
  agg.recomputeGlobal();

  const auto snap = agg.snapshot();
  EXPECT_EQ(snap.mismatch_cells, agg.mismatchCellCount());
  EXPECT_EQ(snap.contributing_cells, agg.contributingCellCount());
  EXPECT_EQ(snap.contributing_robots, 3u);
  // Sanity: cells 0/1/2 each have one voter, cell 4 has two
  // (mismatched) → contributing_cells = 4, mismatch_cells = 1.
  EXPECT_EQ(snap.contributing_cells, 4u);
  EXPECT_EQ(snap.mismatch_cells, 1u);
}

// ─── [SLAM-1] World-frame-aware projection (applyDeltaAt) ──────────────
//
// applyDeltaAt projects each delta cell into the shared global grid using
// the robot's reported grid origin (world x/y/theta) and the delta's own
// resolution, rather than overlaying it cell-for-cell. These tests use a
// 10×10 @ 1.0 m global grid so a cell index equals integer world metres.

TEST(AggregatorSLAM1, ProjectionTranslatesToWorldCell) {
  MultirobotAggregator agg(10, 10, 1.0f);
  // A single FREE cell delta placed at world (4.3, 2.7) → global (4, 2).
  const auto p = png({GLOBAL_FREE}, 1, 1);
  ASSERT_TRUE(agg.applyDeltaAt("r1", p, 4.3, 2.7, 0.0, 1.0f));
  agg.recomputeGlobal();

  const auto & g = agg.globalGrid();
  EXPECT_EQ(g[2 * 10 + 4], GLOBAL_FREE);     // (gx=4, gy=2)
  EXPECT_EQ(agg.contributingCellCount(), 1u);
  EXPECT_EQ(agg.contributingRobotCount(), 1u);
}

TEST(AggregatorSLAM1, ProjectionAppliesRotation) {
  MultirobotAggregator agg(10, 10, 1.0f);
  // 2×1 delta (cells (0,0) and (1,0)), both FREE, placed at world
  // (0.5, 0.5) rotated +90°. The local +x axis maps to world +y, so
  // cell (1,0) lands a row "up" rather than a column "right".
  constexpr double kHalfPi = 1.5707963267948966;
  const auto p = png({GLOBAL_FREE, GLOBAL_FREE}, 2, 1);
  ASSERT_TRUE(agg.applyDeltaAt("r1", p, 0.5, 0.5, kHalfPi, 1.0f));
  agg.recomputeGlobal();

  const auto & g = agg.globalGrid();
  EXPECT_EQ(g[0 * 10 + 0], GLOBAL_FREE);     // cell (0,0) → (0,0)
  EXPECT_EQ(g[1 * 10 + 0], GLOBAL_FREE);     // cell (1,0) → (0,1) via +90°
  // Without rotation cell (1,0) would have landed at (1,0):
  EXPECT_EQ(g[0 * 10 + 1], GLOBAL_UNKNOWN);
}

TEST(AggregatorSLAM1, GlobalOriginOffsetsProjection) {
  MultirobotAggregator agg(10, 10, 1.0f);
  // Grid lower-left corner moved to world (-5, -5): grid covers
  // [-5, 5). A delta at world (0, 0) now lands at global cell (5, 5).
  agg.setGlobalOrigin(-5.0, -5.0);
  const auto p = png({GLOBAL_OCCUPIED}, 1, 1);
  ASSERT_TRUE(agg.applyDeltaAt("r1", p, 0.0, 0.0, 0.0, 1.0f));
  agg.recomputeGlobal();
  EXPECT_EQ(agg.globalGrid()[5 * 10 + 5], GLOBAL_OCCUPIED);
}

TEST(AggregatorSLAM1, OutOfGridDeltaContributesNothing) {
  MultirobotAggregator agg(10, 10, 1.0f);
  const auto p = png({GLOBAL_FREE}, 1, 1);
  // World (100, 100) is well outside the 10 m grid.
  EXPECT_TRUE(agg.applyDeltaAt("r1", p, 100.0, 100.0, 0.0, 1.0f));
  agg.recomputeGlobal();
  EXPECT_EQ(agg.contributingCellCount(), 0u);
}

TEST(AggregatorSLAM1, FinerDeltaVotesGlobalCellOnce) {
  // Coarse 2×2 @ 2.0 m global grid; a finer 2×2 @ 1.0 m delta whose four
  // cells all fall inside global cell (0,0). A single robot must vote
  // that cell only ONCE despite four sub-cells (otherwise it would
  // out-vote other robots on shared cells).
  MultirobotAggregator agg(2, 2, 2.0f);
  const auto pf = png(
    {GLOBAL_FREE, GLOBAL_FREE, GLOBAL_FREE, GLOBAL_FREE}, 2, 2);
  ASSERT_TRUE(agg.applyDeltaAt("r1", pf, 0.0, 0.0, 0.0, 1.0f));

  // A second robot casts exactly one OCCUPIED vote on the same cell.
  const auto po = png({GLOBAL_OCCUPIED}, 1, 1);
  ASSERT_TRUE(agg.applyDeltaAt("r2", po, 0.0, 0.0, 0.0, 2.0f));

  agg.recomputeGlobal();
  // If r1's four sub-cells each voted, free=4 vs occupied=1 → FREE.
  // With per-call dedup it is free=1 vs occupied=1 → tie → UNKNOWN.
  EXPECT_EQ(agg.globalGrid()[0], GLOBAL_UNKNOWN)
    << "finer delta over-voted a single global cell (dedup failed)";
}

TEST(AggregatorSLAM1, ZeroResolutionRejected) {
  MultirobotAggregator agg(10, 10, 1.0f);
  const auto p = png({GLOBAL_FREE}, 1, 1);
  EXPECT_FALSE(agg.applyDeltaAt("r1", p, 0.0, 0.0, 0.0, 0.0f));
}
