#include "SafetyMonitor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "riposte/Clock.h"
#include "riposte/Types.h"

namespace riposte {

namespace {
// SM-1 field-level bound over every stream evaluate() consumes besides
// position/velocity (OBC-SDD §6, P0-04): global position, armed, EKF health —
// and ATTITUDE, which is high-rate but is the FRD->NED rotation basis: a
// stalled attitude stream under a live position feed steers through a frozen
// rotation, so the commanded velocity points an increasingly wrong way as the
// ownship yaws, with no other symptom. The loose flag bound never false-fires
// on a healthy high-rate stream; it exists to catch full stalls. (The mode
// stream is judged separately — its staleness is an SM-2 override, not
// SB_FIELD_STALE.)
bool any_flag_stream_stale(const TelemetrySnapshot& t, uint64_t now_ns,
                           uint64_t bound_ns) {
    return age_ns(now_ns, t.gpos_mono_ns) > bound_ns ||
           age_ns(now_ns, t.armed_mono_ns) > bound_ns ||
           age_ns(now_ns, t.health_mono_ns) > bound_ns ||
           age_ns(now_ns, t.att_mono_ns) > bound_ns;
}
} // namespace

void SafetyMonitor::capture_home(const TelemetrySnapshot& t) {
    home_ned_[0] = t.pos_ned_m[0];
    home_ned_[1] = t.pos_ned_m[1];
    home_ned_[2] = t.pos_ned_m[2];
    have_home_ = true;
    jitter_run_ = 0;
    alt_floor_armed_ = false; // see evaluate(): the floor arms after the climb-out
}

SafetyVerdict SafetyMonitor::evaluate(const TelemetrySnapshot& t, ObcState state,
                                      uint64_t now_ns, uint64_t last_period_ns,
                                      uint64_t engage_start_ns, bool source_ok) {
    SafetyVerdict v;
    const bool active = (state == ObcState::OFFBOARD_ACTIVE);

    // SM-1 telemetry freshness (position/velocity stream, the control basis).
    if ((t.connected == 0U) || age_ns(now_ns, t.mono_ns) > lim_.telem_stale_ns) {
        v.violation_mask |= SB_TELEM_STALE;
    }

    // SM-1 field-level freshness (OBC-SDD §6, P0-04): every stream a safety
    // check below reads must ITSELF be current — a live position feed proves
    // nothing about a stopped armed/altitude/attitude stream (the monitored
    // set and the attitude rationale live at any_flag_stream_stale above).
    // Only while ACTIVE: on the bench/READY these streams may legitimately be
    // quiet, and the engage gate re-checks them anyway.
    if (active) {
        if (any_flag_stream_stale(t, now_ns, lim_.telem_flag_stale_ns)) {
            v.violation_mask |= SB_FIELD_STALE;
        }
        // The EKF declaring its local position invalid is not staleness — the
        // data is current and says "do not trust the position". Guiding on it
        // is prohibited outright.
        if (t.position_ok == 0U) {
            v.violation_mask |= SB_POS_INVALID;
        }
    }

    // SM-6 disarm.
    if (t.armed == 0U) {
        v.violation_mask |= SB_DISARMED;
    }

    // SM-2 external mode change (only meaningful once we are supposed to own
    // it). A STALE mode stream is treated as an override: ownership can no
    // longer be verified, and the override path is the one that never fights
    // a pilot (no stop/hold — D-1 hands the vehicle to the PX4 failsafe).
    if (active && ((t.in_offboard == 0U) ||
                   age_ns(now_ns, t.mode_mono_ns) > lim_.telem_flag_stale_ns)) {
        v.violation_mask |= SB_MODE_OVERRIDE;
    }

    // SM-3 soft geofence (relative to home captured at engage).
    if (active && have_home_) {
        const float dn = t.pos_ned_m[0] - home_ned_[0];
        const float de = t.pos_ned_m[1] - home_ned_[1];
        const float r = std::sqrt((dn * dn) + (de * de));
        if (r > lim_.geofence_r) {
            v.violation_mask |= SB_GEOFENCE;
        }
        // The altitude FLOOR only arms once the vehicle has been above it: an
        // control session that starts on the ground (mission takeoff) is at rel_alt 0
        // and would otherwise violate SM-3 on its first active tick, aborting
        // every takeoff. The ceiling has no such latch — it is a hard bound from
        // the first tick. Once armed, the floor stays armed for the control session,
        // so a descent back into the ground is still caught.
        if (t.rel_alt_m >= lim_.alt_min) {
            alt_floor_armed_ = true;
        }
        if (t.rel_alt_m > lim_.alt_max ||
            (alt_floor_armed_ && t.rel_alt_m < lim_.alt_min)) {
            v.violation_mask |= SB_GEOFENCE;
        }
    }

    // SM-10 polygon boundary (the surveyed test range). Checked against the
    // vehicle's local position, same frame the polygon was projected into. The
    // patrol behaviour is supposed to turn away long before this fires — SM-10
    // reaching means the behaviour layer failed, so it ends the control session.
    if (active && fence_.valid() && !fence_.contains(t.pos_ned_m[0], t.pos_ned_m[1])) {
        v.violation_mask |= SB_FENCE_POLY;
    }

    // SM-5 loop jitter (consecutive over-budget periods).
    if (active && last_period_ns > 0) {
        const double err = std::fabs(static_cast<double>(last_period_ns) -
                                     static_cast<double>(lim_.period_ns));
        last_jitter_ms_ = static_cast<float>(err * 1e-6);
        if (err > lim_.jitter_budget_frac * static_cast<double>(lim_.period_ns)) {
            if (++jitter_run_ >= lim_.jitter_max_consec) {
                v.violation_mask |= SB_JITTER;
            }
        } else {
            jitter_run_ = 0;
        }
    }

    // SM-7 track staleness surfaced by the source's inability to compute.
    if (active && !source_ok) {
        v.violation_mask |= SB_TRACK_STALE;
    }

    // SM-8 engage timebox.
    if (active && engage_start_ns > 0 &&
        (now_ns - engage_start_ns) > lim_.engage_timebox_ns) {
        v.violation_mask |= SB_ENGAGE_TIMEBOX;
    }

    // SM-9 battery (in-flight half). Only a KNOWN-low battery disengages:
    // battery telemetry dropping out mid-flight must not abort the control session
    // by itself (PX4's low-battery failsafe remains the hard floor), whereas
    // a battery we can read and know to be low ends it deliberately here.
    // "Known" requires the reading to be FRESH (P0-04): a stale battery_ok=1
    // is a dropout wearing an old flag, and degrades to unknown, not to a
    // minutes-old percentage driving a disengage decision.
    // The preflight half (engage_battery_ok) additionally refuses to launch
    // on an UNKNOWN battery. Threshold 0 disables the check.
    if (active && lim_.bat_land_frac > 0.F && t.battery_ok != 0U &&
        age_ns(now_ns, t.bat_mono_ns) <= lim_.telem_flag_stale_ns &&
        t.battery_frac < lim_.bat_land_frac) {
        v.violation_mask |= SB_BATTERY;
    }

    v.ok = (v.violation_mask == 0);
    return v;
}

namespace {
// A non-finite (NaN/Inf) setpoint must never reach the FC: std::clamp/min/max
// pass NaN straight through (all comparisons are false), so a degenerate source
// (e.g. divide-by-zero range in guidance) would bypass the G4 clamp. Fail safe:
// substitute a benign value before bounding.
float finite_or(float x, float fallback) {
    return std::isfinite(x) ? x : fallback;
}
} // namespace

void SafetyMonitor::clamp(VelocitySetpointNed& sp) const {
    // Fail-safe against non-finite inputs (G3): 0 velocity / current heading.
    sp.vn_mps = finite_or(sp.vn_mps, 0.F);
    sp.ve_mps = finite_or(sp.ve_mps, 0.F);
    sp.vd_mps = finite_or(sp.vd_mps, 0.F);
    sp.yaw_rad = finite_or(sp.yaw_rad, 0.F);
    // Horizontal magnitude clamp (preserve heading), independent vertical clamp.
    const float h = std::sqrt((sp.vn_mps * sp.vn_mps) + (sp.ve_mps * sp.ve_mps));
    if (h > lim_.vmax_h && h > 1e-6F) {
        const float k = lim_.vmax_h / h;
        sp.vn_mps *= k;
        sp.ve_mps *= k;
    }
    sp.vd_mps = std::min(sp.vd_mps, lim_.vmax_v);
    sp.vd_mps = std::max(sp.vd_mps, -lim_.vmax_v);
}

void SafetyMonitor::clamp_attitude(AttitudeSetpoint& a) const {
    // Fail-safe against non-finite inputs (G3): level attitude, minimum thrust.
    a.roll_deg = finite_or(a.roll_deg, 0.F);
    a.pitch_deg = finite_or(a.pitch_deg, 0.F);
    a.yaw_deg = finite_or(a.yaw_deg, 0.F);
    a.thrust = finite_or(a.thrust, lim_.att_min_thrust);
    // Bound tilt so a bad command cannot flip or dive the vehicle, and keep
    // thrust in a safe band (never fully off in offboard). Yaw wraps freely.
    a.roll_deg = std::clamp(a.roll_deg, -lim_.att_max_tilt_deg, lim_.att_max_tilt_deg);
    a.pitch_deg = std::clamp(a.pitch_deg, -lim_.att_max_tilt_deg, lim_.att_max_tilt_deg);
    a.thrust = std::clamp(a.thrust, lim_.att_min_thrust, lim_.att_max_thrust);
}

bool validate(const SafetyMonitor::Limits& l, std::string& err) {
    if (!(l.vmax_h > 0.F) || !(l.vmax_v > 0.F)) {
        err = "safety.vmax_h/vmax_v must be > 0";
        return false;
    }
    if (!(l.geofence_r > 0.F)) {
        err = "safety.geofence_r must be > 0";
        return false;
    }
    if (l.alt_min < 0.F) {
        err = "safety.alt_min must be >= 0";
        return false;
    }
    if (!(l.alt_max > l.alt_min)) {
        err = "safety.alt_max must be > alt_min";
        return false;
    }
    if (l.engage_timebox_ns == 0) {
        err = "safety.engage_timebox_s must be > 0";
        return false;
    }
    // The field-level bound guards 1 Hz-class streams; tighter than the
    // position bound would false-fire on every healthy heartbeat gap (P0-04).
    if (l.telem_flag_stale_ns < l.telem_stale_ns || l.telem_flag_stale_ns == 0) {
        err = "safety.telem_flag_stale_s must be >= telemetry stale bound and > 0";
        return false;
    }
    if (!(l.att_max_tilt_deg > 0.F) || l.att_max_tilt_deg >= 90.F) {
        err = "safety.att_max_tilt_deg must be in (0, 90)";
        return false;
    }
    if (l.att_min_thrust < 0.F || l.att_min_thrust > 1.F || l.att_max_thrust < 0.F ||
        l.att_max_thrust > 1.F) {
        err = "safety.att_min_thrust/att_max_thrust must be in [0, 1]";
        return false;
    }
    if (!(l.att_max_thrust > l.att_min_thrust)) {
        err = "safety.att_max_thrust must be > att_min_thrust";
        return false;
    }
    // Battery fractions: 0 disables the check (SM-9); otherwise must be in [0,1].
    if (l.bat_engage_min_frac < 0.F || l.bat_engage_min_frac > 1.F ||
        l.bat_land_frac < 0.F || l.bat_land_frac > 1.F) {
        err = "safety.bat_engage_min_frac/bat_land_frac must be in [0, 1]";
        return false;
    }
    // When both are enabled, the land threshold must sit BELOW the engage
    // threshold — otherwise the vehicle would refuse to engage on a battery it
    // considers flyable, or land before it would refuse to engage. (SM-9)
    if (l.bat_engage_min_frac > 0.F && l.bat_land_frac > 0.F &&
        !(l.bat_land_frac < l.bat_engage_min_frac)) {
        err = "safety.bat_land_frac must be < bat_engage_min_frac";
        return false;
    }
    err.clear();
    return true;
}

} // namespace riposte
