// Wide-channel motion candidate generation (DUALEO-REQ R-13, §4.2).
//
// The scenes are synthetic but the geometry is real: a textured background is
// transformed by a KNOWN similarity (the ownship's apparent motion) and a small
// blob is placed so that it does NOT follow that transform. The module must
// recover the background motion and report the blob — and, just as important,
// report NOTHING when the only motion present is the background's, because a
// false candidate sends the narrow channel chasing empty sky.
//
// Pure logic: no camera, no NPU, no OpenCV (G11.2/S-9).
#include "IDetector.h"
#include "MotionCandidates.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

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

constexpr int W = 320;
constexpr int H = 240;
constexpr uint32_t FOURCC_NV12 = 0x3231564E;

// A deterministic, broadband "landscape": several sinusoids at incommensurate
// frequencies plus a value-noise term. It must be rich enough for the corner
// detector and NOT periodic, or block matching finds many equally good
// alignments and the uniqueness test (correctly) rejects everything.
uint8_t background(float x, float y) {
    const float v = (40.F * std::sin((x * 0.11F) + (y * 0.037F))) +
                    (30.F * std::sin((x * 0.023F) - (y * 0.089F))) +
                    (25.F * std::sin((x * 0.181F) + (y * 0.151F))) +
                    (18.F * std::sin((x * 0.005F) + (y * 0.31F)));
    // Value noise: hashed lattice, bilinearly blended, so it survives the
    // decimation box filter instead of averaging to a constant.
    const float lx = x / 7.F;
    const float ly = y / 7.F;
    const int xi = static_cast<int>(std::floor(lx));
    const int yi = static_cast<int>(std::floor(ly));
    const float fx = lx - static_cast<float>(xi);
    const float fy = ly - static_cast<float>(yi);
    auto hash = [](int a, int b) {
        uint32_t hv = (static_cast<uint32_t>(a) * 73856093U) ^
                      (static_cast<uint32_t>(b) * 19349663U);
        hv ^= hv >> 13U;
        hv *= 1274126177U;
        return static_cast<float>((hv >> 16U) & 0xFFU) - 128.F;
    };
    const float n00 = hash(xi, yi);
    const float n10 = hash(xi + 1, yi);
    const float n01 = hash(xi, yi + 1);
    const float n11 = hash(xi + 1, yi + 1);
    const float ntop = n00 + ((n10 - n00) * fx);
    const float nbot = n01 + ((n11 - n01) * fx);
    const float n = 0.35F * (ntop + ((nbot - ntop) * fy));
    const float s = 128.F + v + n;
    const float clamped = std::max(0.F, std::min(255.F, s));
    return static_cast<uint8_t>(clamped);
}

// NV12 buffer whose luma is the background sampled through the INVERSE of the
// given similarity, so the rendered frame shows the scene after that motion.
// `blob` (when sized > 0) paints an opaque square that is placed in FRAME
// coordinates — i.e. it does not follow the background transform.
struct Scene {
    std::vector<uint8_t> buf;
    Frame frame{};
};

Scene render(float a, float b, float tx, float ty, int blob_x, int blob_y,
             int blob_size) {
    Scene s;
    s.buf.assign(static_cast<size_t>(W) * H * 3U / 2U, 128);
    const float det = (a * a) + (b * b);
    const float ia = a / det;
    const float ib = b / det;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const float qx = static_cast<float>(x) - tx;
            const float qy = static_cast<float>(y) - ty;
            const float sx = (ia * qx) + (ib * qy);
            const float sy = (-ib * qx) + (ia * qy);
            s.buf[(static_cast<size_t>(y) * W) + static_cast<size_t>(x)] =
                background(sx, sy);
        }
    }
    for (int j = 0; j < blob_size; ++j) {
        for (int i = 0; i < blob_size; ++i) {
            const int px = blob_x + i;
            const int py = blob_y + j;
            if (px >= 0 && py >= 0 && px < W && py < H) {
                s.buf[(static_cast<size_t>(py) * W) + static_cast<size_t>(px)] = 250;
            }
        }
    }
    s.frame.width = W;
    s.frame.height = H;
    s.frame.stride = W;
    s.frame.fourcc = FOURCC_NV12;
    s.frame.data = s.buf.data();
    return s;
}

// Same scene, at an arbitrary size. Frame size matters for this module: under
// rotation or scale the displacement grows with distance from the transform's
// fixed point, so a defect that a small frame hides shows up on a big one.
struct SizedScene {
    std::vector<uint8_t> buf;
    Frame frame{};
};

