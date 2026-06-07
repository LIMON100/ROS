// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — Link health monitor tests (standalone).
//
// Coverage:
//   K1  Bootstrap: wifi6 ok → active = WIFI6
//   K2  Bootstrap: wifi6 down + lte ok → active = LTE
//   K3  Bootstrap: both down → active = NONE
//   K4  WIFI6 → NO downgrade on transient failure (< N_FAIL)
//   K5  WIFI6 → LTE downgrade after N_FAIL consecutive failures
//   K6  LTE → NO upgrade on transient WiFi6 success (< N_OK)
//   K7  LTE → WIFI6 upgrade after N_OK consecutive successes
//   K8  WIFI6 → NONE when both fail simultaneously
//   K9  LTE → NONE when LTE goes down before WiFi6 recovers
//   K10 switch_event flag only on actual transition
//   K11 switch_count cumulative
//   K12 reset() clears all state

#include "san_comm_link/link_health_monitor.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace san_comm_link
{
namespace
{

LinkProbeUpdate up(bool wifi, bool lte) {return {wifi, lte};}

TEST(LinkHealthMonitor, K1_BootstrapWifi6Active) {
  LinkHealthMonitor m;
  auto d = m.update(up(true, true));
  EXPECT_EQ(d.active_link, ActiveLink::Wifi6);
  EXPECT_TRUE(d.switch_event);
  EXPECT_NE(d.reason.find("wifi6 up"), std::string::npos);
}

TEST(LinkHealthMonitor, K2_BootstrapLteFallback) {
  LinkHealthMonitor m;
  auto d = m.update(up(false, true));
  EXPECT_EQ(d.active_link, ActiveLink::Lte);
  EXPECT_TRUE(d.switch_event);
}

TEST(LinkHealthMonitor, K3_BootstrapNoUplink) {
  LinkHealthMonitor m;
  auto d = m.update(up(false, false));
  EXPECT_EQ(d.active_link, ActiveLink::None);
  EXPECT_FALSE(d.switch_event);  // still None — no transition from None to None
}

TEST(LinkHealthMonitor, K4_TransientWifi6FailureDoesNotDowngrade) {
  LinkHealthMonitor m;
  // Bootstrap to WiFi6
  m.update(up(true, true));
  // One probe failure — below threshold (default N_FAIL=3)
  auto d = m.update(up(false, true));
  EXPECT_EQ(d.active_link, ActiveLink::Wifi6);
  EXPECT_FALSE(d.switch_event);
  // Two failures — still not enough
  d = m.update(up(false, true));
  EXPECT_EQ(d.active_link, ActiveLink::Wifi6);
  EXPECT_FALSE(d.switch_event);
}

TEST(LinkHealthMonitor, K5_DowngradeAfterNFailConsecutiveFailures) {
  LinkHealthMonitor m;   // default N_FAIL = 3
  m.update(up(true, true));
  m.update(up(false, true));
  m.update(up(false, true));
  // Third failure → downgrade
  auto d = m.update(up(false, true));
  EXPECT_EQ(d.active_link, ActiveLink::Lte);
  EXPECT_TRUE(d.switch_event);
  EXPECT_NE(d.reason.find("fail streak"), std::string::npos);
}

TEST(LinkHealthMonitor, K6_TransientWifi6SuccessDoesNotUpgrade) {
  LinkHealthMonitor m;     // default N_OK = 5
  // Push to LTE
  m.update(up(false, true));
  // 4 consecutive successes — still below threshold
  for (int i = 0; i < 4; ++i) {
    auto d = m.update(up(true, true));
    EXPECT_EQ(d.active_link, ActiveLink::Lte);
    EXPECT_FALSE(d.switch_event);
  }
}

TEST(LinkHealthMonitor, K7_UpgradeAfterNOkConsecutiveSuccesses) {
  LinkHealthMonitor m;     // default N_OK = 5
  m.update(up(false, true));
  for (int i = 0; i < 4; ++i) {
    m.update(up(true, true));
  }
  // 5th success → upgrade
  auto d = m.update(up(true, true));
  EXPECT_EQ(d.active_link, ActiveLink::Wifi6);
  EXPECT_TRUE(d.switch_event);
  EXPECT_NE(d.reason.find("upgrade"), std::string::npos);
}

TEST(LinkHealthMonitor, K8_BothFailFromWifi6Goes_None) {
  LinkHealthMonitor m;
  m.update(up(true, true));      // WIFI6 active
  m.update(up(false, false));
  m.update(up(false, false));
  auto d = m.update(up(false, false));   // 3rd failure → downgrade
  EXPECT_EQ(d.active_link, ActiveLink::None);
  EXPECT_TRUE(d.switch_event);
  EXPECT_NE(d.reason.find("lte also down"), std::string::npos);
}

TEST(LinkHealthMonitor, K9_LteGoesDownFromLteState) {
  LinkHealthMonitor m;
  // Bootstrap to LTE (wifi down)
  m.update(up(false, true));
  EXPECT_EQ(m.activeLink(), ActiveLink::Lte);
  // LTE goes down, wifi6 still down
  auto d = m.update(up(false, false));
  EXPECT_EQ(d.active_link, ActiveLink::None);
  EXPECT_TRUE(d.switch_event);
}

TEST(LinkHealthMonitor, K10_SwitchEventOnlyOnTransition) {
  LinkHealthMonitor m;
  m.update(up(true, true));               // transition None→WIFI6
  auto d = m.update(up(true, true));       // stay WIFI6
  EXPECT_FALSE(d.switch_event);
  EXPECT_TRUE(d.reason.empty());
}

TEST(LinkHealthMonitor, K11_SwitchCountCumulative) {
  LinkHealthMonitor m;
  m.update(up(true, true));                                // 1: None→WIFI6
  m.update(up(false, true)); m.update(up(false, true));
  m.update(up(false, true));                                // 2: WIFI6→LTE
  for (int i = 0; i < 5; ++i) {
    m.update(up(true, true));                              // 3: LTE→WIFI6
  }
  EXPECT_EQ(m.switchCount(), 3u);
}

TEST(LinkHealthMonitor, K12_ResetClearsState) {
  LinkHealthMonitor m;
  m.update(up(true, true));
  m.update(up(false, true));
  m.reset();
  EXPECT_EQ(m.activeLink(), ActiveLink::None);
  EXPECT_EQ(m.consecOk(), 0u);
  EXPECT_EQ(m.consecFail(), 0u);
  EXPECT_EQ(m.switchCount(), 0u);
}

// ═══════════════════════════════════════════════════════════════════════
// R-9 deep-dive + main hardening — orthogonal coverage, all kept
// ═══════════════════════════════════════════════════════════════════════

// ─── PK1 (R-9 C6 fix): LTE stabilization streak required from None ────
TEST(PatchLinkHealthMonitor, PK1_NoneToLteRequiresLteStreak) {
  LinkHysteresisConfig cfg;
  cfg.lte_consec_ok_to_stabilize = 3;
  LinkHealthMonitor m(cfg);

  // Single lte_ok tick — must NOT enter Lte yet (stabilisation streak).
  auto d = m.update({false, true});
  EXPECT_EQ(d.active_link, ActiveLink::None);

  d = m.update({false, true});
  EXPECT_EQ(d.active_link, ActiveLink::None);

  // Third consecutive lte_ok → finally enter Lte.
  d = m.update({false, true});
  EXPECT_EQ(d.active_link, ActiveLink::Lte);
  EXPECT_TRUE(d.switch_event);
}

// ─── PK2 (★ C6 fix): LTE streak resets on failure ──────────────────────
TEST(PatchLinkHealthMonitor, PK2_LteStreakResetsOnFailure) {
  LinkHysteresisConfig cfg;
  cfg.lte_consec_ok_to_stabilize = 3;
  LinkHealthMonitor m(cfg);

  m.update({false, true});                 // streak = 1
  m.update({false, true});                 // streak = 2
  m.update({false, false});                // streak = 0 (reset)
  EXPECT_EQ(m.lteConsecOk(), 0u);

  m.update({false, true});                 // streak = 1
  auto d = m.update({false, true});        // streak = 2 — still None
  EXPECT_EQ(d.active_link, ActiveLink::None);
}

// ─── PK3 (★ C6 fix): Wifi6→Lte requires LTE stable too ─────────────────
TEST(PatchLinkHealthMonitor, PK3_Wifi6ToLteWithUnstableLteGoesNone) {
  LinkHysteresisConfig cfg;
  cfg.consec_fail_to_downgrade = 2;
  cfg.lte_consec_ok_to_stabilize = 3;
  LinkHealthMonitor m(cfg);

  // Bootstrap to Wifi6.
  auto d = m.update({true, false});
  ASSERT_EQ(d.active_link, ActiveLink::Wifi6);

  // 2 consecutive wifi6 failures, but LTE has been up only once →
  // not stable yet → drop to None (not Lte).
  m.update({false, true});
  d = m.update({false, true});
  EXPECT_EQ(d.active_link, ActiveLink::None);
}

// ─── PK4 (★ M8 fix): thread-safe accessors don't crash under contention ─
TEST(PatchLinkHealthMonitor, PK4_AccessorsThreadSafe) {
  LinkHealthMonitor m;
  std::atomic<bool> stop{false};
  std::thread reader([&]() {
      while (!stop.load()) {
        (void)m.activeLink();
        (void)m.consecOk();
        (void)m.consecFail();
        (void)m.lteConsecOk();
        (void)m.switchCount();
      }
    });
  for (int i = 0; i < 1000; ++i) {
    m.update({(i & 1) == 0, true});
  }
  stop.store(true);
  reader.join();
  // No assertion needed — TSAN/clean exit is the test.
  SUCCEED();
}

// ─── K13: Custom hysteresis config overrides defaults ────────────────
//
// The default 3/5 hysteresis is tuned for tactical-field WiFi6 jitter.
// Bench / sim configurations want zero hysteresis (immediate response).
// This test asserts the cfg constructor parameter actually flows
// through — guards against accidental constructor regression.
TEST(LinkHealthMonitor, K13_CustomHysteresisConfigImmediate) {
  LinkHysteresisConfig cfg{};
  cfg.consec_ok_to_upgrade = 1;      // immediate upgrade
  cfg.consec_fail_to_downgrade = 1;  // immediate downgrade
  LinkHealthMonitor m(cfg);

  // bootstrap to WiFi6
  m.update(up(true, true));
  EXPECT_EQ(m.activeLink(), ActiveLink::Wifi6);

  // ONE WiFi6 failure with the custom cfg should downgrade
  // immediately (vs default which would need 3 in a row).
  auto d = m.update(up(false, true));
  EXPECT_EQ(d.active_link, ActiveLink::Lte);
  EXPECT_TRUE(d.switch_event);

  // ONE WiFi6 success → upgrade immediately
  d = m.update(up(true, true));
  EXPECT_EQ(d.active_link, ActiveLink::Wifi6);
  EXPECT_TRUE(d.switch_event);
}

// ─── K14: switch_count only ticks on actual transitions ───────────────
//
// Regression guard for "K10 + K11 together" — switch_event must NEVER
// fire when active_link does not change, and switchCount() must equal
// the number of switch_event=true ticks. Catches a class of bugs
// where someone adds a code path that sets switch_event=true without
// actually transitioning (e.g. fail-streak refresh emitting a
// spurious event).
TEST(LinkHealthMonitor, K14_SwitchCountMatchesActualTransitionsOnly) {
  LinkHealthMonitor m;            // default 3/5 hysteresis

  // Bootstrap None → WiFi6 (transition 1)
  auto d = m.update(up(true, true));
  EXPECT_TRUE(d.switch_event);
  EXPECT_EQ(m.switchCount(), 1u);

  // 10 ticks of steady WiFi6 ok — NO transitions
  for (int i = 0; i < 10; ++i) {
    d = m.update(up(true, true));
    EXPECT_FALSE(d.switch_event)
      << "tick " << i << ": switch_event must not fire on no-op";
    EXPECT_EQ(m.switchCount(), 1u);
  }

  // 3 failures → downgrade (transition 2)
  m.update(up(false, true));
  m.update(up(false, true));
  d = m.update(up(false, true));
  EXPECT_TRUE(d.switch_event);
  EXPECT_EQ(m.switchCount(), 2u);

  // 5 ticks of steady LTE — NO transitions
  for (int i = 0; i < 5; ++i) {
    d = m.update(up(false, true));
    EXPECT_FALSE(d.switch_event);
  }
  EXPECT_EQ(m.switchCount(), 2u);
}

// ─── K15: Stress oscillation — switch_count remains bounded ──────────
//
// Phase-7 audit F4 — the dual-hysteresis design (N_FAIL=3 +
// N_OK=5 defaults) is meant to suppress oscillation when WiFi6
// flaps rapidly. This test injects 100 rapid alternations
// (wifi=true, false, true, false, ...) and asserts that
// switchCount() stays well below 100 — hysteresis doing its job.
//
// Invariant: alternating fail/ok never accumulates 3 consecutive
// fails or 5 consecutive oks, so the WIFI6 → LTE downgrade never
// completes. Only the initial bootstrap None → WIFI6 switch
// should be observed.
TEST(LinkHealthMonitor, K15_RapidOscillationSwitchCountBounded) {
  LinkHealthMonitor m;              // defaults: N_FAIL=3, N_OK=5
  m.update(up(true, true));         // bootstrap → WIFI6 (1 switch)

  // 100 alternating wifi6 probes; lte_ok stays true throughout.
  for (int i = 0; i < 100; ++i) {
    const bool wifi = (i % 2 == 0);
    m.update(up(wifi, true));
  }

  EXPECT_LE(m.switchCount(), 5u)
    << "switch_count=" << m.switchCount()
    << " — hysteresis (N_FAIL=3 / N_OK=5) failed to suppress "
    "rapid oscillation; this would cause N-way DDS topic "
    "thrash and operator-visible link flicker.";

  EXPECT_GE(m.switchCount(), 1u)
    << "no transitions observed; test inputs may not have "
    "exercised the state machine at all.";

  EXPECT_EQ(m.activeLink(), ActiveLink::Wifi6)
    << "alternating wifi=true/false should leave us on WIFI6 "
    "(downgrade requires 3 consecutive fails — never "
    "accumulates under alternation).";
}

}  // namespace
}  // namespace san_comm_link
