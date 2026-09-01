// NMS-by-class output parser tests (SDD §4.4.2).
//
// The buffer under test is what the NPU/driver hands back for the
// product-standard HEF. The parser must treat it as untrusted device data:
// the failure modes checked here (NaN counts, truncated records, out-of-range
// boxes) are exactly the ones that would otherwise size a loop from garbage or
// push a NaN score into the tracker.
#include "HailoNmsParse.h"
#include "IDetector.h"
#include "ModelIo.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
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

bool near(float a, float b, float tol = 1e-4F) {
    return std::fabs(a - b) <= tol;
}

// Builder for wire buffers: per class, a float count then count x 5-float
// records — the exact layout the device emits.
class Wire {
public:
    void begin_class(float count) { push(count); }
    void box(float y0, float x0, float y1, float x1, float score) {
        push(y0);
        push(x0);
        push(y1);
        push(x1);
        push(score);
    }
    const std::uint8_t* data() const { return bytes_.data(); }
    std::size_t size() const { return bytes_.size(); }
    void truncate(std::size_t n) { bytes_.resize(bytes_.size() - n); }

private:
    void push(float v) {
        std::uint8_t b[sizeof(float)];
        std::memcpy(b, &v, sizeof v);
        bytes_.insert(bytes_.end(), b, b + sizeof v);
    }
    std::vector<std::uint8_t> bytes_;
};

// ---------------------------------------------------------- happy path --

int test_parses_boxes_across_classes() {
    Wire w;
    w.begin_class(2.F); // class 0
    w.box(0.10F, 0.20F, 0.30F, 0.40F, 0.90F);
    w.box(0.50F, 0.50F, 0.70F, 0.80F, 0.60F);
    w.begin_class(1.F); // class 1
    w.box(0.00F, 0.00F, 1.00F, 1.00F, 0.50F);

    std::vector<Detection> out;
    parse_nms_by_class(w.data(), w.size(), 2, 16, out);
    CHECK(out.size() == 3);
    CHECK(near(out.at(0).cx, 0.30F));
    CHECK(near(out.at(0).cy, 0.20F));
    CHECK(near(out.at(0).w, 0.20F));
    CHECK(near(out.at(0).h, 0.20F));
    CHECK(near(out.at(0).score, 0.90F));
    CHECK(out.at(0).cls == 0); // section index IS the id — 0-based
    CHECK(out.at(1).cls == 0);
    CHECK(out.at(2).cls == 1);
    return 0;
}

int test_zero_count_classes_are_skipped() {
    Wire w;
    w.begin_class(0.F); // class 0: empty
    w.begin_class(1.F); // class 1
    w.box(0.10F, 0.10F, 0.20F, 0.20F, 0.80F);

    std::vector<Detection> out;
    parse_nms_by_class(w.data(), w.size(), 2, 16, out);
    CHECK(out.size() == 1);
    CHECK(out.at(0).cls == 1);
    return 0;
}

int test_out_is_cleared_first() {
    Wire w;
    w.begin_class(0.F);
    std::vector<Detection> out(3);
    parse_nms_by_class(w.data(), w.size(), 1, 16, out);
    CHECK(out.empty());
    return 0;
}

// ------------------------------------------------------- untrusted data --

int test_truncated_header_stops_cleanly() {
    Wire w;
    w.begin_class(1.F);
    w.box(0.10F, 0.10F, 0.20F, 0.20F, 0.80F);
    // Second class header is missing entirely (size ends after class 0).
    std::vector<Detection> out;
    parse_nms_by_class(w.data(), w.size(), 2, 16, out);
    CHECK(out.size() == 1); // class 0 kept, missing header stops the parse
    return 0;
}

int test_truncated_record_drops_the_class_fail_closed() {
    // A count whose records no longer fit the remaining bytes means the buffer
    // is inconsistent — the count itself is untrustworthy, so the whole class
    // is dropped (fail closed), while classes parsed before it survive.
    Wire w;
    w.begin_class(1.F); // class 0: intact
    w.box(0.10F, 0.10F, 0.20F, 0.20F, 0.80F);
    w.begin_class(2.F); // class 1: second record cut short
    w.box(0.30F, 0.30F, 0.40F, 0.40F, 0.70F);
    w.box(0.50F, 0.50F, 0.60F, 0.60F, 0.60F);
    w.truncate(4);
    std::vector<Detection> out;
    parse_nms_by_class(w.data(), w.size(), 2, 16, out);
    CHECK(out.size() == 1);
    CHECK(out.at(0).cls == 0);
    return 0;
}

int test_invalid_counts_break_framing_and_stop() {
    const float bad_counts[] = {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity(), -1.F, 1e9F,
                                17.F /* more records than the buffer holds */};
    for (const float bad : bad_counts) {
        Wire w;
        w.begin_class(1.F);
        w.box(0.10F, 0.10F, 0.20F, 0.20F, 0.80F);
        w.begin_class(bad); // class 1: untrustworthy count
        w.box(0.30F, 0.30F, 0.40F, 0.40F, 0.70F);
        std::vector<Detection> out;
        parse_nms_by_class(w.data(), w.size(), 2, 16, out);
        CHECK(out.size() == 1); // everything after the bad count is unframed
    }
    return 0;
}

