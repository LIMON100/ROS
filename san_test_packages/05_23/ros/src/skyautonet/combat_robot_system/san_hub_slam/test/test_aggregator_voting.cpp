// SAN v1.5.2 — DCN-2026-006 EXT D-021 (Bayesian voting) + D-026
// (mismatch logging) unit tests.

#include <gtest/gtest.h>
#include <vector>

#include "san_hub_slam/aggregator.hpp"

using namespace san_hub_slam;

namespace {

// Build a uniform-value 3×3 grid for tiny test cases.
std::vector<uint8_t> grid3x3(uint8_t v) {
    return std::vector<uint8_t>(9, v);
}

// Build a grid with one cell set to `v`, rest UNKNOWN.
std::vector<uint8_t> grid3x3OneCell(std::size_t idx, uint8_t v) {
    auto g = grid3x3(GLOBAL_UNKNOWN);
    g[idx] = v;
    return g;
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
    EXPECT_EQ(agg_a.globalGrid()[4], GLOBAL_FREE);  // 3:1 free majority
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
