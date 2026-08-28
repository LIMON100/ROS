#pragma once
#include "Geofence.h"

#include <cstdint>
#include <string>

#include "riposte/Clock.h"
#include "riposte/Types.h"

namespace riposte {

struct SafetyVerdict {
    bool ok = true;
    uint32_t violation_mask = 0;
};

// Evaluated every control tick. Any violation while OFFBOARD_ACTIVE triggers
// DISENGAGING. clamp() is the final defensive line (G4/SM-4) and is always the
// last thing applied to a setpoint before it is sent, regardless of source.
class SafetyMonitor {
public:
    struct Limits {
        float vmax_h = 0.F, vmax_v = 0.F;
        float geofence_r = 0.F, alt_min = 0.F, alt_max = 0.F;
        // Attitude-control clamps (used only in attitude mode).
        float att_max_tilt_deg = 35.F;
        float att_min_thrust = 0.10F, att_max_thrust = 0.80F;
        uint64_t telem_stale_ns = 0;
        // SM-1 field-level bound for the low-rate streams (mode/armed/global
        // position/EKF health/battery, mostly 1 Hz heartbeat derived). Must be
        // >= telem_stale_ns; see OBC-SDD §6 (P0-04).
        uint64_t telem_flag_stale_ns = 0;
        uint64_t engage_timebox_ns = 0;
        double jitter_budget_frac = 0.2;
        int jitter_max_consec = 3;
        uint64_t period_ns = 50'000'000;
        // Battery gate (SM-9): remaining-capacity fractions 0..1. 0 disables
        // the respective check (defaults come from Tunables via config).
        float bat_engage_min_frac = 0.F;
        float bat_land_frac = 0.F;
    };

    void configure(const Limits& l) { lim_ = l; }

    // READY-side battery engage gate (the preflight half of SM-9). Pure and
    // static so test_safety_fsm pins it without a controller. Policy: with the
    // gate enabled, UNKNOWN battery blocks engage — the vehicle must not
    // launch on a battery it cannot read, and a STALE reading is the same as
    // unreadable (P0-04: a battery_ok that latched true minutes ago proves
    // nothing). In flight, unknown battery is NOT a violation (see evaluate);
    // PX4's own low-battery failsafe stays the hard floor there.
    static bool engage_battery_ok(const Limits& l, const TelemetrySnapshot& t,
                                  uint64_t now_ns) {
        if (l.bat_engage_min_frac <= 0.F) {
            return true; // gate disabled by configuration
        }
        return (t.battery_ok != 0U) &&
               age_ns(now_ns, t.bat_mono_ns) <= l.telem_flag_stale_ns &&
               (t.battery_frac >= l.bat_engage_min_frac);
    }

    // Called once at engage to freeze the geofence origin.
    void capture_home(const TelemetrySnapshot& t);

    // SM-10 polygon boundary (RIPOSTE-DUALEO-REQ-001 T-1). Independent of, and
    // additional to, the SM-3 radius: the radius is the general operating limit,
    // the polygon is the surveyed test range. An unconfigured fence is not
    // checked. Set before engage; the polygon is already in local NED.
    void set_fence(const Geofence& fence) { fence_ = fence; }
    const Geofence& fence() const { return fence_; }

    // now_ns: current tick time. last_period_ns: measured interval since prior
    // tick (for jitter). source_ok: whether the setpoint source could compute.
    SafetyVerdict evaluate(const TelemetrySnapshot& t, ObcState state, uint64_t now_ns,
                           uint64_t last_period_ns, uint64_t engage_start_ns,
                           bool source_ok);

    void clamp(VelocitySetpointNed& sp) const;      // SM-4, always last (velocity)
    void clamp_attitude(AttitudeSetpoint& a) const; // SM-4, always last (attitude)