int test_bad_records_are_dropped_individually() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Wire w;
    w.begin_class(6.F);
    w.box(nan, 0.10F, 0.20F, 0.20F, 0.80F);   // NaN coordinate
    w.box(0.10F, 0.10F, 0.20F, 1.50F, 0.80F); // out of [0,1]
    w.box(0.30F, 0.40F, 0.20F, 0.50F, 0.80F); // inverted (y_max < y_min)
    w.box(0.10F, 0.10F, 0.20F, 0.20F, 1.50F); // score > 1
    w.box(0.10F, 0.10F, 0.20F, 0.20F, nan);   // NaN score
    w.box(0.10F, 0.10F, 0.30F, 0.20F, 0.75F); // the one good record
    std::vector<Detection> out;
    parse_nms_by_class(w.data(), w.size(), 1, 16, out);
    CHECK(out.size() == 1);
    CHECK(near(out.at(0).score, 0.75F));
    return 0;
}

int test_degenerate_zero_area_boxes_are_dropped() {
    Wire w;
    w.begin_class(2.F);
    w.box(0.10F, 0.50F, 0.30F, 0.50F, 0.90F); // zero width (valid wire record)
    w.box(0.50F, 0.10F, 0.50F, 0.30F, 0.90F); // zero height
    std::vector<Detection> out;
    parse_nms_by_class(w.data(), w.size(), 1, 16, out);
    CHECK(out.empty());
    return 0;
}

int test_max_dets_caps_the_output_without_breaking_framing() {
    // A device compiled with a large max_bboxes_per_class may legitimately
    // report more than we keep. The cap limits the OUTPUT; the walk must stay
    // framed so later classes still parse.
    Wire w;
    w.begin_class(4.F);
    for (int i = 0; i < 4; ++i) {
        const float o = 0.05F * static_cast<float>(i);
        w.box(0.10F + o, 0.10F + o, 0.30F + o, 0.30F + o, 0.90F);
    }
    w.begin_class(1.F);
    w.box(0.60F, 0.60F, 0.80F, 0.80F, 0.70F);

    std::vector<Detection> out;
    parse_nms_by_class(w.data(), w.size(), 2, 3, out);
    CHECK(out.size() == 3); // capped inside class 0
    CHECK(out.at(0).cls == 0 && out.at(2).cls == 0);

    // With room to spare, the class after the big section survives intact.
    parse_nms_by_class(w.data(), w.size(), 2, 16, out);
    CHECK(out.size() == 5);
    CHECK(out.at(4).cls == 1);
    return 0;
}

int test_rejects_degenerate_arguments() {
    Wire w;
    w.begin_class(0.F);
    std::vector<Detection> out;
    parse_nms_by_class(nullptr, 64, 1, 16, out);
    CHECK(out.empty());
    parse_nms_by_class(w.data(), w.size(), 0, 16, out);
    CHECK(out.empty());
    parse_nms_by_class(w.data(), w.size(), 1, 0, out);
    CHECK(out.empty());
    return 0;
}

// ------------------------------------------------- letterbox round trip --

int test_parse_then_letterbox_undo_pipeline() {
    // A detection at the model-input centre of a letterboxed 426x240 tile must
    // land at the tile centre; one in the padding strip must be discarded —
    // same common tail the raw-head path uses (§4.3).
    Wire w;
    w.begin_class(2.F);
    w.box(0.45F, 0.45F, 0.55F, 0.55F, 0.90F); // model centre
    w.box(0.01F, 0.40F, 0.09F, 0.50F, 0.80F); // top padding strip
    std::vector<Detection> parsed;
    parse_nms_by_class(w.data(), w.size(), 1, 16, parsed);
    CHECK(parsed.size() == 2);

    const Letterbox lb = letterbox_fit(426, 240, 640);
    std::vector<Detection> mapped;
    for (auto d : parsed) {
        if (letterbox_undo(lb, d)) {
            mapped.push_back(d);
        }
    }
    CHECK(mapped.size() == 1);
    CHECK(near(mapped.at(0).cx, 0.5F, 5e-3F));
    CHECK(near(mapped.at(0).cy, 0.5F, 5e-3F));
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_parses_boxes_across_classes();
    rc = rc != 0 ? rc : test_zero_count_classes_are_skipped();
    rc = rc != 0 ? rc : test_out_is_cleared_first();
    rc = rc != 0 ? rc : test_truncated_header_stops_cleanly();
    rc = rc != 0 ? rc : test_truncated_record_drops_the_class_fail_closed();
    rc = rc != 0 ? rc : test_invalid_counts_break_framing_and_stop();
    rc = rc != 0 ? rc : test_bad_records_are_dropped_individually();
    rc = rc != 0 ? rc : test_degenerate_zero_area_boxes_are_dropped();
    rc = rc != 0 ? rc : test_max_dets_caps_the_output_without_breaking_framing();
    rc = rc != 0 ? rc : test_rejects_degenerate_arguments();
    rc = rc != 0 ? rc : test_parse_then_letterbox_undo_pipeline();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_hailoparse: %d checks passed\n", checks);
    return 0;
}