SizedScene render_at(int w, int h, float a, float b, float tx, float ty) {
    SizedScene s;
    s.buf.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 3U / 2U, 128);
    const float det = (a * a) + (b * b);
    const float ia = a / det;
    const float ib = b / det;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float qx = static_cast<float>(x) - tx;
            const float qy = static_cast<float>(y) - ty;
            s.buf[(static_cast<size_t>(y) * static_cast<size_t>(w)) +
                  static_cast<size_t>(x)] =
                background((ia * qx) + (ib * qy), (-ib * qx) + (ia * qy));
        }
    }
    s.frame.width = w;
    s.frame.height = h;
    s.frame.stride = static_cast<size_t>(w);
    s.frame.fourcc = FOURCC_NV12;
    s.frame.data = s.buf.data();
    return s;
}

MotionCandidates::Params test_params() {
    MotionCandidates::Params p;
    p.decimate = 2;          // the test frames are small
    p.blob_min_frac = 0.01F; // >= ~3 px at W=320
    p.blob_max_frac = 0.15F;
    return p;
}

bool near(float a, float b, float tol) {
    return std::fabs(a - b) <= tol;
}

// --- ego-motion recovery ---------------------------------------------------

int test_recovers_pure_translation() {
    MotionCandidates m(test_params());
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    const Scene cur = render(1.F, 0.F, 6.F, -4.F, 0, 0, 0);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    // Reported in FULL-RESOLUTION pixels regardless of the internal decimation:
    // the caller works in frame coordinates and should not have to know.
    CHECK(near(r.ego.tx, 6.F, 0.4F));
    CHECK(near(r.ego.ty, -4.F, 0.4F));
    CHECK(near(r.ego.a, 1.F, 0.01F));
    CHECK(near(r.ego.b, 0.F, 0.01F));
    return 0;
}

// Sub-pixel accuracy is not a nicety here: the residual pass compares
// interpolated intensities, so a fraction of a pixel of misalignment on
// textured ground produces a residual comparable to a real moving object.
// Block matching is integer-only, so the exposing case is a motion that is NOT
// a whole number of DECIMATED pixels — without the parabola refinement the
// estimate snaps to the decimation grid, i.e. multiples of 4 full-resolution
// px here. The bound below is a fifth of that step, so it fails if the
// refinement is removed. (Accuracy scales with the correspondence count: this
// 320x240 frame yields ~14 at decimate=4, where a 640x480 one yields ~70 and
// lands inside 0.15 px.)
int test_subpixel_translation_accuracy() {
    MotionCandidates::Params p = test_params();
    p.decimate = 4; // 6 px of motion is 1.5 decimated px: not representable
    MotionCandidates m(p);
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    const Scene cur = render(1.F, 0.F, 6.F, -4.F, 0, 0, 0);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    CHECK(near(r.ego.tx, 6.F, 0.8F));
    CHECK(near(r.ego.ty, -4.F, 0.8F));
    return 0;
}

int test_recovers_rotation_and_scale() {
    MotionCandidates m(test_params());
    const float theta = 0.05F; // ~2.9 deg of roll between frames
    const float scale = 1.02F;
    const float a = scale * std::cos(theta);
    const float b = scale * std::sin(theta);
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    const Scene cur = render(a, b, 2.F, 1.F, 0, 0, 0);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    // Rotation and scale are decimation-invariant; only translation scales.
    CHECK(near(r.ego.a, a, 0.03F));
    CHECK(near(r.ego.b, b, 0.03F));
    return 0;
}

// --- the point of the module: independent motion under ego-motion ----------

int test_finds_a_blob_that_does_not_follow_the_background() {
    MotionCandidates m(test_params());
    // The background pans by 6 px while the blob moves the OTHER way — the case
    // a plain frame difference cannot separate.
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 150, 120, 6);
    const Scene cur = render(1.F, 0.F, 6.F, 0.F, 132, 120, 6);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    CHECK(!r.candidates.empty());
    // A candidate must land on one of the two blob positions (the object
    // vacates one and occupies the other; either is a legitimate report).
    bool hit = false;
    for (const auto& c : r.candidates) {
        const float px = c.cx * static_cast<float>(W);
        const float py = c.cy * static_cast<float>(H);
        if (std::fabs(py - 123.F) < 12.F &&
            (std::fabs(px - 153.F) < 12.F || std::fabs(px - 135.F) < 12.F)) {
            hit = true;
        }
    }
    CHECK(hit);
    CHECK(r.candidates.front().score > 0.F);
    return 0;
}

