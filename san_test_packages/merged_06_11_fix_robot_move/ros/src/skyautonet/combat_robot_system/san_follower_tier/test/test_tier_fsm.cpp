// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

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
#include "san_follower_tier/comm_health.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace san_follower_tier
{
namespace
{

TierInput baseInput()
{
  TierInput in;
  in.prediction_received = true;
  in.prediction_loss_ms = 0;
  in.comm_link_alive = true;
  in.obstacle_on_path = false;
  in.delta_m = 0.5f;
  in.base_distance_d0_m = 5.0f;
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

  // Allow min dwell to elapse (obstacle still true → no transition).
  fsm.step(in, 150);                                  // 150ms > 100ms dwell

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

  fsm.step(in, 150);                                  // dwell tick (obstacle still true)
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

// ═══════════════════════════════════════════════════════════════════════
// PATCH 2026-05-13 — new tests covering deep-dive fixes
// ═══════════════════════════════════════════════════════════════════════

// ─── PT1 (★ C1 fix): step() takes explicit dt ──────────────────────────
// Previously step() hardcoded dwell += 100. Now caller passes real dt.
TEST(PatchTierFsm, PT1_StepHonorsExplicitDt) {
  TierConfig cfg;
  cfg.auto_reroute_min_dwell_ms = 100;
  TierFsm fsm(cfg);
  TierInput in;
  in.obstacle_on_path = true;
  in.base_distance_d0_m = 5.0f;
  fsm.step(in, 100);                       // → T1.5, dwell=0
  EXPECT_EQ(fsm.currentTier(), Tier::T1_5);

  in.obstacle_on_path = false;
  // 50 ms of dwell — not enough to leave T1.5 (need 100).
  fsm.step(in, 50);
  EXPECT_EQ(fsm.currentTier(), Tier::T1_5);
  // Another 60 ms (total 110) — enough.
  fsm.step(in, 60);
  EXPECT_NE(fsm.currentTier(), Tier::T1_5);
}

// ─── PT2 (★ C2 fix): KPP-2 latency probe ───────────────────────────────
// Stamp must be recorded the moment obstacle goes false→true.
TEST(PatchTierFsm, PT2_KPP2LatencyStampRecorded) {
  TierFsm fsm;
  TierInput in;
  in.base_distance_d0_m = 5.0f;
  EXPECT_FALSE(fsm.pendingObstacleTriggerStamp().has_value());

  in.obstacle_on_path = true;
  const auto before = std::chrono::steady_clock::now();
  fsm.step(in, 100);
  const auto stamp = fsm.pendingObstacleTriggerStamp();
  ASSERT_TRUE(stamp.has_value());
  EXPECT_GE(*stamp, before);
  EXPECT_LE(*stamp, std::chrono::steady_clock::now());

  // Second consecutive step with obstacle=true must NOT re-stamp
  // (only false→true edge).
  const auto first_stamp = *stamp;
  fsm.step(in, 100);
  ASSERT_TRUE(fsm.pendingObstacleTriggerStamp().has_value());
  EXPECT_EQ(*fsm.pendingObstacleTriggerStamp(), first_stamp);

  fsm.clearObstacleTriggerStamp();
  EXPECT_FALSE(fsm.pendingObstacleTriggerStamp().has_value());
}

// ─── PT3 (★ KPP-2 < 1ms latency budget — actual measurement) ───────────
// step() itself must execute in well under 1ms even on slow hardware.
TEST(PatchTierFsm, PT3_KPP2StepLatencyUnderOneMs) {
  TierFsm fsm;
  TierInput in;
  in.base_distance_d0_m = 5.0f;
  in.obstacle_on_path = true;
  const auto t0 = std::chrono::steady_clock::now();
  fsm.step(in, 100);
  const auto t1 = std::chrono::steady_clock::now();
  const auto us =
    std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  // F4 KPP-2 budget: well under 1ms. Allow 500µs ceiling for slow CI.
  EXPECT_LT(us, 500);
}

// ─── PT4 (★ M6 fix): T2/T3 anti-flap dwell ──────────────────────────────
// Previously only T1.5 had dwell protection. T2/T3 could ping-pong.
TEST(PatchTierFsm, PT4_T2DwellPreventsFlap) {
  TierConfig cfg;
  cfg.catch_up_min_dwell_ms = 200;   // ★ explicitly enable T2 dwell
  cfg.hysteresis_ratio = 0.0f;          // disable hysteresis to isolate dwell
  TierFsm fsm(cfg);
  TierInput in;
  in.base_distance_d0_m = 5.0f;
  in.prediction_received = true;

  in.delta_m = 8.0f;       // > 1.5 d0 → T2
  fsm.step(in, 100);
  EXPECT_EQ(fsm.currentTier(), Tier::T2);

  // Drop delta below threshold — should NOT downgrade yet (50 < 200).
  in.delta_m = 1.0f;
  fsm.step(in, 50);
  EXPECT_EQ(fsm.currentTier(), Tier::T2);

  // Enough dwell — downgrade ok.
  fsm.step(in, 160);
  EXPECT_NE(fsm.currentTier(), Tier::T2);
}

// ─── PT5 (★ M8 fix): bidirectional hysteresis ──────────────────────────
// Upgrade threshold > nominal, downgrade < nominal → 0.2 d0 deadband.
TEST(PatchTierFsm, PT5_BidirectionalHysteresisDeadband) {
  TierConfig cfg;
  cfg.catch_up_ratio = 1.5f;
  cfg.hysteresis_ratio = 0.1f;
  cfg.upgrade_hysteresis_enabled = true;     // ★ enable bidirectional
  cfg.catch_up_min_dwell_ms = 0;             // remove dwell to isolate hysteresis
  TierFsm fsm(cfg);
  TierInput in;
  in.base_distance_d0_m = 5.0f;
  in.prediction_received = true;

  // δ = 1.55 d0 = 7.75 → between 1.4 and 1.6. From T1 (default) this
  // should NOT upgrade to T2 because upgrade requires > 1.6 d0 = 8.0.
  in.delta_m = 7.75f;
  fsm.step(in, 100);
  EXPECT_EQ(fsm.currentTier(), Tier::T0);   // received prediction

  // Force into T2 with high δ.
  in.delta_m = 9.0f;
  fsm.step(in, 100);
  EXPECT_EQ(fsm.currentTier(), Tier::T2);

  // Now drop δ to 1.45 d0 = 7.25 — between thresholds. Should STAY T2
  // because downgrade requires < 1.4 d0 = 7.0.
  in.delta_m = 7.25f;
  fsm.step(in, 100);
  EXPECT_EQ(fsm.currentTier(), Tier::T2);

  // Drop below downgrade threshold → downgrade allowed.
  in.delta_m = 6.9f;
  fsm.step(in, 100);
  EXPECT_NE(fsm.currentTier(), Tier::T2);
}

// ─── PT6 (★ comm_health module): basic alive/down accounting ────────────
TEST(PatchCommHealth, PT6_CommHealthTracksLoss) {
  CommHealth ch;
  ch.observeCommLink(1000u, true);                // alive at t=1.0s
  auto s = ch.snapshot(1500u);
  EXPECT_TRUE(s.comm_link_alive);
  EXPECT_EQ(s.comm_loss_ms, 0u);

  ch.observeCommLink(2000u, false);               // down at t=2.0s
  s = ch.snapshot(5000u);
  EXPECT_FALSE(s.comm_link_alive);
  EXPECT_EQ(s.comm_loss_ms, 3000u);

  ch.observeCommLink(7000u, true);                // back up
  s = ch.snapshot(7500u);
  EXPECT_TRUE(s.comm_link_alive);
  EXPECT_EQ(s.comm_loss_ms, 0u);
}

// ─── PT7 (★ comm_health module): breadcrumb TTL ─────────────────────────
TEST(PatchCommHealth, PT7_BreadcrumbTtl) {
  // brace-init to avoid Most Vexing Parse:
  // `CommHealth ch(3000u);` would be parsed as a function declaration.
  CommHealth ch{3000u};
  EXPECT_FALSE(ch.snapshot(0u).breadcrumb_available);

  ch.observeBreadcrumb(1000u);
  EXPECT_TRUE(ch.snapshot(3000u).breadcrumb_available);   // 2s elapsed → fresh
  EXPECT_TRUE(ch.snapshot(4000u).breadcrumb_available);   // 3s → still fresh (edge)
  EXPECT_FALSE(ch.snapshot(4001u).breadcrumb_available);  // 3.001s → stale
}

// ─── PT8 (★ M7 fix): comm down + no prediction → T4 reachable ───────────
// At boot when no FollowerTarget has arrived AND comm is down, the
// FSM should reach T4 via the comm-timeout branch instead of being
// stuck at T1 because prediction_loss_ms was always 0.
TEST(PatchTierFsm, PT8_T4ReachableAtBootViaCommTimeout) {
  TierConfig cfg;
  cfg.comm_timeout_ms = 100;        // shrink for the test
  TierFsm fsm(cfg);
  TierInput in;
  in.base_distance_d0_m = 5.0f;
  in.prediction_received = false;
  in.comm_link_alive = false;
  in.prediction_loss_ms = 0;        // boot state

  fsm.step(in, 50);
  // With both prediction_loss_ms=0 AND comm_link_alive=false, the
  // existing fallback path (T4 from comm-down branch) catches this.
  EXPECT_EQ(fsm.currentTier(), Tier::T4);

  // Even more direct: prediction_loss_ms exceeds timeout via the
  // CommHealth boot-anchor pattern.
  in.prediction_loss_ms = 200;
  in.comm_link_alive = false;
  fsm.step(in, 100);
  EXPECT_EQ(fsm.currentTier(), Tier::T4);
}

// ─── PT9 (★ regression): step() backward-compat single-arg overload ────
TEST(PatchTierFsm, PT9_BackwardCompatStepOverload) {
  TierFsm fsm;
  TierInput in;
  in.base_distance_d0_m = 5.0f;
  in.obstacle_on_path = true;
  // Old API (no dt) still works — defaults to 100ms.
  auto changed = fsm.step(in);
  ASSERT_TRUE(changed.has_value());
  EXPECT_EQ(*changed, Tier::T1_5);
}

}  // namespace
}  // namespace san_follower_tier
