#pragma once
#include <cmath>
#include <cstdint>

#include "riposte/Tunables.h"
#include "riposte/Types.h"

namespace riposte {

// Validation of a TrackState read off the shared-memory TrackBus (review CR-03).
//
// The bus crosses a PROCESS boundary: riposte-seeker writes it, riposte-obc
// reads it and steers on it. Everything the OBC knew about the sample was
// `valid` and `quality`, so a NaN position from a bad detection, a corrupted
// frame or a seeker defect went straight into the IMM and out through the
// guidance geometry. The final setpoint clamp turns the resulting non-finite
// command into zero, but compute() still reports success, so source_ok stays
// true and SM-7 never fires — the session looks healthy while the estimator
// carries poison that survives into later samples.
//
// So the rule at this boundary is default-deny: a sample is used only if every
// field it carries makes physical sense. A rejected sample is treated exactly
// like "no new sample" (the cache and the IMM are left untouched), which is a
// state the coast logic already handles.
//
// Pure and header-only so test_guidance pins it without a bus or a seeker.

// Widest relative position the geometry is meaningful over. The seeker's own
// acquisition range is far shorter (REQ-001: ~300 m for the narrow channel);
// this is a sanity ceiling, not a mission limit.
inline constexpr float TRACK_MAX_REL_POS_M = 20'000.F;
// Relative speed ceiling. Two aircraft closing at more than this are not a
// tracking problem the guidance loop can solve, and a value above it is far
// more likely to be corruption than a real closure rate.
inline constexpr float TRACK_MAX_REL_VEL_MPS = 400.F;

inline bool track_vec_ok(const float v[3], float limit) {
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(v[i]) || std::fabs(v[i]) > limit) {
            return false;
        }
    }
    return true;
}

// True when `ts` may be consumed. `now_ns` is the reader's monotonic clock;
// both clocks come from the same host so a sample stamped in the FUTURE means a
// corrupt or mismatched writer, not clock skew.
inline bool validate_track_state(const TrackState& ts, uint64_t now_ns) {
    if (ts.valid == 0U) {
        return false; // "no target" is well-formed, just not usable
    }
    if (ts.mono_ns == 0U || ts.mono_ns > now_ns) {
        return false;
    }
    if (!std::isfinite(ts.quality) || ts.quality < 0.F || ts.quality > 1.F) {
        return false;
    }
    if (!track_vec_ok(ts.rel_pos_frd_m, TRACK_MAX_REL_POS_M) ||
        !track_vec_ok(ts.rel_vel_frd_mps, TRACK_MAX_REL_VEL_MPS)) {
        return false;
    }
    // Enumerated bytes: anything outside the defined set means the writer and
    // this reader disagree about the layout, which invalidates the whole sample.
    if (ts.visual_coast > 1U) {
        return false;
    }
    if (ts.num_targets > tun::TRACKER_MAX_TRACKS) {
        return false;
    }
    return true;
}

} // namespace riposte
