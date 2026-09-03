#include "sources/GuidanceSource.h"

#include "TargetImm.h"
#include "TrackValidate.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "riposte/Clock.h"
#include "riposte/Log.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"

namespace riposte {

GuidanceSource::GuidanceSource() {
    // READER attaches to the seeker's segment; may not exist yet at startup, so
    // we retry inside compute() via ensure_open().
    track_bus_.open(tun::SHM_TRACK, ShmSeqSlot<TrackState>::Role::READER);
}

// G16.6 deviation: freshness gate + EKF + PN law form one control-tick pipeline (G16.6)
// NOLINTNEXTLINE(readability-function-size)
bool GuidanceSource::compute(const TelemetrySnapshot& t, uint64_t now_ns,
                             VelocitySetpointNed& out) {
    out = VelocitySetpointNed{};
    track_bus_.ensure_open(tun::SHM_TRACK, ShmSeqSlot<TrackState>::Role::READER);

    TrackState ts{};
    // Default-deny at the process boundary (CR-03): a malformed sample counts as
    // "no new sample" — cache and IMM untouched — rather than being folded in.
    // That is how a NaN position used to reach the guidance geometry while the
    // session still reported healthy, since the final clamp zeroed the command
    // but compute() still returned true and SM-7 never saw a reason to fire.
    const bool got = track_bus_.read(ts) && validate_track_state(ts, now_ns);
    if (got && ts.quality >= tun::MIN_TRACK_QUALITY) {
        // Track identity continuity: differencing the unit LOS across two
        // DIFFERENT aircraft would spike the lead term, so a track_id change
        // re-seeds the lead filter before the new target is used.
        if (last_valid_ && ts.track_id != last_track_.track_id) {
            RLOG_WARN("obc", "target switch (track %u -> %u) — PN lead re-seeded",
                      last_track_.track_id, ts.track_id);
            have_prev_los_ = false;
        }
        last_track_ = ts; // cache the FULL sample; coast guides on this copy
        last_valid_ = true;
        // A visual-coast sample refines the LOS (TR-2) but is NOT detection
        // evidence: only a detection-anchored sample advances the freshness
        // clock the coast budget is measured against.
        if (ts.visual_coast == 0U) {
            last_detection_ns_ = ts.mono_ns;
        }

        // EST-P4/P2-07: fold this sample into the moving-target IMM with the own
        // state from telemetry, then overwrite the cached RELATIVE VELOCITY with
        // the blended estimate (own motion removed). Position is left as the
        // seeker reported it — the flight-tuned PN geometry below must not shift.
        TargetImm::OwnState own;
        for (int i = 0; i < 3; ++i) {
            own.pos_ned[i] = t.pos_ned_m[i];
            own.vel_ned[i] = t.vel_ned_mps[i];
        }
        own.roll_rad = t.roll_rad;
        own.pitch_rad = t.pitch_rad;
        own.yaw_rad = t.yaw_rad;
        if (imm_prev_ns_ != 0U && now_ns > imm_prev_ns_) {
            imm_.predict(ns_to_s(now_ns - imm_prev_ns_));
        }
        imm_.update(ts.rel_pos_frd_m, own);
        imm_prev_ns_ = now_ns;
        float rp[3];
        float rv[3];
        imm_.relative_state(own, rp, rv);
        if (imm_.initialized()) {
            last_track_.rel_vel_frd_mps[0] = rv[0];
            last_track_.rel_vel_frd_mps[1] = rv[1];
            last_track_.rel_vel_frd_mps[2] = rv[2];
            // EST-6: degrade quality toward the gate as the IMM position sigma
            // grows (poor range observability, or the two models disagreeing
            // through a maneuver — P2-07), but never below it — keep guiding on
            // the cached track while flagging low confidence. Cached only; the
            // freshness/quality GATE above still uses the seeker's published
            // quality, so this cannot itself trip a disengage.
            const float sig = imm_.position_sigma();
            float scale = (tun::EKF_SIGMA_QUALITY_ZERO_M - sig) /
                          (tun::EKF_SIGMA_QUALITY_ZERO_M - tun::EKF_SIGMA_QUALITY_FULL_M);
            scale = std::clamp(scale, 0.F, 1.F);
            last_track_.quality =
                tun::MIN_TRACK_QUALITY +
                ((last_track_.quality - tun::MIN_TRACK_QUALITY) * scale);
        }
    }

    // Freshness gate (SM-7 in-source half): if we have never had a
    // detection-anchored track, or the newest one is older than the coast
    // budget, we cannot guide. Measuring from last_detection_ns_ (not the
    // publish timestamp) means a stream of fresh template-only samples cannot
    // hold the control session open indefinitely — visual coast lives strictly
    // inside the same window a motion coast does (TRACKER-REQ TR-D-b). Inside
    // the window an invalid/low-quality sample never reaches the geometry
    // below; guidance flies on the cached last-valid track.
    const uint64_t age = last_valid_ ? age_ns(now_ns, last_detection_ns_) : UINT64_MAX;
    if (!last_valid_ || age > tun::TRACK_STALE_NS + tun::TRACK_COAST_NS) {
        have_prev_los_ = false;
        return false; // -> controller disengages
    }

    // Rotate relative target position from body FRD to NED with the FULL attitude
    // DCM (aerospace ZYX: yaw*pitch*roll). Yaw-only is wrong once the vehicle
    // pitches/rolls to track (exactly the attitude-control regime): a nose-down
    // pitch tilts the body-forward axis, redistributing range between N/E and Down.
    // FRD: x fwd, y right, z down.
    const float cr = std::cos(t.roll_rad);
    const float sr = std::sin(t.roll_rad);
    const float cp = std::cos(t.pitch_rad);
    const float sp = std::sin(t.pitch_rad);
    const float cy = std::cos(t.yaw_rad);
    const float sy = std::sin(t.yaw_rad);
    const float xf = last_track_.rel_pos_frd_m[0];
    const float yr = last_track_.rel_pos_frd_m[1];
    const float zd = last_track_.rel_pos_frd_m[2];
    float los_ned[3];
    los_ned[0] = (cp * cy * xf) + (((sr * sp * cy) - (cr * sy)) * yr) +
                 (((cr * sp * cy) + (sr * sy)) * zd); // North
    los_ned[1] = (cp * sy * xf) + (((sr * sp * sy) + (cr * cy)) * yr) +
                 (((cr * sp * sy) - (sr * cy)) * zd);          // East
    los_ned[2] = (-sp * xf) + (sr * cp * yr) + (cr * cp * zd); // Down

    const float rng = std::sqrt((los_ned[0] * los_ned[0]) + (los_ned[1] * los_ned[1]) +
                                (los_ned[2] * los_ned[2]));
    if (rng < 0.5F) {
        // Effectively on target; hold (zero cmd). Drop the lead history too: it
        // is not updated on this tick, so keeping it would difference the next
        // LOS against an arbitrarily old one and spike the lead term when the
        // target opens the range again.
        have_prev_los_ = false;
        return true;
    }

    // Unit LOS.
    const float ux = los_ned[0] / rng;
    const float uy = los_ned[1] / rng;
    const float uz = los_ned[2] / rng;

    // Tracking + lead: command velocity along LOS at a fixed closing speed, plus
    // a lead of N*V*(u - u_prev) applied per control tick — the /dt below and
    // the *dt at the gain cancel, so this is a discrete LOS-delta lead, NOT the
    // rate-based PN term the gain naming suggests. The behavior is test-pinned
    // and flight-validated: do NOT "fix" it to a true LOS-rate term without
    // re-tuning PN_GAIN in flight.
    float vx = ux * tun::ENGAGE_SPEED_MPS;
    float vy = uy * tun::ENGAGE_SPEED_MPS;
    float vz = uz * tun::ENGAGE_SPEED_MPS;

    if (have_prev_los_) {
        const double dt = ns_to_s(tun::CONTROL_PERIOD_NS);
        const float rdx = (ux - prev_los_ned_[0]) / static_cast<float>(dt);
        const float rdy = (uy - prev_los_ned_[1]) / static_cast<float>(dt);
        const float rdz = (uz - prev_los_ned_[2]) / static_cast<float>(dt);
        vx += tun::PN_GAIN * tun::ENGAGE_SPEED_MPS * rdx * static_cast<float>(dt);
        vy += tun::PN_GAIN * tun::ENGAGE_SPEED_MPS * rdy * static_cast<float>(dt);
        vz += tun::PN_GAIN * tun::ENGAGE_SPEED_MPS * rdz * static_cast<float>(dt);
    }
    prev_los_ned_[0] = ux;
    prev_los_ned_[1] = uy;
    prev_los_ned_[2] = uz;
    have_prev_los_ = true;

    out.vn_mps = vx;
    out.ve_mps = vy;
    out.vd_mps = vz;
    out.yaw_rad = std::atan2(los_ned[1], los_ned[0]); // face the target
    return true;
}

} // namespace riposte
