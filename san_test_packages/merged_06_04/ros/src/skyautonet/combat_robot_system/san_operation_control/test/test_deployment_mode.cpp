// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 7 - deployment_mode policy unit test.

#include <gtest/gtest.h>

#include "san_operation_control/deployment_mode.hpp"

using namespace san_operation_control;

TEST(DeploymentMode, ParseKnownStrings) {
  EXPECT_EQ(fromString("development"), DeploymentMode::DEVELOPMENT);
  EXPECT_EQ(fromString("DEV"), DeploymentMode::DEVELOPMENT);
  EXPECT_EQ(fromString("bench"), DeploymentMode::BENCH);
  EXPECT_EQ(fromString("lab_test"), DeploymentMode::LAB_TEST);
  EXPECT_EQ(fromString("LAB"), DeploymentMode::LAB_TEST);
  EXPECT_EQ(fromString("demo"), DeploymentMode::DEMO);
  EXPECT_EQ(fromString("production"), DeploymentMode::PRODUCTION);
}

TEST(DeploymentMode, UnknownStringDefaultsToProduction) {
  EXPECT_EQ(fromString("foo"), DeploymentMode::PRODUCTION);
  EXPECT_EQ(fromString(""), DeploymentMode::PRODUCTION);
}

TEST(DeploymentMode, RoundTripStrings) {
  for (auto m : {DeploymentMode::DEVELOPMENT, DeploymentMode::BENCH,
      DeploymentMode::LAB_TEST, DeploymentMode::DEMO,
      DeploymentMode::PRODUCTION})
  {
    EXPECT_EQ(fromString(toString(m)), m);
  }
}

TEST(DeploymentMode, DemoSequencerWidenedToLabTest) {
  // PHASE 7 widening: DEMO sequencer is enabled for BOTH demo and
  // lab_test. The remaining modes leave it off.
  EXPECT_TRUE(demoSequencerEnabled(DeploymentMode::DEMO));
  EXPECT_TRUE(demoSequencerEnabled(DeploymentMode::LAB_TEST));
  EXPECT_FALSE(demoSequencerEnabled(DeploymentMode::PRODUCTION));
  EXPECT_FALSE(demoSequencerEnabled(DeploymentMode::DEVELOPMENT));
  EXPECT_FALSE(demoSequencerEnabled(DeploymentMode::BENCH));
}

TEST(DeploymentMode, WatchdogForcedExceptInDevelopment) {
  EXPECT_TRUE(watchdogIsForceEnabled(DeploymentMode::PRODUCTION));
  EXPECT_TRUE(watchdogIsForceEnabled(DeploymentMode::DEMO));
  EXPECT_TRUE(watchdogIsForceEnabled(DeploymentMode::LAB_TEST));
  EXPECT_TRUE(watchdogIsForceEnabled(DeploymentMode::BENCH));
  EXPECT_FALSE(watchdogIsForceEnabled(DeploymentMode::DEVELOPMENT));
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