// THE regression this verification was written for. The pure-translation
// false-positive control above does not reach it: under ROTATION and SCALE the
// displacement grows with distance from the transform's fixed point, so a
// zero-centred block-matching window covers the image centre and misses the
// periphery. The fit was then carried by the central correspondences alone,
// passed the consensus floor at 59 % inliers, and reported SIXTEEN candidates
// in a scene with nothing moving in it. The guided second pass — searching
// around the model's own prediction — is what closes it.
//
// The frame here is 640x480 ON PURPOSE: the same scene at 320x240 displaces the
// periphery by half as much, stays inside the search window and hides the
// defect entirely (measured: 0 false candidates at 320, 16 at 640).
int test_rotating_scene_alone_yields_no_candidates() {
    MotionCandidates::Params p = test_params();
    p.blob_min_frac = 0.01F;
    MotionCandidates m(p);
    const float theta = 0.05F; // ~2.9 deg between consecutive frames
    const float scale = 1.02F;
    const float a = scale * std::cos(theta);
    const float b = scale * std::sin(theta);
    const SizedScene prev = render_at(640, 480, 1.F, 0.F, 0.F, 0.F);
    const SizedScene cur = render_at(640, 480, a, b, 3.F, 2.F);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    CHECK(r.candidates.empty());
    // Consensus must be near-total, not merely over the floor: a model only
    // half the correspondences agree with is the signature of the defect.
    CHECK(r.ego.inliers * 4 >= r.ego.correspondences * 3);
    return 0;
}

// Rotation beyond what the correspondence search can follow must FAIL CLOSED —
// report no fit — rather than align badly and turn the background into
// candidates. Measured: usable to ~6 deg between frames (360 deg/s at 60 Hz,
// far beyond any real airframe), no fit past ~10 deg.
int test_excessive_rotation_fails_closed() {
    MotionCandidates m(test_params());
    const float theta = 0.35F; // ~20 deg between consecutive frames
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    const Scene cur = render(std::cos(theta), std::sin(theta), 0.F, 0.F, 0, 0, 0);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(!r.ego.ok);
    CHECK(r.candidates.empty());
    return 0;
}

// A repetitive scene (rooftops, water, crop rows) offers many alignments that
// look equally good. The safety property is NOT "it must give up" — it is that
// it must never be CONFIDENTLY WRONG: either no fit is reported, or the fit is
// right and no candidates are invented. Measured on a period-8 px grid: at
// decimate=2 the pattern still resolves and the estimate is correct to 0.06 px
// with full consensus; at decimate=4 the period falls to two decimated pixels,
// the uniqueness gate rejects nearly every correspondence and no fit is
// reported. Both outcomes are acceptable; a wrong alignment with candidates
// spread over the frame is not, because that is what sends the narrow channel
// hunting empty sky.
int test_repetitive_texture_is_never_confidently_wrong() {
    for (const int decimate : {2, 4}) {
        MotionCandidates::Params p = test_params();
        p.decimate = decimate;
        MotionCandidates m(p);
        Scene prev = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
        Scene cur = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const float fy = 60.F * std::sin(static_cast<float>(y) * 0.785F);
                const float v = (60.F * std::sin(static_cast<float>(x) * 0.785F)) + fy;
                const float v2 =
                    (60.F * std::sin((static_cast<float>(x) - 6.F) * 0.785F)) + fy;
                prev.buf[(static_cast<size_t>(y) * W) + static_cast<size_t>(x)] =
                    static_cast<uint8_t>(std::max(0.F, std::min(255.F, 128.F + v)));
                cur.buf[(static_cast<size_t>(y) * W) + static_cast<size_t>(x)] =
                    static_cast<uint8_t>(std::max(0.F, std::min(255.F, 128.F + v2)));
            }
        }
        const auto r = m.process(prev.frame, cur.frame);
        if (r.ego.ok) {
            CHECK(near(r.ego.tx, 6.F, 0.5F)); // right, if it answers at all
            CHECK(near(r.ego.ty, 0.F, 0.5F));
        }
        CHECK(r.candidates.empty()); // nothing moved independently, either way
    }
    return 0;
}

// Sensitivity: the target only has to move slightly differently from the
// background. Measured, a single pixel of RELATIVE motion is enough — which is
// what makes this useful at range, where a drone subtends a few pixels.
int test_detects_one_pixel_of_relative_motion() {
    MotionCandidates m(test_params());
    // The background pans 6 px; the blob moves 7 — one pixel of its own.
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 150, 120, 6);
    const Scene cur = render(1.F, 0.F, 6.F, 0.F, 157, 120, 6);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    CHECK(!r.candidates.empty());
    return 0;
}

