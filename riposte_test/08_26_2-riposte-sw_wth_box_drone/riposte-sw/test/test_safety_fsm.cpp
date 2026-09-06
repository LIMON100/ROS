// SafetyMonitor unit tests. Replaces the bring-up stub with real coverage,
// added together with SM-9 (battery gate): the battery policy has two halves
// with deliberately different unknown-battery behavior, and only a test pins
// that asymmetry:
//   - engage (READY):  UNKNOWN battery blocks engage  (don't launch blind)
//   - in flight:       UNKNOWN battery is NOT a violation (PX4 failsafe owns
//                      the hard floor; a telemetry dropout must not abort)
// Also pins the basic SM-1/SM-3/SM-6/SM-8 bits so the evaluate() contract has
// regression coverage at all (the previous file was `int main(){return 0;}`).
#include "OffboardController.h"
#include "SafetyMonitor.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "riposte/Tunables.h"
#include "riposte/Types.h"
#include "sources/TestAttitudeSource.h"

using namespace riposte;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
int checks = 0;

#define CHECK(c)                                                    \
    do {                                                            \
        ++checks;                                                   \
        if (!(c)) {                                                 \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            return 1;                                               \
        }                                                           \
    } while (0)

constexpr uint64_t NOW_NS = 10'000'000'000ULL; // arbitrary monotonic instant

SafetyMonitor::Limits nominal_limits() {
    SafetyMonitor::Limits l;
    l.vmax_h = 8.F;
    l.vmax_v = 3.F;
    l.geofence_r = 150.F;
    l.alt_min = 2.F;
    l.alt_max = 60.F;
    l.telem_stale_ns = 500'000'000ULL;
    l.telem_flag_stale_ns = 3'000'000'000ULL;
    l.engage_timebox_ns = 60'000'000'000ULL;
    l.bat_engage_min_frac = 0.30F;
    l.bat_land_frac = 0.20F;
    return l;
}

// A snapshot that violates nothing under nominal_limits() while engaged.
TelemetrySnapshot healthy_snapshot() {
    TelemetrySnapshot t{};
    t.stamp_all_streams(NOW_NS); // every stream fresh (field-level SM-1)
    t.rel_alt_m = 10.F;
    t.armed = 1;
    t.in_offboard = 1;
    t.position_ok = 1;
    t.connected = 1;
    t.battery_frac = 0.90F;
    t.battery_ok = 1;
    return t;
}

SafetyVerdict eval_active(SafetyMonitor& sm, const TelemetrySnapshot& t) {
    // last_period=0 keeps SM-5 out; engage 1 s ago keeps SM-8 out.
    return sm.evaluate(t, ObcState::OFFBOARD_ACTIVE, NOW_NS,
                       /*last_period_ns=*/0,
                       /*engage_start_ns=*/NOW_NS - 1'000'000'000ULL,
                       /*source_ok=*/true);
}

int test_healthy_snapshot_is_clean() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    const TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    const SafetyVerdict v = eval_active(sm, t);
    CHECK(v.ok);
    CHECK(v.violation_mask == 0U);
    return 0;
}

// ---- SM-9 in-flight half -------------------------------------------------

int test_low_battery_disengages_in_flight() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.battery_frac = 0.19F; // below bat_land_frac = 0.20
    const SafetyVerdict v = eval_active(sm, t);
    CHECK(!v.ok);
    CHECK((v.violation_mask & SB_BATTERY) != 0U);
    return 0;
}

int test_battery_exactly_at_threshold_is_ok() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.battery_frac = 0.20F; // == bat_land_frac: strict '<' keeps it flying
    CHECK(eval_active(sm, t).ok);
    return 0;
}

int test_unknown_battery_is_not_a_flight_violation() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.battery_ok = 0;      // telemetry dropout mid-flight
    t.battery_frac = 0.0F; // value must be ignored while battery_ok == 0
    CHECK(eval_active(sm, t).ok);
    return 0;
}

int test_zero_threshold_disables_flight_check() {
    SafetyMonitor sm;
    SafetyMonitor::Limits l = nominal_limits();
    l.bat_land_frac = 0.F;
    sm.configure(l);
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.battery_frac = 0.01F;
    CHECK(eval_active(sm, t).ok);
    return 0;
}

