// Unit tests for GuidanceSource (L4 PN guidance folded into the OBC). Drives the
// REAL compute() path by publishing TrackState into the TrackBus shm as a WRITER,
// exercising the freshness/quality gate (SM-7 in-source half), coast on the cached
// last-valid track, the track-id lead re-seed, the body-FRD -> NED full attitude
// DCM, and the tracking + LOS-delta lead command — no seeker/FC needed.
#include "TrackValidate.h"

#include <sys/mman.h> // shm_unlink

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"
#include "sources/GuidanceSource.h"

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

constexpr float ENGAGE = tun::ENGAGE_SPEED_MPS;
constexpr float HALF_PI = 1.57079632679F;  // M_PI is not standard C++
constexpr float DEG = HALF_PI / 90.F;      // radians per degree (pi/180)
constexpr uint64_t T0 = 10'000'000'000ULL; // arbitrary monotonic base

using Writer = ShmSeqSlot<TrackState>;

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

// Like publish() but with an explicit track_id; always a good (valid, high
// quality) sample — used by the identity-continuity tests.
void publish_id(Writer& w, uint32_t track_id, float x, float y, float z,
                uint64_t mono_ns) {
    TrackState ts{};
    ts.mono_ns = mono_ns;
    ts.track_id = track_id;
    ts.rel_pos_frd_m[0] = x;
    ts.rel_pos_frd_m[1] = y;
    ts.rel_pos_frd_m[2] = z;
    ts.quality = 0.9F;
    ts.valid = 1;
    w.write(ts);
}

// A fresh, valid, high-quality sample flagged as a T2 visual coast (position
// came from the template, not a detection this frame — TR-D-b).
void publish_coast(Writer& w, float x, float y, float z, uint64_t mono_ns) {
    TrackState ts{};
    ts.mono_ns = mono_ns;
    ts.track_id = 1;
    ts.rel_pos_frd_m[0] = x;
    ts.rel_pos_frd_m[1] = y;
    ts.rel_pos_frd_m[2] = z;
    ts.quality = 0.9F;
    ts.valid = 1;
    ts.visual_coast = 1;
    w.write(ts);
}

TelemetrySnapshot telem(float yaw_rad, uint64_t now_ns) {
    TelemetrySnapshot t{};
    t.mono_ns = now_ns;
    t.yaw_rad = yaw_rad;
    t.position_ok = 1;
    return t;
}

TelemetrySnapshot telem_rpy(float roll, float pitch, float yaw, uint64_t now_ns) {
    TelemetrySnapshot t{};
    t.mono_ns = now_ns;
    t.roll_rad = roll;
    t.pitch_rad = pitch;
    t.yaw_rad = yaw;
    t.position_ok = 1;
    return t;
}

float mag3(const VelocitySetpointNed& v) {
    return std::sqrt((v.vn_mps * v.vn_mps) + (v.ve_mps * v.ve_mps) +
                     (v.vd_mps * v.vd_mps));
}

// ---- freshness / quality gate (returns false => controller disengages) ----

int test_no_track_disengage(Writer& /*w*/) {
    // Segment exists but nothing published yet (seq==0): read() fails, so the
    // source has never had a valid track.
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(!gs.compute(telem(0.F, T0), T0, out));
    CHECK(!gs.last_track_valid());
    return 0;
}

int test_invalid_flag_disengage(Writer& w) {
    publish(w, 10.F, 0.F, 0.F, 0.9F, /*valid=*/0, T0);
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(!gs.compute(telem(0.F, T0), T0, out)); // valid==0 gated out
    return 0;
}

int test_low_quality_disengage(Writer& w) {
    publish(w, 10.F, 0.F, 0.F, tun::MIN_TRACK_QUALITY - 0.01F, 1, T0);
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(!gs.compute(telem(0.F, T0), T0, out)); // below quality floor
    return 0;
}

int test_stale_track_disengage(Writer& w) {
    // A valid track older than the coast budget must disengage (SM-7).
    const uint64_t old = T0;
    const uint64_t now = old + tun::TRACK_STALE_NS + tun::TRACK_COAST_NS + 1'000'000ULL;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, old);
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(!gs.compute(telem(0.F, now), now, out));
    CHECK(gs.last_track_valid()); // it WAS valid, just too old
    return 0;
}

// ---- geometry: body-FRD -> NED yaw rotation + PN command ------------------

