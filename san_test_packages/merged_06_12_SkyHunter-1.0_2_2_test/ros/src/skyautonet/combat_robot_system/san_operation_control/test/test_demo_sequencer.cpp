// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 7 - DEMO 6-phase sequencer unit test.

#include <gtest/gtest.h>

#include <vector>

#include "san_operation_control/demo_sequencer.hpp"

using namespace san_operation_control;

TEST(DemoSequencer, EnableGatedByMode) {
  DemoSequencer s;
  EXPECT_TRUE(s.enableForMode(DeploymentMode::DEMO));
  EXPECT_TRUE(s.isEnabled());
  s.disable();
  EXPECT_FALSE(s.isEnabled());

  // PHASE 7 widening: lab_test must also enable the sequencer.
  EXPECT_TRUE(s.enableForMode(DeploymentMode::LAB_TEST));
  EXPECT_TRUE(s.isEnabled());

  // Other modes refuse.
  DemoSequencer s2;
  EXPECT_FALSE(s2.enableForMode(DeploymentMode::PRODUCTION));
  EXPECT_FALSE(s2.enableForMode(DeploymentMode::DEVELOPMENT));
  EXPECT_FALSE(s2.enableForMode(DeploymentMode::BENCH));
}

TEST(DemoSequencer, FullSixPhaseProgression) {
  DemoSequencer s;
  s.setPhaseDurationSec(0.05);    // 50 ms per phase, test-fast
  s.enableForMode(DeploymentMode::DEMO);

  std::vector<DemoPhase> observed;
  s.setPhaseCallback([&](DemoPhase p) {observed.push_back(p);});

  // Tick over 6 phases worth of time (300 ms).
  for (uint64_t t = 0; t <= 350; t += 5) {
    s.tick(t);
  }
  // Expected: STANDBY -> DEPLOY -> FORMATION -> PATROL -> ENGAGE -> RTB
  ASSERT_GE(observed.size(), 6u);
  EXPECT_EQ(observed.front(), DemoPhase::STANDBY);
  EXPECT_EQ(s.currentPhase(), DemoPhase::RTB)
    << "after full sequence sequencer parks at RTB";
}

TEST(DemoSequencer, DisabledSequencerDoesNotAdvance) {
  DemoSequencer s;
  s.setPhaseDurationSec(0.01);
  // No enable.
  int callbacks = 0;
  s.setPhaseCallback([&](DemoPhase) {callbacks++;});
  for (uint64_t t = 0; t < 200; t += 10) {
    s.tick(t);
  }
  EXPECT_EQ(callbacks, 0);
  EXPECT_EQ(s.currentPhase(), DemoPhase::STANDBY);
}

TEST(DemoSequencer, PhaseNamesAreStable) {
  EXPECT_STREQ(demoPhaseName(DemoPhase::STANDBY), "STANDBY");
  EXPECT_STREQ(demoPhaseName(DemoPhase::DEPLOY), "DEPLOY");
  EXPECT_STREQ(demoPhaseName(DemoPhase::FORMATION), "FORMATION");
  EXPECT_STREQ(demoPhaseName(DemoPhase::PATROL), "PATROL");
  EXPECT_STREQ(demoPhaseName(DemoPhase::ENGAGE), "ENGAGE");
  EXPECT_STREQ(demoPhaseName(DemoPhase::RTB), "RTB");
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