    float last_jitter_ms() const { return last_jitter_ms_; }

private:
    Limits lim_;
    Geofence fence_; // SM-10; empty = not checked
    float home_ned_[3] = {0, 0, 0};
    bool have_home_ = false;
    // SM-3 altitude floor arms after the vehicle first reaches alt_min (see
    // evaluate()); cleared per control session by capture_home().
    bool alt_floor_armed_ = false;
    int jitter_run_ = 0;
    float last_jitter_ms_ = 0.F;
};

// ---- Offboard-transition gate predicates (OBC-SDD §4, P1-02/P1-03) ----
// Pure so test_safety_fsm pins them without a controller/FC.

// A FRESH flight-mode sample says the FC is NOT in Offboard. Used for two
// decisions:
//  - P1-03: DISENGAGING may report READY only once this CONFIRMS the FC
//    actually left Offboard (a stop_offboard() whose result was ignored once
//    let the OBC report READY while PX4 was still in Offboard on a dead
//    stream).
//  - P1-02: at the PRESTREAM confirm TIMEOUT, this distinguishes "the pilot
//    holds another mode" (no-command exit) from "we never heard back" (a link
//    problem, commanded exit).
// It deliberately does NOT look at whether the sample post-dates the offboard
// start ack: PX4's ~1 Hz heartbeat can deliver a sample stamped after the ack
// that still carries the PRE-ack mode, and treating that lag as a takeover
// aborted every legitimate engage (caught in PX4 SITL, SM-3 scenario).
inline bool mode_reports_non_offboard(const TelemetrySnapshot& t, uint64_t now_ns,
                                      uint64_t flag_stale_ns) {
    return age_ns(now_ns, t.mode_mono_ns) <= flag_stale_ns && (t.in_offboard == 0U);
}

// SM-10 engage precondition (review CR-04). Pure so test_safety_fsm pins it.
//
// A CONFIGURED polygon is an operator decision that this flight happens inside
// that boundary, and projecting it into local NED needs a GPS fix that belongs
// to THIS instant (gps_ok alone latches true after the first fix ever seen).
// The previous policy started the session with SM-10 silently disabled when the
// fix was missing or stale — removing an explicitly configured hard limit
// because a stream faulted. Refusing to engage keeps the operator's decision
// intact; a profile with no polygon is unaffected.
inline bool fence_projectable(bool polygon_configured, const TelemetrySnapshot& t,
                              uint64_t now_ns, uint64_t flag_stale_ns) {
    return !polygon_configured ||
           ((t.gps_ok != 0U) && age_ns(now_ns, t.gpos_mono_ns) <= flag_stale_ns);
}

// AUTO_LANDING disarm authorization (review CR-01). Pure so test_safety_fsm can
// pin it without a controller or an FC.
//
// Cutting the motors is the least reversible thing this software does, so every
// field the decision rests on must be proved fresh by ITS OWN stream stamp —
// not by t.mono_ns, which covers only position/velocity. The failure this
// closes: position/velocity keep flowing while global-position freezes at a low
// altitude, and that frozen "0.3 m" keeps authorizing a disarm on a vehicle
// that has since climbed away.
//
// `source_requests` is the setpoint source's hint (it compares rel_alt against a
// CONFIGURED land altitude); it is additionally bounded by disarm_max_alt_m so a
// mistyped land_alt_m cannot cut motors in the air.
inline bool disarm_authorized(const TelemetrySnapshot& t, uint64_t now_ns,
                              uint64_t flag_stale_ns, bool source_requests,
                              float disarm_max_alt_m) {
    // Acting on a stale armed flag would command a disarm from a belief about
    // the vehicle rather than an observation of it.
    if (age_ns(now_ns, t.armed_mono_ns) > flag_stale_ns || t.armed == 0U) {
        return false;
    }
    const bool by_landed =
        (t.landed != 0U) && age_ns(now_ns, t.landed_mono_ns) <= flag_stale_ns;
    const bool by_source = source_requests &&
                           age_ns(now_ns, t.gpos_mono_ns) <= flag_stale_ns &&
                           (t.rel_alt_m <= disarm_max_alt_m);
    return by_landed || by_source;
}

// AUTO_LANDING may report completion only on an OBSERVED disarm: a stale armed
// flag is not evidence, and ending the landing watch on it would leave a
// possibly still-armed vehicle unwatched (CR-01).
inline bool landing_complete(const TelemetrySnapshot& t, uint64_t now_ns,
                             uint64_t flag_stale_ns) {
    return age_ns(now_ns, t.armed_mono_ns) <= flag_stale_ns && (t.armed == 0U);
}

// Rejects an out-of-range safety configuration at startup (AGENTS §7.9): flight
// safety limits are the last thing that should run on a bad value. Returns true
// if every limit is sane; otherwise false with `err` set to the first specific
// reason. A negative geofence/altitude/speed, an inverted alt or thrust band, a
// tilt outside (0,90), or a battery land-threshold not below the engage
// threshold would each silently defeat the protection they exist for. Battery
// fractions of 0 are allowed — that means "check disabled" by design (SM-9).
bool validate(const SafetyMonitor::Limits& l, std::string& err);

} // namespace riposte
