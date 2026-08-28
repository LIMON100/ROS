// Lead-rendezvous solver tests (RIPOSTE-DUALEO-REQ-001 R-2).
//
// The property under test is that the vehicle is sent to where the target WILL
// be, not where the GCS saw it: for a crossing target the aim point must lead,
// and the ownship must actually arrive at the same time as the target. Several
// tests verify that arrival directly rather than checking the algebra, so a sign
// error in the quadratic cannot pass.
#include "Rendezvous.h"

#include <cmath>
#include <cstdint>
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

bool near(float a, float b, float tol = 1e-2F) {
    return std::fabs(a - b) <= tol;
}

constexpr uint64_t NOW = 100'000'000'000ULL; // arbitrary monotonic instant

RendezvousCue cue_at(float n, float e, float d, float vn = 0.F, float ve = 0.F,
                     float vd = 0.F, uint64_t when = NOW) {
    RendezvousCue c;
    c.pos_ned_m[0] = n;
    c.pos_ned_m[1] = e;
    c.pos_ned_m[2] = d;
    c.vel_ned_mps[0] = vn;
    c.vel_ned_mps[1] = ve;
    c.vel_ned_mps[2] = vd;
    c.mono_ns = when;
    return c;
}

// Does the ownship, flying straight at `speed` toward the aim point, arrive at
// the same place and time as the target? This is the property the solver exists
// to provide, independent of how it computes it.
bool arrival_consistent(const float own[3], const RendezvousCue& c,
                        const RendezvousResult& r, float speed) {
    float dist2 = 0.F;
    for (int i = 0; i < 3; ++i) {
        const float d = r.point_ned_m[i] - own[i];
        dist2 += d * d;
    }
    const float pursuer_t = std::sqrt(dist2) / speed;
    // Where the target is at t_go (measured from now).
    for (int i = 0; i < 3; ++i) {
        const float target_at_tgo = c.pos_ned_m[i] + (c.vel_ned_mps[i] * r.t_go_s);
        if (std::fabs(target_at_tgo - r.point_ned_m[i]) > 0.05F) {
            return false; // aim point is not where the target will be
        }
    }
    return std::fabs(pursuer_t - r.t_go_s) < 0.05F;
}

int test_static_cue_is_the_cue_point() {
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c = cue_at(100.F, 0.F, -20.F);
    const RendezvousResult r = solve_rendezvous(own, c, 10.F, NOW);
    CHECK(r.ok);
    CHECK(near(r.point_ned_m[0], 100.F));
    CHECK(near(r.point_ned_m[1], 0.F));
    CHECK(near(r.lead_m, 0.F)); // nothing to lead
    CHECK(near(r.t_go_s, std::sqrt((100.F * 100.F) + (20.F * 20.F)) / 10.F, 0.1F));
    return 0;
}

int test_head_on_target_shortens_the_transit() {
    // Target 100 m north closing at 5 m/s; ownship at 10 m/s. They meet in
    // 100/15 s, north of the ownship but SHORT of the cue point.
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c = cue_at(100.F, 0.F, 0.F, -5.F, 0.F, 0.F);
    const RendezvousResult r = solve_rendezvous(own, c, 10.F, NOW);
    CHECK(r.ok);
    CHECK(near(r.t_go_s, 100.F / 15.F, 0.05F));
    CHECK(r.point_ned_m[0] < 100.F); // meet before the cue point
    CHECK(near(r.point_ned_m[0], 100.F - (5.F * r.t_go_s), 0.1F));
    CHECK(arrival_consistent(own, c, r, 10.F));
    return 0;
}

int test_crossing_target_produces_a_lead() {
    // The case the requirement is about: the target crosses, so aiming at the
    // cue point would put the vehicle behind it.
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c = cue_at(100.F, 0.F, 0.F, 0.F, 8.F, 0.F); // moving east
    const RendezvousResult r = solve_rendezvous(own, c, 12.F, NOW);
    CHECK(r.ok);
    CHECK(r.lead_m > 10.F);               // a real lead, not a rounding artefact
    CHECK(r.point_ned_m[1] > 0.F);        // aim east of the cue
    CHECK(near(r.point_ned_m[0], 100.F)); // ...but at the same northing
    CHECK(near(r.point_ned_m[1], 8.F * r.t_go_s, 0.1F));
    CHECK(arrival_consistent(own, c, r, 12.F));
    return 0;
}

int test_receding_slower_target_is_still_caught() {
    // Target running away at 4 m/s, ownship at 10 m/s: catchable, and the aim
    // point is BEYOND the cue.
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c = cue_at(100.F, 0.F, 0.F, 4.F, 0.F, 0.F);
    const RendezvousResult r = solve_rendezvous(own, c, 10.F, NOW);
    CHECK(r.ok);
    CHECK(near(r.t_go_s, 100.F / 6.F, 0.05F)); // closing speed 10-4
    CHECK(r.point_ned_m[0] > 100.F);
    CHECK(arrival_consistent(own, c, r, 10.F));
    return 0;
}