int test_low_battery_ignored_when_not_engaged() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    t.in_offboard = 0;
    t.battery_frac = 0.05F;
    const SafetyVerdict v = sm.evaluate(t, ObcState::READY, NOW_NS, 0, 0, true);
    CHECK((v.violation_mask & SB_BATTERY) == 0U);
    return 0;
}

// ---- SM-9 engage (preflight) half ---------------------------------------

int test_engage_gate_accepts_healthy_battery() {
    CHECK(SafetyMonitor::engage_battery_ok(nominal_limits(), healthy_snapshot(), NOW_NS));
    return 0;
}

int test_engage_gate_rejects_low_battery() {
    TelemetrySnapshot t = healthy_snapshot();
    t.battery_frac = 0.29F; // below bat_engage_min_frac = 0.30
    CHECK(!SafetyMonitor::engage_battery_ok(nominal_limits(), t, NOW_NS));
    return 0;
}

int test_engage_gate_rejects_unknown_battery() {
    TelemetrySnapshot t = healthy_snapshot();
    t.battery_ok = 0;
    t.battery_frac = 1.0F; // stale value must not rescue an unreadable battery
    CHECK(!SafetyMonitor::engage_battery_ok(nominal_limits(), t, NOW_NS));
    return 0;
}

int test_engage_gate_disabled_by_zero_threshold() {
    SafetyMonitor::Limits l = nominal_limits();
    l.bat_engage_min_frac = 0.F;
    TelemetrySnapshot t = healthy_snapshot();
    t.battery_ok = 0; // even unknown passes when the gate is off
    CHECK(SafetyMonitor::engage_battery_ok(l, t, NOW_NS));
    return 0;
}

int test_engage_threshold_is_inclusive() {
    TelemetrySnapshot t = healthy_snapshot();
    t.battery_frac = 0.30F; // == bat_engage_min_frac: '>=' admits it
    CHECK(SafetyMonitor::engage_battery_ok(nominal_limits(), t, NOW_NS));
    return 0;
}

// ---- SM-1 field-level freshness (OBC-SDD §6, P0-04) ----------------------
// One shared stamp once let a live position feed hide stopped mode / armed /
// altitude / battery streams; these pin the per-stream checks.

int test_stale_mode_stream_is_treated_as_override() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.mode_mono_ns = NOW_NS - 4'000'000'000ULL; // > telem_flag_stale_ns
    const SafetyVerdict v = eval_active(sm, t);
    // Ownership can no longer be verified -> the no-command override path.
    CHECK((v.violation_mask & SB_MODE_OVERRIDE) != 0U);
    return 0;
}

int test_stale_gpos_stream_sets_field_stale() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.gpos_mono_ns = NOW_NS - 4'000'000'000ULL; // rel_alt no longer trustworthy
    CHECK((eval_active(sm, t).violation_mask & SB_FIELD_STALE) != 0U);
    return 0;
}

int test_stale_armed_stream_sets_field_stale() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.armed_mono_ns = NOW_NS - 4'000'000'000ULL;
    CHECK((eval_active(sm, t).violation_mask & SB_FIELD_STALE) != 0U);
    return 0;
}

int test_stale_health_stream_sets_field_stale() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.health_mono_ns = NOW_NS - 4'000'000'000ULL;
    CHECK((eval_active(sm, t).violation_mask & SB_FIELD_STALE) != 0U);
    return 0;
}

int test_stale_attitude_stream_sets_field_stale() {
    // The attitude stream is the FRD->NED rotation basis: position can keep
    // flowing while attitude freezes, and guidance then rotates every track
    // vector through a frozen roll/pitch/yaw — increasingly wrong commands
    // with no other symptom. The stream needs its own field-level check.
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.att_mono_ns = NOW_NS - 4'000'000'000ULL;
    const SafetyVerdict v = eval_active(sm, t);
    CHECK((v.violation_mask & SB_FIELD_STALE) != 0U);
    CHECK(!v.ok);
    return 0;
}

int test_position_invalid_sets_pos_invalid() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.position_ok = 0; // EKF says: do not trust the local position
    const SafetyVerdict v = eval_active(sm, t);
    CHECK((v.violation_mask & SB_POS_INVALID) != 0U);
    CHECK(!v.ok);
    return 0;
}

