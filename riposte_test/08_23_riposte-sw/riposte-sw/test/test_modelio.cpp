// Model I/O tests: the letterbox transform, NMS, and the YOLOv8 head decode.
//
// This is the part of the detector that can be wrong in ways a test can catch,
// which is exactly why it lives outside the HailoRT adapter. The letterbox tests
// matter most: an aspect distortion here does not crash anything, it quietly
// biases the bbox width/height ratio, and that ratio is what the estimator turns
// into the target's range.
#include "IDetector.h"
#include "ModelIo.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "riposte/Tunables.h"

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

bool near(float a, float b, float tol = 1e-3F) {
    return std::fabs(a - b) <= tol;
}

Detection det(float cx, float cy, float w, float h, float score = 0.9F, int cls = 0) {
    Detection d{};
    d.cx = cx;
    d.cy = cy;
    d.w = w;
    d.h = h;
    d.score = score;
    d.cls = cls;
    return d;
}

// ------------------------------------------------------------- letterbox --

int test_fit_square_source_has_no_padding() {
    const Letterbox lb = letterbox_fit(640, 640, 640);
    CHECK(lb.valid());
    CHECK(near(lb.scale, 1.F));
    CHECK(lb.pad_x == 0 && lb.pad_y == 0);
    CHECK(lb.content_w == 640 && lb.content_h == 640);
    return 0;
}

int test_fit_wide_source_pads_vertically() {
    // A 3x3 tile of 1280x720 is 426x240 — the case that made this necessary.
    const Letterbox lb = letterbox_fit(426, 240, 640);
    CHECK(lb.valid());
    CHECK(near(lb.scale, 640.F / 426.F, 1e-4F)); // width is the limiting axis
    CHECK(lb.content_w == 640);
    CHECK(lb.content_h == 361); // 240 * 640/426
    CHECK(lb.pad_x == 0);
    CHECK(lb.pad_y == (640 - 361) / 2);
    return 0;
}

int test_fit_tall_source_pads_horizontally() {
    const Letterbox lb = letterbox_fit(240, 426, 640);
    CHECK(near(lb.scale, 640.F / 426.F, 1e-4F));
    CHECK(lb.content_h == 640);
    CHECK(lb.content_w == 361);
    CHECK(lb.pad_y == 0);
    CHECK(lb.pad_x == (640 - 361) / 2);
    return 0;
}

int test_fit_rejects_degenerate() {
    CHECK(!letterbox_fit(0, 240, 640).valid());
    CHECK(!letterbox_fit(426, 0, 640).valid());
    CHECK(!letterbox_fit(426, 240, 0).valid());
    CHECK(!letterbox_fit(-1, 240, 640).valid());
    return 0;
}

int test_forward_maps_centre_to_centre() {
    const Letterbox lb = letterbox_fit(426, 240, 640);
    float mx = 0.F;
    float my = 0.F;
    letterbox_forward(lb, 0.5F, 0.5F, mx, my);
    CHECK(near(mx, 0.5F, 2e-3F)); // centre stays centred on both axes
    CHECK(near(my, 0.5F, 2e-3F));
    return 0;
}

int test_round_trip_position_and_size() {
    // The property that matters: forward then undo returns the original box,
    // sizes included.
    const Letterbox lb = letterbox_fit(426, 240, 640);
    const float src_cx = 0.3F;
    const float src_cy = 0.7F;
    float mx = 0.F;
    float my = 0.F;
    letterbox_forward(lb, src_cx, src_cy, mx, my);
    // A box 20% of source width and 30% of source height, expressed in model
    // (square) normalization the way the network would report it.
    const float mw = 0.20F * static_cast<float>(lb.src_w) * lb.scale /
                     static_cast<float>(lb.model_size);
    const float mh = 0.30F * static_cast<float>(lb.src_h) * lb.scale /
                     static_cast<float>(lb.model_size);
    Detection d = det(mx, my, mw, mh);
    CHECK(letterbox_undo(lb, d));
    CHECK(near(d.cx, src_cx, 2e-3F));
    CHECK(near(d.cy, src_cy, 2e-3F));
    CHECK(near(d.w, 0.20F, 2e-3F));
    CHECK(near(d.h, 0.30F, 2e-3F));
    return 0;
}

