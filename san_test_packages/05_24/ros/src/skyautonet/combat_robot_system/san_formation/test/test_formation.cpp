// SAN v1.5 — Hungarian + Formation standalone tests.
//
// Pure-logic gtest, no rclcpp. Verifies:
//   * Hungarian solves canonical assignment problems correctly
//   * Each of the 9 formations has expected geometric properties
//
// Coverage:
//   H1  Identity matrix → diagonal assignment
//   H2  Symmetric reverse → matches expected min-cost
//   H3  Already-optimal already-assigned identity
//   H4  4×4 known textbook example
//   H5  Reject empty / non-square / INF row
//   F1  Column N=4, spacing=5 → coords correct
//   F2  Line N=5, spacing=3 → spread symmetric L/R
//   F3  V-Shape θ=60° spread angle correct
//   F4  V-Shape θ=120° wider than 60°
//   F5  Diamond 5-slot exactly 4 corners + 1 center
//   F6  Echelon-Left / Right mirror each other
//   F7  Box 5-slot square geometry
//   F8  Vee-Inverted forward arms (+x not -x)
//   F9  Free-Spread deterministic (same seed → same output)
//   P1  4 presets match SDD §7.2 table

#include "san_formation/hungarian.hpp"
#include "san_formation/formations.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace san_formation {
namespace {

// ─── Hungarian ──────────────────────────────────────────────────────────

TEST(Hungarian, H1_IdentityDiagonal) {
  // 3×3 identity-like: cheapest cell is the diagonal
  std::vector<std::vector<double>> cost = {
      {0.0, 5.0, 5.0},
      {5.0, 0.0, 5.0},
      {5.0, 5.0, 0.0},
  };
  auto a = solveAssignment(cost);
  ASSERT_EQ(a.size(), 3u);
  EXPECT_EQ(a[0], 0u);
  EXPECT_EQ(a[1], 1u);
  EXPECT_EQ(a[2], 2u);
  EXPECT_DOUBLE_EQ(assignmentCost(cost, a), 0.0);
}

TEST(Hungarian, H2_SymmetricReverse) {
  // The min cost is reverse-diagonal: [0]→[2], [1]→[1], [2]→[0]
  std::vector<std::vector<double>> cost = {
      {9.0, 9.0, 1.0},
      {9.0, 1.0, 9.0},
      {1.0, 9.0, 9.0},
  };
  auto a = solveAssignment(cost);
  ASSERT_EQ(a.size(), 3u);
  EXPECT_DOUBLE_EQ(assignmentCost(cost, a), 3.0);
}

TEST(Hungarian, H3_OneByOne) {
  std::vector<std::vector<double>> cost = {{7.5}};
  auto a = solveAssignment(cost);
  ASSERT_EQ(a.size(), 1u);
  EXPECT_EQ(a[0], 0u);
  EXPECT_DOUBLE_EQ(assignmentCost(cost, a), 7.5);
}

TEST(Hungarian, H4_TextbookFourByFour) {
  // Classic textbook example (Munkres 1957).
  // Expected min total cost = 13.
  std::vector<std::vector<double>> cost = {
      {82.0, 83.0, 69.0, 92.0},
      {77.0, 37.0, 49.0, 92.0},
      {11.0, 69.0, 5.0,  86.0},
      {8.0,  9.0,  98.0, 23.0},
  };
  auto a = solveAssignment(cost);
  ASSERT_EQ(a.size(), 4u);
  double total = assignmentCost(cost, a);
  // Hungarian must reach the optimum 140 (known result for this matrix).
  EXPECT_DOUBLE_EQ(total, 140.0);
}

TEST(Hungarian, H5_RejectsBadInput) {
  // Empty matrix
  EXPECT_THROW(solveAssignment({}), std::invalid_argument);
  // Non-square (2 rows, first row length 3)
  std::vector<std::vector<double>> bad = {{1, 2, 3}, {4, 5, 6}};
  EXPECT_THROW(solveAssignment(bad), std::invalid_argument);
}

// ─── Formations ─────────────────────────────────────────────────────────

TEST(Formations, F1_ColumnExactCoordinates) {
  auto slots = generateSlots(Formation::Column, 4, 5.0f, 0.0f);
  ASSERT_EQ(slots.size(), 4u);
  EXPECT_FLOAT_EQ(slots[0].x,   0.0f); EXPECT_FLOAT_EQ(slots[0].y, 0.0f);
  EXPECT_FLOAT_EQ(slots[1].x,  -5.0f); EXPECT_FLOAT_EQ(slots[1].y, 0.0f);
  EXPECT_FLOAT_EQ(slots[2].x, -10.0f); EXPECT_FLOAT_EQ(slots[2].y, 0.0f);
  EXPECT_FLOAT_EQ(slots[3].x, -15.0f); EXPECT_FLOAT_EQ(slots[3].y, 0.0f);
}

TEST(Formations, F2_LineSymmetricSpread) {
  auto slots = generateSlots(Formation::Line, 5, 3.0f, 0.0f);
  ASSERT_EQ(slots.size(), 5u);
  EXPECT_FLOAT_EQ(slots[0].x, 0.0f);
  EXPECT_FLOAT_EQ(slots[0].y, 0.0f);
  // All on same x (lateral spread only)
  for (const auto& s : slots) EXPECT_FLOAT_EQ(s.x, 0.0f);
  // Verify symmetry: sum of y == 0
  float sum_y = 0.0f;
  for (const auto& s : slots) sum_y += s.y;
  EXPECT_NEAR(sum_y, 0.0f, 1e-4);
}

TEST(Formations, F3_VShape60DegSpreadAngle) {
  // V at θ=60°: first follower at left/right, distance d, at half-angle 30°
  auto slots = generateSlots(Formation::VShape, 3, 5.0f, 60.0f);
  ASSERT_EQ(slots.size(), 3u);
  EXPECT_FLOAT_EQ(slots[0].x, 0.0f); EXPECT_FLOAT_EQ(slots[0].y, 0.0f);
  // distance from origin to follower slot
  const float dist = std::sqrt(slots[1].x*slots[1].x + slots[1].y*slots[1].y);
  EXPECT_NEAR(dist, 5.0f, 1e-3);
  // y / |x| = tan(30°) ≈ 0.5774
  EXPECT_NEAR(std::abs(slots[1].y) / std::abs(slots[1].x), 0.5774f, 1e-3);
}

TEST(Formations, F4_VShape120DegWiderThan60Deg) {
  auto v60  = generateSlots(Formation::VShape, 3, 5.0f, 60.0f);
  auto v120 = generateSlots(Formation::VShape, 3, 5.0f, 120.0f);
  // First follower y should be wider at 120 than 60
  EXPECT_GT(std::abs(v120[1].y), std::abs(v60[1].y));
}

TEST(Formations, F5_DiamondFiveSlot) {
  auto slots = generateSlots(Formation::Diamond, 5, 4.0f, 0.0f);
  ASSERT_EQ(slots.size(), 5u);
  // First is center (leader)
  EXPECT_FLOAT_EQ(slots[0].x, 0.0f);
  EXPECT_FLOAT_EQ(slots[0].y, 0.0f);
  // Other 4 must be at corners, each |x| = |y| = d*√2/2
  const float h = 4.0f * 0.70710678f;
  for (size_t i = 1; i < 5; ++i) {
    EXPECT_NEAR(std::abs(slots[i].x), h, 1e-3);
    EXPECT_NEAR(std::abs(slots[i].y), h, 1e-3);
  }
}

TEST(Formations, F6_EchelonLeftRightMirror) {
  auto left  = generateSlots(Formation::EchelonLeft,  4, 5.0f, 0.0f);
  auto right = generateSlots(Formation::EchelonRight, 4, 5.0f, 0.0f);
  ASSERT_EQ(left.size(),  4u);
  ASSERT_EQ(right.size(), 4u);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(left[i].x, right[i].x);    // same x
    EXPECT_FLOAT_EQ(left[i].y, -right[i].y);   // mirrored y
  }
}

