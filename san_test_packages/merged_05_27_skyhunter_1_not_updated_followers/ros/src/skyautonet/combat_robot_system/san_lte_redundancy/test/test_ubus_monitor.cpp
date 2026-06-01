// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 2 v2 - ubus monitor unit test.
//
// On a CI host without ubusd, ubus_connect returns NULL and the
// monitor enters "offline mode" - isLteUp() returns the latched flag
// and reloadMwan3Service() reports success. We exercise the public
// API surface that the role manager uses, using injectStatusEventForTest
// to drive the callback path.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <string>
#include <utility>

#include "san_lte_redundancy/mwan3_ubus_monitor.hpp"

class UbusMonitorTest : public ::testing::Test
{
protected:
  rclcpp::Logger logger_ = rclcpp::get_logger("test_ubus");
};

TEST_F(UbusMonitorTest, ConstructsInOfflineMode) {
  san_lte_redundancy::Mwan3UbusMonitor monitor(logger_);
  EXPECT_FALSE(monitor.isLteUp());
}

TEST_F(UbusMonitorTest, ReloadIsSafeOffline) {
  san_lte_redundancy::Mwan3UbusMonitor monitor(logger_);
  // On a non-OpenWrt host this should not throw nor block forever.
  EXPECT_TRUE(monitor.reloadMwan3Service());
}

TEST_F(UbusMonitorTest, InjectedEventDispatchesToCallback) {
  san_lte_redundancy::Mwan3UbusMonitor monitor(logger_);

  std::atomic<bool> received(false);
  std::atomic<bool> last_state(false);
  monitor.onLteStatusChange(
    [&](const std::string & iface, bool is_up) {
      if (iface == "wan_lte") {
        received.store(true);
        last_state.store(is_up);
      }
    });

  monitor.injectStatusEventForTest("wan_lte", true);
  EXPECT_TRUE(received.load());
  EXPECT_TRUE(last_state.load());
  EXPECT_TRUE(monitor.isLteUp());

  monitor.injectStatusEventForTest("wan_lte", false);
  EXPECT_FALSE(monitor.isLteUp());
  EXPECT_FALSE(last_state.load());
}

TEST_F(UbusMonitorTest, NonLteInterfaceEventDoesNotAffectLteFlag) {
  san_lte_redundancy::Mwan3UbusMonitor monitor(logger_);
  monitor.injectStatusEventForTest("wan_lte", true);
  EXPECT_TRUE(monitor.isLteUp());

  // Another interface coming up should not flip wan_lte_up_.
  monitor.injectStatusEventForTest("wan", false);
  EXPECT_TRUE(monitor.isLteUp());
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
