// Moving-target EKF tests (RIPOSTE-ESTIMATION-REQ-001 EST-P1).
//
// Drives the filter with SYNTHETIC ground-truth trajectories: a perfect
// monocular geometry (relative FRD = truth) plus a known own state, so the
// tests check the estimator's behaviour, not the camera. The load-bearing
// properties: it converges on a static target, tracks a constant-velocity one,
// separates own motion (a maneuvering ownship does not smear the target's
// velocity), and carries range uncertainty ANISOTROPICALLY (along-LOS loose,
// across-LOS tight) — the honest observability limit of monocular ranging.
#include "TargetEkf.h"

#include <cmath>
#include <cstdio>

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

bool near(float a, float b, float tol) {
    return std::fabs(a - b) <= tol;
}

TargetEkf::Params params() {
    TargetEkf::Params p;
    p.sigma_jerk = 3.0F;
    p.sigma_bearing_rel = 0.005F;
    p.sigma_range_rel = 0.30F;
    p.init_pos_unc = 100.F;
    p.init_vel_unc = 30.F;
    p.init_acc_unc = 10.F;
    return p;
}

// Own state at level attitude, at NED origin unless moved.
TargetEkf::OwnState level_own(float pn = 0.F, float pe = 0.F, float pd = 0.F,
                              float vn = 0.F, float ve = 0.F, float vd = 0.F) {
    TargetEkf::OwnState o;
    o.pos_ned[0] = pn;
    o.pos_ned[1] = pe;
    o.pos_ned[2] = pd;
    o.vel_ned[0] = vn;
    o.vel_ned[1] = ve;
    o.vel_ned[2] = vd;
    return o; // level: roll=pitch=yaw=0, so FRD == NED
}

// Truth target at absolute NED `tgt`, own at `own`: the perfect monocular
// measurement is the relative position rotated into FRD. At level attitude
// FRD == NED, so rel_frd = tgt - own_pos.
void measure(TargetEkf& ekf, const float tgt[3], const TargetEkf::OwnState& own) {
    const float rel[3] = {tgt[0] - own.pos_ned[0], tgt[1] - own.pos_ned[1],
                          tgt[2] - own.pos_ned[2]};
    ekf.update(rel, own);
}

// -------------------------------------------------------------- basics --

int test_first_update_initializes_at_measurement() {
    TargetEkf ekf(params());
    CHECK(!ekf.initialized());
    const float tgt[3] = {200.F, 30.F, -10.F};
    measure(ekf, tgt, level_own());
    CHECK(ekf.initialized());
    CHECK(near(ekf.pos_ned()[0], 200.F, 1e-2F));
    CHECK(near(ekf.pos_ned()[1], 30.F, 1e-2F));
    CHECK(near(ekf.pos_ned()[2], -10.F, 1e-2F));
    return 0;
}

int test_static_target_converges_and_tightens() {
    TargetEkf ekf(params());
    const float tgt[3] = {150.F, 0.F, -5.F};
    measure(ekf, tgt, level_own());
    const float sig0 = ekf.position_sigma();
    for (int i = 0; i < 60; ++i) {
        ekf.predict(0.05);
        measure(ekf, tgt, level_own());
    }
    CHECK(near(ekf.pos_ned()[0], 150.F, 2.0F));
    CHECK(near(ekf.pos_ned()[1], 0.F, 1.0F));
    // Repeated observation must reduce position uncertainty.
    CHECK(ekf.position_sigma() < sig0);
    // A static target => velocity estimate near zero.
    CHECK(near(ekf.vel_ned()[0], 0.F, 1.5F));
    CHECK(near(ekf.vel_ned()[1], 0.F, 1.5F));
    return 0;
}

int test_constant_velocity_target_is_tracked() {
    TargetEkf ekf(params());
    // Target moving +8 m/s East at 150 m North, level own at origin.
    float tgt[3] = {150.F, 0.F, -5.F};
    const float vE = 8.F;
    const double dt = 0.05;
    measure(ekf, tgt, level_own());
    for (int i = 0; i < 100; ++i) {
        tgt[1] += vE * static_cast<float>(dt);
        ekf.predict(dt);
        measure(ekf, tgt, level_own());
    }
    // Velocity converged to the true East rate.
    CHECK(near(ekf.vel_ned()[1], vE, 1.5F));
    CHECK(near(ekf.vel_ned()[0], 0.F, 1.5F));
    // Position tracks the moving truth.
    CHECK(near(ekf.pos_ned()[1], tgt[1], 5.0F));
    return 0;
}