int test_forward_command(Writer& w) {
    // Target dead ahead, vehicle facing North: command points North at ENGAGE.
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(gs.compute(telem(0.F, T0), T0, out));
    CHECK(std::fabs(out.vn_mps - ENGAGE) < 1e-3F);
    CHECK(std::fabs(out.ve_mps) < 1e-3F);
    CHECK(std::fabs(out.vd_mps) < 1e-3F);
    CHECK(std::fabs(out.yaw_rad) < 1e-3F);        // faces the target (North)
    CHECK(std::fabs(mag3(out) - ENGAGE) < 1e-3F); // raw cmd = ENGAGE (pre-clamp)
    return 0;
}

int test_yaw_rotation_east(Writer& w) {
    // Same body-frame target, but vehicle yawed +90 deg: target now lies East.
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    GuidanceSource gs;
    VelocitySetpointNed out;
    const float yaw = HALF_PI;
    CHECK(gs.compute(telem(yaw, T0), T0, out));
    CHECK(std::fabs(out.vn_mps) < 1e-3F);
    CHECK(std::fabs(out.ve_mps - ENGAGE) < 1e-3F);
    CHECK(std::fabs(out.yaw_rad - yaw) < 1e-3F);
    return 0;
}

int test_down_component(Writer& w) {
    // Target 45 deg below the nose: Down velocity component is positive.
    publish(w, 10.F, 0.F, 10.F, 0.9F, 1, T0);
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(gs.compute(telem(0.F, T0), T0, out));
    const float expect = (10.F / std::sqrt(200.F)) * ENGAGE; // uz * ENGAGE
    CHECK(out.vd_mps > 0.F);
    CHECK(std::fabs(out.vd_mps - expect) < 1e-3F);
    return 0;
}

int test_dcm_pitch_up(Writer& w) {
    // Vehicle pitched nose-up 30 deg, target dead-ahead along the nose (10 m fwd):
    // full DCM tilts the LOS into North + UP (Down < 0). Yaw-only would wrongly
    // report the target as level North.
    const float p = 30.F * DEG;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(gs.compute(telem_rpy(0.F, p, 0.F, T0), T0, out));
    CHECK(std::fabs(out.vn_mps - (std::cos(p) * ENGAGE)) < 1e-2F);  // N = cosθ*ENGAGE
    CHECK(out.vd_mps < 0.F);                                        // climbs toward it
    CHECK(std::fabs(out.vd_mps - (-std::sin(p) * ENGAGE)) < 1e-2F); // D = -sinθ*ENGAGE
    CHECK(std::fabs(out.ve_mps) < 1e-2F);
    return 0;
}

int test_dcm_roll_right(Writer& w) {
    // Vehicle rolled 90 deg right, target off the right wing (10 m): the body
    // right axis now points straight DOWN, so the LOS is pure Down. Yaw-only
    // would put it due East.
    publish(w, 0.F, 10.F, 0.F, 0.9F, 1, T0);
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(gs.compute(telem_rpy(90.F * DEG, 0.F, 0.F, T0), T0, out));
    CHECK(std::fabs(out.vn_mps) < 1e-2F);
    CHECK(std::fabs(out.ve_mps) < 1e-2F);
    CHECK(std::fabs(out.vd_mps - ENGAGE) < 1e-2F); // straight down
    return 0;
}

int test_convergence_hold(Writer& w) {
    // Within the terminal radius (<0.5 m): hold with a zero command, still engaged.
    publish(w, 0.3F, 0.F, 0.F, 0.9F, 1, T0);
    GuidanceSource gs;
    VelocitySetpointNed out;
    CHECK(gs.compute(telem(0.F, T0), T0, out)); // returns true (engaged)
    CHECK(std::fabs(out.vn_mps) < 1e-6F);
    CHECK(std::fabs(out.ve_mps) < 1e-6F);
    CHECK(std::fabs(out.vd_mps) < 1e-6F);
    return 0;
}

int test_visual_coast_still_guides_inside_window(Writer& w) {
    // A fresh T2 visual-coast sample INSIDE the coast window guides normally
    // (TR-2): its refined LOS is used just like a detection sample would be.
    GuidanceSource gs;
    VelocitySetpointNed out;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0); // detection-anchored acquire
    CHECK(gs.compute(telem(0.F, T0), T0, out));

    const uint64_t t1 = T0 + tun::TRACK_STALE_NS;  // inside the window
    publish_coast(w, 8.F, 0.F, 0.F, t1);           // template still sees target
    CHECK(gs.compute(telem(0.F, t1), t1, out));    // still guiding
    CHECK(std::fabs(out.vn_mps - ENGAGE) < 1e-3F); // on the coast sample's LOS
    return 0;
}