int test_aspect_is_preserved_not_stretched() {
    // The regression this whole file exists for. A SQUARE object in a non-square
    // source must come back square in PIXEL terms: equal pixel extents, which in
    // the split normalization means w*src_w == h*src_h.
    const Letterbox lb = letterbox_fit(426, 240, 640);
    // Square in pixels: 64 x 64 source pixels, expressed in model normalization.
    const float side_model = 64.F * lb.scale / static_cast<float>(lb.model_size);
    Detection d = det(0.5F, 0.5F, side_model, side_model);
    CHECK(letterbox_undo(lb, d));
    const float px_w = d.w * static_cast<float>(lb.src_w);
    const float px_h = d.h * static_cast<float>(lb.src_h);
    CHECK(near(px_w, 64.F, 0.5F));
    CHECK(near(px_h, 64.F, 0.5F));
    CHECK(near(px_w, px_h, 0.5F)); // still square in pixels
    return 0;
}

int test_undo_rejects_detections_in_the_padding() {
    // 426x240 into 640 pads top and bottom; a detection centred in the top strip
    // is the network firing on the fill and must not become a target.
    const Letterbox lb = letterbox_fit(426, 240, 640);
    CHECK(lb.pad_y > 0);
    const float in_pad_y =
        (static_cast<float>(lb.pad_y) * 0.5F) / static_cast<float>(lb.model_size);
    Detection d = det(0.5F, in_pad_y, 0.05F, 0.05F);
    CHECK(!letterbox_undo(lb, d));
    // ...and the bottom strip likewise.
    const float below = (static_cast<float>(lb.pad_y + lb.content_h) + 5.F) /
                        static_cast<float>(lb.model_size);
    Detection d2 = det(0.5F, below, 0.05F, 0.05F);
    CHECK(!letterbox_undo(lb, d2));
    // A detection just inside the content area is accepted.
    const float inside =
        (static_cast<float>(lb.pad_y) + 5.F) / static_cast<float>(lb.model_size);
    Detection d3 = det(0.5F, inside, 0.05F, 0.05F);
    CHECK(letterbox_undo(lb, d3));
    return 0;
}

int test_undo_on_invalid_letterbox_fails() {
    const Letterbox bad = letterbox_fit(0, 0, 0);
    Detection d = det(0.5F, 0.5F, 0.1F, 0.1F);
    CHECK(!letterbox_undo(bad, d));
    return 0;
}

int test_corners_map_to_corners() {
    const Letterbox lb = letterbox_fit(426, 240, 640);
    float mx = 0.F;
    float my = 0.F;
    letterbox_forward(lb, 0.F, 0.F, mx, my);
    Detection d = det(mx, my, 0.01F, 0.01F);
    CHECK(letterbox_undo(lb, d));
    CHECK(near(d.cx, 0.F, 2e-3F));
    CHECK(near(d.cy, 0.F, 2e-3F));
    letterbox_forward(lb, 1.F, 1.F, mx, my);
    Detection d2 = det(mx, my, 0.01F, 0.01F);
    CHECK(letterbox_undo(lb, d2));
    CHECK(near(d2.cx, 1.F, 2e-3F));
    CHECK(near(d2.cy, 1.F, 2e-3F));
    return 0;
}

// ------------------------------------------------------------------ NMS --

int test_nms_suppresses_heavy_overlap() {
    std::vector<Detection> d{det(0.50F, 0.50F, 0.20F, 0.20F, 0.90F),
                             det(0.51F, 0.51F, 0.20F, 0.20F, 0.80F)};
    nms(d, 0.45F);
    CHECK(d.size() == 1);
    CHECK(near(d.at(0).score, 0.90F)); // the higher-scoring box survives
    return 0;
}

int test_nms_keeps_separate_boxes() {
    std::vector<Detection> d{det(0.20F, 0.20F, 0.10F, 0.10F, 0.90F),
                             det(0.80F, 0.80F, 0.10F, 0.10F, 0.80F)};
    nms(d, 0.45F);
    CHECK(d.size() == 2);
    return 0;
}

int test_nms_is_per_class() {
    // Two coincident boxes of DIFFERENT classes are two findings, not one.
    std::vector<Detection> d{det(0.5F, 0.5F, 0.2F, 0.2F, 0.9F, 0),
                             det(0.5F, 0.5F, 0.2F, 0.2F, 0.8F, 1)};
    nms(d, 0.45F);
    CHECK(d.size() == 2);
    return 0;
}

