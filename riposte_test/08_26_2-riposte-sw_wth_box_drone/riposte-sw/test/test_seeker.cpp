// Unit tests for the seeker's pure perception logic (G11.2): Tracker
// (gating + alpha-beta filter) and TargetEstimator (pixel -> relative-NED
// geometry), plus a detect->track->estimate chain driven through a mock
// IDetector (HAL-mock host test, G17.13). No camera or NPU required.
#include "AssocCost.h"
#include "IDetector.h"
#include "IEmbedder.h"
#include "TargetEstimator.h"
#include "Tracker.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "riposte/Tunables.h"
#include "riposte/Types.h"

using namespace riposte;

namespace {

// Test-run counter, incremented by CHECK across all test functions.
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

Detection make_det(float cx, float cy, float size, float score, int cls = 0) {
    Detection d{};
    d.cx = cx;
    d.cy = cy;
    d.w = size;
    d.h = size;
    d.score = score;
    d.cls = cls;
    return d;
}

// Scriptable IDetector for the HAL-mock chain test (G17.13/G17.2). Each detect()
// call pops the next scripted frame of detections; `healthy_`/`init_ok_` model
// a faulted accelerator.
class MockDetector final : public IDetector {
public:
    std::vector<std::vector<Detection>> frames;
    bool init_ok = true;
    bool healthy_flag = true;
    size_t idx = 0;

