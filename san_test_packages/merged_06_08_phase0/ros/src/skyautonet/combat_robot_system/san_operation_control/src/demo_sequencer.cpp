// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_operation_control/demo_sequencer.hpp"

#include <utility>

namespace san_operation_control
{

const char * demoPhaseName(DemoPhase p)
{
  switch (p) {
    case DemoPhase::STANDBY:   return "STANDBY";
    case DemoPhase::DEPLOY:    return "DEPLOY";
    case DemoPhase::FORMATION: return "FORMATION";
    case DemoPhase::PATROL:    return "PATROL";
    case DemoPhase::ENGAGE:    return "ENGAGE";
    case DemoPhase::RTB:       return "RTB";
  }
  return "?";
}

DemoSequencer::DemoSequencer() = default;

bool DemoSequencer::enableForMode(DeploymentMode mode)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (!demoSequencerEnabled(mode)) {
    enabled_ = false;
    return false;
  }
  enabled_ = true;
  phase_ = DemoPhase::STANDBY;
  phase_started_ms_ = 0;
  return true;
}

bool DemoSequencer::isEnabled() const
{
  // Audit A2 (P2) — moved out-of-line + locked.
  std::lock_guard<std::mutex> lock(mutex_);
  return enabled_;
}

void DemoSequencer::disable()
{
  std::lock_guard<std::mutex> lock(mutex_);
  enabled_ = false;
  phase_ = DemoPhase::STANDBY;
}

void DemoSequencer::tick(uint64_t now_ms)
{
  DemoPhase fire_for = DemoPhase::STANDBY;
  bool fire = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) {return;}
    if (phase_started_ms_ == 0) {
      phase_started_ms_ = now_ms;
      fire_for = phase_;
      fire = true;
    } else {
      const double elapsed_s =
        (now_ms - phase_started_ms_) / 1000.0;
      if (elapsed_s >= phase_duration_sec_) {
        int next = static_cast<int>(phase_) + 1;
        if (next >= kDemoPhaseCount) {
          // Sequence complete - stay in RTB until disabled
          // (lab operator typically tears it down then).
          next = static_cast<int>(DemoPhase::RTB);
        }
        if (next != static_cast<int>(phase_)) {
          phase_ = static_cast<DemoPhase>(next);
          phase_started_ms_ = now_ms;
          fire_for = phase_;
          fire = true;
        }
      }
    }
  }
  if (fire) {fireCallback(fire_for);}
}

DemoPhase DemoSequencer::currentPhase() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return phase_;
}

void DemoSequencer::setPhaseCallback(PhaseCallback cb)
{
  std::lock_guard<std::mutex> lock(mutex_);
  phase_cb_ = std::move(cb);
}

void DemoSequencer::fireCallback(DemoPhase p)
{
  PhaseCallback cb;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = phase_cb_;
  }
  if (cb) {cb(p);}
}

}  // namespace san_operation_control