int test_field_staleness_not_checked_when_not_active() {
    // On the bench/READY the low-rate streams may legitimately be quiet; the
    // engage gate re-verifies them before anything flies.
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    t.mode_mono_ns = 0;
    t.gpos_mono_ns = 0;
    t.armed_mono_ns = 0;
    t.health_mono_ns = 0;
    const SafetyVerdict v = sm.evaluate(t, ObcState::READY, NOW_NS, 0, 0, true);
    CHECK((v.violation_mask & (SB_FIELD_STALE | SB_MODE_OVERRIDE | SB_POS_INVALID)) ==
          0U);
    return 0;
}

int test_stale_battery_in_flight_degrades_to_unknown() {
    // battery_ok=1 with an old stamp is a dropout wearing an old flag: SM-9
    // must treat it as unknown (no violation), not act on the stale fraction.
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.battery_frac = 0.05F; // would be a violation if the reading were fresh
    t.bat_mono_ns = NOW_NS - 4'000'000'000ULL;
    CHECK((eval_active(sm, t).violation_mask & SB_BATTERY) == 0U);
    return 0;
}

int test_engage_gate_rejects_stale_battery() {
    TelemetrySnapshot t = healthy_snapshot();
    t.bat_mono_ns = NOW_NS - 4'000'000'000ULL; // stale == unreadable at engage
    CHECK(!SafetyMonitor::engage_battery_ok(nominal_limits(), t, NOW_NS));
    return 0;
}

// ---- offboard transition gates (OBC-SDD §4, P1-02/P1-03) ------------------

// P1-02 (revised after PX4 SITL): the PRESTREAM override judgment is made ONLY
// at the confirm timeout, and only from a FRESH non-offboard mode sample. The
// earlier "sample stamped after the ack" rule false-fired on PX4's ~1 Hz
// heartbeat still carrying the pre-ack mode and aborted every legitimate
// engage, so the predicate deliberately ignores the ack timestamp.
int test_mode_reports_non_offboard_predicate() {
    const uint64_t flag_stale = 3'000'000'000ULL;
    TelemetrySnapshot t = healthy_snapshot();
    t.in_offboard = 0;
    t.mode_mono_ns = NOW_NS; // fresh, says another mode
    CHECK(mode_reports_non_offboard(t, NOW_NS, flag_stale));
    t.mode_mono_ns = NOW_NS - 4'000'000'000ULL; // stale: proves nothing
    CHECK(!mode_reports_non_offboard(t, NOW_NS, flag_stale));
    t.mode_mono_ns = NOW_NS;
    t.in_offboard = 1; // fresh and IN offboard
    CHECK(!mode_reports_non_offboard(t, NOW_NS, flag_stale));
    return 0;
}

int test_offboard_exit_confirmed_requires_fresh_non_offboard() {
    const uint64_t flag_stale = 3'000'000'000ULL;
    TelemetrySnapshot t = healthy_snapshot();
    t.in_offboard = 0;
    t.mode_mono_ns = NOW_NS;
    CHECK(mode_reports_non_offboard(t, NOW_NS, flag_stale));
    t.mode_mono_ns = NOW_NS - 4'000'000'000ULL; // stale: cannot confirm
    CHECK(!mode_reports_non_offboard(t, NOW_NS, flag_stale));
    t.mode_mono_ns = NOW_NS;
    t.in_offboard = 1; // still in offboard: definitely not confirmed
    CHECK(!mode_reports_non_offboard(t, NOW_NS, flag_stale));
    return 0;
}

// ---- regression pins for the pre-existing bits ---------------------------

int test_stale_telemetry_sets_sm1() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.mono_ns = NOW_NS - 600'000'000ULL; // older than telem_stale_ns
    const SafetyVerdict v = eval_active(sm, t);
    CHECK((v.violation_mask & SB_TELEM_STALE) != 0U);
    return 0;
}

int test_disarm_sets_sm6() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.armed = 0;
    CHECK((eval_active(sm, t).violation_mask & SB_DISARMED) != 0U);
    return 0;
}

int test_geofence_radius_sets_sm3() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t); // home at origin
    t.pos_ned_m[0] = 151.F;
    CHECK((eval_active(sm, t).violation_mask & SB_GEOFENCE) != 0U);
    return 0;
}