// The documented LIMIT, pinned so it cannot be mistaken for a defect later: an
// object whose image motion matches the background — a target closing head-on
// while the ownship tracks it — produces no residual and is invisible to this
// path. R-13 is a second detection route alongside the model, not a substitute
// for it.
int test_target_moving_with_the_background_is_invisible() {
    MotionCandidates m(test_params());
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 150, 120, 6);
    const Scene cur = render(1.F, 0.F, 6.F, 0.F, 156, 120, 6); // blob pans WITH it
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    CHECK(r.candidates.empty());
    return 0;
}

int test_static_scene_yields_no_candidates() {
    // Identical frames: nothing moved at all, so nothing may be reported.
    MotionCandidates m(test_params());
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    const Scene cur = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    CHECK(r.candidates.empty());
    return 0;
}

// THE false-positive control: the ownship moves and NOTHING else does. Without
// ego-motion compensation every textured pixel differs and the whole frame
// reads as motion; with it, the residual must be empty.
int test_ego_motion_alone_yields_no_candidates() {
    MotionCandidates m(test_params());
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    const Scene cur = render(1.F, 0.F, 8.F, 5.F, 0, 0, 0);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    CHECK(r.candidates.empty());
    return 0;
}

int test_large_region_is_rejected_as_parallax() {
    // A moving region far bigger than the size band is near structure sliding
    // with parallax, not a small airborne object.
    const MotionCandidates::Params p = test_params();
    MotionCandidates m(p);
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 40, 40, 90);
    const Scene cur = render(1.F, 0.F, 4.F, 0.F, 60, 40, 90);
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    for (const auto& c : r.candidates) {
        CHECK(c.w <= p.blob_max_frac + 1e-3F); // nothing oversized survived
    }
    return 0;
}

// --- fail-closed behaviour --------------------------------------------------

int test_unusable_input_reports_nothing() {
    MotionCandidates m(test_params());
    const Scene good = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    const Frame null_frame{};
    const auto a = m.process(null_frame, good.frame);
    CHECK(!a.ego.ok && a.candidates.empty());

    Frame wrong = good.frame;
    wrong.fourcc = 0x56595559; // YUYV: only NV12 luma is understood
    const auto b = m.process(wrong, good.frame);
    CHECK(!b.ego.ok && b.candidates.empty());

    Frame mismatched = good.frame;
    mismatched.width = W / 2; // geometry must match between the pair
    const auto c = m.process(mismatched, good.frame);
    CHECK(!c.ego.ok && c.candidates.empty());
    return 0;
}

int test_textureless_scene_fails_closed() {
    // A flat sky has no corners: with no correspondences there is no background
    // model, and the module must say so rather than align on noise and then
    // report the whole frame as moving.
    MotionCandidates m(test_params());
    Scene flat = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    for (int i = 0; i < W * H; ++i) {
        flat.buf[static_cast<size_t>(i)] = 128;
    }
    Scene flat2 = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    for (int i = 0; i < W * H; ++i) {
        flat2.buf[static_cast<size_t>(i)] = 128;
    }
    const auto r = m.process(flat.frame, flat2.frame);
    CHECK(!r.ego.ok);
    CHECK(r.candidates.empty());
    return 0;
}

int test_result_is_deterministic() {
    // The RANSAC sampler is fixed-seed on purpose: a fit that varies run to run
    // cannot be regression-tested, and a candidate list that flickers cannot be
    // debugged from a recording.
    MotionCandidates m1(test_params());
    MotionCandidates m2(test_params());
    const Scene prev = render(1.F, 0.F, 0.F, 0.F, 150, 120, 6);
    const Scene cur = render(1.F, 0.F, 5.F, 2.F, 138, 126, 6);
    const auto a = m1.process(prev.frame, cur.frame);
    const auto b = m2.process(prev.frame, cur.frame);
    CHECK(a.ego.ok == b.ego.ok);
    CHECK(a.ego.tx == b.ego.tx && a.ego.ty == b.ego.ty);
    CHECK(a.candidates.size() == b.candidates.size());
    for (size_t i = 0; i < a.candidates.size(); ++i) {
        CHECK(a.candidates[i].cx == b.candidates[i].cx);
        CHECK(a.candidates[i].score == b.candidates[i].score);
    }
    return 0;
}

