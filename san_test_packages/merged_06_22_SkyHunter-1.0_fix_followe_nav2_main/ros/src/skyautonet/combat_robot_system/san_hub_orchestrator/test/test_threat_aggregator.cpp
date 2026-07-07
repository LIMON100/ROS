// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — ThreatAggregator tests (standalone).
//
// Pure-logic state machine, no ROS deps.
//
// Coverage:
//   T1  First alert creates new slot, instance_count=1, returns true
//   T2  Duplicate within window folds (instance_count++)
//   T3  Severity promotion (WARNING → CRITICAL) returns true
//   T4  Severity demotion ignored (keeps highest)
//   T5  pollReady before window elapses returns empty
//   T6  pollReady after window returns folded alert + clears slot
//   T7  Different (robot, type) keys aggregate independently
//   T8  Same type but different source_robot tracks separately
//   T9  Reset clears all slots
//   T10 peek returns current fold state
//   T11 Window expiry on next ingest creates fresh slot
//   T12 Window expiry on next ingest does NOT lose the old slot —
//       pollReady still publishes the accumulated alert (regression)

#include "san_hub_orchestrator/threat_aggregator.hpp"
#include <gtest/gtest.h>

namespace san_hub_orchestrator
{
namespace
{

ThreatInput makeAlert(
  const std::string & robot, uint8_t type, uint8_t sev,
  uint64_t ts_ms, const std::string & msg = "")
{
  ThreatInput a;
  a.source_robot_id = robot;
  a.threat_type = type;
  a.severity = sev;
  a.timestamp_ms = ts_ms;
  a.message_ko = msg;
  return a;
}

TEST(ThreatAggregator, T1_FirstAlertCreatesSlot) {
  ThreatAggregator ag(5.0);
  auto a = makeAlert("robot_1", 1, threat_severity::WARNING, 1000);
  EXPECT_TRUE(ag.ingest(a));
  EXPECT_EQ(ag.activeCount(), 1u);
  auto p = ag.peek("robot_1", 1);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->instance_count, 1u);
  EXPECT_EQ(p->severity, threat_severity::WARNING);
}

TEST(ThreatAggregator, T2_DuplicateWithinWindowFolds) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000));
  // Same robot+type within 5s → fold
  EXPECT_FALSE(
    ag.ingest(
      makeAlert("robot_1", 1, threat_severity::WARNING, 2000)));
  EXPECT_FALSE(
    ag.ingest(
      makeAlert("robot_1", 1, threat_severity::WARNING, 3000)));
  auto p = ag.peek("robot_1", 1);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->instance_count, 3u);
  EXPECT_EQ(p->timestamp_ms, 3000u);     // last update
}

TEST(ThreatAggregator, T3_SeverityPromotion) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000, "warn"));
  // Higher severity → promote, returns true
  EXPECT_TRUE(
    ag.ingest(
      makeAlert("robot_1", 1, threat_severity::CRITICAL, 2000, "crit")));
  auto p = ag.peek("robot_1", 1);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->severity, threat_severity::CRITICAL);
  EXPECT_EQ(p->message_ko, "crit");      // newer message wins on promote
  EXPECT_EQ(p->instance_count, 2u);
}

TEST(ThreatAggregator, T4_SeverityDemotionIgnored) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::CRITICAL, 1000, "crit"));
  EXPECT_FALSE(
    ag.ingest(
      makeAlert("robot_1", 1, threat_severity::WARNING, 2000, "warn")));
  auto p = ag.peek("robot_1", 1);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->severity, threat_severity::CRITICAL);   // unchanged
  EXPECT_EQ(p->message_ko, "crit");                    // unchanged
  EXPECT_EQ(p->instance_count, 2u);
}

TEST(ThreatAggregator, T5_PollReadyBeforeWindowEmpty) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000));
  // now = 3 s after start, window is 5 s → not ready
  auto ready = ag.pollReady(4000);
  EXPECT_TRUE(ready.empty());
  EXPECT_EQ(ag.activeCount(), 1u);
}

