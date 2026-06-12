// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 7 - sensor watchdog policy unit test.
//
// Key invariants:
//   1. yaml hw_watchdog_enabled=false IS honored in DEVELOPMENT
//   2. yaml hw_watchdog_enabled=false IS IGNORED in production / demo /
//      lab_test / bench (force-on)
//   3. checkSensorState() returns true while disabled, false on staleness
//   4. fresh updates clear staleness within threshold

#include <gtest/gtest.h>

#include "san_operation_control/sensor_watchdog.hpp"

using namespace san_operation_control;

TEST(SensorWatchdog, YamlDisableHonoredInDevelopment) {
  SensorWatchdog w;
  w.configure(DeploymentMode::DEVELOPMENT, /*yaml_enabled=*/ false);
  EXPECT_FALSE(w.isEnabled());

  w.configure(DeploymentMode::DEVELOPMENT, /*yaml_enabled=*/ true);
  EXPECT_TRUE(w.isEnabled());
}

TEST(SensorWatchdog, YamlDisableIgnoredInProductionDemoLabTestBench) {
  SensorWatchdog w;
  for (auto m : {DeploymentMode::PRODUCTION, DeploymentMode::DEMO,
      DeploymentMode::LAB_TEST, DeploymentMode::BENCH})
  {
    w.configure(m, /*yaml_enabled=*/ false);
    EXPECT_TRUE(w.isEnabled())
      << "watchdog must be force-enabled in mode "
      << toString(m);
  }
}

TEST(SensorWatchdog, DisabledAlwaysReturnsTrue) {
  SensorWatchdog w;
  w.configure(DeploymentMode::DEVELOPMENT, false);
  w.update("imu", 0);
  EXPECT_TRUE(w.checkSensorState(10'000));     // 10 s gap, still ok
}

TEST(SensorWatchdog, StalenessExceedsThresholdReturnsFalse) {
  SensorWatchdog w;
  w.configure(DeploymentMode::PRODUCTION, true);
  w.setStaleThresholdSec(3.0);

  w.update("imu", 1000);         // t = 1 s
  w.update("lidar", 1500);

  EXPECT_TRUE(w.checkSensorState(2000));     // 1 s gap on imu
  EXPECT_TRUE(w.checkSensorState(4000));     // 3 s gap, at threshold
  EXPECT_FALSE(w.checkSensorState(5000))     // 4 s gap on imu - stale
    << "imu is 4 s stale, must return false";
}

TEST(SensorWatchdog, ReportsStaleSensorNames) {
  SensorWatchdog w;
  w.configure(DeploymentMode::PRODUCTION, true);
  w.setStaleThresholdSec(2.0);

  w.update("imu", 1000);
  w.update("lidar", 4000);       // fresh
  auto stale = w.staleSensors(4500);
  EXPECT_EQ(stale.size(), 1u);
  if (!stale.empty()) {EXPECT_EQ(stale[0], "imu");}
}

TEST(SensorWatchdog, NeverUpdatedSensorTreatedAsFresh) {
  // A sensor whose first sample has not yet arrived must not trip
  // the watchdog - otherwise launch ordering would race the first
  // sensor data on every boot.
  SensorWatchdog w;
  w.configure(DeploymentMode::PRODUCTION, true);
  w.setStaleThresholdSec(1.0);
  w.update("imu", 0);            // sentinel = never seen
  EXPECT_TRUE(w.checkSensorState(10'000));
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