int test_alt_ceiling_sets_sm3() {
    // The ceiling is a hard bound from the first active tick (no arming latch).
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t);
    t.rel_alt_m = 61.F;
    CHECK((eval_active(sm, t).violation_mask & SB_GEOFENCE) != 0U);
    return 0;
}

int test_alt_floor_does_not_fire_before_climbout() {
    // A mission engage starts ON THE GROUND (rel_alt ~0) and climbs. The floor
    // must not fire during that climb-out, or every takeoff aborts on its first
    // active tick.
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    t.rel_alt_m = 0.F;
    sm.capture_home(t);
    CHECK((eval_active(sm, t).violation_mask & SB_GEOFENCE) == 0U);
    t.rel_alt_m = 1.5F; // still climbing through the floor
    CHECK((eval_active(sm, t).violation_mask & SB_GEOFENCE) == 0U);
    return 0;
}

int test_alt_floor_arms_after_climbout() {
    // Once the vehicle has been above alt_min, a descent back below it IS a
    // violation — the floor protects against flying into the ground while
    // engaged, and that protection must survive the takeoff exemption.
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    t.rel_alt_m = 0.F;
    sm.capture_home(t);
    (void)eval_active(sm, t);
    t.rel_alt_m = 10.F; // climb-out complete -> floor arms
    CHECK((eval_active(sm, t).violation_mask & SB_GEOFENCE) == 0U);
    t.rel_alt_m = 1.F; // descending back through the floor
    CHECK((eval_active(sm, t).violation_mask & SB_GEOFENCE) != 0U);
    return 0;
}

int test_alt_floor_rearms_each_session() {
    // capture_home() runs at every engage, so the next control session gets the
    // takeoff exemption again rather than inheriting the armed floor.
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot t = healthy_snapshot();
    sm.capture_home(t); // control session 1, airborne at 10 m -> floor arms
    (void)eval_active(sm, t);
    t.rel_alt_m = 0.F;
    CHECK((eval_active(sm, t).violation_mask & SB_GEOFENCE) != 0U);
    sm.capture_home(t); // control session 2, on the ground
    CHECK((eval_active(sm, t).violation_mask & SB_GEOFENCE) == 0U);
    return 0;
}

