// Unit tests for the pitch/yaw attitude-control path: the SM-4 attitude clamp
// (bounds tilt and thrust), the TestAttitudeSource passthrough, and the
// AttitudeTrackingSource steering law driven through its REAL compute() with a
// TrackState injected into the TrackBus shm (yaw-to-target, nose-down pitch,
// elevation thrust bias via the full FRD->NED attitude DCM, coast on the cached
// last-valid track, safe hold + disengage when the track is stale).
#include "SafetyMonitor.h"

#include <sys/mman.h> // shm_unlink

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"
#include "sources/AttitudeTrackingSource.h"
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

constexpr uint64_t T0 = 10'000'000'000ULL;
constexpr float HALF_PI = 1.57079632679F; // M_PI is not standard C++
constexpr float DEG = HALF_PI / 90.F;     // radians per degree (pi/180)

using Writer = ShmSeqSlot<TrackState>;

SafetyMonitor::Limits att_limits() {
    SafetyMonitor::Limits l;
    l.att_max_tilt_deg = 35.F;
    l.att_min_thrust = 0.10F;
    l.att_max_thrust = 0.80F;
    return l;
}

void publish(Writer& w, float x, float y, float z, float quality, uint8_t valid,
             uint64_t mono_ns) {
    TrackState ts{};
    ts.mono_ns = mono_ns;
    ts.track_id = 1;
    ts.rel_pos_frd_m[0] = x;
    ts.rel_pos_frd_m[1] = y;
    ts.rel_pos_frd_m[2] = z;
    ts.quality = quality;
    ts.valid = valid;
    w.write(ts);
}

TelemetrySnapshot telem(float yaw_rad, uint64_t now_ns) {
    TelemetrySnapshot t{};
    t.stamp_all_streams(now_ns); // field-level SM-1: all streams fresh
    t.yaw_rad = yaw_rad;
    t.position_ok = 1;
    return t;
}

TelemetrySnapshot telem_rpy(float roll, float pitch, float yaw, uint64_t now_ns) {
    TelemetrySnapshot t{};
    t.stamp_all_streams(now_ns); // field-level SM-1: all streams fresh
    t.roll_rad = roll;
    t.pitch_rad = pitch;
    t.yaw_rad = yaw;
    t.position_ok = 1;
    return t;
}

// ---- SM-4 attitude clamp ---------------------------------------------------

int test_clamp_tilt() {
    SafetyMonitor sm;
    sm.configure(att_limits());
    AttitudeSetpoint a;
    a.roll_deg = 80.F;
    a.pitch_deg = -70.F;
    a.thrust = 0.5F;
    sm.clamp_attitude(a);
    CHECK(std::fabs(a.roll_deg - 35.F) < 1e-4F);  // clamped to +max
    CHECK(std::fabs(a.pitch_deg + 35.F) < 1e-4F); // clamped to -max
    return 0;
}

int test_clamp_thrust_band() {
    SafetyMonitor sm;
    sm.configure(att_limits());
    AttitudeSetpoint lo;
    lo.thrust = 0.F; // motors-off request
    sm.clamp_attitude(lo);
    CHECK(std::fabs(lo.thrust - 0.10F) < 1e-4F); // floored to min

    AttitudeSetpoint hi;
    hi.thrust = 1.0F;
    sm.clamp_attitude(hi);
    CHECK(std::fabs(hi.thrust - 0.80F) < 1e-4F); // capped to max
    return 0;
}

int test_clamp_inrange_and_yaw_free() {
    SafetyMonitor sm;
    sm.configure(att_limits());
    AttitudeSetpoint a;
    a.roll_deg = 10.F;
    a.pitch_deg = -12.F;
    a.yaw_deg = 200.F; // yaw is not clamped (wraps)
    a.thrust = 0.55F;
    sm.clamp_attitude(a);
    CHECK(std::fabs(a.roll_deg - 10.F) < 1e-4F);
    CHECK(std::fabs(a.pitch_deg + 12.F) < 1e-4F);
    CHECK(std::fabs(a.yaw_deg - 200.F) < 1e-4F);
    CHECK(std::fabs(a.thrust - 0.55F) < 1e-4F);
    return 0;
}