int test_nms_sorts_by_score() {
    std::vector<Detection> d{det(0.20F, 0.20F, 0.10F, 0.10F, 0.30F),
                             det(0.80F, 0.80F, 0.10F, 0.10F, 0.95F),
                             det(0.50F, 0.50F, 0.10F, 0.10F, 0.60F)};
    nms(d, 0.45F);
    CHECK(d.size() == 3);
    CHECK(d.at(0).score > d.at(1).score);
    CHECK(d.at(1).score > d.at(2).score);
    return 0;
}

int test_nms_threshold_boundary() {
    // Boxes overlapping at exactly 1/3 IoU: kept at 0.45, dropped at 0.30.
    // Two 0.2-wide boxes offset by 0.1 in x, same y: inter = 0.1*0.2 = 0.02,
    // union = 0.04 + 0.04 - 0.02 = 0.06 -> IoU = 1/3.
    const auto make = [] {
        return std::vector<Detection>{det(0.40F, 0.50F, 0.20F, 0.20F, 0.90F),
                                      det(0.50F, 0.50F, 0.20F, 0.20F, 0.80F)};
    };
    std::vector<Detection> keep = make();
    nms(keep, 0.45F);
    CHECK(keep.size() == 2);
    std::vector<Detection> drop = make();
    nms(drop, 0.30F);
    CHECK(drop.size() == 1);
    return 0;
}

int test_nms_handles_trivial_inputs() {
    std::vector<Detection> empty;
    nms(empty, 0.45F);
    CHECK(empty.empty());
    std::vector<Detection> one{det(0.5F, 0.5F, 0.1F, 0.1F)};
    nms(one, 0.45F);
    CHECK(one.size() == 1);
    return 0;
}

int test_nms_chain_keeps_the_best_of_a_cluster() {
    // Three mutually overlapping boxes must collapse to the single best one.
    std::vector<Detection> d{det(0.50F, 0.50F, 0.20F, 0.20F, 0.70F),
                             det(0.51F, 0.50F, 0.20F, 0.20F, 0.95F),
                             det(0.52F, 0.50F, 0.20F, 0.20F, 0.60F)};
    nms(d, 0.45F);
    CHECK(d.size() == 1);
    CHECK(near(d.at(0).score, 0.95F));
    return 0;
}

// --------------------------------------------------------------- decode --

// Builds a [4+nc, na] row-major tensor with one target anchor filled in.
// G16.6 deviation: fixture builder mirrors the tensor layout field-by-field (G16.6)
// NOLINTNEXTLINE(readability-function-size)
std::vector<float> make_tensor(int nc, int na, int anchor, float cx_px, float cy_px,
                               float w_px, float h_px, int cls, float score) {
    std::vector<float> t(static_cast<std::size_t>(4 + nc) * static_cast<std::size_t>(na),
                         0.F);
    const std::size_t A = static_cast<std::size_t>(na);
    const std::size_t a = static_cast<std::size_t>(anchor);
    t.at(a) = cx_px;
    t.at(A + a) = cy_px;
    t.at((2 * A) + a) = w_px;
    t.at((3 * A) + a) = h_px;
    t.at((static_cast<std::size_t>(4 + cls) * A) + a) = score;
    return t;
}

// P1-06: the tensor is untrusted device output — non-finite scalars must be
// rejected outright (a NaN width once passed `w <= 0`, since NaN compares
// false both ways, and rode into the tracker).
int test_decode_rejects_non_finite_scalars() {
    const int nc = 1;
    const int na = 4;
    std::vector<Detection> out;
    auto t = make_tensor(nc, na, 1, 320.F, 320.F, 40.F, 40.F, 0, 0.9F);
    t.at((2U * na) + 1U) = std::nanf(""); // width row
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.25F, out);
    CHECK(out.empty());
    t = make_tensor(nc, na, 1, 320.F, 320.F, 40.F, 40.F, 0, 0.9F);
    t.at(1) = HUGE_VALF; // cx row
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.25F, out);
    CHECK(out.empty());
    t = make_tensor(nc, na, 1, 320.F, 320.F, 40.F, 40.F, 0, 0.9F);
    t.at((4U * na) + 1U) = std::nanf(""); // score row: NaN fails `>=` -> no class
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.25F, out);
    CHECK(out.empty());
    return 0;
}