int test_engage_timebox_sets_sm8() {
    SafetyMonitor sm;
    sm.configure(nominal_limits());
    TelemetrySnapshot const t = healthy_snapshot();
    sm.capture_home(t);
    const SafetyVerdict v =
        sm.evaluate(t, ObcState::OFFBOARD_ACTIVE, NOW_NS, 0,
                    /*engage_start_ns=*/NOW_NS - 61'000'000'000ULL, true);
    CHECK((v.violation_mask & SB_ENGAGE_TIMEBOX) != 0U);
    return 0;
}

// ---- AUTO_LANDING disarm authorization (review CR-01) ----
// Cutting the motors must rest on OBSERVED state. Each field arrives on its own
// callback, so a partial telemetry freeze can leave t.mono_ns fresh while the
// altitude, landed flag or armed flag are minutes old.

int test_disarm_needs_fresh_altitude_not_just_fresh_position() {
    const uint64_t stale = 3'000'000'000ULL;
    TelemetrySnapshot t = healthy_snapshot();
    t.rel_alt_m = 0.3F; // low enough for the source to ask for a disarm
    t.landed = 0;       // the FC does NOT think it has landed

    // Everything fresh: the source's request is honoured.
    CHECK(disarm_authorized(t, NOW_NS, stale, true, 1.0F));

    // Global position freezes 10 s back while position/velocity keep flowing —
    // exactly the case the old t.mono_ns-only check could not see. The 0.3 m is
    // now a memory, so it must not authorize anything.
    t.gpos_mono_ns = NOW_NS - 10'000'000'000ULL;
    CHECK(!disarm_authorized(t, NOW_NS, stale, true, 1.0F));

    // A fresh landed flag is independent evidence and still authorizes.
    t.landed = 1;
    CHECK(disarm_authorized(t, NOW_NS, stale, true, 1.0F));
    return 0;
}

int test_disarm_requires_a_fresh_armed_flag() {
    const uint64_t stale = 3'000'000'000ULL;
    TelemetrySnapshot t = healthy_snapshot();
    t.landed = 1;
    CHECK(disarm_authorized(t, NOW_NS, stale, false, 1.0F));

    t.armed_mono_ns = NOW_NS - 10'000'000'000ULL; // armed=1 is stale
    CHECK(!disarm_authorized(t, NOW_NS, stale, false, 1.0F));
    return 0;
}

int test_source_request_still_bounded_by_max_altitude() {
    const uint64_t stale = 3'000'000'000ULL;
    TelemetrySnapshot t = healthy_snapshot();
    t.rel_alt_m = 5.0F; // fresh, but well above the fixed AGL bound
    t.landed = 0;
    CHECK(!disarm_authorized(t, NOW_NS, stale, true, 1.0F));
    return 0;
}

int test_landing_completes_only_on_an_observed_disarm() {
    const uint64_t stale = 3'000'000'000ULL;
    TelemetrySnapshot t = healthy_snapshot();
    t.armed = 0;
    CHECK(landing_complete(t, NOW_NS, stale));

    // armed=0 read minutes ago proves nothing about now.
    t.armed_mono_ns = NOW_NS - 10'000'000'000ULL;
    CHECK(!landing_complete(t, NOW_NS, stale));

    t.armed_mono_ns = NOW_NS;
    t.armed = 1;
    CHECK(!landing_complete(t, NOW_NS, stale));
    return 0;
}

// ---- SM-10 engage precondition (review CR-04) ----
// The old policy logged "SM-10 DISABLED" and engaged anyway, so one GPS stream
// fault silently removed an operator-configured hard boundary.
int test_configured_polygon_requires_a_fresh_fix_to_engage() {
    const uint64_t stale = 3'000'000'000ULL;
    TelemetrySnapshot t = healthy_snapshot();
    t.gps_ok = 1;

    // No polygon configured: GPS is irrelevant to engaging.
    CHECK(fence_projectable(false, t, NOW_NS, stale));
    t.gps_ok = 0;
    CHECK(fence_projectable(false, t, NOW_NS, stale));

    // Polygon configured: a fresh fix is now a precondition.
    t.gps_ok = 1;
    CHECK(fence_projectable(true, t, NOW_NS, stale));

    t.gps_ok = 0; // fix lost
    CHECK(!fence_projectable(true, t, NOW_NS, stale));

    // gps_ok latches true after the first fix ever seen, so a stale stream must
    // fail even while the flag still reads 1.
    t.gps_ok = 1;
    t.gpos_mono_ns = NOW_NS - 10'000'000'000ULL;
    CHECK(!fence_projectable(true, t, NOW_NS, stale));
    return 0;
}

// ---- attitude-mode command semantics (OBC-SDD §8) -------------------------
//
// In attitude mode there is no velocity source to execute HOLD / RETURN_HOME,
// and no consumer for a mission cue. The previous handlers forwarded to a null
// source and logged "requested" — an operator command silently doing nothing
// mid-session, and a TARGET that dropped its cue yet still latched engage.
// These drive the REAL controller (with the SIL FcuLink) and judge by the FSM's
// observable behaviour, since the request latches themselves are private.

// Advances the controller until it settles, so a latched request is acted on.
void pump(OffboardController& c, uint64_t& now_ns, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        now_ns += tun::CONTROL_PERIOD_NS;
        c.tick(now_ns);
    }
}

ObcCommand make_cmd(ObcCommandType type, const char* token) {
    ObcCommand cmd{};
    cmd.magic = OBC_COMMAND_MAGIC;
    cmd.type = type;
    (void)std::snprintf(cmd.token, sizeof(cmd.token), "%s", token);
    return cmd;
}

// Brings a fresh attitude-mode controller up to READY (SIL FcuLink reports a
// connected, armed, position-valid vehicle from connect()).
bool bring_up_attitude(OffboardController& c, TestAttitudeSource& att, uint64_t& now_ns) {
    OffboardController::Config cfg;
    cfg.limits = nominal_limits();
    cfg.operator_token = "test-token";
    cfg.attitude_mode = true;
    if (!c.init(cfg, /*source=*/nullptr, &att)) {
        return false;
    }
    pump(c, now_ns, 4); // IDLE -> CONNECTING -> READY
    return c.state() == ObcState::READY;
}