int test_clamp_nonfinite_failsafe() {
    // NaN/Inf must never survive the clamp (std::clamp passes NaN through): a
    // degenerate guidance source must not flip/dive the vehicle.
    SafetyMonitor sm;
    sm.configure(att_limits());
    AttitudeSetpoint a;
    a.roll_deg = std::nanf("");
    a.pitch_deg = INFINITY;
    a.yaw_deg = -INFINITY;
    a.thrust = std::nanf("");
    sm.clamp_attitude(a);
    CHECK(std::isfinite(a.roll_deg));
    CHECK(std::fabs(a.roll_deg) < 1e-4F); // -> level
    CHECK(std::isfinite(a.pitch_deg));
    CHECK(std::fabs(a.pitch_deg) < 1e-4F); // -> level
    CHECK(std::isfinite(a.yaw_deg));
    CHECK(std::fabs(a.yaw_deg) < 1e-4F); // -> 0
    CHECK(std::isfinite(a.thrust));
    CHECK(a.thrust >= 0.10F); // safe thrust band
    CHECK(a.thrust <= 0.80F);
    return 0;
}

// ---- TestAttitudeSource ----------------------------------------------------

int test_fixed_source_passthrough() {
    TestAttitudeSource src(TestAttitudeSource::Params{5.F, -10.F, 45.F, 0.6F});
    AttitudeSetpoint out{};
    CHECK(src.compute(telem(0.F, T0), T0, out));
    CHECK(std::fabs(out.roll_deg - 5.F) < 1e-4F);
    CHECK(std::fabs(out.pitch_deg + 10.F) < 1e-4F);
    CHECK(std::fabs(out.yaw_deg - 45.F) < 1e-4F);
    CHECK(std::fabs(out.thrust - 0.6F) < 1e-4F);
    return 0;
}

// ---- AttitudeTrackingSource (real compute via TrackBus shm) -----------------

int test_tracking_no_track_safe_hold(Writer& /*w*/) {
    // No publish yet: source cannot steer -> false, but leaves a SAFE hold.
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    CHECK(!src.compute(telem(0.F, T0), T0, out));
    CHECK(std::fabs(out.roll_deg) < 1e-4F);
    CHECK(std::fabs(out.pitch_deg) < 1e-4F);      // level
    CHECK(std::fabs(out.thrust - 0.50F) < 1e-4F); // hover
    return 0;
}

int test_tracking_stale_track_disengage(Writer& w) {
    const uint64_t now = T0 + tun::TRACK_STALE_NS + tun::TRACK_COAST_NS + 1'000'000ULL;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    CHECK(!src.compute(telem(0.F, now), now, out)); // too old -> disengage
    return 0;
}

// P2-04: the coast window is anchored to the last DETECTION, not the last
// publish. A stream of fresh VISUAL-COAST (template-only) samples must not hold
// the control session open past the coast budget — a motion coast could not, and the
// invariant must hold identically here (TRACKER-REQ TR-D-b).
int test_tracking_visual_coast_does_not_extend_window(Writer& w) {
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    // A detection-anchored sample at T0 seeds the coast clock.
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    CHECK(src.compute(telem(0.F, T0), T0, out));
    // Past the coast budget, feed a FRESH visual-coast sample: it refines the
    // LOS but must not reset the detection clock, so we still disengage.
    const uint64_t late = T0 + tun::TRACK_STALE_NS + tun::TRACK_COAST_NS + 1'000'000ULL;
    TrackState coast{};
    coast.mono_ns = late;
    coast.track_id = 1;
    coast.rel_pos_frd_m[0] = 10.F;
    coast.quality = 0.9F;
    coast.valid = 1;
    coast.visual_coast = 1; // template-only
    w.write(coast);
    CHECK(!src.compute(telem(0.F, late), late, out)); // window not extended
    return 0;
}

int test_tracking_faces_target_nose_down(Writer& w) {
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0); // dead ahead, vehicle facing North
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    CHECK(src.compute(telem(0.F, T0), T0, out));
    CHECK(std::fabs(out.yaw_deg) < 1e-2F);          // face North
    CHECK(std::fabs(out.pitch_deg + 15.F) < 1e-3F); // nose down (forward)
    CHECK(std::fabs(out.thrust - 0.50F) < 1e-3F);   // centered elevation -> hover
    return 0;
}