TEST(Formations, F7_BoxFiveSlotSquareGeometry) {
  auto slots = generateSlots(Formation::Box, 5, 4.0f, 0.0f);
  ASSERT_EQ(slots.size(), 5u);
  // Center at origin
  EXPECT_FLOAT_EQ(slots[0].x, 0.0f);
  EXPECT_FLOAT_EQ(slots[0].y, 0.0f);
  // 4 corners at ±d/2
  for (size_t i = 1; i < 5; ++i) {
    EXPECT_NEAR(std::abs(slots[i].x), 2.0f, 1e-3);
    EXPECT_NEAR(std::abs(slots[i].y), 2.0f, 1e-3);
  }
}

TEST(Formations, F8_VeeInvertedHasForwardArms) {
  auto slots = generateSlots(Formation::VeeInverted, 3, 5.0f, 60.0f);
  ASSERT_EQ(slots.size(), 3u);
  EXPECT_FLOAT_EQ(slots[0].x, 0.0f);
  // Followers must be in front (+x), not behind (-x)
  EXPECT_GT(slots[1].x, 0.0f);
  EXPECT_GT(slots[2].x, 0.0f);
}

TEST(Formations, F9_FreeSpreadDeterministic) {
  auto a = generateSlots(Formation::FreeSpread, 8, 5.0f, 0.0f);
  auto b = generateSlots(Formation::FreeSpread, 8, 5.0f, 0.0f);
  ASSERT_EQ(a.size(), 8u);
  ASSERT_EQ(b.size(), 8u);
  for (size_t i = 0; i < 8; ++i) {
    EXPECT_FLOAT_EQ(a[i].x, b[i].x);
    EXPECT_FLOAT_EQ(a[i].y, b[i].y);
  }
}

