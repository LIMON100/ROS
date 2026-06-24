// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 PHASE 8 - shared enums + timing constants for role management.
//
// References:
//   * SAN-SDD-SWARM-001 v1.4 §5.5 (Hub-Deputy redundancy)
//   * SAN-SDD-SWARM-001 v1.4 §5.6 (4-tier Leader succession)
//   * SAN-SDD-SWARM-001 v1.4 §5.7 (Limp Mode)
//   * SAN-IDS-CMD-001 v1.4 §5.15/§5.16

#pragma once

#include <cstdint>

namespace san_role_management
{

enum class LeaderRole : uint8_t
{
  NORMAL    = 0,     // 정상 (this robot is not the active Leader)
  CANDIDATE = 1,     // self-declared candidate (grace-period in progress)
  PROMOTED  = 2,     // succeeded - currently performing Leader duties
  DEMOTED   = 3,     // original Leader recovered; stepped down
};

enum class HubRole : uint8_t
{
  NORMAL    = 0,     // Hub UGV nominal (or non-Hub robot just observing)
  FAILING   = 1,     // Hub heartbeat lagging - Deputy awaiting promotion
  PROMOTED  = 2,     // Deputy took over LTE + SLAM + video relay
  DEMOTED   = 3,     // original Hub recovered; Deputy stepped down
};

// Order matters: smaller value = higher priority. Grace-period sleep
// is computed as `static_cast<int>(priority) * kGraceStepMs` so a
// higher priority self-promotes sooner than a lower priority.
enum class SuccessionPriority : uint8_t
{
  DEPUTY      = 1,     // 1순위: Deputy UGV (S3) - 1st-priority
  HUB         = 2,     // 2순위: Hub UGV (S2)
  BATTERY_MAX = 3,     // 3순위: highest-battery follower
  LIMP_MODE   = 4,     // 4순위: no eligible candidate -> Limp Mode
};

// ----- v1.4 timing constants (spec'd in §5.5 / §5.6 / §5.7) -----

// 200 ms heartbeat × 7 missing samples = 1400 ms.
constexpr int LEADER_HEARTBEAT_TIMEOUT_MS = 1400;

// Hub failure detected if no RobotStatus from Hub UGV in 5 s.
constexpr int HUB_HEARTBEAT_TIMEOUT_MS = 5000;

// Hub+Deputy both timed-out, then a 2 s grace for the Deputy to
// finish takeover. After that we declare Limp Mode.
constexpr int LIMP_MODE_ENTRY_GUARD_MS = 7000;

// Battery thresholds. Deputy / Hub need at least 20% to take the
// Leader role; a follower needs only 10% but the highest-battery
// candidate wins.
constexpr float MIN_BATTERY_FOR_LEADER = 20.0f;
constexpr float MIN_BATTERY_FOLLOWER = 10.0f;

// 200 ms grace-step (Deputy=200 ms, Hub=400 ms, BatteryMax=600 ms).
constexpr int SUCCESSION_GRACE_STEP_MS = 200;

}  // namespace san_role_management