    bool init() override { return init_ok; }
    bool detect(const Frame& /*f*/, std::vector<Detection>& out) override {
        if (!healthy_flag) {
            return false; // faulted: no detections produced this frame
        }
        out = (idx < frames.size()) ? frames.at(idx) : std::vector<Detection>{};
        ++idx;
        return true;
    }
    bool healthy() const override { return healthy_flag; }
    const char* name() const override { return "MockDetector"; }
};

const double DT = 1.0 / 30.0;

// ---- Tracker --------------------------------------------------------------

int test_tracker_acquire() {
    Tracker tk;
    const std::vector<Detection> dets{make_det(0.4F, 0.6F, 0.05F, 0.9F)};
    const Tracker::Track& glint = tk.update(dets, DT);
    CHECK(!glint.valid); // one hit: unconfirmed, nothing publish-eligible yet
    // Same detection again: residual 0, so the smoothed state is unchanged and
    // the second hit confirms the track (MIN_TRACK_HITS = 2).
    const Tracker::Track& t = tk.update(dets, DT);
    CHECK(t.valid);
    CHECK(t.id == 1);
    CHECK(std::fabs(t.cx - 0.4F) < 1e-5F);
    CHECK(std::fabs(t.cy - 0.6F) < 1e-5F);
    CHECK(std::fabs(t.size - 0.05F) < 1e-5F);
    CHECK(t.quality > tun::MIN_TRACK_QUALITY); // 0.5*0.45 + 0.5*0.9 = 0.675
    CHECK(t.misses == 0);
    return 0;
}

int test_tracker_class_filter() {
    Tracker tk;
    const std::vector<Detection> dets{make_det(0.5F, 0.5F, 0.05F, 0.95F, /*cls=*/3)};
    const Tracker::Track& t = tk.update(dets, DT);
    CHECK(!t.valid); // non-target class ignored
    return 0;
}

int test_tracker_alpha_beta() {
    Tracker tk;
    tk.update({make_det(0.5F, 0.5F, 0.05F, 0.9F)}, DT); // acquire at center
    // Move detection +0.02 in x, within the gate (120/1280 = 0.094 normalized).
    const Tracker::Track& t = tk.update({make_det(0.52F, 0.5F, 0.05F, 0.9F)}, DT);
    CHECK(t.valid);
    // Predicted center was 0.5 (v=0); residual 0.02 -> cx += alpha*0.02.
    CHECK(std::fabs(t.cx - (0.5F + (tun::TRACKER_ALPHA * 0.02F))) < 1e-5F);
    CHECK(std::fabs(t.vx - (tun::TRACKER_BETA * 0.02F)) < 1e-5F);
    return 0;
}

int test_tracker_gating() {
    Tracker tk;
    tk.update({make_det(0.5F, 0.5F, 0.05F, 0.9F)}, DT); // acquire at center
    tk.update({make_det(0.5F, 0.5F, 0.05F, 0.9F)}, DT); // confirm (2nd hit)
    // Detection far outside the gate: must be rejected -> counts as a miss.
    const Tracker::Track& t = tk.update({make_det(0.85F, 0.5F, 0.05F, 0.95F)}, DT);
    CHECK(t.valid);                        // track still alive, coasting
    CHECK(t.misses == 1);                  // the out-of-gate detection was not associated
    CHECK(std::fabs(t.cx - 0.5F) < 1e-5F); // center unchanged (no association)
    return 0;
}

int test_tracker_highest_score() {
    Tracker tk;
    tk.update({make_det(0.5F, 0.5F, 0.05F, 0.9F)}, DT); // acquire at center
    // Two in-gate detections; the higher score must win association.
    const std::vector<Detection> dets{make_det(0.49F, 0.5F, 0.05F, 0.6F),
                                      make_det(0.51F, 0.5F, 0.05F, 0.95F)};
    const Tracker::Track& t = tk.update(dets, DT);
    CHECK(t.cx > 0.5F); // pulled toward the 0.51 (higher-score) detection
    return 0;
}

int test_tracker_multi_spawn() {
    // Two well-separated detections spawn two distinct tracks with unique ids;
    // both confirm on the second frame and the higher-quality one is primary.
    Tracker tk;
    const std::vector<Detection> dets{make_det(0.25F, 0.5F, 0.05F, 0.9F),
                                      make_det(0.75F, 0.5F, 0.05F, 0.8F)};
    tk.update(dets, DT);                           // spawn (unconfirmed)
    const Tracker::Track& p = tk.update(dets, DT); // 2nd hit confirms both
    CHECK(tk.tracks().size() == 2);
    CHECK(p.valid);
    CHECK(tk.tracks().at(0).id != tk.tracks().at(1).id);
    // Both centers are tracked independently (no association cross-talk).
    CHECK(std::fabs(tk.tracks().at(0).cx - 0.25F) < 1e-5F);
    CHECK(std::fabs(tk.tracks().at(1).cx - 0.75F) < 1e-5F);
    return 0;
}

int test_tracker_multi_independent_update() {
    // Two targets, each moving; association keeps them separate frame to frame.
    Tracker tk;
    tk.update({make_det(0.25F, 0.5F, 0.05F, 0.9F), make_det(0.75F, 0.5F, 0.05F, 0.9F)},
              DT);
    const uint32_t id_left = tk.tracks().at(0).id;
    // Each detection nudges only its own track (targets stay far apart).
    tk.update({make_det(0.27F, 0.5F, 0.05F, 0.9F), make_det(0.73F, 0.5F, 0.05F, 0.9F)},
              DT);
    CHECK(tk.tracks().size() == 2);
    CHECK(tk.tracks().at(0).id == id_left); // ids stable across frames
    CHECK(tk.tracks().at(0).cx > 0.25F);    // left track pulled toward 0.27
    CHECK(tk.tracks().at(0).cx < 0.5F);     // but not toward the right target
    CHECK(tk.tracks().at(1).cx < 0.75F);    // right track pulled toward 0.73
    CHECK(tk.tracks().at(1).cx > 0.5F);
    return 0;
}

int test_primary_is_the_largest_target() {
    // R-8: with several targets up, the BIGGEST (= nearest, for comparable
    // physical sizes) is the one engaged — not the highest-scoring one.
    Tracker tk;
    const std::vector<Detection> dets{
        make_det(0.25F, 0.5F, 0.04F, 0.95F),  // small, best score
        make_det(0.75F, 0.5F, 0.10F, 0.60F)}; // large
    tk.update(dets, DT);
    const Tracker::Track& p = tk.update(dets, DT); // both confirmed
    CHECK(tk.tracks().size() == 2);
    CHECK(p.valid);
    CHECK(std::fabs(p.cx - 0.75F) < 1e-3F); // the large one, despite lower score
    return 0;
}

int test_larger_target_appearing_later_takes_over_before_commit() {
    // A smaller target confirms first; a bigger one shows up two frames later.
    // Until the control session commits, the bigger one must take the primary slot.
    Tracker tk;
    const std::vector<Detection> small{make_det(0.25F, 0.5F, 0.04F, 0.9F)};
    tk.update(small, DT);
    tk.update(small, DT); // small confirmed, and is primary (only candidate)
    const uint32_t small_id = tk.current().id;
    CHECK(tk.current().valid);

    const std::vector<Detection> both{make_det(0.25F, 0.5F, 0.04F, 0.9F),
                                      make_det(0.75F, 0.5F, 0.12F, 0.9F)};
    tk.update(both, DT);
    tk.update(both, DT); // the large one confirms
    CHECK(tk.current().id != small_id);
    CHECK(std::fabs(tk.current().cx - 0.75F) < 1e-3F);
    return 0;
}

int test_prelock_similar_sizes_do_not_flip_primary() {
    // Two confirmed tracks whose smoothed sizes cross by a few percent every
    // frame: without hysteresis the pre-lock reselection flips the primary on
    // each crossing, and every flip resets the scheduler's confirmation window
    // (TR-7 identity binding) — with both targets alive, confirmation then
    // never completes and no tracked object is ever published. The incumbent
    // must stand until a challenger is DECISIVELY larger.
    Tracker tk;
    const std::vector<Detection> seed{make_det(0.25F, 0.5F, 0.050F, 0.9F),
                                      make_det(0.75F, 0.5F, 0.048F, 0.9F)};
    tk.update(seed, DT);
    tk.update(seed, DT); // both confirmed; the larger left one is primary
    const uint32_t incumbent = tk.current().id;
    CHECK(tk.current().valid);
    for (int i = 0; i < 20; ++i) {
        // Per-frame box noise: the measured sizes swap ranks every frame.
        const float left = (i % 2 == 0) ? 0.048F : 0.052F;
        const float right = (i % 2 == 0) ? 0.052F : 0.048F;
        tk.update({make_det(0.25F, 0.5F, left, 0.9F), make_det(0.75F, 0.5F, right, 0.9F)},
                  DT);
        CHECK(tk.current().id == incumbent); // never flips inside the margin
    }
    return 0;
}

int test_prelock_decisively_larger_challenger_takes_over() {
    // The hysteresis must not defeat R-8: a challenger clearly past the switch
    // margin (a genuinely nearer object) still takes the primary before lock.
    Tracker tk;
    const std::vector<Detection> seed{make_det(0.25F, 0.5F, 0.050F, 0.9F),
                                      make_det(0.75F, 0.5F, 0.050F, 0.9F)};
    tk.update(seed, DT);
    tk.update(seed, DT);
    const uint32_t first = tk.current().id;
    for (int i = 0; i < 12; ++i) {
        tk.update(
            {make_det(0.25F, 0.5F, 0.050F, 0.9F), make_det(0.75F, 0.5F, 0.080F, 0.9F)},
            DT);
    }
    CHECK(tk.current().id != first);
    CHECK(std::fabs(tk.current().cx - 0.75F) < 1e-2F);
    return 0;
}

int test_committed_primary_is_not_stolen_by_a_bigger_target() {
    // Once locked, the engaged target stays engaged even if a bigger one appears
    // — the whole point of the sticky rule (S-3).
    Tracker tk;
    const std::vector<Detection> small{make_det(0.25F, 0.5F, 0.04F, 0.9F)};
    tk.update(small, DT);
    tk.update(small, DT);
    const uint32_t engaged = tk.current().id;
    tk.lock_primary(true); // the search scheduler confirmed this target

    const std::vector<Detection> both{make_det(0.25F, 0.5F, 0.04F, 0.9F),
                                      make_det(0.75F, 0.5F, 0.20F, 0.9F)};
    tk.update(both, DT);
    tk.update(both, DT);
    CHECK(tk.tracks().size() == 2);
    CHECK(tk.current().id == engaged); // not stolen
    CHECK(tk.primary_locked());
    return 0;
}

int test_locked_primary_still_hands_off_when_it_dies() {
    // Sticky must not mean "stuck": a locked primary that dies still hands over.
    Tracker tk;
    const std::vector<Detection> both{make_det(0.25F, 0.5F, 0.04F, 0.9F),
                                      make_det(0.75F, 0.5F, 0.10F, 0.9F)};
    tk.update(both, DT);
    tk.update(both, DT);
    tk.lock_primary(true);
    const uint32_t engaged = tk.current().id;
    // Feed only the OTHER target until the engaged one is dropped.
    float other_cx = 0.F;
    for (const auto& t : tk.tracks()) {
        if (t.id != engaged) {
            other_cx = t.cx;
        }
    }
    for (int i = 0; i <= tun::TRACKER_MAX_MISSES; ++i) {
        tk.update({make_det(other_cx, 0.5F, 0.05F, 0.9F)}, DT);
    }
    CHECK(tk.tracks().size() == 1);
    CHECK(tk.current().valid);
    CHECK(tk.current().id != engaged);
    return 0;
}

int test_confirmed_count_excludes_glints() {
    // R-8's target count: only confirmed tracks are counted, so a one-frame
    // glint never inflates what the operator is told.
    Tracker tk;
    CHECK(tk.confirmed_count() == 0);
    const std::vector<Detection> two{make_det(0.25F, 0.5F, 0.05F, 0.9F),
                                     make_det(0.75F, 0.5F, 0.05F, 0.9F)};
    tk.update(two, DT);
    CHECK(tk.tracks().size() == 2);
    CHECK(tk.confirmed_count() == 0); // spawned but unconfirmed
    tk.update(two, DT);
    CHECK(tk.confirmed_count() == 2); // both confirmed on the second hit
    // A third, transient detection appears for exactly one frame.
    tk.update({make_det(0.25F, 0.5F, 0.05F, 0.9F), make_det(0.75F, 0.5F, 0.05F, 0.9F),
               make_det(0.50F, 0.9F, 0.05F, 0.9F)},
              DT);
    CHECK(tk.tracks().size() == 3);
    CHECK(tk.confirmed_count() == 2); // the glint is not a target
    return 0;
}

int test_tracker_primary_sticky_then_handoff() {
    // Primary is the highest-quality track; it hands off to the survivor when it
    // is dropped after MAX_MISSES of coasting.
    Tracker tk;
    const std::vector<Detection> dets{make_det(0.30F, 0.5F, 0.05F, 0.7F),
                                      make_det(0.70F, 0.5F, 0.05F, 0.95F)};
    tk.update(dets, DT); // spawn (unconfirmed)
    tk.update(dets, DT); // confirm both; primary = higher quality
    const uint32_t first = tk.current().id;
    CHECK(tk.current().valid);
    // Locate the NON-primary track and keep feeding only it; the primary coasts
    // to death while the other survives.
    float other_cx = 0.F;
    for (const auto& t : tk.tracks()) {
        if (t.id != first) {
            other_cx = t.cx;
        }
    }
    for (int i = 0; i <= tun::TRACKER_MAX_MISSES; ++i) {
        tk.update({make_det(other_cx, 0.5F, 0.05F, 0.9F)}, DT);
    }
    CHECK(tk.tracks().size() == 1);  // dead primary reaped
    CHECK(tk.current().valid);       // primary handed off
    CHECK(tk.current().id != first); // to the surviving track
    return 0;
}

int test_tracker_capacity_cap() {
    // No more than TRACKER_MAX_TRACKS tracks are kept, even with more targets.
    // Space detections > gate (0.094) apart so each is a distinct target.
    Tracker tk;
    std::vector<Detection> dets;
    for (int i = 0; i < tun::TRACKER_MAX_TRACKS + 2; ++i) {
        const float cx = 0.02F + (0.105F * static_cast<float>(i));
        dets.push_back(make_det(cx, 0.5F, 0.03F, 0.9F));
    }
    tk.update(dets, DT);
    CHECK(tk.tracks().size() == static_cast<std::size_t>(tun::TRACKER_MAX_TRACKS));
    return 0;
}

int test_tracker_coast_and_drop() {
    Tracker tk;
    tk.update({make_det(0.5F, 0.5F, 0.05F, 0.9F)}, DT); // acquire
    tk.update({make_det(0.5F, 0.5F, 0.05F, 0.9F)}, DT); // confirm (2nd hit)
    // Miss decays quality but keeps the track until MAX_MISSES exceeded.
    const float q_before = tk.current().quality;
    const Tracker::Track& t1 = tk.update({}, DT); // 1st miss
    CHECK(t1.valid);
    CHECK(t1.quality < q_before); // decayed
    for (int i = 0; i < tun::TRACKER_MAX_MISSES - 1; ++i) {
        CHECK(tk.update({}, DT).valid); // misses 2..MAX_MISSES: still alive
    }
    const Tracker::Track& dropped = tk.update({}, DT); // MAX_MISSES+1 -> drop
    CHECK(!dropped.valid);
    return 0;
}

int test_tracker_confirmation_gate() {
    // A one-frame glint must never become a published (valid) primary; a target
    // seen MIN_TRACK_HITS consecutive frames must.
    Tracker tk;
    const Tracker::Track& glint = tk.update({make_det(0.5F, 0.5F, 0.05F, 0.99F)}, DT);
    CHECK(!glint.valid);
    CHECK(!tk.current().valid);
    CHECK(tk.tracks().size() == 1); // tracked internally, just not publish-eligible
    const Tracker::Track& confirmed = tk.update({make_det(0.51F, 0.5F, 0.05F, 0.9F)}, DT);
    CHECK(confirmed.valid);
    CHECK(confirmed.id == 1);
    return 0;
}

int test_tracker_aspect_size() {
    // 16:9 sensor: a bbox square IN PIXELS has h (height-normalized) = w*aspect.
    // size must come out width-normalized — max(w, h/aspect) — not max(w, h).
    const float aspect = 16.0F / 9.0F;
    Tracker tk(aspect);
    Detection d = make_det(0.5F, 0.5F, 0.05F, 0.9F);
    d.h = 0.05F * aspect; // same pixel extent vertically as horizontally
    tk.update({d}, DT);
    const Tracker::Track& t = tk.update({d}, DT);
    CHECK(t.valid);
    CHECK(std::fabs(t.size - 0.05F) < 1e-5F);
    return 0;
}

int test_tracker_aspect_gate() {
    // 16:9 at 1280x720: a vertical offset of 0.12 (height-normalized) is only
    // ~86 px — inside the 120 px gate. The aspect-corrected metric must
    // associate where the old mixed-units metric (0.12 > 120/1280) rejected.
    const float aspect = 16.0F / 9.0F;
    Tracker tk(aspect);
    tk.update({make_det(0.5F, 0.30F, 0.05F, 0.9F)}, DT);
    tk.update({make_det(0.5F, 0.30F, 0.05F, 0.9F)}, DT); // confirm
    const Tracker::Track& t = tk.update({make_det(0.5F, 0.42F, 0.05F, 0.9F)}, DT);
    CHECK(t.misses == 0);           // associated, not treated as a miss
    CHECK(t.cy > 0.30F);            // pulled toward the offset detection
    CHECK(tk.tracks().size() == 1); // and no spurious second track spawned
    return 0;
}

// ---- TargetEstimator ------------------------------------------------------

Tracker::Track valid_track(float cx, float cy, float size) {
    Tracker::Track t;
    t.valid = true;
    t.id = 7;
    t.cx = cx;
    t.cy = cy;
    t.size = size;
    t.quality = 0.9F;
    return t;
}

int test_estimator_invalid_rejected() {
    TargetEstimator est(TargetEstimator::Params{});
    TrackState out{};
    const Tracker::Track bad; // valid=false by default
    CHECK(!est.estimate(bad, 1000, DT, out));
    CHECK(out.valid == 0);

    Tracker::Track low = valid_track(0.5F, 0.5F, 0.05F);
    low.quality = tun::MIN_TRACK_QUALITY - 0.01F;
    CHECK(!est.estimate(low, 1000, DT, out));
    CHECK(out.valid == 0);
    return 0;
}

int test_estimator_centered() {
    const TargetEstimator::Params p;
    TargetEstimator est(p);
    TrackState out{};
    const float size = 0.05F;
    CHECK(est.estimate(valid_track(0.5F, 0.5F, size), 1000, DT, out));
    CHECK(out.valid == 1);
    CHECK(out.track_id == 7);
    // Centered: az=el=0 -> all range on +X (forward), Y and Z ~ 0.
    const float focal = 0.5F / std::tan(0.5F * p.hfov_rad);
    const float range = (p.target_size_m * focal) / size;
    CHECK(std::fabs(out.rel_pos_frd_m[0] - range) < 1e-3F);
    CHECK(std::fabs(out.rel_pos_frd_m[1]) < 1e-4F);
    CHECK(std::fabs(out.rel_pos_frd_m[2]) < 1e-4F);
    return 0;
}

int test_estimator_offaxis_signs() {
    TargetEstimator est(TargetEstimator::Params{});
    TrackState out{};
    // Target right-of-center and below-center -> +Y (right) and +Z (down) in FRD.
    CHECK(est.estimate(valid_track(0.7F, 0.7F, 0.05F), 1000, DT, out));
    CHECK(out.rel_pos_frd_m[1] > 0.F); // right
    CHECK(out.rel_pos_frd_m[2] > 0.F); // down
    return 0;
}

int test_estimator_range_inverse_size() {
    TargetEstimator est1(TargetEstimator::Params{});
    TargetEstimator est2(TargetEstimator::Params{});
    TrackState big{};
    TrackState small{};
    est1.estimate(valid_track(0.5F, 0.5F, 0.10F), 1000, DT, big);
    est2.estimate(valid_track(0.5F, 0.5F, 0.05F), 1000, DT, small);
    // Half the apparent size -> roughly double the range.
    CHECK(small.rel_pos_frd_m[0] > big.rel_pos_frd_m[0]);
    CHECK(std::fabs(small.rel_pos_frd_m[0] - (2.F * big.rel_pos_frd_m[0])) < 1e-2F);
    return 0;
}

int test_estimator_closing_velocity() {
    TargetEstimator est(TargetEstimator::Params{});
    TrackState a{};
    TrackState b{};
    // Frame 1: no previous sample -> zero velocity.
    CHECK(est.estimate(valid_track(0.5F, 0.5F, 0.05F), 0, DT, a));
    CHECK(std::fabs(a.rel_vel_frd_mps[0]) < 1e-6F);
    // Frame 2: target grew (closer) -> range shrinks -> negative X velocity.
    CHECK(est.estimate(valid_track(0.5F, 0.5F, 0.06F), 33'000'000, DT, b));
    CHECK(b.rel_vel_frd_mps[0] < 0.F); // closing
    return 0;
}

int test_estimator_id_switch_resets_velocity() {
    TargetEstimator est(TargetEstimator::Params{});
    TrackState out{};
    CHECK(est.estimate(valid_track(0.5F, 0.5F, 0.05F), 0, DT, out));
    CHECK(est.estimate(valid_track(0.5F, 0.5F, 0.06F), 33'000'000, DT, out));
    CHECK(out.rel_vel_frd_mps[0] < 0.F); // same track: finite difference active
    // Primary handoff: a new id at half the range must NOT produce a huge
    // one-tick velocity spike — the differentiator resets and publishes zero.
    Tracker::Track other = valid_track(0.5F, 0.5F, 0.10F);
    other.id = 8;
    CHECK(est.estimate(other, 66'000'000, DT, out));
    CHECK(std::fabs(out.rel_vel_frd_mps[0]) < 1e-6F);
    CHECK(std::fabs(out.rel_vel_frd_mps[1]) < 1e-6F);
    CHECK(std::fabs(out.rel_vel_frd_mps[2]) < 1e-6F);
    // Next tick with the SAME new id differentiates again (seeded from current).
    other.size = 0.11F;
    CHECK(est.estimate(other, 99'000'000, DT, out));
    CHECK(out.rel_vel_frd_mps[0] < 0.F);
    return 0;
}

// ---- detect -> track -> estimate chain through a mock IDetector -----------

int test_chain_nominal() {
    MockDetector det;
    // Approaching target drifting right: growing size, cx increasing slightly.
    det.frames = {
        {make_det(0.50F, 0.5F, 0.05F, 0.9F)},
        {make_det(0.51F, 0.5F, 0.055F, 0.9F)},
        {make_det(0.52F, 0.5F, 0.060F, 0.9F)},
    };
    CHECK(det.init());
    Tracker tk;
    TargetEstimator est(TargetEstimator::Params{});
    const Frame f{};
    std::vector<Detection> out;
    TrackState ts{};
    bool any_valid = false;
    for (uint64_t i = 0; i < det.frames.size(); ++i) {
        CHECK(det.detect(f, out));
        const Tracker::Track& t = tk.update(out, DT);
        if (est.estimate(t, (i + 1) * 33'000'000ULL, DT, ts)) {
            any_valid = true;
            CHECK(ts.valid == 1);
            CHECK(ts.rel_pos_frd_m[0] > 0.F); // in front
        }
    }
    CHECK(any_valid);
    return 0;
}

int test_chain_detector_fault() {
    MockDetector det;
    det.frames = {{make_det(0.5F, 0.5F, 0.05F, 0.9F)},
                  {make_det(0.5F, 0.5F, 0.05F, 0.9F)}};
    Tracker tk;
    TargetEstimator est(TargetEstimator::Params{});
    const Frame f{};
    std::vector<Detection> out;
    TrackState ts{};

    // Acquire + confirm a track on the good frames.
    CHECK(det.detect(f, out));
    CHECK(!tk.update(out, DT).valid); // first hit: not yet confirmed
    CHECK(det.detect(f, out));
    CHECK(tk.update(out, DT).valid);
    CHECK(est.estimate(tk.current(), 33'000'000, DT, ts));

    // Detector faults: detect() returns false, no detections. The track must
    // coast then drop, and the estimator must report valid=0 (=> SM-7 upstream).
    det.healthy_flag = false;
    bool dropped = false;
    for (int i = 0; i < tun::TRACKER_MAX_MISSES + 2; ++i) {
        out.clear();
        (void)det.detect(f, out); // returns false; out stays empty
        const Tracker::Track& t = tk.update(out, DT);
        const bool ok = est.estimate(t, (i + 2) * 33'000'000ULL, DT, ts);
        if (!t.valid) {
            CHECK(!ok);
            CHECK(ts.valid == 0);
            dropped = true;
            break;
        }
    }
    CHECK(dropped);
    return 0;
}

// ---- T1 ReID association wiring (TRACKER-REQ TR-B) -------------------------

Embedding unit_emb(float x, float y, float z) {
    Embedding e;
    e.v = {x, y, z};
    l2_normalize(e.v);
    return e;
}

// Spawns two confirmed tracks far apart (spawn gating forbids close births),
// with distinct appearance galleries: id1 "red" left, id2 "blue" right.
int seed_two_tracks(Tracker& tk, const Embedding& red, const Embedding& blue) {
    const std::vector<Detection> f1{make_det(0.40F, 0.5F, 0.05F, 0.9F),
                                    make_det(0.60F, 0.5F, 0.05F, 0.9F)};
    tk.update(f1, {red, blue}, DT);
    const std::vector<Detection> f2{make_det(0.46F, 0.5F, 0.05F, 0.9F),
                                    make_det(0.54F, 0.5F, 0.05F, 0.9F)};
    tk.update(f2, {red, blue}, DT);
    CHECK(tk.tracks().size() == 2);
    CHECK(tk.tracks()[0].valid && tk.tracks()[1].valid);
    CHECK(tk.tracks()[0].id == 1 && tk.tracks()[1].id == 2);
    return 0;
}

int test_t1_crossing_preserves_track_ids() {
    // THE TR-1 scenario at tracker level: the targets converge and cross. From
    // the crossing frame on, each detection lies closer to the OTHER track's
    // prediction, so motion alone would swap the IDs; appearance hard-reject
    // forbids the swapped pairings and each track follows its own target
    // through the cross.
    Tracker tk;
    const Embedding red = unit_emb(1.F, 0.F, 0.F);
    const Embedding blue = unit_emb(0.F, 1.F, 0.F);
    if (seed_two_tracks(tk, red, blue) != 0) {
        return 1;
    }
    const std::vector<Detection> f3{make_det(0.52F, 0.5F, 0.05F, 0.9F),
                                    make_det(0.48F, 0.5F, 0.05F, 0.9F)};
    tk.update(f3, {red, blue}, DT); // red target now RIGHT of blue
    const std::vector<Detection> f4{make_det(0.56F, 0.5F, 0.05F, 0.9F),
                                    make_det(0.44F, 0.5F, 0.05F, 0.9F)};
    tk.update(f4, {red, blue}, DT);

    CHECK(tk.tracks().size() == 2);
    const Tracker::Track& t1 = tk.tracks()[0]; // id 1: seeded red, started LEFT
    const Tracker::Track& t2 = tk.tracks()[1]; // id 2: seeded blue, started RIGHT
    CHECK(t1.id == 1 && t2.id == 2);
    CHECK(t1.misses == 0 && t2.misses == 0); // both matched through the cross
    // Identity followed appearance, not position: red (id 1) is now RIGHT.
    CHECK(t1.cx > 0.5F && t2.cx < 0.5F);
    float d = -1.F;
    CHECK(cosine_dist(t1.emb, red, d) && d < 0.1F); // gallery stayed red
    CHECK(cosine_dist(t2.emb, blue, d) && d < 0.1F);
    return 0;
}

int test_t1_impostor_on_predicted_position_is_rejected() {
    // A detection standing exactly where a track predicts, but wearing the
    // OTHER target's appearance, must not be associated: both tracks coast
    // (one miss each) instead of quietly exchanging identities.
    Tracker tk;
    const Embedding red = unit_emb(1.F, 0.F, 0.F);
    const Embedding blue = unit_emb(0.F, 1.F, 0.F);
    if (seed_two_tracks(tk, red, blue) != 0) {
        return 1;
    }
    const float cx1 = tk.tracks()[0].cx;
    const float cx2 = tk.tracks()[1].cx;
    // Impostors: swapped embeddings on (nearly) the predicted positions.
    const std::vector<Detection> dets{
        make_det(cx1 + tk.tracks()[0].vx, 0.5F, 0.05F, 0.9F),
        make_det(cx2 + tk.tracks()[1].vx, 0.5F, 0.05F, 0.9F)};
    tk.update(dets, {blue, red}, DT);
    CHECK(tk.tracks().size() == 2); // rejected dets are in-gate: no spawns
    CHECK(tk.tracks()[0].misses == 1 && tk.tracks()[1].misses == 1);
    return 0;
}

int test_t1_missing_or_misaligned_embeddings_fall_back_to_legacy() {
    Tracker tk;
    const std::vector<Detection> dets{make_det(0.4F, 0.5F, 0.05F, 0.9F)};
    // Misaligned embedding list (wrong size): legacy path, association works.
    const std::vector<Embedding> misaligned(3);
    tk.update(dets, misaligned, DT);
    tk.update(dets, misaligned, DT);
    CHECK(tk.tracks().size() == 1 && tk.tracks()[0].hits == 2);

    // Aligned but ALL invalid: still the legacy path (nothing to compare).
    Tracker tk2;
    const std::vector<Embedding> invalid(1);
    tk2.update(dets, invalid, DT);
    tk2.update(dets, invalid, DT);
    CHECK(tk2.tracks().size() == 1 && tk2.tracks()[0].hits == 2);
    return 0;
}

int test_synthetic_embedder_is_deterministic() {
    SyntheticEmbedder se;
    CHECK(se.init());
    const Frame f{};
    const std::vector<Detection> dets{make_det(0.3F, 0.3F, 0.05F, 0.9F),
                                      make_det(0.8F, 0.8F, 0.05F, 0.9F, /*cls=*/1)};
    std::vector<Embedding> a;
    std::vector<Embedding> b;
    CHECK(se.embed(f, dets, a) && se.embed(f, dets, b));
    CHECK(a.size() == 2 && b.size() == 2);
    float d = -1.F;
    CHECK(cosine_dist(a[0], b[0], d) && d < 1e-6F); // same det -> same vector
    CHECK(cosine_dist(a[0], a[1], d) && d > 0.1F);  // different targets differ
    float n2 = 0.F;
    for (const float x : a[0].v) {
        n2 += x * x;
    }
    CHECK(std::fabs(n2 - 1.F) < 1e-5F); // unit norm
    return 0;
}

// ---- R-12: narrow-channel samples carry more weight (S-15) ----
// The narrow channel resolves ~3.8x finer, so its measurement should move the
// smoothed estimate further than a wide one for the SAME residual. Without the
// weighting both channels move it identically and the extra resolution is
// thrown away.
int test_narrow_sample_moves_the_estimate_further() {
    const std::vector<Detection> seed{make_det(0.50F, 0.50F, 0.05F, 0.9F)};
    const std::vector<Detection> moved{make_det(0.55F, 0.50F, 0.05F, 0.9F)};

    Tracker wide;
    wide.update(seed, DT);
    wide.update(seed, DT);
    wide.set_measurement_gain_scale(1.F); // wide: tuned baseline
    const float wide_cx = wide.update(moved, DT).cx;

    Tracker narrow;
    narrow.update(seed, DT);
    narrow.update(seed, DT);
    narrow.set_measurement_gain_scale(tun::NARROW_GAIN_SCALE);
    const float narrow_cx = narrow.update(moved, DT).cx;

    // Both move toward the measurement; the narrow one goes further.
    CHECK(wide_cx > 0.50F && wide_cx < 0.55F);
    CHECK(narrow_cx > wide_cx);
    CHECK(narrow_cx <= 0.55F); // never overshoots the measurement
    return 0;
}

int test_gain_scale_is_clamped_and_defaults_to_baseline() {
    const std::vector<Detection> seed{make_det(0.50F, 0.50F, 0.05F, 0.9F)};
    const std::vector<Detection> moved{make_det(0.55F, 0.50F, 0.05F, 0.9F)};

    // Default (never set) must reproduce the tuned single-camera behaviour.
    Tracker base;
    base.update(seed, DT);
    base.update(seed, DT);
    const float base_cx = base.update(moved, DT).cx;

    // A wild value cannot retune the filter: it clamps to the ceiling, and a
    // value below 1 cannot make the tracker trust measurements LESS than tuned.
    Tracker wild;
    wild.update(seed, DT);
    wild.update(seed, DT);
    wild.set_measurement_gain_scale(1000.F);
    const float wild_cx = wild.update(moved, DT).cx;
    CHECK(wild_cx <= 0.55F);

    Tracker low;
    low.update(seed, DT);
    low.update(seed, DT);
    low.set_measurement_gain_scale(0.01F);
    const float low_cx = low.update(moved, DT).cx;
    CHECK(std::fabs(low_cx - base_cx) < 1e-6F);
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_tracker_acquire();
    rc = rc != 0 ? rc : test_narrow_sample_moves_the_estimate_further();
    rc = rc != 0 ? rc : test_gain_scale_is_clamped_and_defaults_to_baseline();
    rc = rc != 0 ? rc : test_tracker_class_filter();
    rc = rc != 0 ? rc : test_tracker_alpha_beta();
    rc = rc != 0 ? rc : test_tracker_gating();
    rc = rc != 0 ? rc : test_tracker_highest_score();
    rc = rc != 0 ? rc : test_tracker_multi_spawn();
    rc = rc != 0 ? rc : test_tracker_multi_independent_update();
    rc = rc != 0 ? rc : test_primary_is_the_largest_target();
    rc = rc != 0 ? rc : test_larger_target_appearing_later_takes_over_before_commit();
    rc = rc != 0 ? rc : test_prelock_similar_sizes_do_not_flip_primary();
    rc = rc != 0 ? rc : test_prelock_decisively_larger_challenger_takes_over();
    rc = rc != 0 ? rc : test_committed_primary_is_not_stolen_by_a_bigger_target();
    rc = rc != 0 ? rc : test_locked_primary_still_hands_off_when_it_dies();
    rc = rc != 0 ? rc : test_confirmed_count_excludes_glints();
    rc = rc != 0 ? rc : test_tracker_primary_sticky_then_handoff();
    rc = rc != 0 ? rc : test_tracker_capacity_cap();
    rc = rc != 0 ? rc : test_tracker_coast_and_drop();
    rc = rc != 0 ? rc : test_tracker_confirmation_gate();
    rc = rc != 0 ? rc : test_tracker_aspect_size();
    rc = rc != 0 ? rc : test_tracker_aspect_gate();
    rc = rc != 0 ? rc : test_estimator_invalid_rejected();
    rc = rc != 0 ? rc : test_estimator_centered();
    rc = rc != 0 ? rc : test_estimator_offaxis_signs();
    rc = rc != 0 ? rc : test_estimator_range_inverse_size();
    rc = rc != 0 ? rc : test_estimator_closing_velocity();
    rc = rc != 0 ? rc : test_estimator_id_switch_resets_velocity();
    rc = rc != 0 ? rc : test_chain_nominal();
    rc = rc != 0 ? rc : test_chain_detector_fault();
    rc = rc != 0 ? rc : test_t1_crossing_preserves_track_ids();
    rc = rc != 0 ? rc : test_t1_impostor_on_predicted_position_is_rejected();
    rc = rc != 0 ? rc : test_t1_missing_or_misaligned_embeddings_fall_back_to_legacy();
    rc = rc != 0 ? rc : test_synthetic_embedder_is_deterministic();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_seeker: %d checks passed\n", checks);
    return 0;
}
