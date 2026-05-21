// SAN v1.3 PHASE 7 - DEMO 6-phase sequencer.
//
// Drives the public-demo / lab-HIL choreography:
//   1. STANDBY      - all robots idle, awaiting operator
//   2. DEPLOY        - column-of-twos exit from staging
//   3. FORMATION     - assume tactical wedge
//   4. PATROL        - sweep nominal route
//   5. ENGAGE        - simulated weapons engage on planted target
//   6. RTB           - return to base, dock
//
// Transitions are driven on a fixed cadence (default 10 s per phase)
// once enabled. The sequencer is enabled only when the resolved
// deployment_mode is DEMO or LAB_TEST (PHASE 7 widening).

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

#include "san_operation_control/deployment_mode.hpp"

namespace san_operation_control {

enum class DemoPhase : uint8_t {
    STANDBY    = 0,
    DEPLOY     = 1,
    FORMATION  = 2,
    PATROL     = 3,
    ENGAGE     = 4,
    RTB        = 5,
};

constexpr int kDemoPhaseCount = 6;

const char* demoPhaseName(DemoPhase p);

class DemoSequencer {
public:
    using PhaseCallback = std::function<void(DemoPhase)>;

    DemoSequencer();

    // Honor the deployment_mode gate. Returns true if enabled, false
    // when the mode does not support DEMO (e.g. PRODUCTION). Once
    // enabled, the caller is expected to tick() on a 1 Hz watchdog.
    bool enableForMode(DeploymentMode mode);

    void disable();
    bool isEnabled() const { return enabled_; }

    // Phase-transition cadence. Defaults to 10 s; tests can shorten.
    void setPhaseDurationSec(double secs) { phase_duration_sec_ = secs; }

    // Drive the sequencer forward. `now_ms` is wall-clock or rclcpp
    // time as the caller prefers; only the delta matters.
    void tick(uint64_t now_ms);

    DemoPhase currentPhase() const;

    void setPhaseCallback(PhaseCallback cb);

private:
    mutable std::mutex mutex_;
    bool      enabled_ = false;
    DemoPhase phase_ = DemoPhase::STANDBY;
    uint64_t  phase_started_ms_ = 0;
    double    phase_duration_sec_ = 10.0;
    PhaseCallback phase_cb_;

    void fireCallback(DemoPhase p);
};

}  // namespace san_operation_control