int test_attitude_mode_target_is_rejected_without_engaging() {
    uint64_t now = NOW_NS;
    TestAttitudeSource att(TestAttitudeSource::Params{});
    OffboardController c;
    CHECK(bring_up_attitude(c, att, now));
    // A properly authenticated TARGET: the token is valid, so this is not a
    // rejection by authorization — it is rejected because nothing in attitude
    // mode can consume the cue. It must NOT start a control session.
    c.on_command(make_cmd(ObcCommandType::TARGET, "test-token"));
    pump(c, now, 6);
    CHECK(c.state() == ObcState::READY); // no engage latched
    // The harness itself is sound: a plain ENGAGE from the same state does
    // leave READY, so the check above is not passing vacuously.
    c.on_command(make_cmd(ObcCommandType::ENGAGE, "test-token"));
    pump(c, now, 2);
    CHECK(c.state() != ObcState::READY);
    return 0;
}

int test_attitude_mode_hold_stops_the_session() {
    uint64_t now = NOW_NS;
    TestAttitudeSource att(TestAttitudeSource::Params{});
    OffboardController c;
    CHECK(bring_up_attitude(c, att, now));
    c.on_command(make_cmd(ObcCommandType::ENGAGE, "test-token"));
    pump(c, now, 2);
    CHECK(c.state() == ObcState::PRESTREAM); // session under way
    // OPERATOR_HOLD in attitude mode maps onto the disengage path (stream stops
    // and PX4 is commanded Hold) instead of being logged and ignored.
    c.on_command(make_cmd(ObcCommandType::OPERATOR_HOLD, ""));
    pump(c, now, 4);
    CHECK(c.state() == ObcState::READY);
    return 0;
}

int test_attitude_mode_return_home_stops_the_session() {
    uint64_t now = NOW_NS;
    TestAttitudeSource att(TestAttitudeSource::Params{});
    OffboardController c;
    CHECK(bring_up_attitude(c, att, now));
    c.on_command(make_cmd(ObcCommandType::ENGAGE, "test-token"));
    pump(c, now, 2);
    CHECK(c.state() == ObcState::PRESTREAM);
    // RETURN_HOME cannot fly the vehicle home (FcuLink has no RTL action), so
    // it takes the same safe-side exit rather than doing nothing at all.
    c.on_command(make_cmd(ObcCommandType::RETURN_HOME, ""));
    pump(c, now, 4);
    CHECK(c.state() == ObcState::READY);
    return 0;
}

} // namespace

// ---- config validation (AGENTS §7.9) --------------------------------------

int test_validate_accepts_nominal() {
    std::string err;
    CHECK(validate(nominal_limits(), err));
    CHECK(err.empty());
    return 0;
}

// G16.6 deviation: reject-matrix enumeration; one CHECK per bad limit (G16.6)
// NOLINTNEXTLINE(readability-function-size)
int test_validate_rejects_bad_limits() {
    auto bad = [](SafetyMonitor::Limits l) -> bool {
        std::string err;
        return !validate(l, err) && !err.empty();
    };
    SafetyMonitor::Limits l;
    l = nominal_limits();
    l.vmax_h = 0.F;
    CHECK(bad(l));
    l = nominal_limits();
    l.vmax_v = -1.F;
    CHECK(bad(l));
    l = nominal_limits();
    l.geofence_r = 0.F;
    CHECK(bad(l));
    l = nominal_limits();
    l.alt_min = -1.F;
    CHECK(bad(l));
    l = nominal_limits();
    l.alt_max = l.alt_min;
    CHECK(bad(l)); // not > min
    l = nominal_limits();
    l.engage_timebox_ns = 0;
    CHECK(bad(l));
    l = nominal_limits();
    l.att_max_tilt_deg = 0.F;
    CHECK(bad(l));
    l = nominal_limits();
    l.att_max_tilt_deg = 90.F;
    CHECK(bad(l));
    l = nominal_limits();
    l.att_min_thrust = -0.1F;
    CHECK(bad(l));
    l = nominal_limits();
    l.att_max_thrust = 1.1F;
    CHECK(bad(l));
    l = nominal_limits();
    l.att_min_thrust = 0.9F;
    l.att_max_thrust = 0.8F;
    CHECK(bad(l));
    l = nominal_limits();
    l.bat_engage_min_frac = 1.5F;
    CHECK(bad(l));
    l = nominal_limits();
    l.bat_land_frac = -0.1F;
    CHECK(bad(l));
    // land >= engage (both enabled) is unsafe.
    l = nominal_limits();
    l.bat_land_frac = 0.4F;
    l.bat_engage_min_frac = 0.3F;
    CHECK(bad(l));
    return 0;
}