// P1-06: centres far outside the model square / oversized boxes are tensor
// garbage, not detections — letterbox_undo is not the first line of defense.
int test_decode_rejects_out_of_range_boxes() {
    const int nc = 1;
    const int na = 4;
    std::vector<Detection> out;
    auto t = make_tensor(nc, na, 1, 6400.F, 320.F, 40.F, 40.F, 0, 0.9F); // cx = 10.0
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.25F, out);
    CHECK(out.empty());
    t = make_tensor(nc, na, 1, 320.F, 320.F, 640.F * 3.F, 40.F, 0, 0.9F); // w = 3.0
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.25F, out);
    CHECK(out.empty());
    return 0;
}

// P1-06: more threshold-passing anchors than RAW_DECODE_MAX_CANDS -> only the
// top-K by score survive, bounding the O(N^2) NMS that follows.
int test_decode_caps_candidates_by_score() {
    const int nc = 1;
    const int na = tun::RAW_DECODE_MAX_CANDS + 100;
    const auto A = static_cast<std::size_t>(na);
    std::vector<float> t(5U * A, 0.F);
    for (std::size_t a = 0; a < A; ++a) {
        t.at(a) = 320.F;           // cx
        t.at(A + a) = 320.F;       // cy
        t.at((2U * A) + a) = 40.F; // w
        t.at((3U * A) + a) = 40.F; // h
        // Monotonically increasing score: the LAST `cap` anchors are the best.
        t.at((4U * A) + a) =
            0.3F + (0.6F * static_cast<float>(a) / static_cast<float>(na));
    }
    std::vector<Detection> out;
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.25F, out);
    CHECK(out.size() == static_cast<std::size_t>(tun::RAW_DECODE_MAX_CANDS));
    // Every survivor must outscore every dropped anchor (top-K, not first-K).
    const float cut = 0.3F + (0.6F * static_cast<float>(na - tun::RAW_DECODE_MAX_CANDS) /
                              static_cast<float>(na));
    for (const auto& d : out) {
        CHECK(d.score >= cut - 1e-4F);
    }
    return 0;
}

int test_decode_one_anchor() {
    const int nc = 2;
    const int na = 8;
    const std::vector<float> t =
        make_tensor(nc, na, 3, 320.F, 160.F, 64.F, 32.F, 1, 0.8F);
    std::vector<Detection> out;
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.4F, out);
    CHECK(out.size() == 1);
    CHECK(near(out.at(0).cx, 0.5F));  // 320/640
    CHECK(near(out.at(0).cy, 0.25F)); // 160/640
    CHECK(near(out.at(0).w, 0.1F));   // 64/640
    CHECK(near(out.at(0).h, 0.05F));  // 32/640
    CHECK(out.at(0).cls == 1);
    CHECK(near(out.at(0).score, 0.8F));
    return 0;
}

int test_decode_applies_the_threshold() {
    const int nc = 2;
    const int na = 8;
    const std::vector<float> t =
        make_tensor(nc, na, 3, 320.F, 160.F, 64.F, 32.F, 1, 0.2F);
    std::vector<Detection> out;
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.4F, out);
    CHECK(out.empty());
    return 0;
}

int test_decode_picks_the_best_class_per_anchor() {
    const int nc = 3;
    const int na = 4;
    std::vector<float> t = make_tensor(nc, na, 1, 100.F, 200.F, 20.F, 20.F, 0, 0.55F);
    // Same anchor also scores on class 2, higher.
    t.at((static_cast<std::size_t>(4 + 2) * static_cast<std::size_t>(na)) + 1) = 0.85F;
    std::vector<Detection> out;
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.4F, out);
    CHECK(out.size() == 1); // one anchor -> one detection, not one per class
    CHECK(out.at(0).cls == 2);
    CHECK(near(out.at(0).score, 0.85F));
    return 0;
}

int test_decode_rejects_undersized_buffer() {
    // A mismatched num_classes/num_anchors must not walk off the tensor.
    const int nc = 2;
    const int na = 8;
    const std::vector<float> t =
        make_tensor(nc, na, 3, 320.F, 160.F, 64.F, 32.F, 1, 0.8F);
    std::vector<Detection> out;
    decode_yolov8(t.data(), t.size(), nc, na * 4, 640, 0.4F, out); // claims 4x anchors
    CHECK(out.empty());
    decode_yolov8(nullptr, 0, nc, na, 640, 0.4F, out);
    CHECK(out.empty());
    decode_yolov8(t.data(), t.size(), 0, na, 640, 0.4F, out);
    CHECK(out.empty());
    return 0;
}

