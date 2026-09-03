#include "TargetEstimator.h"

#include "Tracker.h"

#include <cmath>
#include <cstdint>

#include "riposte/Tunables.h"
#include "riposte/Types.h"

namespace riposte {

bool TargetEstimator::estimate(const Tracker::Track& trk, uint64_t frame_mono_ns,
                               double dt_s, TrackState& out) {
    out = TrackState{};
    out.mono_ns = frame_mono_ns;

    if (!trk.valid || trk.quality < tun::MIN_TRACK_QUALITY || trk.size <= 1e-4F) {
        have_prev_ = false;
        out.valid = 0;
        return false;
    }

    // Line-of-sight angles from normalized center offset (origin = image center).
    const float vfov = p_.hfov_rad / p_.aspect;
    const float az = (trk.cx - 0.5F) * p_.hfov_rad; // right positive -> +Y (FRD)
    const float el = (trk.cy - 0.5F) * vfov;        // down positive  -> +Z (FRD)

    // Range from apparent size. focal (in normalized-width units) = 0.5/tan(hfov/2).
    const float focal_norm = 0.5F / std::tan(0.5F * p_.hfov_rad);
    const float range = (p_.target_size_m * focal_norm) / trk.size;

    // Body FRD: x forward, y right, z down. Small-angle-safe with trig.
    const float x = range * std::cos(el) * std::cos(az);
    const float y = range * std::cos(el) * std::sin(az);
    const float z = range * std::sin(el);

    out.rel_pos_frd_m[0] = x;
    out.rel_pos_frd_m[1] = y;
    out.rel_pos_frd_m[2] = z;

    // Finite-difference velocity is only meaningful against the SAME track: a
    // primary handoff (id change) would difference two different targets and
    // spike rel_vel for one tick. On id change publish zero and reseed.
    if (have_prev_ && trk.id == prev_id_ && dt_s > 1e-3) {
        const float inv = static_cast<float>(1.0 / dt_s);
        out.rel_vel_frd_mps[0] = (x - prev_pos_[0]) * inv;
        out.rel_vel_frd_mps[1] = (y - prev_pos_[1]) * inv;
        out.rel_vel_frd_mps[2] = (z - prev_pos_[2]) * inv;
    }
    prev_pos_[0] = x;
    prev_pos_[1] = y;
    prev_pos_[2] = z;
    prev_ns_ = frame_mono_ns;
    prev_id_ = trk.id;
    have_prev_ = true;

    out.track_id = trk.id;
    out.quality = trk.quality;
    out.valid = 1;
    return true;
}

} // namespace riposte