int test_tracking_target_above_pitches_up_r9(Writer& w) {
    // R-9 vertical centering: a target ABOVE the horizon (down<0) makes the nose
    // pitch UP relative to the fixed lean, so a body-fixed camera keeps it in
    // the vertical FOV instead of losing it (the S-G4 failure).
    publish(w, 10.F, 0.F, -5.F, 0.9F, 1, T0); // 5 m above, 10 m ahead, level vehicle
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    CHECK(src.compute(telem(0.F, T0), T0, out));
    const float el_deg = std::atan2(-5.F, 10.F) * 57.2957795F; // negative (up)
    CHECK(std::fabs(out.pitch_deg - (-el_deg - 15.F)) < 1e-2F);
    CHECK(out.pitch_deg > -15.F); // tracks upward: above the fixed nose-down lean
    return 0;
}

int test_tracking_target_below_pitches_down_r9(Writer& w) {
    // A target BELOW (down>0) pitches the nose further down to track it.
    publish(w, 10.F, 0.F, 5.F, 0.9F, 1, T0); // 5 m below
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    CHECK(src.compute(telem(0.F, T0), T0, out));
    const float el_deg = std::atan2(5.F, 10.F) * 57.2957795F; // positive (down)
    CHECK(std::fabs(out.pitch_deg - (-el_deg - 15.F)) < 1e-2F);
    CHECK(out.pitch_deg < -15.F); // more nose-down than the fixed lean
    return 0;
}

int test_tracking_yaw_rotation(Writer& w) {
    // Vehicle yawed +90 deg: a body-forward target now lies East -> yaw ~ +90.
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    CHECK(src.compute(telem(HALF_PI, T0), T0, out));
    CHECK(std::fabs(out.yaw_deg - 90.F) < 1e-2F);
    return 0;
}

int test_tracking_full_dcm_pitch(Writer& w) {
    // Vehicle already pitched 15 deg nose-down (the tracking regime), target
    // along the body nose (10 m): the full DCM puts it BELOW the horizon
    // (down = sin(15)*10 m), so the thrust bias commands a descent:
    //   thrust = hover + gain * (-down/range) = 0.50 + 0.15*sin(-15 deg).
    // Yaw-only conversion would read zero elevation and hold hover thrust —
    // wrong by ~sin(pitch)*range in elevation.
    const float p = -15.F * DEG;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    CHECK(src.compute(telem_rpy(0.F, p, 0.F, T0), T0, out));
    CHECK(std::fabs(out.yaw_deg) < 1e-2F); // still dead ahead in azimuth
    const float expect = 0.50F + (0.15F * std::sin(p));
    CHECK(out.thrust < 0.50F); // descend toward it
    CHECK(std::fabs(out.thrust - expect) < 1e-3F);
    return 0;
}

int test_tracking_full_dcm_roll(Writer& w) {
    // Rolled 90 deg right with the target off the right wing: the body right
    // axis points straight DOWN, so the target sits at elevation -1 (straight
    // below) — full descend bias. Yaw-only would read it as level East.
    publish(w, 0.F, 10.F, 0.F, 0.9F, 1, T0);
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    CHECK(src.compute(telem_rpy(HALF_PI, 0.F, 0.F, T0), T0, out));
    CHECK(std::fabs(out.thrust - (0.50F - 0.15F)) < 1e-3F);
    return 0;
}

int test_tracking_coast_uses_cached_track(Writer& w) {
    // A valid track ABOVE (climb bias), then an invalid sample placing garbage
    // BELOW: inside the coast window steering must keep using the cached
    // last-valid track, not the invalid current sample.
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    publish(w, 10.F, 0.F, -10.F, 0.9F, 1, T0); // target above (down < 0)
    CHECK(src.compute(telem(0.F, T0), T0, out));
    CHECK(out.thrust > 0.50F); // climb

    const uint64_t t1 = T0 + tun::TRACK_STALE_NS; // inside stale+coast budget
    publish(w, 10.F, 0.F, 10.F, 0.9F, /*valid=*/0, t1);
    CHECK(src.compute(telem(0.F, t1), t1, out)); // still steering (coast)
    CHECK(out.thrust > 0.50F);                   // cached ABOVE target used
    return 0;
}

