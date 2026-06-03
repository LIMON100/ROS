// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 7 - command_id echo accountability.
//
// Records every SwarmRobotCommand variant the controller observes,
// then exposes last_received_command_id so RobotStatus and
// SwarmHealthSummary can echo the value back to the operator console.
// Lets operators verify replay protection / accountability.

#pragma once

#include <atomic>
#include <cstdint>

namespace san_operation_control
{

class CommandEcho
{
public:
  CommandEcho()
  : last_id_(0), last_ms_(0) {}

  // Record one observed command_id. Idempotent for duplicates.
  void note(uint32_t command_id, uint64_t now_ms)
  {
    last_id_.store(command_id);
    last_ms_.store(now_ms);
  }

  uint32_t lastId() const {return last_id_.load();}
  uint64_t lastMs() const {return last_ms_.load();}

  void reset() {last_id_.store(0); last_ms_.store(0);}

private:
  std::atomic<uint32_t> last_id_;
  std::atomic<uint64_t> last_ms_;
};

}  // namespace san_operation_control
