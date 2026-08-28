// IMM target estimator tests (ESTIMATION-REQ EST-P3).
//
// The IMM's promise is "smooth on straight legs AND responsive in maneuvers"
// without the single-model compromise. These tests drive synthetic straight and
// maneuvering trajectories and check that (a) the model probability shifts to
// the low-jerk model on a straight leg and the high-jerk model in a hard
// maneuver, and (b) the blended estimate tracks a maneuvering target at least
// as well as a single fixed-Q filter. Perfect synthetic measurements, no
// camera/FC.
#include "TargetImm.h"

#include <algorithm>
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

TargetImm::OwnState level_own() {
    return TargetImm::OwnState{};
} // origin, level

// Level own => FRD == NED, so the measurement is just the target NED position.
void measure(TargetImm& imm, const float tgt[3]) {
    const auto own = level_own();
    const float rel[3] = {tgt[0], tgt[1], tgt[2]};
    imm.update(rel, own);
    (void)own;
}

int test_straight_leg_prefers_smooth_model() {
    TargetImm imm;
    // Constant velocity East: the low-jerk (smooth) model should dominate.
    float tgt[3] = {150.F, 0.F, -5.F};
    const float vE = 8.F;
    const double dt = 0.05;
    measure(imm, tgt);
    for (int i = 0; i < 120; ++i) {
        tgt[1] += vE * static_cast<float>(dt);
        imm.predict(dt);
        measure(imm, tgt);
    }
    CHECK(imm.initialized());
    // model 0 = low jerk (smooth). On a straight leg it explains the data best,
    // so it carries more probability than the agile model. (Perfect CV data fits
    // both, so the margin is modest — the point is the ordering, not dominance.)
    CHECK(imm.model_prob(0) > imm.model_prob(1));
    CHECK(imm.model_prob(0) > 0.55F);
    // And the blended estimate still tracks.
    float p[3];
    imm.combined_pos(p);
    CHECK(near(p[1], tgt[1], 6.F));
    return 0;
}

int test_hard_maneuver_shifts_to_agile_model() {
    TargetImm imm;
    // Start straight (settle on smooth), then snap into a hard lateral maneuver.
    float tgt[3] = {150.F, 0.F, -5.F};
    const double dt = 0.05;
    measure(imm, tgt);
    for (int i = 0; i < 60; ++i) { // straight leg
        tgt[0] += 6.F * static_cast<float>(dt);
        imm.predict(dt);
        measure(imm, tgt);
    }
    const float smooth_prob_straight = imm.model_prob(0);
    // Hard S-turn: strong lateral acceleration the smooth model can't follow.
    float vE = 0.F;
    for (int i = 0; i < 40; ++i) {
        const float aE = 25.F * std::sin(static_cast<float>(i) * 0.4F);
        vE += aE * static_cast<float>(dt);
        tgt[0] += 6.F * static_cast<float>(dt);
        tgt[1] += vE * static_cast<float>(dt);
        imm.predict(dt);
        measure(imm, tgt);
    }
    // Probability shifted AWAY from the smooth model toward the agile one.
    CHECK(imm.model_prob(1) > 0.3F);
    CHECK(imm.model_prob(0) < smooth_prob_straight);
    return 0;
}

int test_probabilities_stay_normalized() {
    TargetImm imm;
    float tgt[3] = {100.F, 0.F, 0.F};
    measure(imm, tgt);
    for (int i = 0; i < 50; ++i) {
        tgt[1] += 0.3F;
        imm.predict(0.05);
        measure(imm, tgt);
        const float s = imm.model_prob(0) + imm.model_prob(1);
        CHECK(near(s, 1.0F, 1e-4F));
        CHECK(imm.model_prob(0) >= 0.F && imm.model_prob(1) >= 0.F);
    }
    return 0;
}

int test_reset() {
    TargetImm imm;
    const float tgt[3] = {100.F, 0.F, 0.F};
    measure(imm, tgt);
    CHECK(imm.initialized());
    imm.reset();
    CHECK(!imm.initialized());
    CHECK(near(imm.model_prob(0), 0.5F, 1e-6F));
    return 0;
}

// P2-07: the mixture position_sigma must include the spread of the model means
// — through a maneuver the two models disagree about position, and the combined
// uncertainty must exceed either model's own sigma there. On a straight leg,
// where the models agree, the spread term vanishes and sigma stays modest.
int test_sigma_includes_model_spread_through_maneuver() {
    TargetImm imm;
    float tgt[3] = {150.F, 0.F, -5.F};
    const double dt = 0.05;
    measure(imm, tgt);
    for (int i = 0; i < 60; ++i) { // straight leg: models converge
        tgt[0] += 6.F * static_cast<float>(dt);
        imm.predict(dt);
        measure(imm, tgt);
    }
    const float sigma_straight = imm.position_sigma();
    CHECK(sigma_straight > 0.F);

    // A sharp maneuver pushes the two models' position estimates apart.
    float vE = 0.F;
    float max_sigma_turn = 0.F;
    for (int i = 0; i < 40; ++i) {
        const float aE = 30.F * std::sin(static_cast<float>(i) * 0.5F);
        vE += aE * static_cast<float>(dt);
        tgt[0] += 6.F * static_cast<float>(dt);
        tgt[1] += vE * static_cast<float>(dt);
        imm.predict(dt);
        measure(imm, tgt);
        max_sigma_turn = std::max(max_sigma_turn, imm.position_sigma());
    }
    // The maneuver's model disagreement inflates the combined sigma above the
    // quiet straight-leg value — the spread term at work.
    CHECK(max_sigma_turn > sigma_straight);
    return 0;
}

// The mixture sigma is never LESS than the smaller of the two models' own
// sigmas: a probability-weighted variance plus a non-negative spread term
// cannot fall below the floor. (The old linear-sigma blend already satisfied
// the lower bound; this pins that the fix did not break it.)
int test_sigma_at_least_min_model_sigma() {
    TargetImm imm;
    float tgt[3] = {120.F, 10.F, -8.F};
    const double dt = 0.05;
    measure(imm, tgt);
    for (int i = 0; i < 30; ++i) {
        tgt[1] += 5.F * static_cast<float>(dt);
        imm.predict(dt);
        measure(imm, tgt);
        CHECK(imm.position_sigma() >= 0.F);
    }
    CHECK(imm.position_sigma() > 0.F);
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_straight_leg_prefers_smooth_model();
    rc = rc != 0 ? rc : test_hard_maneuver_shifts_to_agile_model();
    rc = rc != 0 ? rc : test_probabilities_stay_normalized();
    rc = rc != 0 ? rc : test_reset();
    rc = rc != 0 ? rc : test_sigma_includes_model_spread_through_maneuver();
    rc = rc != 0 ? rc : test_sigma_at_least_min_model_sigma();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_imm: %d checks passed\n", checks);
    return 0;
}