int test_decode_skips_degenerate_boxes() {
    const int nc = 1;
    const int na = 4;
    const std::vector<float> t = make_tensor(nc, na, 2, 320.F, 320.F, 0.F, 40.F, 0, 0.9F);
    std::vector<Detection> out;
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.4F, out);
    CHECK(out.empty()); // zero-width box is not a detection
    return 0;
}

int test_decode_then_nms_then_undo_pipeline() {
    // The real order: decode in model space, suppress in model space, only then
    // map back to the source frame. Two near-duplicate anchors on one target.
    const int nc = 1;
    const int na = 16;
    std::vector<float> t(static_cast<std::size_t>(4 + nc) * static_cast<std::size_t>(na),
                         0.F);
    const std::size_t A = na;
    const auto put = [&](std::size_t a, float cx, float cy, float w, float h, float sc) {
        t.at(a) = cx;
        t.at(A + a) = cy;
        t.at((2 * A) + a) = w;
        t.at((3 * A) + a) = h;
        t.at((4 * A) + a) = sc;
    };
    put(0, 320.F, 320.F, 64.F, 64.F, 0.90F);
    put(1, 322.F, 321.F, 64.F, 64.F, 0.70F); // duplicate of the same target
    put(2, 100.F, 100.F, 32.F, 32.F, 0.80F); // a second, distinct target

    std::vector<Detection> dets;
    decode_yolov8(t.data(), t.size(), nc, na, 640, 0.4F, dets);
    CHECK(dets.size() == 3);
    nms(dets, 0.45F);
    CHECK(dets.size() == 2); // the duplicate is gone

    const Letterbox lb = letterbox_fit(426, 240, 640);
    std::vector<Detection> mapped;
    for (auto d : dets) {
        if (letterbox_undo(lb, d)) {
            mapped.push_back(d);
        }
    }
    // The 320,320 target is at the model centre -> the source centre. The
    // 100,100 one falls in the top padding strip and is correctly discarded.
    CHECK(mapped.size() == 1);
    CHECK(near(mapped.at(0).cx, 0.5F, 5e-3F));
    CHECK(near(mapped.at(0).cy, 0.5F, 5e-3F));
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_fit_square_source_has_no_padding();
    rc = rc != 0 ? rc : test_fit_wide_source_pads_vertically();
    rc = rc != 0 ? rc : test_fit_tall_source_pads_horizontally();
    rc = rc != 0 ? rc : test_fit_rejects_degenerate();
    rc = rc != 0 ? rc : test_forward_maps_centre_to_centre();
    rc = rc != 0 ? rc : test_round_trip_position_and_size();
    rc = rc != 0 ? rc : test_aspect_is_preserved_not_stretched();
    rc = rc != 0 ? rc : test_undo_rejects_detections_in_the_padding();
    rc = rc != 0 ? rc : test_undo_on_invalid_letterbox_fails();
    rc = rc != 0 ? rc : test_corners_map_to_corners();
    rc = rc != 0 ? rc : test_nms_suppresses_heavy_overlap();
    rc = rc != 0 ? rc : test_nms_keeps_separate_boxes();
    rc = rc != 0 ? rc : test_nms_is_per_class();
    rc = rc != 0 ? rc : test_nms_sorts_by_score();
    rc = rc != 0 ? rc : test_nms_threshold_boundary();
    rc = rc != 0 ? rc : test_nms_handles_trivial_inputs();
    rc = rc != 0 ? rc : test_nms_chain_keeps_the_best_of_a_cluster();
    rc = rc != 0 ? rc : test_decode_one_anchor();
    rc = rc != 0 ? rc : test_decode_applies_the_threshold();
    rc = rc != 0 ? rc : test_decode_picks_the_best_class_per_anchor();
    rc = rc != 0 ? rc : test_decode_rejects_undersized_buffer();
    rc = rc != 0 ? rc : test_decode_skips_degenerate_boxes();
    rc = rc != 0 ? rc : test_decode_rejects_non_finite_scalars();
    rc = rc != 0 ? rc : test_decode_rejects_out_of_range_boxes();
    rc = rc != 0 ? rc : test_decode_caps_candidates_by_score();
    rc = rc != 0 ? rc : test_decode_then_nms_then_undo_pipeline();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_modelio: %d checks passed\n", checks);
    return 0;
}