int test_visual_coast_cannot_extend_the_window(Writer& w) {
    // The safety point of TR-D-b: a STREAM of fresh visual-coast samples must
    // not hold the control session open past the coast budget. The freshness clock
    // runs from the last DETECTION-anchored sample, so once detection stops,
    // fresh template-only publishes still time out and SM-7 fires.
    GuidanceSource gs;
    VelocitySetpointNed out;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0); // last detection at T0
    CHECK(gs.compute(telem(0.F, T0), T0, out));

    // Feed fresh visual-coast samples right up to the edge of the window: still
    // guiding, because now - last_detection stays within budget.
    const uint64_t edge = T0 + tun::TRACK_STALE_NS + tun::TRACK_COAST_NS;
    publish_coast(w, 8.F, 0.F, 0.F, edge);
    CHECK(gs.compute(telem(0.F, edge), edge, out)); // still inside

    // One tick past the window, with a STILL-FRESH visual-coast publish: the
    // publish timestamp is current, but the last detection is now too old —
    // guidance must disengage (compute() false -> SM-7).
    const uint64_t past = edge + 1'000'000ULL;
    publish_coast(w, 8.F, 0.F, 0.F, past); // fresh publish, but template-only
    CHECK(!gs.compute(telem(0.F, past), past, out));
    return 0;
}

int test_detection_anchor_resets_the_coast_clock(Writer& w) {
    // A fresh detection-anchored sample after some visual coasting resets the
    // freshness clock, so the control session continues — the guard only bites when
    // detection has genuinely stopped.
    GuidanceSource gs;
    VelocitySetpointNed out;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    CHECK(gs.compute(telem(0.F, T0), T0, out));

    const uint64_t t1 = T0 + tun::TRACK_STALE_NS;
    publish_coast(w, 9.F, 0.F, 0.F, t1); // visual coast
    CHECK(gs.compute(telem(0.F, t1), t1, out));

    // Detection re-acquires at t2 (well before t1's window would expire).
    const uint64_t t2 = t1 + tun::TRACK_STALE_NS;
    publish(w, 8.F, 0.F, 0.F, 0.9F, 1, t2); // detection-anchored again
    CHECK(gs.compute(telem(0.F, t2), t2, out));

    // Now coast from t2's fresh detection: a sample that would have been past
    // the ORIGINAL window is still inside the reset one.
    const uint64_t t3 = t2 + tun::TRACK_STALE_NS + tun::TRACK_COAST_NS;
    publish_coast(w, 7.F, 0.F, 0.F, t3);
    CHECK(gs.compute(telem(0.F, t3), t3, out)); // still guiding: clock reset
    return 0;
}

int test_ekf_refines_relative_velocity(Writer& w) {
    // EST-P4: the OBC-side EKF refines last_track's relative velocity from the
    // seeker's raw per-sample field. A target closing at a steady rate should,
    // after a few detection-anchored samples, show a relative velocity near the
    // true closing rate on last_track() — while the PN command (position-based)
    // is unaffected. Own vehicle stationary at origin, level.
    GuidanceSource gs;
    VelocitySetpointNed out;
    // Target due North, approaching from 120 m at 10 m/s (own is still), so the
    // relative FRD velocity is -10 m/s forward (closing).
    float north = 120.F;
    const float closing = 10.F;
    const double dt = 1.0 / 20.0; // 20 Hz control
    uint64_t tnow = T0;
    for (int i = 0; i < 40; ++i) {
        publish(w, north, 0.F, 0.F, 0.9F, 1, tnow);
        CHECK(gs.compute(telem(0.F, tnow), tnow, out));
        north -= closing * static_cast<float>(dt);
        tnow += static_cast<uint64_t>(dt * 1e9);
    }
    // Relative forward velocity converged near the true closing rate (-10 m/s).
    CHECK(gs.last_track_valid());
    CHECK(std::fabs(gs.last_track().rel_vel_frd_mps[0] - (-closing)) < 2.5F);
    return 0;
}

int test_est6_quality_tracks_convergence(Writer& w) {
    // EST-6: right after acquisition the EKF is uncertain (large init sigma), so
    // the cached quality is pulled toward the gate; as the estimate converges,
    // quality recovers. It never drops below the gate — guidance keeps flying.
    // (This covers the initialization-uncertainty and divergence cases; the
    // systematic far-range under-confidence needs a size/range-bias state, an
    // EST follow-up — see ESTIMATION-REQ §5.2.)
    GuidanceSource gs;
    VelocitySetpointNed out;
    publish(w, 100.F, 0.F, 0.F, 0.9F, 1, T0);
    CHECK(gs.compute(telem(0.F, T0), T0, out));
    const float q_initial = gs.last_track().quality; // just initialized: degraded
    uint64_t tnow = T0;
    for (int i = 0; i < 40; ++i) {
        tnow += 50'000'000ULL; // 20 Hz
        publish(w, 100.F, 0.F, 0.F, 0.9F, 1, tnow);
        (void)gs.compute(telem(0.F, tnow), tnow, out);
    }
    const float q_converged = gs.last_track().quality;
    CHECK(q_initial >= tun::MIN_TRACK_QUALITY); // never below the gate
    CHECK(q_initial < 0.9F - 1e-3F);            // acquisition: degraded from raw 0.9
    CHECK(q_converged > q_initial);             // recovered as sigma shrank
    return 0;
}