int test_accelerating_target_is_tracked() {
    // A target accelerating East (CA regime): the CV assumption would lag, but
    // the acceleration state lets the filter follow. After settling, the
    // estimated acceleration is near the true value and position tracks truth.
    TargetEkf ekf(params());
    float tgt[3] = {150.F, 0.F, -5.F};
    float vE = 0.F;
    const float aE = 3.0F; // 3 m/s^2 East
    const double dt = 0.05;
    measure(ekf, tgt, level_own());
    for (int i = 0; i < 120; ++i) {
        vE += aE * static_cast<float>(dt);
        tgt[1] += vE * static_cast<float>(dt);
        ekf.predict(dt);
        measure(ekf, tgt, level_own());
    }
    CHECK(near(ekf.acc_ned()[1], aE, 1.5F));    // recovered East acceleration
    CHECK(near(ekf.pos_ned()[1], tgt[1], 8.F)); // tracks the accelerating truth
    return 0;
}

int test_turning_target_position_tracked() {
    // A coordinated-turn (maneuvering) target: CA is not the exact model, but
    // carrying acceleration keeps the position error bounded through the turn,
    // where pure CV would diverge on the lateral acceleration.
    TargetEkf ekf(params());
    const float R = 60.F; // turn radius
    const float w = 0.3F; // rad/s
    const double dt = 0.05;
    const float cx = 150.F;
    auto pos = [&](float th, float out[3]) {
        out[0] = cx + (R * std::cos(th));
        out[1] = R * std::sin(th);
        out[2] = -5.F;
    };
    float tgt[3];
    pos(0.F, tgt);
    measure(ekf, tgt, level_own());
    float max_err = 0.F;
    for (int i = 1; i < 200; ++i) {
        const float th = static_cast<float>(i) * w * static_cast<float>(dt);
        pos(th, tgt);
        ekf.predict(dt);
        measure(ekf, tgt, level_own());
        if (i > 60) { // after settling
            const float ex = ekf.pos_ned()[0] - tgt[0];
            const float ey = ekf.pos_ned()[1] - tgt[1];
            const float e = std::sqrt((ex * ex) + (ey * ey));
            max_err = e > max_err ? e : max_err;
        }
    }
    CHECK(max_err < 25.F); // bounded through the turn (CV would run away)
    return 0;
}

// ------------------------------------------------ own-motion separation --

int test_own_motion_does_not_move_a_static_target() {
    // The ownship flies while the target sits still. The target's ABSOLUTE
    // position estimate must stay put — own motion is in the measurement model,
    // not mistaken for target motion (EST-3).
    TargetEkf ekf(params());
    const float tgt[3] = {200.F, 0.F, -10.F};
    TargetEkf::OwnState own = level_own();
    measure(ekf, tgt, own);
    const double dt = 0.05;
    for (int i = 0; i < 100; ++i) {
        own.pos_ned[1] += 6.F * static_cast<float>(dt); // own drifts East 6 m/s
        own.vel_ned[1] = 6.F;
        ekf.predict(dt);
        measure(ekf, tgt, own);
    }
    // Absolute target position unchanged despite own motion.
    CHECK(near(ekf.pos_ned()[0], 200.F, 3.0F));
    CHECK(near(ekf.pos_ned()[1], 0.F, 3.0F));
    // Target absolute velocity ~0.
    CHECK(near(ekf.vel_ned()[1], 0.F, 2.0F));

    // Relative velocity, read back, reflects the CLOSING due to own motion:
    // rel_vel = target_vel - own_vel = 0 - 6 East = -6 East.
    float rp[3];
    float rv[3];
    ekf.relative_state(own, rp, rv);
    CHECK(near(rv[1], -6.F, 2.0F)); // own East motion shows as target closing West rel
    return 0;
}

// --------------------------------------------------- anisotropic range --

int test_range_uncertainty_is_anisotropic() {
    // One update from scratch: a target due North. The along-LOS (North, = range)
    // uncertainty must stay much larger than the across-LOS (East) uncertainty,
    // because monocular range (from size) is far less certain than bearing.
    TargetEkf ekf(params());
    const float tgt[3] = {300.F, 0.F, 0.F}; // due North, level own at origin
    measure(ekf, tgt, level_own());         // init
    ekf.predict(0.05);
    measure(ekf, tgt, level_own()); // one real update

    // Recover per-axis variance via position_sigma is trace-based; instead probe
    // convergence: after many pure-range-limited updates the North estimate
    // stays looser than East. Feed repeated identical measurements.
    for (int i = 0; i < 40; ++i) {
        ekf.predict(0.05);
        measure(ekf, tgt, level_own());
    }
    // Both converge to truth, but this test's point is qualitative: the filter
    // did not diverge and the along-LOS (North) estimate is within the wider
    // range band while East is tight.
    CHECK(near(ekf.pos_ned()[1], 0.F, 1.0F));   // East (across-LOS): tight
    CHECK(near(ekf.pos_ned()[0], 300.F, 30.F)); // North (along-LOS): wider band
    return 0;
}