int test_validate_battery_zero_disables_ok() {
    // 0 fractions mean "disable the check" (SM-9) and must be accepted, even
    // though the land<engage ordering does not apply.
    std::string err;
    SafetyMonitor::Limits l = nominal_limits();
    l.bat_engage_min_frac = 0.F;
    l.bat_land_frac = 0.F;
    CHECK(validate(l, err));
    l = nominal_limits();
    l.bat_land_frac = 0.F; // engage enabled, land disabled: ordering N/A
    CHECK(validate(l, err));
    return 0;
}

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_validate_accepts_nominal();
    rc = rc != 0 ? rc : test_validate_rejects_bad_limits();
    rc = rc != 0 ? rc : test_validate_battery_zero_disables_ok();
    rc = rc != 0 ? rc : test_healthy_snapshot_is_clean();
    rc = rc != 0 ? rc : test_low_battery_disengages_in_flight();
    rc = rc != 0 ? rc : test_battery_exactly_at_threshold_is_ok();
    rc = rc != 0 ? rc : test_unknown_battery_is_not_a_flight_violation();
    rc = rc != 0 ? rc : test_zero_threshold_disables_flight_check();
    rc = rc != 0 ? rc : test_low_battery_ignored_when_not_engaged();
    rc = rc != 0 ? rc : test_engage_gate_accepts_healthy_battery();
    rc = rc != 0 ? rc : test_engage_gate_rejects_low_battery();
    rc = rc != 0 ? rc : test_engage_gate_rejects_unknown_battery();
    rc = rc != 0 ? rc : test_engage_gate_disabled_by_zero_threshold();
    rc = rc != 0 ? rc : test_engage_threshold_is_inclusive();
    rc = rc != 0 ? rc : test_stale_mode_stream_is_treated_as_override();
    rc = rc != 0 ? rc : test_stale_gpos_stream_sets_field_stale();
    rc = rc != 0 ? rc : test_stale_armed_stream_sets_field_stale();
    rc = rc != 0 ? rc : test_stale_health_stream_sets_field_stale();
    rc = rc != 0 ? rc : test_stale_attitude_stream_sets_field_stale();
    rc = rc != 0 ? rc : test_position_invalid_sets_pos_invalid();
    rc = rc != 0 ? rc : test_field_staleness_not_checked_when_not_active();
    rc = rc != 0 ? rc : test_stale_battery_in_flight_degrades_to_unknown();
    rc = rc != 0 ? rc : test_engage_gate_rejects_stale_battery();
    rc = rc != 0 ? rc : test_mode_reports_non_offboard_predicate();
    rc = rc != 0 ? rc : test_offboard_exit_confirmed_requires_fresh_non_offboard();
    rc = rc != 0 ? rc : test_stale_telemetry_sets_sm1();
    rc = rc != 0 ? rc : test_disarm_sets_sm6();
    rc = rc != 0 ? rc : test_geofence_radius_sets_sm3();
    rc = rc != 0 ? rc : test_alt_ceiling_sets_sm3();
    rc = rc != 0 ? rc : test_alt_floor_does_not_fire_before_climbout();
    rc = rc != 0 ? rc : test_alt_floor_arms_after_climbout();
    rc = rc != 0 ? rc : test_alt_floor_rearms_each_session();
    rc = rc != 0 ? rc : test_disarm_needs_fresh_altitude_not_just_fresh_position();
    rc = rc != 0 ? rc : test_disarm_requires_a_fresh_armed_flag();
    rc = rc != 0 ? rc : test_source_request_still_bounded_by_max_altitude();
    rc = rc != 0 ? rc : test_landing_completes_only_on_an_observed_disarm();
    rc = rc != 0 ? rc : test_configured_polygon_requires_a_fresh_fix_to_engage();
    rc = rc != 0 ? rc : test_engage_timebox_sets_sm8();
    rc = rc != 0 ? rc : test_attitude_mode_target_is_rejected_without_engaging();
    rc = rc != 0 ? rc : test_attitude_mode_hold_stops_the_session();
    rc = rc != 0 ? rc : test_attitude_mode_return_home_stops_the_session();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_safety_fsm: %d checks passed\n", checks);
    return 0;
}