int test_faster_receding_target_has_no_solution() {
    // Target outruns the ownship directly away: no meeting point exists. The
    // result must say so AND still aim somewhere sensible (direct tracking).
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c = cue_at(100.F, 0.F, 0.F, 20.F, 0.F, 0.F);
    const RendezvousResult r = solve_rendezvous(own, c, 8.F, NOW);
    CHECK(!r.ok);
    CHECK(near(r.point_ned_m[0], 100.F)); // the target's current position
    return 0;
}

int test_faster_crossing_target_can_still_be_reached() {
    // A target faster than the ownship is not automatically hopeless: crossing
    // geometry can still yield a meeting point. This is the case a naive
    // "closing speed = s - |v|" formulation gets wrong.
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c = cue_at(100.F, -200.F, 0.F, 0.F, 12.F, 0.F);
    const RendezvousResult r = solve_rendezvous(own, c, 10.F, NOW);
    CHECK(r.ok);
    CHECK(r.t_go_s > 0.F);
    CHECK(arrival_consistent(own, c, r, 10.F));
    return 0;
}

int test_cue_is_aged_forward() {
    // A cue taken 4 s ago describes a target that has since moved. The solver
    // must start from where it is NOW, not from the stale report.
    const float own[3] = {0.F, 0.F, 0.F};
    const uint64_t taken = NOW - 4'000'000'000ULL;
    const RendezvousCue c = cue_at(100.F, 0.F, 0.F, 0.F, 5.F, 0.F, taken);
    const RendezvousResult r = solve_rendezvous(own, c, 10.F, NOW);
    CHECK(r.ok);
    // 4 s of eastward drift is already banked before any lead is added.
    CHECK(r.point_ned_m[1] > 20.F);
    return 0;
}

int test_stale_static_cue_is_unchanged() {
    // Ageing a cue with no reported motion must not move it.
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c =
        cue_at(50.F, 30.F, -10.F, 0.F, 0.F, 0.F, NOW - 30'000'000'000ULL);
    const RendezvousResult r = solve_rendezvous(own, c, 6.F, NOW);
    CHECK(r.ok);
    CHECK(near(r.point_ned_m[0], 50.F));
    CHECK(near(r.point_ned_m[1], 30.F));
    return 0;
}

int test_absurdly_distant_solution_is_refused() {
    // A solution minutes away is extrapolating constant velocity far past where
    // it is credible; the solver reports not-ok so the caller tracks instead.
    const float own[3] = {0.F, 0.F, 0.F};
    // Nearly matched speeds on a shallow tail chase: t_go blows up.
    const RendezvousCue c = cue_at(1000.F, 0.F, 0.F, 9.99F, 0.F, 0.F);
    const RendezvousResult r = solve_rendezvous(own, c, 10.F, NOW);
    CHECK(!r.ok);
    CHECK(near(r.point_ned_m[0], 1000.F));
    return 0;
}

int test_already_on_target() {
    const float own[3] = {10.F, 10.F, -5.F};
    const RendezvousCue c = cue_at(10.F, 10.F, -5.F, 3.F, 0.F, 0.F);
    const RendezvousResult r = solve_rendezvous(own, c, 10.F, NOW);
    CHECK(r.ok);
    CHECK(near(r.t_go_s, 0.F, 0.1F));
    return 0;
}

int test_zero_speed_falls_back_to_the_target() {
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c = cue_at(100.F, 0.F, 0.F, 5.F, 0.F, 0.F);
    const RendezvousResult r = solve_rendezvous(own, c, 0.F, NOW);
    CHECK(!r.ok);
    CHECK(near(r.point_ned_m[0], 100.F));
    return 0;
}

int test_three_dimensional_rendezvous() {
    // Descending crossing target: the vertical component must be led too.
    const float own[3] = {0.F, 0.F, 0.F};
    const RendezvousCue c = cue_at(80.F, 40.F, -50.F, -3.F, 6.F, 2.F);
    const RendezvousResult r = solve_rendezvous(own, c, 12.F, NOW);
    CHECK(r.ok);
    CHECK(arrival_consistent(own, c, r, 12.F));
    CHECK(r.point_ned_m[2] > -50.F); // target descending -> aim lower
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_static_cue_is_the_cue_point();
    rc = rc != 0 ? rc : test_head_on_target_shortens_the_transit();
    rc = rc != 0 ? rc : test_crossing_target_produces_a_lead();
    rc = rc != 0 ? rc : test_receding_slower_target_is_still_caught();
    rc = rc != 0 ? rc : test_faster_receding_target_has_no_solution();
    rc = rc != 0 ? rc : test_faster_crossing_target_can_still_be_reached();
    rc = rc != 0 ? rc : test_cue_is_aged_forward();
    rc = rc != 0 ? rc : test_stale_static_cue_is_unchanged();
    rc = rc != 0 ? rc : test_absurdly_distant_solution_is_refused();
    rc = rc != 0 ? rc : test_already_on_target();
    rc = rc != 0 ? rc : test_zero_speed_falls_back_to_the_target();
    rc = rc != 0 ? rc : test_three_dimensional_rendezvous();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_rendezvous: %d checks passed\n", checks);
    return 0;
}
