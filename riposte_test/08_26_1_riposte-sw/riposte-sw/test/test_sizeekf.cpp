// Augmented-state size/range EKF tests (ESTIMATION-REQ §5.5 / EST-P7a).
//
// The whole point is observability: with the target size augmented into the
// state, an own-STATIONARY observer keeps the size/range ambiguity in the
// covariance (honest low confidence — no optimistic collapse), while an
// own-WEAVING observer separates size and range through parallax, so range
// converges toward truth even when the initial size prior is wrong. These are
// driven by a synthetic PERFECT observation (truth az/el/log-apparent), so they
// test the filter, not the camera.
#include "SizeRangeEkf.h"

#include <math.h>

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

SizeRangeEkf::Params params() {
    SizeRangeEkf::Params p;
    p.sigma_jerk = 1.0F;
    p.sigma_logsize = 0.005F;
    p.sigma_bearing_rad = 0.002F;
    p.sigma_logsize_meas = 0.10F;
    p.log_size_prior = std::log(0.35F); // assume a 0.35 m target
    p.init_size_unc = 0.6F;
    p.init_vel_unc = 20.F;
    p.init_acc_unc = 5.F;
    p.init_range_unc_rel = 0.5F;
    return p;
}

SizeRangeEkf::OwnState own_at(float pn, float pe, float pd = 0.F) {
    SizeRangeEkf::OwnState o;
    o.pos_ned[0] = pn;
    o.pos_ned[1] = pe;
    o.pos_ned[2] = pd;
    return o; // level attitude
}

// Perfect observation of a target at absolute NED `tgt` with true size
// `size_m`, seen from `own` (level attitude => FRD == NED).
void observe_truth(const float tgt[3], float size_m, const SizeRangeEkf::OwnState& own,
                   float& az, float& el, float& logapp) {
    const float d[3] = {tgt[0] - own.pos_ned[0], tgt[1] - own.pos_ned[1],
                        tgt[2] - own.pos_ned[2]};
    const float horiz = std::sqrt((d[0] * d[0]) + (d[1] * d[1]));
    const float rng = std::sqrt((horiz * horiz) + (d[2] * d[2]));
    az = std::atan2(d[1], d[0]);
    el = std::atan2(d[2], horiz);
    logapp = std::log(size_m) - std::log(rng);
}

int test_initializes_range_from_size_prior() {
    SizeRangeEkf ekf(params());
    const float tgt[3] = {200.F, 0.F, 0.F};
    const auto own = own_at(0.F, 0.F);
    float az = NAN;
    float el = NAN;
    float la = NAN;
    observe_truth(tgt, 0.35F, own, az, el, la); // true size matches the prior
    ekf.update(az, el, la, own);
    CHECK(ekf.initialized());
    // With the prior == truth, seeded range == true range.
    CHECK(near(ekf.range(own), 200.F, 5.F));
    return 0;
}

int test_stationary_keeps_size_range_ambiguity() {
    // Own stationary, target stationary, TRUE size differs from the prior. With
    // no parallax the filter cannot separate size from range, so the range
    // estimate stays biased AND its uncertainty stays large (honest low
    // confidence — the systematic error is NOT optimistically averaged away).
    SizeRangeEkf ekf(params());
    const float tgt[3] = {200.F, 0.F, 0.F};
    const float true_size = 0.7F; // twice the 0.35 m prior => true range differs
    const auto own = own_at(0.F, 0.F);
    float az = NAN;
    float el = NAN;
    float la = NAN;
    observe_truth(tgt, true_size, own, az, el, la);
    ekf.update(az, el, la, own);
    const float sig0 = ekf.range_sigma();
    for (int i = 0; i < 200; ++i) {
        ekf.predict(0.05);
        observe_truth(tgt, true_size, own, az, el, la);
        ekf.update(az, el, la, own);
    }
    // Range uncertainty did NOT collapse to near-zero: the ambiguity remains.
    CHECK(ekf.range_sigma() > 0.3F * sig0);
    // And the range is still biased away from truth (seeded from the wrong
    // prior; parallax would be needed to correct it).
    CHECK(std::fabs(ekf.range(own) - 400.F) > 30.F ||
          std::fabs(ekf.range(own) - 200.F) > 30.F);
    return 0;
}

int test_weaving_separates_size_and_range() {
    // Same wrong prior, but now the observer WEAVES laterally. Parallax makes
    // size and range separately observable, so range converges toward the TRUE
    // range (which, for true size 0.7 m seeded as 0.35 m, differs from the
    // initial seed) and its uncertainty shrinks well below the stationary case.
    SizeRangeEkf ekf(params());
    const float tgt[3] = {200.F, 0.F, 0.F};
    const float true_size = 0.7F;
    // First (stationary) init.
    auto own = own_at(0.F, 0.F);
    float az = NAN;
    float el = NAN;
    float la = NAN;
    observe_truth(tgt, true_size, own, az, el, la);
    ekf.update(az, el, la, own);
    const float seed_range = ekf.range(own);

    for (int i = 1; i < 400; ++i) {
        const float phase = std::sin(static_cast<float>(i) * 0.15F);
        own = own_at(0.F, 60.F * phase); // weave +-60 m East
        ekf.predict(0.05);
        observe_truth(tgt, true_size, own, az, el, la);
        ekf.update(az, el, la, own);
    }
    // Range converged toward truth (200 m), away from the wrong seed.
    CHECK(near(ekf.range(own_at(0.F, 0.F)), 200.F, 40.F));
    // The true range differs from the size-prior seed, and weaving moved us
    // toward truth: the estimate is meaningfully different from the naive seed.
    CHECK(std::fabs(ekf.range(own_at(0.F, 0.F)) - seed_range) > 15.F);
    // Uncertainty is tighter than the stationary-ambiguity floor.
    CHECK(ekf.range_sigma() < 60.F);
    // Log-size moved from the prior toward the true (larger) size.
    CHECK(ekf.log_size() > std::log(0.35F) + 0.05F);
    return 0;
}

int test_reset() {
    SizeRangeEkf ekf(params());
    const float tgt[3] = {100.F, 0.F, 0.F};
    const auto own = own_at(0.F, 0.F);
    float az = NAN;
    float el = NAN;
    float la = NAN;
    observe_truth(tgt, 0.35F, own, az, el, la);
    ekf.update(az, el, la, own);
    CHECK(ekf.initialized());
    ekf.reset();
    CHECK(!ekf.initialized());
    CHECK(near(ekf.range(own), 0.F, 1e-6F));
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_initializes_range_from_size_prior();
    rc = rc != 0 ? rc : test_stationary_keeps_size_range_ambiguity();
    rc = rc != 0 ? rc : test_weaving_separates_size_and_range();
    rc = rc != 0 ? rc : test_reset();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_sizeekf: %d checks passed\n", checks);
    return 0;
}
