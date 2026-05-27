// SAN v1.5 — TierFsm standalone tests.
//
// Verifies all transition paths in SDD-SWARM §6.2 (5-Tier FSM) and
// includes a KPP-2 timing test (T0→T1.5 latency).
//
// Test coverage:
//   F1   Initial state = T1 (defensive default)
//   F2   T1→T0 on first valid prediction
//   F3   T0→T1 on prediction loss (comm alive)
//   F4   T0→T1.5 on obstacle — **KPP-2 timing: ≤ 1ms FSM response**
//   F5   T1.5→T0 on obstacle cleared (after min_dwell)
//   F6   T0→T2 at δ > 1.5 d₀
//   F7   T2→T3 at δ > 2.0 d₀
//   F8   T3→T4 at δ > 4.0 d₀
//   F9   T0→T4 on 60s comm timeout (even with δ=0)
//   F10  T4→T0 on comm restored
//   F11  Obstacle priority — overrides T3 catchup
//   F12  Hysteresis — δ slightly under threshold doesn't downgrade
//   F13  T1.5 min dwell — anti-flap (cannot exit immediately)
//   F14  lastReason() reports correct trigger text
//   F15  step() returns nullopt when tier unchanged
//   F16  Comm down + no prediction → T4 fallback (not T1)

#include "san_follower_tier/tier_fsm.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace san_follower_tier {
namespace {

TierInput baseInput() {
  TierInput in;
  in.prediction_received  = true;
  in.prediction_loss_ms   = 0;
  in.comm_link_alive      = true;
  in.obstacle_on_path     = false;
  in.delta_m              = 0.5f;
  in.base_distance_d0_m   = 5.0f;
  in.breadcrumb_available = true;
  return in;
}

// ─── F1: Initial state ─────────────────────────────────────────────────

TEST(TierFsm, F1_InitialStateIsT1) {
  TierFsm fsm;
  EXPECT_EQ(fsm.currentTier(), Tier::T1);
}

// ─── F2: T1 → T0 ──────────────────────────────────────────────────────

TEST(TierFsm, F2_TransitionsToT0OnFirstPrediction) {
  TierFsm fsm;
  auto in = baseInput();
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T0);
  EXPECT_EQ(fsm.currentTier(), Tier::T0);
}

// ─── F3: T0 → T1 ──────────────────────────────────────────────────────

TEST(TierFsm, F3_TransitionsToT1OnPredictionLoss) {
  TierFsm fsm;
  fsm.step(baseInput());            // → T0
  auto in = baseInput();
  in.prediction_received = false;
  in.comm_link_alive = true;
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T1);
}

// ─── F4: T0 → T1.5 + KPP-2 timing ─────────────────────────────────────

TEST(TierFsm, F4_TransitionsToT1_5OnObstacle_KPP2Timing) {
  TierFsm fsm;
  fsm.step(baseInput());            // → T0

  auto in = baseInput();
  in.obstacle_on_path = true;

  // Measure FSM step latency — this is the FSM-side budget contribution
  // to the KPP-2 ≤ 300ms total system response. The FSM itself should
  // be well under 1ms; the budget is consumed by cost map traversal +
  // controller_server + actuator response.
  const auto t0 = std::chrono::high_resolution_clock::now();
  auto changed = fsm.step(in);
  const auto t1 = std::chrono::high_resolution_clock::now();
  const auto elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T1_5);
  EXPECT_LT(elapsed_us, 1000)        // < 1ms FSM-side
      << "TierFsm step latency " << elapsed_us
      << "us (KPP-2 budget impact)";
}

// ─── F5: T1.5 → T0 after dwell ────────────────────────────────────────

TEST(TierFsm, F5_LeavesT1_5OnObstacleClearedAfterDwell) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0

  auto in = baseInput();
  in.obstacle_on_path = true;
  fsm.step(in);                                       // → T1.5

  // Allow min dwell to elapse
  fsm.stepWithDt(150);                                // 150ms > 100ms dwell

  in.obstacle_on_path = false;
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T0);
}

// ─── F6/F7/F8: Catchup tier thresholds ────────────────────────────────

TEST(TierFsm, F6_TransitionsToT2AtDeltaGt1_5d0) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0

  auto in = baseInput();
  in.delta_m = 1.6f * in.base_distance_d0_m;          // 8.0m > 7.5m
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T2);
}

TEST(TierFsm, F7_TransitionsToT3AtDeltaGt2_0d0) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0

  auto in = baseInput();
  in.delta_m = 2.1f * in.base_distance_d0_m;          // 10.5m > 10m
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T3);
}

TEST(TierFsm, F8_TransitionsToT4AtDeltaGt4_0d0) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0

  auto in = baseInput();
  in.delta_m = 4.1f * in.base_distance_d0_m;          // 20.5m > 20m
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T4);
}

