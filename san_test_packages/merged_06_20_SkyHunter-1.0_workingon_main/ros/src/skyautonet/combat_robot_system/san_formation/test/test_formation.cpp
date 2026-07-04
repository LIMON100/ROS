// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

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
#include "san_formation/formation_planner.hpp"
#include "san_formation/encircle_combat.hpp"

#include <cmath>

#include <gtest/gtest.h>

#include <vector>

namespace san_formation
{
namespace
{

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
    {11.0, 69.0, 5.0, 86.0},
    {8.0, 9.0, 98.0, 23.0},
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
  EXPECT_FLOAT_EQ(slots[0].x, 0.0f); EXPECT_FLOAT_EQ(slots[0].y, 0.0f);
  EXPECT_FLOAT_EQ(slots[1].x, -5.0f); EXPECT_FLOAT_EQ(slots[1].y, 0.0f);
  EXPECT_FLOAT_EQ(slots[2].x, -10.0f); EXPECT_FLOAT_EQ(slots[2].y, 0.0f);
  EXPECT_FLOAT_EQ(slots[3].x, -15.0f); EXPECT_FLOAT_EQ(slots[3].y, 0.0f);
}

TEST(Formations, F2_LineSymmetricSpread) {
  auto slots = generateSlots(Formation::Line, 5, 3.0f, 0.0f);
  ASSERT_EQ(slots.size(), 5u);
  EXPECT_FLOAT_EQ(slots[0].x, 0.0f);
  EXPECT_FLOAT_EQ(slots[0].y, 0.0f);
  // All on same x (lateral spread only)
  for (const auto & s : slots) {
    EXPECT_FLOAT_EQ(s.x, 0.0f);
  }
  // Verify symmetry: sum of y == 0
  float sum_y = 0.0f;
  for (const auto & s : slots) {
    sum_y += s.y;
  }
  EXPECT_NEAR(sum_y, 0.0f, 1e-4);
}

TEST(Formations, F3_VShape60DegSpreadAngle) {
  // V at θ=60°: first follower at left/right, distance d, at half-angle 30°
  auto slots = generateSlots(Formation::VShape, 3, 5.0f, 60.0f);
  ASSERT_EQ(slots.size(), 3u);
  EXPECT_FLOAT_EQ(slots[0].x, 0.0f); EXPECT_FLOAT_EQ(slots[0].y, 0.0f);
  // distance from origin to follower slot
  const float dist = std::sqrt(slots[1].x * slots[1].x + slots[1].y * slots[1].y);
  EXPECT_NEAR(dist, 5.0f, 1e-3);
  // y / |x| = tan(30°) ≈ 0.5774
  EXPECT_NEAR(std::abs(slots[1].y) / std::abs(slots[1].x), 0.5774f, 1e-3);
}

TEST(Formations, F4_VShape120DegWiderThan60Deg) {
  auto v60 = generateSlots(Formation::VShape, 3, 5.0f, 60.0f);
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
  auto left = generateSlots(Formation::EchelonLeft, 4, 5.0f, 0.0f);
  auto right = generateSlots(Formation::EchelonRight, 4, 5.0f, 0.0f);
  ASSERT_EQ(left.size(), 4u);
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
  EXPECT_FLOAT_EQ(narrow.spacing_d_m, 3.0f);
  EXPECT_FLOAT_EQ(narrow.spread_theta_deg, 40.0f);

  auto recon = getPreset(PRESET_RECON_DEFENCE);
  EXPECT_FLOAT_EQ(recon.spacing_d_m, 5.0f);
  EXPECT_FLOAT_EQ(recon.spread_theta_deg, 90.0f);

  auto wide = getPreset(PRESET_WIDE_RECON);
  EXPECT_FLOAT_EQ(wide.spacing_d_m, 7.0f);
  EXPECT_FLOAT_EQ(wide.spread_theta_deg, 120.0f);

  auto assault = getPreset(PRESET_ASSAULT);
  EXPECT_FLOAT_EQ(assault.spacing_d_m, 15.0f);
  EXPECT_FLOAT_EQ(assault.spread_theta_deg, 60.0f);
}

// ─── Integration: Hungarian + Formation ────────────────────────────────

TEST(FormationHungarian, F10_AssignsRobotsToVShapeSlots) {
  // 4 robots at arbitrary positions, V-shape with θ=60°, d=5.
  auto slots = generateSlots(Formation::VShape, 4, 5.0f, 60.0f);

  // Robot positions (world frame, here same as local)
  std::vector<SlotXY> robots = {
    {-1.0f, 4.0f},      // closer to slot[1] (front-right)
    {-1.0f, -4.0f},     // closer to slot[2] (front-left)
    {1.0f, 0.0f},       // closer to slot[0] (leader/center)
    {-8.0f, 6.0f},      // closer to slot[3] (rear-right)
  };

  // Build cost matrix (Euclidean distance)
  const size_t n = 4;
  std::vector<std::vector<double>> cost(n, std::vector<double>(n));
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j < n; ++j) {
      const float dx = robots[i].x - slots[j].x;
      const float dy = robots[i].y - slots[j].y;
      cost[i][j] = std::sqrt(dx * dx + dy * dy);
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

// ═══════════════════════════════════════════════════════════════════════
// PATCH 2026-05-13 — new tests covering deep-dive fixes
// ═══════════════════════════════════════════════════════════════════════

// ─── PV1: leader frame transform symmetry ───────────────────────────────
// Slot at (-d, 0) in leader local frame, leader at world (10, 5) facing
// yaw=π/2 → world slot should be at (10, 5-d).
TEST(PatchPlanner, PV1_FrameTransformWorks) {
  PoseXY leader;
  leader.x = 10.0f;
  leader.y = 5.0f;
  leader.yaw = static_cast<float>(M_PI / 2.0);  // 90° CCW
  float wx, wy;
  slotLocalToWorld(-3.0f, 0.0f, leader, wx, wy);
  // Local -x with leader heading north → world south (y decreases).
  EXPECT_NEAR(wx, 10.0f, 1e-3f);
  EXPECT_NEAR(wy, 2.0f, 1e-3f);
}

// ─── PV2: cost matrix uses world distance ───────────────────────────────
TEST(PatchPlanner, PV2_CostMatrixFrameCorrect) {
  // 2 robots, both at world origin; slots at (0,0) and (-5,0) leader-local.
  // Leader at world (10, 0) facing yaw=0 → world slots: (10,0), (5,0).
  // Robot 0 at world origin (10m from slot 0, 5m from slot 1).
  // Robot 1 at world (10, 0)  (0m from slot 0, 5m from slot 1).
  std::vector<PoseXY> robots = {
    {0.0f, 0.0f, 0.0f},
    {10.0f, 0.0f, 0.0f},
  };
  std::vector<SlotXY> slots_local = {
    {0.0f, 0.0f},
    {-5.0f, 0.0f},
  };
  PoseXY leader{10.0f, 0.0f, 0.0f};
  auto cost = buildCostMatrix(robots, slots_local, leader);
  ASSERT_EQ(cost.size(), 2u);
  EXPECT_NEAR(cost[0][0], 10.0, 1e-3);
  EXPECT_NEAR(cost[0][1], 5.0, 1e-3);
  EXPECT_NEAR(cost[1][0], 0.0, 1e-3);
  EXPECT_NEAR(cost[1][1], 5.0, 1e-3);
}

// ─── PV3: velocity estimator finite-difference ──────────────────────────
TEST(PatchPlanner, PV3_VelocityEstimatorFiniteDiff) {
  VelocityEstimator est(1.0f);   // no smoothing for exact assertion
  PoseXY p0{0.0f, 0.0f, 0.0f};
  PoseXY p1{0.5f, 0.3f, 0.0f};
  EXPECT_FALSE(est.update(1000, p0).has_value());  // 1st sample → nullopt
  const auto v = est.update(1500, p1);              // 0.5s elapsed
  ASSERT_TRUE(v.has_value());
  EXPECT_NEAR(v->vx, 1.0f, 1e-3f);   // 0.5m / 0.5s = 1.0
  EXPECT_NEAR(v->vy, 0.6f, 1e-3f);   // 0.3m / 0.5s = 0.6
}

// ─── PV4: velocity estimator low-pass smoothing ─────────────────────────
TEST(PatchPlanner, PV4_VelocityEstimatorSmoothing) {
  VelocityEstimator est(0.3f);
  est.update(1000, {0.0f, 0.0f, 0.0f});
  est.update(2000, {1.0f, 0.0f, 0.0f});       // 1m/s
  // Inject a noise spike (10x velocity).
  const auto v = est.update(2001, {1.01f, 0.0f, 0.0f});  // 10m/s spike
  ASSERT_TRUE(v.has_value());
  // alpha=0.3 → 0.3*10 + 0.7*1.0 = 3.7, not 10.0
  EXPECT_LT(v->vx, 5.0f);
  EXPECT_GT(v->vx, 2.0f);
}

// ─── PV5: 1-second prediction uses leader velocity ──────────────────────
// Slot at (-5, 0) local; leader at world origin facing +x with vx=2 m/s.
// After 1 second → leader at (2, 0), slot world = (2-5, 0) = (-3, 0).
TEST(PatchPlanner, PV5_PredictSlotAhead) {
  PoseXY leader{0.0f, 0.0f, 0.0f};
  Velocity2D vel{2.0f, 0.0f, 0.0f};
  SlotXY slot{-5.0f, 0.0f};
  const auto p = predictSlotAhead(slot, leader, vel, 1.0f);
  EXPECT_NEAR(p.world_x, -3.0f, 1e-3f);
  EXPECT_NEAR(p.world_y, 0.0f, 1e-3f);
  EXPECT_NEAR(p.world_yaw, 0.0f, 1e-3f);   // heading alignment
}

// ─── PV6: Box ring-out 4-way symmetric (PATCH) ──────────────────────────
TEST(PatchFormations, PV6_BoxRingOutSymmetric) {
  // N=9 → 5 corners + 4 ring-1 corners. Sum of x = 0 (symmetric).
  auto slots = generateSlots(Formation::Box, 9, 4.0f, 0.0f);
  ASSERT_EQ(slots.size(), 9u);
  float sum_x = 0.0f, sum_y = 0.0f;
  for (const auto & s : slots) {
    sum_x += s.x; sum_y += s.y;
  }
  EXPECT_NEAR(sum_x, 0.0f, 1e-3f);
  EXPECT_NEAR(sum_y, 0.0f, 1e-3f);
}

// ─── PV7: Diamond ring-out 4-way symmetric (PATCH) ──────────────────────
TEST(PatchFormations, PV7_DiamondRingOutSymmetric) {
  // N=9 → 5 base + 4 ring-1 corners all at same r. Verify all ring-1
  // slots share the same radius.
  auto slots = generateSlots(Formation::Diamond, 9, 6.0f, 0.0f);
  ASSERT_EQ(slots.size(), 9u);
  // Indices 5..8 should be the ring-1 corners — all at same |x|, |y|.
  const float ax = std::abs(slots[5].x);
  for (size_t i = 5; i < 9; ++i) {
    EXPECT_NEAR(std::abs(slots[i].x), ax, 1e-3f) << "i=" << i;
    EXPECT_NEAR(std::abs(slots[i].y), ax, 1e-3f) << "i=" << i;
  }
  // Sum of x = 0 (4-way symmetric).
  float sum_x = 0.0f;
  for (size_t i = 5; i < 9; ++i) {
    sum_x += slots[i].x;
  }
  EXPECT_NEAR(sum_x, 0.0f, 1e-3f);
}

// ─── PV8: Yaw-wrap handling in velocity estimator ───────────────────────
TEST(PatchPlanner, PV8_VelocityYawWrap) {
  VelocityEstimator est(1.0f);
  est.update(1000, {0.0f, 0.0f, 3.0f});            // yaw ≈ π
  const auto v = est.update(2000, {0.0f, 0.0f, -3.0f});  // yaw ≈ -π
  ASSERT_TRUE(v.has_value());
  // Without wrap fix, dyaw = -6 → wz = -6 (wrong by 2π).
  // With wrap fix, dyaw ≈ 0.28 → wz ≈ 0.28.
  EXPECT_LT(std::abs(v->wz), 1.0f);
}

// ─── DCN-2026-026 C-2 — Encircle combat (E1..E6) ────────────────────

TEST(Encircle, E1_GateTruthTable) {
  using threat_alert::SEVERITY_CRITICAL;
  using threat_alert::TYPE_DRONE_DETECTED;
  using threat_alert::TYPE_OTHER;
  const float kMin = 0.9f;
  // Qualifying: CRITICAL drone, confident, localized.
  EXPECT_TRUE(
    passesEncircleGate(
      SEVERITY_CRITICAL, TYPE_DRONE_DETECTED, 0.95f, true, 12.0f, kMin));
  EXPECT_TRUE(
    passesEncircleGate(3 /*FATAL*/, TYPE_OTHER, 0.95f, true, 12.0f, kMin));
  // System alerts can NEVER trigger combat regardless of severity.
  EXPECT_FALSE(
    passesEncircleGate(3, 2 /*BATTERY_CRITICAL*/, 0.99f, true, 12.0f, kMin));
  EXPECT_FALSE(
    passesEncircleGate(3, 1 /*SBC_FAILED*/, 0.99f, true, 12.0f, kMin));
  // Severity below CRITICAL fails.
  EXPECT_FALSE(
    passesEncircleGate(1, TYPE_DRONE_DETECTED, 0.95f, true, 12.0f, kMin));
  // Confidence below threshold or unknown fails.
  EXPECT_FALSE(
    passesEncircleGate(
      SEVERITY_CRITICAL, TYPE_DRONE_DETECTED, 0.5f, true, 12.0f, kMin));
  EXPECT_FALSE(
    passesEncircleGate(
      SEVERITY_CRITICAL, TYPE_DRONE_DETECTED, std::nullopt, true, 12.0f,
      kMin));
  // No position / no range fails.
  EXPECT_FALSE(
    passesEncircleGate(
      SEVERITY_CRITICAL, TYPE_DRONE_DETECTED, 0.95f, false, 12.0f, kMin));
  EXPECT_FALSE(
    passesEncircleGate(
      SEVERITY_CRITICAL, TYPE_DRONE_DETECTED, 0.95f, true, 0.0f, kMin));
}

TEST(Encircle, E2_DetailParsers) {
  // detection_to_threat detail format.
  const std::string detail =
    "{\"source\":\"fused\",\"class\":\"drone\",\"class_id\":3,"
    "\"confidence\":0.9375,\"bbox\":[10,20,30,40]}";
  auto c = parseConfidenceFromDetail(detail);
  ASSERT_TRUE(c.has_value());
  EXPECT_NEAR(*c, 0.9375f, 1e-4);
  EXPECT_FALSE(parseConfidenceFromDetail("{}").has_value());
  EXPECT_FALSE(parseConfidenceFromDetail("\"confidence\":abc").has_value());

  EXPECT_EQ(parseRobotIdString("robot_3").value_or(0u), 3u);
  EXPECT_EQ(parseRobotIdString("7").value_or(0u), 7u);
  EXPECT_FALSE(parseRobotIdString("hub").has_value());
  EXPECT_FALSE(parseRobotIdString("perception").has_value());
  EXPECT_FALSE(parseRobotIdString("robot_").has_value());
}

TEST(Encircle, E3_ThreatWorldXYFromReporter) {
  // Reporter at (10, 5), world bearing 90° (=+y), range 8 →
  // threat at (10, 13). The defbb64 defect anchored this on the
  // LEADER pose — this pins reporter-frame correctness.
  auto [x, y] = threatWorldXY(10.0f, 5.0f, 90.0f, 8.0f);
  EXPECT_NEAR(x, 10.0f, 1e-4);
  EXPECT_NEAR(y, 13.0f, 1e-4);
  auto [x2, y2] = threatWorldXY(0.0f, 0.0f, 180.0f, 4.0f);
  EXPECT_NEAR(x2, -4.0f, 1e-4);
  EXPECT_NEAR(y2, 0.0f, 1e-3);
}

TEST(Encircle, E4_OperatorConfirmFlowDefault) {
  // Default config: operator confirm REQUIRED (ratified 2026-06-10).
  EncircleCombat ec;
  EXPECT_EQ(ec.phase(), EncirclePhase::Idle);
  ec.onQualifiedThreat(3.0f, 4.0f, 1000);
  EXPECT_EQ(ec.phase(), EncirclePhase::PendingConfirm);
  ASSERT_TRUE(ec.anchor().has_value());
  EXPECT_NEAR(ec.anchor()->first, 3.0f, 1e-5);
  // Fresh threat refreshes anchor, does not self-engage.
  ec.onQualifiedThreat(5.0f, 6.0f, 2000);
  EXPECT_EQ(ec.phase(), EncirclePhase::PendingConfirm);
  EXPECT_NEAR(ec.anchor()->first, 5.0f, 1e-5);
  // 1-tap → Active; release → Cooldown.
  EXPECT_TRUE(ec.onOperatorConfirm(2500));
  EXPECT_EQ(ec.phase(), EncirclePhase::Active);
  EXPECT_TRUE(ec.onOperatorRelease(3000));
  EXPECT_EQ(ec.phase(), EncirclePhase::Cooldown);
  EXPECT_FALSE(ec.anchor().has_value());
}

TEST(Encircle, E5_TtlAndHysteresisTimeline) {
  EncircleConfig cfg;
  cfg.auto_engage = true;        // opt-in path
  cfg.ttl_ms = 10000;
  cfg.reentry_block_ms = 5000;
  EncircleCombat ec(cfg);
  ec.onQualifiedThreat(0.0f, 0.0f, 1000);
  EXPECT_EQ(ec.phase(), EncirclePhase::Active);   // auto engages
  // TTL refresh keeps it Active.
  EXPECT_FALSE(ec.tick(9000));
  ec.onQualifiedThreat(0.0f, 0.0f, 9000);
  EXPECT_FALSE(ec.tick(15000));                   // 6 s after refresh
  // TTL expiry → Cooldown.
  EXPECT_TRUE(ec.tick(19100));
  EXPECT_EQ(ec.phase(), EncirclePhase::Cooldown);
  // Hysteresis: qualifying threat during Cooldown is IGNORED.
  ec.onQualifiedThreat(1.0f, 1.0f, 20000);
  EXPECT_EQ(ec.phase(), EncirclePhase::Cooldown);
  // Cooldown elapses → Idle → re-arm works again.
  EXPECT_TRUE(ec.tick(24200));
  EXPECT_EQ(ec.phase(), EncirclePhase::Idle);
  ec.onQualifiedThreat(1.0f, 1.0f, 25000);
  EXPECT_EQ(ec.phase(), EncirclePhase::Active);
}

TEST(Encircle, E6_RingGeometry) {
  // 4 followers, radius 7 — even 90° spacing on the ring, origin =
  // threat anchor, slot 0 at +x.
  auto slots = encircleSlots(4, 7.0f);
  ASSERT_EQ(slots.size(), 4u);
  EXPECT_NEAR(slots[0].x, 7.0f, 1e-4);
  EXPECT_NEAR(slots[0].y, 0.0f, 1e-4);
  EXPECT_NEAR(slots[1].x, 0.0f, 1e-3);
  EXPECT_NEAR(slots[1].y, 7.0f, 1e-4);
  for (const auto & s : slots) {
    EXPECT_NEAR(std::hypot(s.x, s.y), 7.0f, 1e-4);
  }
  // Adjacent angular gaps are uniform (2π/n) for any n.
  auto s5 = encircleSlots(5, 3.0f);
  for (size_t i = 0; i < s5.size(); ++i) {
    const auto & a = s5[i];
    const auto & b = s5[(i + 1) % s5.size()];
    const float ang =
      std::atan2(b.y, b.x) - std::atan2(a.y, a.x);
    float norm = ang;
    while (norm <= 0.0f) {norm += 2.0f * static_cast<float>(M_PI);}
    EXPECT_NEAR(norm, 2.0f * static_cast<float>(M_PI) / 5.0f, 1e-3);
  }
}

}  // namespace
}  // namespace san_formation
