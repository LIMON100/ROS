#pragma once
#include "IDetector.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace riposte {

// Parser for the HailoRT NMS-by-class output buffer (the product-standard HEF
// path, SDD §4.4 S-7). Header-only and HailoRT-free so it compiles and
// unit-tests on the host (test/test_hailoparse.cpp); HailoDetector.cpp is the
// production caller.
//
// Wire layout, for each class c in [0, class_count) in order:
//   float32 det_count;
//   det_count x { float32 y_min, x_min, y_max, x_max, score }   // 20 bytes
//
// Coordinates are normalized [0,1] against the MODEL INPUT (the network saw the
// letterboxed square), so letterbox_undo() is the common tail for this path and
// the raw-head fallback alike. The class section index IS the class id — emitted
// 0-based, matching seeker.target_class and decode_yolov8. (A predecessor
// implementation re-numbered classes 1-based for its own label map; that
// convention is deliberately NOT carried over.)
//
// The buffer is written by the NPU/driver and is treated as UNTRUSTED device
// data: every read is bounds-checked against `size`, and det_count is validated
// (finite, 0..max_dets) BEFORE the float->int conversion — NaN/negative/huge
// floats make that conversion UB, and a malformed count must never size a loop.
// A bad count or truncated record breaks the framing for everything after it,
// so parsing stops there rather than guessing.

// Mirrors hailo_bbox_float32_t (HailoRT) field-for-field so this header builds
// without the SDK.
struct HailoBboxF32 {
    float y_min;
    float x_min;
    float y_max;
    float x_max;
    float score;
};
static_assert(sizeof(HailoBboxF32) == 5 * sizeof(float), "packed bbox record");

// A record is only trustworthy if every field is finite, coordinates are
// normalized [0,1] and not inverted, and the score is in [0,1]. A NaN score
// would otherwise slip through downstream `score < threshold` comparisons
// (NaN compares false) and propagate into the tracker.
inline bool hailo_bbox_valid(const HailoBboxF32& b) {
    if (!std::isfinite(b.x_min) || !std::isfinite(b.y_min) || !std::isfinite(b.x_max) ||
        !std::isfinite(b.y_max) || !std::isfinite(b.score)) {
        return false;
    }
    if (b.score < 0.F || b.score > 1.F) {
        return false;
    }
    if (b.x_min < 0.F || b.x_min > 1.F || b.x_max < 0.F || b.x_max > 1.F ||
        b.y_min < 0.F || b.y_min > 1.F || b.y_max < 0.F || b.y_max > 1.F) {
        return false;
    }
    if (b.x_max < b.x_min || b.y_max < b.y_min) {
        return false; // inverted box
    }
    return true;
}

// Parses an NMS-by-class buffer of `size` bytes into model-input-normalized
// Detections (cx/cy/w/h, 0-based cls). Appends at most `max_dets` results to a
// cleared `out`; stops at the first truncated read or untrustworthy count.
// Individual bad records are dropped; degenerate (zero-area) boxes are dropped
// too — a zero width would divide the tracker's size-based range estimate.
inline void parse_nms_by_class(const std::uint8_t* data, std::size_t size,
                               int class_count, int max_dets,
                               std::vector<Detection>& out) {
    out.clear();
    if (data == nullptr || class_count <= 0 || max_dets <= 0) {
        return;
    }
    std::size_t off = 0;
    for (int c = 0; c < class_count; ++c) {
        float count_f = 0.F;
        if (size - off < sizeof count_f) {
            return; // truncated header
        }
        std::memcpy(&count_f, data + off, sizeof count_f);
        off += sizeof count_f;
        // The framing bound is what the remaining bytes can actually hold: a
        // count beyond that cannot be walked, so nothing after it can be
        // trusted either. (max_dets caps the OUTPUT below, not the count — a
        // device legitimately compiled with a larger max_bboxes_per_class must
        // still parse.)
        const std::size_t whole_boxes = (size - off) / sizeof(HailoBboxF32);
        const float max_fit = static_cast<float>(whole_boxes);
        if (!std::isfinite(count_f) || count_f < 0.F || count_f > max_fit) {
            return; // framing is broken past an invalid count
        }
        const auto n = static_cast<std::uint32_t>(count_f);
        for (std::uint32_t j = 0; j < n; ++j) {
            HailoBboxF32 b{};
            if (size - off < sizeof b) {
                return; // truncated record
            }
            std::memcpy(&b, data + off, sizeof b);
            off += sizeof b;
            if (!hailo_bbox_valid(b)) {
                continue; // untrusted record: drop
            }
            Detection d{};
            d.cx = 0.5F * (b.x_min + b.x_max);
            d.cy = 0.5F * (b.y_min + b.y_max);
            d.w = b.x_max - b.x_min;
            d.h = b.y_max - b.y_min;
            d.score = b.score;
            d.cls = c;
            if (d.w <= 0.F || d.h <= 0.F) {
                continue; // degenerate box
            }
            // Past the output cap, keep WALKING (the offsets must stay framed
            // for the remaining classes) but stop keeping.
            if (out.size() < static_cast<std::size_t>(max_dets)) {
                out.push_back(d);
            }
        }
    }
}

} // namespace riposte