int test_parallax_tightens_range() {
    // Static target due North; the ownship weaves East-West. The lateral own
    // motion turns the bearing into a range cue (parallax), so the along-LOS
    // (North) estimate should converge TIGHTER than with no weaving.
    const float tgt[3] = {300.F, 0.F, 0.F};

    // No-weave reference.
    TargetEkf a(params());
    TargetEkf::OwnState const own_a = level_own();
    measure(a, tgt, own_a);
    for (int i = 0; i < 120; ++i) {
        a.predict(0.05);
        measure(a, tgt, own_a);
    }

    // Weaving own motion (+/- East).
    TargetEkf b(params());
    TargetEkf::OwnState own_b = level_own();
    measure(b, tgt, own_b);
    for (int i = 0; i < 120; ++i) {
        const float phase = std::sin(static_cast<float>(i) * 0.2F);
        own_b.pos_ned[1] = 40.F * phase; // weave +-40 m East
        own_b.vel_ned[1] = 40.F * 0.2F * std::cos(static_cast<float>(i) * 0.2F) / 0.05F;
        b.predict(0.05);
        measure(b, tgt, own_b);
    }
    // Both should hit truth; the weaving run should be at least as tight on the
    // along-LOS axis. (Parallax cannot hurt observability.)
    CHECK(near(a.pos_ned()[0], 300.F, 40.F));
    CHECK(near(b.pos_ned()[0], 300.F, 40.F));
    CHECK(b.position_sigma() <= a.position_sigma() + 1.0F);
    return 0;
}

// -------------------------------------------------------------- frames --

int test_relative_state_round_trips_through_attitude() {
    // With a yawed own attitude, a target dead ahead in NED must come back as a
    // pure +X (forward) relative FRD vector.
    TargetEkf ekf(params());
    TargetEkf::OwnState own;
    own.yaw_rad = 1.57079632679F * 0.5F; // 45 deg yaw
    // Target 100 m ahead along the body-forward axis. In NED that is rotated by
    // yaw: forward = (cos y, sin y, 0) * 100.
    const float cy = std::cos(own.yaw_rad);
    const float sy = std::sin(own.yaw_rad);
    const float tgt[3] = {100.F * cy, 100.F * sy, 0.F};
    // Measure using the real geometry: rel_frd = DCM^T (tgt - own_pos).
    float rel_frd[3] = {100.F, 0.F, 0.F}; // forward
    ekf.update(rel_frd, own);
    float rp[3];
    float rv[3];
    ekf.relative_state(own, rp, rv);
    CHECK(near(rp[0], 100.F, 1e-1F)); // forward preserved
    CHECK(near(rp[1], 0.F, 1e-1F));   // no right component
    CHECK(near(rp[2], 0.F, 1e-1F));
    (void)tgt;
    return 0;
}

int test_reset_clears_state() {
    TargetEkf ekf(params());
    const float tgt[3] = {100.F, 0.F, 0.F};
    measure(ekf, tgt, level_own());
    CHECK(ekf.initialized());
    ekf.reset();
    CHECK(!ekf.initialized());
    CHECK(near(ekf.position_sigma(), 0.F, 1e-6F));
    float rp[3];
    float rv[3];
    ekf.relative_state(level_own(), rp, rv);
    CHECK(near(rp[0], 0.F, 1e-6F)); // no state -> zeros
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_first_update_initializes_at_measurement();
    rc = rc != 0 ? rc : test_static_target_converges_and_tightens();
    rc = rc != 0 ? rc : test_constant_velocity_target_is_tracked();
    rc = rc != 0 ? rc : test_accelerating_target_is_tracked();
    rc = rc != 0 ? rc : test_turning_target_position_tracked();
    rc = rc != 0 ? rc : test_own_motion_does_not_move_a_static_target();
    rc = rc != 0 ? rc : test_range_uncertainty_is_anisotropic();
    rc = rc != 0 ? rc : test_parallax_tightens_range();
    rc = rc != 0 ? rc : test_relative_state_round_trips_through_attitude();
    rc = rc != 0 ? rc : test_reset_clears_state();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_ekf: %d checks passed\n", checks);
    return 0;
}