int test_tracking_elevation_thrust_bias(Writer& w) {
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint above{};
    publish(w, 10.F, 0.F, -10.F, 0.9F, 1, T0); // target above (down < 0)
    CHECK(src.compute(telem(0.F, T0), T0, above));
    CHECK(above.thrust > 0.50F); // climb

    AttitudeTrackingSource src2(AttitudeTrackingSource::Params{});
    AttitudeSetpoint below{};
    publish(w, 10.F, 0.F, 10.F, 0.9F, 1, T0); // target below (down > 0)
    CHECK(src2.compute(telem(0.F, T0), T0, below));
    CHECK(below.thrust < 0.50F); // descend
    return 0;
}

int test_tracking_on_engage_drops_cached_track(Writer& w) {
    // Session boundary: a second control session must not steer on the FIRST
    // session's cached track. Same shape as the coast test above, but with an
    // on_engage() between the sessions — the invalid bus sample that a coast
    // would ride through must now yield "cannot steer" instead.
    AttitudeTrackingSource src(AttitudeTrackingSource::Params{});
    AttitudeSetpoint out{};
    publish(w, 10.F, 0.F, -10.F, 0.9F, 1, T0); // session 1: target above
    CHECK(src.compute(telem(0.F, T0), T0, out));
    CHECK(out.thrust > 0.50F); // steering on it

    src.on_engage(); // session 2 begins; the cache must be gone

    const uint64_t t1 = T0 + tun::TRACK_STALE_NS;       // inside the old coast budget
    publish(w, 10.F, 0.F, 10.F, 0.9F, /*valid=*/0, t1); // no target on the bus
    CHECK(!src.compute(telem(0.F, t1), t1, out));       // refuses: no cached carryover
    CHECK(std::fabs(out.pitch_deg) < 1e-4F);            // and the hold is level/safe
    CHECK(std::fabs(out.thrust - 0.50F) < 1e-4F);
    return 0;
}

} // namespace

int main() {
    shm_unlink(tun::SHM_TRACK);
    Writer w;
    if (!w.open(tun::SHM_TRACK, Writer::Role::WRITER)) {
        std::printf("FAIL: cannot create TrackBus shm\n");
        return 1;
    }

    int rc = 0;
    rc = rc != 0 ? rc : test_clamp_tilt();
    rc = rc != 0 ? rc : test_clamp_thrust_band();
    rc = rc != 0 ? rc : test_clamp_inrange_and_yaw_free();
    rc = rc != 0 ? rc : test_clamp_nonfinite_failsafe();
    rc = rc != 0 ? rc : test_fixed_source_passthrough();
    rc = rc != 0 ? rc : test_tracking_no_track_safe_hold(w); // must precede publishes
    rc = rc != 0 ? rc : test_tracking_stale_track_disengage(w);
    rc = rc != 0 ? rc : test_tracking_visual_coast_does_not_extend_window(w);
    rc = rc != 0 ? rc : test_tracking_faces_target_nose_down(w);
    rc = rc != 0 ? rc : test_tracking_target_above_pitches_up_r9(w);
    rc = rc != 0 ? rc : test_tracking_target_below_pitches_down_r9(w);
    rc = rc != 0 ? rc : test_tracking_yaw_rotation(w);
    rc = rc != 0 ? rc : test_tracking_full_dcm_pitch(w);
    rc = rc != 0 ? rc : test_tracking_full_dcm_roll(w);
    rc = rc != 0 ? rc : test_tracking_coast_uses_cached_track(w);
    rc = rc != 0 ? rc : test_tracking_elevation_thrust_bias(w);
    rc = rc != 0 ? rc : test_tracking_on_engage_drops_cached_track(w);

    w.close();
    shm_unlink(tun::SHM_TRACK);

    if (rc != 0) {
        return rc;
    }
    std::printf("test_attitude: %d checks passed\n", checks);
    return 0;
}