int test_candidates_are_capped_and_sorted() {
    MotionCandidates::Params p = test_params();
    p.max_candidates = 2;
    MotionCandidates m(p);
    // Several independently-moving blobs at once (a flock, or clutter).
    Scene prev = render(1.F, 0.F, 0.F, 0.F, 0, 0, 0);
    Scene cur = render(1.F, 0.F, 3.F, 0.F, 0, 0, 0);
    const int xs[5] = {60, 110, 160, 210, 260};
    for (const int x : xs) {
        for (int j = 0; j < 6; ++j) {
            for (int i = 0; i < 6; ++i) {
                prev.buf[(static_cast<size_t>(100 + j) * W) +
                         static_cast<size_t>(x + i)] = 250;
                cur.buf[(static_cast<size_t>(140 + j) * W) + static_cast<size_t>(x + i)] =
                    250;
            }
        }
    }
    const auto r = m.process(prev.frame, cur.frame);
    CHECK(r.ego.ok);
    CHECK(r.candidates.size() <= 2); // cap respected
    for (size_t i = 1; i < r.candidates.size(); ++i) {
        CHECK(r.candidates[i - 1].score >= r.candidates[i].score); // score order
    }
    return 0;
}

// --- solve_similarity (the numerically delicate step) -----------------------

int test_solve_similarity_exact_and_degenerate() {
    // An exact similarity through 3 points is recovered to float precision.
    const float a = 0.98F;
    const float b = 0.17F;
    const float tx = 4.5F;
    const float ty = -2.25F;
    std::vector<float> sx = {0.F, 10.F, 3.F};
    std::vector<float> sy = {0.F, 2.F, 9.F};
    std::vector<float> dx;
    std::vector<float> dy;
    for (size_t i = 0; i < sx.size(); ++i) {
        dx.push_back(((a * sx[i]) - (b * sy[i])) + tx);
        dy.push_back(((b * sx[i]) + (a * sy[i])) + ty);
    }
    float ra = 0.F;
    float rb = 0.F;
    float rtx = 0.F;
    float rty = 0.F;
    CHECK(solve_similarity(sx, sy, dx, dy, ra, rb, rtx, rty));
    CHECK(near(ra, a, 1e-3F) && near(rb, b, 1e-3F));
    CHECK(near(rtx, tx, 1e-2F) && near(rty, ty, 1e-2F));

    // All source points coincident: rotation and scale are unobservable, so the
    // solve must refuse rather than divide by ~0 and return an infinity.
    const std::vector<float> cx = {5.F, 5.F, 5.F};
    const std::vector<float> cy = {7.F, 7.F, 7.F};
    CHECK(!solve_similarity(cx, cy, dx, dy, ra, rb, rtx, rty));
    // Fewer than two pairs cannot fix a similarity at all.
    const std::vector<float> one = {1.F};
    CHECK(!solve_similarity(one, one, one, one, ra, rb, rtx, rty));
    // Mismatched lengths are a caller error, not something to guess through.
    const std::vector<float> two = {1.F, 2.F};
    CHECK(!solve_similarity(two, two, one, one, ra, rb, rtx, rty));
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_solve_similarity_exact_and_degenerate();
    rc = rc != 0 ? rc : test_recovers_pure_translation();
    rc = rc != 0 ? rc : test_recovers_rotation_and_scale();
    rc = rc != 0 ? rc : test_subpixel_translation_accuracy();
    rc = rc != 0 ? rc : test_rotating_scene_alone_yields_no_candidates();
    rc = rc != 0 ? rc : test_excessive_rotation_fails_closed();
    rc = rc != 0 ? rc : test_repetitive_texture_is_never_confidently_wrong();
    rc = rc != 0 ? rc : test_detects_one_pixel_of_relative_motion();
    rc = rc != 0 ? rc : test_target_moving_with_the_background_is_invisible();
    rc = rc != 0 ? rc : test_finds_a_blob_that_does_not_follow_the_background();
    rc = rc != 0 ? rc : test_static_scene_yields_no_candidates();
    rc = rc != 0 ? rc : test_ego_motion_alone_yields_no_candidates();
    rc = rc != 0 ? rc : test_large_region_is_rejected_as_parallax();
    rc = rc != 0 ? rc : test_unusable_input_reports_nothing();
    rc = rc != 0 ? rc : test_textureless_scene_fails_closed();
    rc = rc != 0 ? rc : test_result_is_deterministic();
    rc = rc != 0 ? rc : test_candidates_are_capped_and_sorted();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_motion: %d checks passed\n", checks);
    return 0;
}