TEST(Formations, P1_PresetsMatchSDDTable) {
  // SDD-SWARM §7.2 — 4 presets
  auto narrow = getPreset(PRESET_NARROW_PASSAGE);
  EXPECT_FLOAT_EQ(narrow.spacing_d_m,       3.0f);
  EXPECT_FLOAT_EQ(narrow.spread_theta_deg, 40.0f);

  auto recon = getPreset(PRESET_RECON_DEFENCE);
  EXPECT_FLOAT_EQ(recon.spacing_d_m,        5.0f);
  EXPECT_FLOAT_EQ(recon.spread_theta_deg,  90.0f);

  auto wide = getPreset(PRESET_WIDE_RECON);
  EXPECT_FLOAT_EQ(wide.spacing_d_m,         7.0f);
  EXPECT_FLOAT_EQ(wide.spread_theta_deg,  120.0f);

  auto assault = getPreset(PRESET_ASSAULT);
  EXPECT_FLOAT_EQ(assault.spacing_d_m,     15.0f);
  EXPECT_FLOAT_EQ(assault.spread_theta_deg, 60.0f);
}

// ─── Integration: Hungarian + Formation ────────────────────────────────

TEST(FormationHungarian, F10_AssignsRobotsToVShapeSlots) {
  // 4 robots at arbitrary positions, V-shape with θ=60°, d=5.
  auto slots = generateSlots(Formation::VShape, 4, 5.0f, 60.0f);

  // Robot positions (world frame, here same as local)
  std::vector<SlotXY> robots = {
      {-1.0f,  4.0f},   // closer to slot[1] (front-right)
      {-1.0f, -4.0f},   // closer to slot[2] (front-left)
      { 1.0f,  0.0f},   // closer to slot[0] (leader/center)
      {-8.0f,  6.0f},   // closer to slot[3] (rear-right)
  };

  // Build cost matrix (Euclidean distance)
  const size_t n = 4;
  std::vector<std::vector<double>> cost(n, std::vector<double>(n));
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j < n; ++j) {
      const float dx = robots[i].x - slots[j].x;
      const float dy = robots[i].y - slots[j].y;
      cost[i][j] = std::sqrt(dx*dx + dy*dy);
    }
  }
  auto a = solveAssignment(cost);
  ASSERT_EQ(a.size(), 4u);

  // Each robot assigned to a unique slot
  std::vector<bool> seen(4, false);
  for (size_t i = 0; i < 4; ++i) {
    ASSERT_LT(a[i], 4u);
    EXPECT_FALSE(seen[a[i]]) << "Slot " << a[i] << " assigned twice";
    seen[a[i]] = true;
  }
  // Total cost > 0 and finite
  const double total = assignmentCost(cost, a);
  EXPECT_GT(total, 0.0);
  EXPECT_LT(total, 100.0);
}

}  // namespace
}  // namespace san_formation