int test_coast_uses_cached_position(Writer& w) {
    // A valid track (10 m North) followed by an INVALID bus sample carrying
    // garbage position: inside the coast window guidance must fly on the
    // CACHED last-valid position, not on the current invalid sample.
    GuidanceSource gs;
    VelocitySetpointNed out;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    CHECK(gs.compute(telem(0.F, T0), T0, out));

    const uint64_t t1 = T0 + tun::TRACK_STALE_NS; // inside stale+coast budget
    publish(w, 999.F, 999.F, 0.F, 0.9F, /*valid=*/0, t1);
    CHECK(gs.compute(telem(0.F, t1), t1, out));    // still guiding (coast)
    CHECK(std::fabs(out.vn_mps - ENGAGE) < 1e-3F); // cached target: due North
    CHECK(std::fabs(out.ve_mps) < 1e-3F);          // garbage East ignored
    return 0;
}

// ---- Process-boundary validation (review CR-03) ----
// The TrackBus is written by another process. Before validation, a NaN position
// in an otherwise well-formed sample (valid=1, quality in range, fresh stamp)
// went into the IMM and out through the geometry; the final clamp zeroed the
// command but compute() still returned true, so the session looked healthy
// while the estimator carried poison.

int test_nan_position_is_rejected_not_consumed(Writer& w) {
    GuidanceSource gs;
    VelocitySetpointNed out;

    // Establish a good track first so there is something to corrupt.
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0);
    CHECK(gs.compute(telem(0.F, T0), T0, out));
    const float good_vn = out.vn_mps;
    CHECK(good_vn > 0.F);

    // A NaN arrives. The sample must be ignored entirely: guidance keeps flying
    // the cached track (inside the coast window), and nothing NaN comes out.
    const uint64_t t1 = T0 + tun::CONTROL_PERIOD_NS;
    publish(w, std::numeric_limits<float>::quiet_NaN(), 0.F, 0.F, 0.9F, 1, t1);
    CHECK(gs.compute(telem(0.F, t1), t1, out));
    CHECK(std::isfinite(out.vn_mps));
    CHECK(std::isfinite(out.ve_mps));
    CHECK(std::isfinite(out.vd_mps));
    CHECK(std::fabs(out.vn_mps - good_vn) < 1e-3F); // still the cached track

    // Recovery: a clean sample after the bad one must be used normally, which
    // only holds if the bad one never entered the estimator.
    const uint64_t t2 = t1 + tun::CONTROL_PERIOD_NS;
    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, t2);
    CHECK(gs.compute(telem(0.F, t2), t2, out));
    CHECK(std::isfinite(out.vn_mps));
    CHECK(out.vn_mps > 0.F);
    return 0;
}

int test_malformed_fields_are_rejected() {
    const uint64_t now = T0 + 1'000'000ULL;
    TrackState ts{};
    ts.mono_ns = T0;
    ts.track_id = 1;
    ts.rel_pos_frd_m[0] = 10.F;
    ts.quality = 0.9F;
    ts.valid = 1;
    CHECK(validate_track_state(ts, now)); // baseline: well-formed

    { // quality outside [0,1] — the gate only ever checked the LOWER bound
        TrackState bad = ts;
        bad.quality = 4.2F;
        CHECK(!validate_track_state(bad, now));
    }
    { // infinite velocity
        TrackState bad = ts;
        bad.rel_vel_frd_mps[2] = std::numeric_limits<float>::infinity();
        CHECK(!validate_track_state(bad, now));
    }
    { // implausible range: corruption, not a target
        TrackState bad = ts;
        bad.rel_pos_frd_m[1] = 1e9F;
        CHECK(!validate_track_state(bad, now));
    }
    { // a stamp from the future means writer/reader disagree, not clock skew
        TrackState bad = ts;
        bad.mono_ns = now + 1'000'000'000ULL;
        CHECK(!validate_track_state(bad, now));
    }
    { // enumerated bytes outside their defined set
        TrackState bad = ts;
        bad.visual_coast = 7;
        CHECK(!validate_track_state(bad, now));
    }
    {
        TrackState bad = ts;
        bad.num_targets = 200;
        CHECK(!validate_track_state(bad, now));
    }
    { // valid=0 is well-formed, just unusable
        TrackState none = ts;
        none.valid = 0;
        CHECK(!validate_track_state(none, now));
    }
    return 0;
}

