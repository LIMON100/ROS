#include "sources/AttitudeTrackingSource.h"

#include "TrackValidate.h"

#include <cmath>
#include <cstdint>

#include "riposte/Clock.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"

namespace riposte {

namespace {
constexpr float RAD_TO_DEG = 57.295779513F;
} // namespace

AttitudeTrackingSource::AttitudeTrackingSource(Params p) : p_(p) {
    track_bus_.open(tun::SHM_TRACK, ShmSeqSlot<TrackState>::Role::READER);
}

bool AttitudeTrackingSource::compute(const TelemetrySnapshot& t, uint64_t now_ns,
                                     AttitudeSetpoint& out) {
    // Default to a SAFE hold: keep current heading, level, hover thrust. Used
    // both as the pre-guidance seed and as the one tick sent before disengage.
    out.roll_deg = 0.F;
    out.pitch_deg = 0.F;
    out.yaw_deg = t.yaw_rad * RAD_TO_DEG;
    out.thrust = p_.hover_thrust;

    track_bus_.ensure_open(tun::SHM_TRACK, ShmSeqSlot<TrackState>::Role::READER);
    TrackState ts{};
    // Same process-boundary default-deny as GuidanceSource (CR-03): this path
    // steers attitude from the very same untrusted bus.
    const bool got = track_bus_.read(ts) && validate_track_state(ts, now_ns);
    if (got && ts.quality >= tun::MIN_TRACK_QUALITY) {
        last_track_ = ts; // cache the FULL sample; coast steers on this copy
        last_valid_ = true;
        // Only a DETECTION-anchored sample advances the coast clock (P2-04): a
        // visual-coast (template-only) sample refines the LOS but is not fresh
        // detection evidence, so it must not extend the coast window.
        if (ts.visual_coast == 0U) {
            last_detection_ns_ = ts.mono_ns;
        }
    }

    // Freshness gate (SM-7 in-source half): no fresh valid track -> cannot steer.
    // Measured from the last DETECTION (not the last publish), so template-only
    // coast lives strictly inside the same window a motion coast would
    // (TRACKER-REQ TR-D-b). Inside it an invalid/low-quality bus sample never
    // reaches the geometry below — steering keeps using the cached track.
    const uint64_t age = last_valid_ ? age_ns(now_ns, last_detection_ns_) : UINT64_MAX;
    if (!last_valid_ || age > tun::TRACK_STALE_NS + tun::TRACK_COAST_NS) {
        return false; // controller disengages; out stays a safe hold
    }

    // Rotate the relative target from body FRD to NED with the FULL attitude
    // DCM (aerospace ZYX: yaw*pitch*roll), same construction as GuidanceSource.
    // Yaw-only is wrong exactly in this source's operating regime: it commands
    // a nose-down tracking pitch itself, which would misplace the target by
    // ~sin(pitch)*range in elevation. FRD: x fwd, y right, z down.
    const float cr = std::cos(t.roll_rad);
    const float sr = std::sin(t.roll_rad);
    const float cp = std::cos(t.pitch_rad);
    const float sp = std::sin(t.pitch_rad);
    const float cy = std::cos(t.yaw_rad);
    const float sy = std::sin(t.yaw_rad);
    const float xf = last_track_.rel_pos_frd_m[0];
    const float yr = last_track_.rel_pos_frd_m[1];
    const float zd = last_track_.rel_pos_frd_m[2];
    const float north = (cp * cy * xf) + (((sr * sp * cy) - (cr * sy)) * yr) +
                        (((cr * sp * cy) + (sr * sy)) * zd);
    const float east = (cp * sy * xf) + (((sr * sp * sy) + (cr * cy)) * yr) +
                       (((cr * sp * sy) - (sr * cy)) * zd);
    const float down = (-sp * xf) + (sr * cp * yr) + (cr * cp * zd);
    const float range = std::sqrt((north * north) + (east * east) + (down * down));

    // R-9 line-of-sight centering (RIPOSTE-DUALEO-REQ §6). With no gimbal the
    // camera is body-fixed, so keeping the target centered IS an attitude
    // command: yaw faces the target (horizontal centering), and pitch tracks the
    // target's LOS elevation so it stays vertically centered — PLUS a nose-down
    // tracking bias to accelerate forward. Before this, pitch was a fixed
    // nose-down lean, so a climbing/descending target left the vertical FOV
    // (flight-found in S-G4, the reason R-9 is a prerequisite for the narrow
    // channel whose FOV is far tighter). Gains are flight-tune items.
    out.yaw_deg = std::atan2(east, north) * RAD_TO_DEG;
    const float horiz = std::sqrt((north * north) + (east * east));
    // LOS elevation, NED: down>0 (target below the horizon) => positive, so the
    // nose must pitch DOWN (negative pitch_deg) to point at it.
    const float el_los_deg = std::atan2(down, horiz) * RAD_TO_DEG;
    out.pitch_deg = -el_los_deg - p_.track_pitch_deg; // track LOS + forward lean

    // Thrust bias from target elevation: target above (down<0) -> climb, below
    // -> descend. Normalized by range so the bias is a bounded fraction.
    if (range > 0.5F) {
        const float elev = -down / range; // +1 straight up, -1 straight down
        out.thrust = p_.hover_thrust + (p_.thrust_elev_gain * elev);
    }
    return true;
}

} // namespace riposte
