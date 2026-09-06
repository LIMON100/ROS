#pragma once
#include <cstdint>

namespace riposte {

// Lead-rendezvous geometry for the GCS target cue (RIPOSTE-DUALEO-REQ-001 R-2).
//
// The GCS reports where a target is and how it is moving; flying at where it
// WAS wastes the whole transit, because by arrival the target has moved. This
// solves for the point where the ownship and the target arrive together, so the
// vehicle transits to a position from which the seeker can acquire rather than
// trailing behind.
//
// Pure geometry: no FC, no buses, no clock of its own — the caller passes the
// current time so the cue can be aged forward. That keeps it host-testable.

struct RendezvousCue {
    float pos_ned_m[3] = {0.F, 0.F, 0.F};   // target position when the cue was taken
    float vel_ned_mps[3] = {0.F, 0.F, 0.F}; // target velocity, NED (all zero = static)
    uint64_t mono_ns = 0;                   // when the cue was taken
};

struct RendezvousResult {
    // True when a genuine lead solution exists. False means the target cannot be
    // caught at this speed on this geometry (it is faster and opening); the
    // point is then the target's CURRENT extrapolated position — a direct-tracking
    // fallback, which is the best available answer and still flies the right way.
    bool ok = false;
    float point_ned_m[3] = {0.F, 0.F, 0.F};
    float t_go_s = 0.F; // predicted time to the meeting point
    float lead_m = 0.F; // distance between the aim point and the target's
                        // current position — 0 for a static cue
};

// Longest lead the solver will fly to. A cue that predicts minutes ahead is
// extrapolating a constant-velocity assumption far past where it is credible;
// beyond this the solution is clamped and reported as not-ok, so the vehicle
// heads toward the target rather than toward a fantasy.
inline constexpr float RENDEZVOUS_MAX_TGO_S = 120.F;

// Solves |P_target(t) - own| = speed * t for the earliest positive t, where the
// target moves at constant velocity from its cue position aged to `now_ns`.
//
// speed_mps must be > 0. A cue with zero velocity degenerates to "fly at the
// target", which is exactly right for a static cue.
RendezvousResult solve_rendezvous(const float own_ned_m[3], const RendezvousCue& cue,
                                  float speed_mps, uint64_t now_ns);

} // namespace riposte