int test_id_switch_resets_lead(Writer& w) {
    // Same LOS drift as test_pn_lead_term, but the second sample carries a NEW
    // track_id: the lead filter must re-seed instead of differencing the LOS
    // across two different aircraft — direct tracking, no lead spike.
    GuidanceSource gs;
    VelocitySetpointNed out;
    publish_id(w, 1, 10.F, 0.F, 0.F, T0);
    CHECK(gs.compute(telem(0.F, T0), T0, out));

    const uint64_t t1 = T0 + tun::CONTROL_PERIOD_NS;
    publish_id(w, 2, 10.F, 1.F, 0.F, t1);
    CHECK(gs.compute(telem(0.F, t1), t1, out));
    const float uy = 1.F / std::sqrt(101.F);              // unit-LOS East component
    CHECK(std::fabs(out.ve_mps - (uy * ENGAGE)) < 1e-3F); // no (1+N) amplification
    return 0;
}

int test_pn_lead_term(Writer& w) {
    // Two ticks with a laterally rotating LOS: PN amplifies the cross-track
    // command by (1 + PN_GAIN) relative to the direct tracking term.
    GuidanceSource gs;
    VelocitySetpointNed out1;
    VelocitySetpointNed out2;

    publish(w, 10.F, 0.F, 0.F, 0.9F, 1, T0); // tick 1: dead ahead -> prev LOS = N
    CHECK(gs.compute(telem(0.F, T0), T0, out1));

    const uint64_t t1 = T0 + tun::CONTROL_PERIOD_NS;
    publish(w, 10.F, 1.F, 0.F, 0.9F, 1, t1); // tick 2: target drifted right
    CHECK(gs.compute(telem(0.F, t1), t1, out2));

    const float uy = 1.F / std::sqrt(101.F);          // unit-LOS East component
    const float pure = uy * ENGAGE;                   // tracking-only term
    const float expect = pure * (1.F + tun::PN_GAIN); // + PN lead (dt cancels)
    CHECK(out2.ve_mps > pure);                        // lead adds cross-track velocity
    CHECK(std::fabs(out2.ve_mps - expect) < 1e-2F);
    return 0;
}

} // namespace

int main() {
    // Start from a clean TrackBus segment so seq==0 for the no-track case.
    shm_unlink(tun::SHM_TRACK);
    Writer w;
    if (!w.open(tun::SHM_TRACK, Writer::Role::WRITER)) {
        std::printf("FAIL: cannot create TrackBus shm\n");
        return 1;
    }

    int rc = 0;
    rc = rc != 0 ? rc : test_no_track_disengage(w); // must run first (seq==0)
    rc = rc != 0 ? rc : test_invalid_flag_disengage(w);
    rc = rc != 0 ? rc : test_low_quality_disengage(w);
    rc = rc != 0 ? rc : test_stale_track_disengage(w);
    rc = rc != 0 ? rc : test_forward_command(w);
    rc = rc != 0 ? rc : test_yaw_rotation_east(w);
    rc = rc != 0 ? rc : test_down_component(w);
    rc = rc != 0 ? rc : test_dcm_pitch_up(w);
    rc = rc != 0 ? rc : test_dcm_roll_right(w);
    rc = rc != 0 ? rc : test_convergence_hold(w);
    rc = rc != 0 ? rc : test_coast_uses_cached_position(w);
    rc = rc != 0 ? rc : test_visual_coast_still_guides_inside_window(w);
    rc = rc != 0 ? rc : test_visual_coast_cannot_extend_the_window(w);
    rc = rc != 0 ? rc : test_detection_anchor_resets_the_coast_clock(w);
    rc = rc != 0 ? rc : test_ekf_refines_relative_velocity(w);
    rc = rc != 0 ? rc : test_est6_quality_tracks_convergence(w);
    rc = rc != 0 ? rc : test_id_switch_resets_lead(w);
    rc = rc != 0 ? rc : test_pn_lead_term(w);
    rc = rc != 0 ? rc : test_nan_position_is_rejected_not_consumed(w);
    rc = rc != 0 ? rc : test_malformed_fields_are_rejected();

    w.close();
    shm_unlink(tun::SHM_TRACK);

    if (rc != 0) {
        return rc;
    }
    std::printf("test_guidance: %d checks passed\n", checks);
    return 0;
}
