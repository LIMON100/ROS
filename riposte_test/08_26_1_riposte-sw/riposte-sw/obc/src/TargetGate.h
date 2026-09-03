#pragma once
#include <cmath>
#include <cstdint>

#include "riposte/Tunables.h"
#include "riposte/Types.h"

// Payload gate for an ALREADY-AUTHENTICATED TARGET command, applied before the
// cue is latched as a MissionTarget. Authorization (D-2) proves who sent it;
// this proves the numbers are usable: every float finite and inside the coarse
// physical bounds in Tunables, and the sequence number non-decreasing so a
// stale datagram (replayed or reordered) cannot re-latch an outdated cue.
// Equal sequence numbers pass: the operator CLI defaults to seq=1 for every
// cue, and an equal-seq replay carries the identical payload anyway.

namespace riposte {

// nullptr = acceptable; otherwise a static reason string for the reject log.
inline const char* target_reject_reason(const ObcCommand& cmd, uint32_t last_seq) {
    for (const float v : cmd.target_pos_ned_m) {
        if (!std::isfinite(v) || std::fabs(v) > tun::TARGET_POS_MAX_M) {
            return "position non-finite or out of range";
        }
    }
    for (const float v : cmd.target_vel_ned_mps) {
        if (!std::isfinite(v) || std::fabs(v) > tun::TARGET_VEL_MAX_MPS) {
            return "velocity non-finite or out of range";
        }
    }
    if (!std::isfinite(cmd.target_heading_rad) ||
        std::fabs(cmd.target_heading_rad) > tun::TARGET_HEADING_MAX_RAD) {
        return "heading non-finite or out of range";
    }
    if (cmd.target_seq < last_seq) {
        return "sequence decreased (stale or replayed cue)";
    }
    return nullptr;
}

} // namespace riposte
