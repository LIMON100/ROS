// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — LinkSelector tests (standalone).
//
// PATCH 2026-05-13: LinkSelector is [[deprecated]] but still tested
// for backward-compatibility with legacy CommUplinkNode self-selecting
// mode. Suppress the warning for the whole TU.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
//
// Pure-logic gtest — covers the state-machine transitions Python's
// CommProcess implements in line-by-line if/elif/else branches.
//
// Coverage:
//   S1  Cold start, WiFi6 reachable → Wifi6
//   S2  Cold start, WiFi6 down, LTE usable → Lte
//   S3  Cold start, both down → None
//   S4  WiFi6 → Lte failover (single probe)
//   S5  Lte → no recovery before threshold (1, 2 of 3 successes)
//   S6  Lte → Wifi6 after threshold reached (3 consecutive)
//   S7  Recovery counter resets on WiFi6 flap
//   S8  LTE dropped while on LTE: switch to WiFi6 immediately if avail
//   S9  Both drop while on Wifi6 → None
//   S10 LTE registered but no PDP context → not usable (stays where it was)
//   S11 Stats counters increment correctly

#include "san_comm/link_selector.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace san_comm
{
namespace
{

LinkProbe probe(bool wifi, bool reg, bool pdp)
{
  return LinkProbe{wifi, reg, pdp};
}

TEST(LinkSelector, S1_ColdStart_Wifi6Available) {
  LinkSelector s;
  EXPECT_EQ(s.update(probe(true, false, false)), ActiveLink::Wifi6);
}

TEST(LinkSelector, S2_ColdStart_OnlyLteAvailable) {
  LinkSelector s;
  EXPECT_EQ(s.update(probe(false, true, true)), ActiveLink::Lte);
  EXPECT_EQ(s.failover_count(), 1u);
}

TEST(LinkSelector, S3_ColdStart_BothDown) {
  LinkSelector s;
  EXPECT_EQ(s.update(probe(false, false, false)), ActiveLink::None);
}

TEST(LinkSelector, S4_Wifi6_To_Lte_Failover) {
  LinkSelector s;
  EXPECT_EQ(s.update(probe(true, false, false)), ActiveLink::Wifi6);
  EXPECT_EQ(s.update(probe(false, true, true)), ActiveLink::Lte);
  EXPECT_EQ(s.failover_count(), 1u);
}

TEST(LinkSelector, S5_Lte_NoRecoveryBeforeThreshold) {
  // Default threshold = 3
  LinkSelector s;
  s.update(probe(false, true, true));   // Lte
  ASSERT_EQ(s.current(), ActiveLink::Lte);
  // 2 consecutive WiFi6 successes — still on Lte
  EXPECT_EQ(s.update(probe(true, true, true)), ActiveLink::Lte);
  EXPECT_EQ(s.update(probe(true, true, true)), ActiveLink::Lte);
  EXPECT_EQ(s.wifi_recovery_count(), 2u);
  EXPECT_EQ(s.recovery_count(), 0u);
}

TEST(LinkSelector, S6_Lte_RecoverAfterThreshold) {
  LinkSelector s;
  s.update(probe(false, true, true));   // Lte
  s.update(probe(true, true, true));    // 1
  s.update(probe(true, true, true));    // 2
  EXPECT_EQ(s.update(probe(true, true, true)), ActiveLink::Wifi6);  // 3 → recover
  EXPECT_EQ(s.recovery_count(), 1u);
  EXPECT_EQ(s.wifi_recovery_count(), 0u);  // reset on recovery
}

TEST(LinkSelector, S7_RecoveryCounterResetsOnFlap) {
  LinkSelector s;
  s.update(probe(false, true, true));   // Lte
  s.update(probe(true, true, true));    // 1
  s.update(probe(true, true, true));    // 2
  s.update(probe(false, true, true));   // wifi flap — counter resets
  EXPECT_EQ(s.current(), ActiveLink::Lte);
  EXPECT_EQ(s.wifi_recovery_count(), 0u);
  // Still need 3 NEW consecutive
  s.update(probe(true, true, true));    // 1
  s.update(probe(true, true, true));    // 2
  EXPECT_EQ(s.current(), ActiveLink::Lte);
  EXPECT_EQ(s.update(probe(true, true, true)), ActiveLink::Wifi6);  // 3 → recover
}

TEST(LinkSelector, S8_LteDropped_FallbackToWifi6Immediately) {
  LinkSelector s;
  s.update(probe(false, true, true));   // Lte
  // LTE drops (PDP gone), WiFi6 available → immediate switch
  EXPECT_EQ(s.update(probe(true, true, false)), ActiveLink::Wifi6);
}

TEST(LinkSelector, S9_BothDropFromWifi6) {
  LinkSelector s;
  s.update(probe(true, false, false));    // Wifi6
  EXPECT_EQ(s.update(probe(false, false, false)), ActiveLink::None);
}

TEST(LinkSelector, S10_LteRegisteredButNoPdp) {
  LinkSelector s;
  // Cold start: WiFi6 down, LTE registered but no PDP → None
  EXPECT_EQ(s.update(probe(false, true, false)), ActiveLink::None);
}

TEST(LinkSelector, S11_StatsCountersIncrement) {
  LinkSelector s;
  s.update(probe(true, true, true));    // Wifi6
  s.update(probe(false, true, true));   // Lte (failover 1)
  s.update(probe(true, true, true));    // ↑1
  s.update(probe(true, true, true));    // ↑2
  s.update(probe(true, true, true));    // ↑3 → Wifi6 (recovery 1)
  s.update(probe(false, true, true));   // Lte (failover 2)
  EXPECT_EQ(s.failover_count(), 2u);
  EXPECT_EQ(s.recovery_count(), 1u);
}

// ═══════════════════════════════════════════════════════════════════════
// R-9 deep-dive + main hardening — orthogonal coverage, all kept
// ═══════════════════════════════════════════════════════════════════════

// ─── PS1 (R-9 C7 fix): LinkSelector accessors are thread-safe ──────────
TEST(PatchLinkSelector, PS1_ThreadSafeAccessors) {
  LinkSelector s;
  std::atomic<bool> stop{false};
  std::thread reader([&]() {
      while (!stop.load()) {
        (void)s.current();
        (void)s.wifi_recovery_count();
        (void)s.failover_count();
        (void)s.recovery_count();
      }
    });
  for (int i = 0; i < 1000; ++i) {
    s.update({(i & 1) == 0, true, true});
  }
  stop.store(true);
  reader.join();
  SUCCEED();
}

// ─── S12: Custom wifi_recovery_threshold exact boundary ────────────────
TEST(LinkSelector, S12_CustomRecoveryThresholdExactBoundary) {
  LinkSelectorConfig cfg{};
  cfg.wifi_recovery_threshold = 5;
  LinkSelector s(cfg);

  ASSERT_EQ(s.update(probe(false, true, true)), ActiveLink::Lte);

  for (int i = 1; i <= 4; ++i) {
    EXPECT_EQ(s.update(probe(true, true, true)), ActiveLink::Lte)
      << "success #" << i << " of 5 must NOT trigger recovery";
  }
  EXPECT_EQ(s.update(probe(true, true, true)), ActiveLink::Wifi6);
  EXPECT_EQ(s.recovery_count(), 1u);
}

// ─── S13: LTE registered but PDP not active — not usable ───────────────
TEST(LinkSelector, S13_LteRegisteredWithoutPdpIsNotUsable) {
  LinkSelector s;
  s.update(probe(true, false, false));      // Wifi6 active
  ASSERT_EQ(s.current(), ActiveLink::Wifi6);

  EXPECT_EQ(s.update(probe(false, true, false)), ActiveLink::None)
    << "LTE registered without PDP must NOT be selected as fallback";
  EXPECT_EQ(s.failover_count(), 0u);
}

}  // namespace
}  // namespace san_comm

#pragma GCC diagnostic pop
