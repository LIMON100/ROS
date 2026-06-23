// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 7 - sensor staleness watchdog.
//
// Tracks the time since each sensor source last produced a message.
// If any tracked source exceeds the configured threshold, checkSensorState()
// returns false so the operation controller can halt motion.
//
// PHASE 7 polish: yaml-controlled `hw_watchdog_enabled` flag lets
// developers run with simulators that don't tick every sensor. The
// flag is IGNORED in production / demo / lab_test / bench - those
// modes always force the watchdog on regardless of yaml.

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "san_operation_control/deployment_mode.hpp"

namespace san_operation_control
{

class SensorWatchdog
{
public:
  SensorWatchdog();

  // Configure with deployment_mode + yaml override. The yaml flag
  // only takes effect in DEVELOPMENT; other modes force-enable.
  void configure(DeploymentMode mode, bool yaml_enabled);

  void setStaleThresholdSec(double secs) {stale_threshold_sec_ = secs;}
  double staleThresholdSec() const {return stale_threshold_sec_;}

  bool isEnabled() const {return enabled_;}

  // Update the most-recent timestamp for a named sensor.
  void update(const std::string & sensor_name, uint64_t now_ms);

  // True when every tracked sensor is fresh (or the watchdog is
  // disabled). Returns false the moment any sensor exceeds the
  // staleness threshold. When disabled this always returns true.
  bool checkSensorState(uint64_t now_ms) const;

  // List of currently-stale sensor names. Useful for log messages
  // and the OperatorAlert payload.
  std::vector<std::string> staleSensors(uint64_t now_ms) const;

private:
  mutable std::mutex mutex_;
  bool enabled_ = true;
  double stale_threshold_sec_ = 3.0;
  std::unordered_map<std::string, uint64_t> last_ms_;
};

}  // namespace san_operation_control
