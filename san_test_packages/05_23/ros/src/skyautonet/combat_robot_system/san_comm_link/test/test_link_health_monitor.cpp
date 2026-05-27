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

namespace san_comm_link {
namespace {

LinkProbeUpdate up(bool wifi, bool lte) { return {wifi, lte}; }

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
  for (int i = 0; i < 4; ++i) m.update(up(true, true));
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
  for (int i = 0; i < 5; ++i) m.update(up(true, true));    // 3: LTE→WIFI6
  EXPECT_EQ(m.switchCount(), 3u);
}

TEST(LinkHealthMonitor, K12_ResetClearsState) {
  LinkHealthMonitor m;
  m.update(up(true, true));
  m.update(up(false, true));
  m.reset();
  EXPECT_EQ(m.activeLink(), ActiveLink::None);
  EXPECT_EQ(m.consecOk(),   0u);
  EXPECT_EQ(m.consecFail(), 0u);
  EXPECT_EQ(m.switchCount(), 0u);
}

}  // namespace
}  // namespace san_comm_link
