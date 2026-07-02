// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — RTK Fix→Float mitigation Layer 3: RtkQualityFsm tests.
//
//   Q1  starts OK; sustained Fix stays OK
//   Q2  loss → DEGRADED_GRACE immediately, behaviour unchanged in grace
//   Q3  grace_sec of loss → DEGRADED_ACTIVE (defensive constraints)
//   Q4  active_lost_sec of loss → LOST
//   Q5  sustained Fix recovery returns to OK (after recover_sec)
//   Q6  brief Fix blip does NOT recover, and the escalation clock persists
//       (flapping still escalates)
//   Q7  constraint values per state

#include "san_rtk_gnss/rtk_quality_fsm.hpp"

#include <gtest/gtest.h>

using san_rtk_gnss::RtkQualityFsm;
using san_rtk_gnss::RtkQualityParams;
using san_rtk_gnss::RtkQualityState;

namespace
{
RtkQualityParams fastParams()
{
  RtkQualityParams p;
  p.grace_sec = 5.0;
  p.active_lost_sec = 30.0;
  p.recover_sec = 2.0;
  return p;
}
}  // namespace

TEST(RtkQualityFsm, Q1_StartsOkAndStaysOkWhileFixed) {
  RtkQualityFsm fsm(fastParams());
  EXPECT_EQ(fsm.state(), RtkQualityState::Ok);
  for (double t = 0.0; t < 100.0; t += 1.0) {
    EXPECT_EQ(fsm.update(true, t), RtkQualityState::Ok);
  }
}

TEST(RtkQualityFsm, Q2_LossEntersGraceImmediatelyBehaviourHeld) {
  RtkQualityFsm fsm(fastParams());
  fsm.update(true, 0.0);
  // First non-Fixed sample → grace, but nominal speed/tolerance held.
  EXPECT_EQ(fsm.update(false, 1.0), RtkQualityState::DegradedGrace);
  EXPECT_FLOAT_EQ(fsm.maxSpeed(), fastParams().ok_speed_mps);
  EXPECT_FALSE(fsm.formationLoose());
}

TEST(RtkQualityFsm, Q3_SustainedLossBecomesActiveAfterGrace) {
  RtkQualityFsm fsm(fastParams());
  fsm.update(true, 0.0);
  fsm.update(false, 1.0);                 // grace starts at t=1
  EXPECT_EQ(fsm.update(false, 5.0), RtkQualityState::DegradedGrace);   // 4s
  EXPECT_EQ(fsm.update(false, 6.5), RtkQualityState::DegradedActive);  // 5.5s
  EXPECT_FLOAT_EQ(fsm.maxSpeed(), fastParams().degraded_speed_mps);
  EXPECT_TRUE(fsm.formationLoose());
}

TEST(RtkQualityFsm, Q4_ProlongedLossBecomesLost) {
  RtkQualityFsm fsm(fastParams());
  fsm.update(true, 0.0);
  fsm.update(false, 1.0);                 // bad_since = 1
  EXPECT_EQ(fsm.update(false, 10.0), RtkQualityState::DegradedActive);
  EXPECT_EQ(fsm.update(false, 31.5), RtkQualityState::Lost);   // 30.5s bad
}

TEST(RtkQualityFsm, Q5_SustainedRecoveryReturnsToOk) {
  RtkQualityFsm fsm(fastParams());
  fsm.update(true, 0.0);
  fsm.update(false, 1.0);
  fsm.update(false, 8.0);
  ASSERT_EQ(fsm.state(), RtkQualityState::DegradedActive);
  // Fix returns at t=10 but must be sustained recover_sec (2s).
  EXPECT_EQ(fsm.update(true, 10.0), RtkQualityState::DegradedActive);
  EXPECT_EQ(fsm.update(true, 11.0), RtkQualityState::DegradedActive);  // 1s
  EXPECT_EQ(fsm.update(true, 12.5), RtkQualityState::Ok);              // 2.5s
}

TEST(RtkQualityFsm, Q6_BlipDoesNotRecoverAndClockPersists) {
  RtkQualityFsm fsm(fastParams());
  fsm.update(true, 0.0);
  fsm.update(false, 1.0);            // bad_since = 1
  fsm.update(false, 4.0);           // still grace (3s)
  // A single Fix blip at t=4.5 (< recover_sec) must NOT clear degradation.
  EXPECT_NE(fsm.update(true, 4.5), RtkQualityState::Ok);
  // Loss resumes; escalation clock persisted from t=1, so by t=7
  // (6s since first loss) we are ACTIVE despite the blip.
  EXPECT_EQ(fsm.update(false, 7.0), RtkQualityState::DegradedActive);
}

TEST(RtkQualityFsm, Q7_ConstraintValuesPerState) {
  RtkQualityParams p = fastParams();
  RtkQualityFsm fsm(p);
  // OK
  EXPECT_FLOAT_EQ(fsm.maxSpeed(), p.ok_speed_mps);
  EXPECT_FLOAT_EQ(fsm.pathTolerance(), p.ok_tolerance_m);
  EXPECT_FALSE(fsm.formationLoose());
  // Drive to LOST
  fsm.update(true, 0.0);
  fsm.update(false, 1.0);
  fsm.update(false, 40.0);
  ASSERT_EQ(fsm.state(), RtkQualityState::Lost);
  EXPECT_FLOAT_EQ(fsm.maxSpeed(), p.degraded_speed_mps);
  EXPECT_FLOAT_EQ(fsm.pathTolerance(), p.degraded_tolerance_m);
  EXPECT_TRUE(fsm.formationLoose());
}