// ─── F9: T0 → T4 on 60s timeout ────────────────────────────────────────

TEST(TierFsm, F9_TransitionsToT4OnCommTimeout60s) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0

  auto in = baseInput();
  in.prediction_received = false;
  in.prediction_loss_ms = 61000;                      // > 60s
  in.delta_m = 0.5f;                                  // small delta
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T4);
}

// ─── F10: T4 → T0 on comm restored ─────────────────────────────────────

TEST(TierFsm, F10_RecoveryFromT4OnCommRestored) {
  TierFsm fsm;
  // Force into T4
  auto in = baseInput();
  in.prediction_received = false;
  in.prediction_loss_ms = 61000;
  fsm.step(in);
  ASSERT_EQ(fsm.currentTier(), Tier::T4);

  // Restore comm
  in.prediction_received = true;
  in.prediction_loss_ms = 0;
  in.delta_m = 0.5f;
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T0);
}

// ─── F11: Obstacle priority over catchup ───────────────────────────────

TEST(TierFsm, F11_ObstaclePriorityOverridesCatchup) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0

  // δ above T3 threshold AND obstacle — must pick T1.5 (safety wins)
  auto in = baseInput();
  in.delta_m = 2.5f * in.base_distance_d0_m;
  in.obstacle_on_path = true;
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T1_5);
}

// ─── F12: Hysteresis at boundaries ─────────────────────────────────────

TEST(TierFsm, F12_HysteresisPreventsBoundaryOscillation) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0
  // Enter T2 cleanly
  auto in = baseInput();
  in.delta_m = 1.6f * in.base_distance_d0_m;          // 8.0m
  fsm.step(in);
  ASSERT_EQ(fsm.currentTier(), Tier::T2);

  // δ drops just BELOW 1.5 d₀ (7.4m). With 10% hysteresis the
  // effective threshold for leaving T2 is 1.5 - 0.1 = 1.4 d₀ = 7.0m,
  // so 7.4m keeps us in T2.
  in.delta_m = 1.48f * in.base_distance_d0_m;         // 7.4m
  auto changed = fsm.step(in);
  EXPECT_FALSE(changed.has_value());                  // no transition
  EXPECT_EQ(fsm.currentTier(), Tier::T2);

  // δ drops below hysteresis margin (7.0m) — now we leave T2
  in.delta_m = 1.3f * in.base_distance_d0_m;          // 6.5m
  changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T0);                      // back to predictive
}

// ─── F13: T1.5 min dwell anti-flap ────────────────────────────────────

TEST(TierFsm, F13_T1_5MinDwellAntiFlap) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0

  auto in = baseInput();
  in.obstacle_on_path = true;
  fsm.step(in);                                       // → T1.5
  ASSERT_EQ(fsm.currentTier(), Tier::T1_5);

  // Obstacle gone immediately (< min dwell 100ms) — must NOT exit
  in.obstacle_on_path = false;
  auto changed = fsm.step(in);
  EXPECT_FALSE(changed.has_value());
  EXPECT_EQ(fsm.currentTier(), Tier::T1_5);
}

// ─── F14: lastReason() text ────────────────────────────────────────────

TEST(TierFsm, F14_LastReasonReportsCorrectTrigger) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0
  EXPECT_EQ(fsm.lastReason(), "prediction_received");

  auto in = baseInput();
  in.obstacle_on_path = true;
  fsm.step(in);                                       // → T1.5
  EXPECT_EQ(fsm.lastReason(), "obstacle_on_path");

  fsm.stepWithDt(150);
  in.obstacle_on_path = false;
  fsm.step(in);                                       // → T0 (obstacle_cleared)
  EXPECT_EQ(fsm.lastReason(), "obstacle_cleared");
}

// ─── F15: step() returns nullopt when stable ──────────────────────────

TEST(TierFsm, F15_StepReturnsNulloptWhenStable) {
  TierFsm fsm;
  fsm.step(baseInput());                              // → T0 (changed)
  auto changed = fsm.step(baseInput());               // unchanged
  EXPECT_FALSE(changed.has_value());
}

// ─── F16: Comm down + no prediction → T4 ──────────────────────────────

TEST(TierFsm, F16_CommDownNoPredictionFallsBackToT4) {
  TierFsm fsm;
  // First go to T0 cleanly
  fsm.step(baseInput());
  // Now break comm + lose prediction with small δ
  auto in = baseInput();
  in.prediction_received = false;
  in.comm_link_alive = false;
  in.delta_m = 0.5f;
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  // Comm down + no prediction → safest is T4 (breadcrumb recovery),
  // not T1 (which assumes comm OK).
  EXPECT_EQ(*changed, Tier::T4);
}

}  // namespace
}  // namespace san_follower_tier