TEST(ThreatAggregator, T6_PollReadyAfterWindowEmits) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000, "msg"));
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 2000));
  // now = 7 s after window start → ready (1000+5000=6000 ≤ 7000)
  auto ready = ag.pollReady(7000);
  ASSERT_EQ(ready.size(), 1u);
  EXPECT_EQ(ready[0].instance_count, 2u);
  EXPECT_EQ(ready[0].message_ko, "msg");
  EXPECT_EQ(ag.activeCount(), 0u);       // slot cleared
}

TEST(ThreatAggregator, T7_IndependentKeys) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000));
  ag.ingest(makeAlert("robot_1", 2, threat_severity::WARNING, 1000));
  ag.ingest(makeAlert("robot_1", 3, threat_severity::WARNING, 1000));
  EXPECT_EQ(ag.activeCount(), 3u);
}

TEST(ThreatAggregator, T8_DifferentRobotsSeparate) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000));
  ag.ingest(makeAlert("robot_2", 1, threat_severity::WARNING, 1000));
  ag.ingest(makeAlert("robot_3", 1, threat_severity::WARNING, 1000));
  EXPECT_EQ(ag.activeCount(), 3u);
}

TEST(ThreatAggregator, T9_ResetClears) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000));
  ag.ingest(makeAlert("robot_2", 1, threat_severity::WARNING, 1000));
  ag.reset();
  EXPECT_EQ(ag.activeCount(), 0u);
  EXPECT_FALSE(ag.peek("robot_1", 1).has_value());
}

TEST(ThreatAggregator, T10_PeekReturnsFoldState) {
  ThreatAggregator ag(5.0);
  EXPECT_FALSE(ag.peek("robot_1", 1).has_value());
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000));
  auto p = ag.peek("robot_1", 1);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->source_robot_id, "robot_1");
  EXPECT_EQ(p->threat_type, 1);
}

TEST(ThreatAggregator, T11_WindowExpiryOnIngestStartsFresh) {
  ThreatAggregator ag(5.0);
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000, "old"));
  // 6 s later — window elapsed → ingest starts a NEW slot
  EXPECT_TRUE(
    ag.ingest(
      makeAlert("robot_1", 1, threat_severity::WARNING, 7000, "new")));
  auto p = ag.peek("robot_1", 1);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->instance_count, 1u);    // fresh slot, not folded
  EXPECT_EQ(p->message_ko, "new");
  EXPECT_EQ(p->timestamp_ms, 7000u);
}

TEST(ThreatAggregator, T12_WindowExpiryOnIngestPreservesOldSlot) {
  ThreatAggregator ag(5.0);
  // Two WARNINGs fold into one slot (instance_count=2), window_start=1000.
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 1000, "old"));
  ag.ingest(makeAlert("robot_1", 1, threat_severity::WARNING, 2000, "old"));

  // 6 s after start (> window) a NEW same-key alert arrives → the old slot
  // expires. Regression: it must NOT be silently dropped — the next
  // pollReady() must still return the accumulated old alert.
  EXPECT_TRUE(
    ag.ingest(
      makeAlert("robot_1", 1, threat_severity::WARNING, 7000, "new")));

  auto ready = ag.pollReady(7000);
  ASSERT_EQ(ready.size(), 1u) << "old accumulated slot was lost";
  EXPECT_EQ(ready[0].message_ko, "old");
  EXPECT_EQ(ready[0].instance_count, 2u);

  // The fresh slot is still active (its window starts at 7000) and is
  // itself published later — neither alert is lost.
  EXPECT_EQ(ag.activeCount(), 1u);
  auto p = ag.peek("robot_1", 1);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->message_ko, "new");

  auto ready2 = ag.pollReady(13000);
  ASSERT_EQ(ready2.size(), 1u);
  EXPECT_EQ(ready2[0].message_ko, "new");
}

}  // namespace
}  // namespace san_hub_orchestrator
