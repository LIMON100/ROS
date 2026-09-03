#include "ModelIo.h"

#include "IDetector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "riposte/Tunables.h"

namespace riposte {

namespace {
// Intersection-over-union of two boxes given as centre + size, in one shared
// normalization (model space — see the nms() contract).
float iou(const Detection& a, const Detection& b) {
    const float ax0 = a.cx - (a.w * 0.5F);
    const float ax1 = a.cx + (a.w * 0.5F);
    const float ay0 = a.cy - (a.h * 0.5F);
    const float ay1 = a.cy + (a.h * 0.5F);
    const float bx0 = b.cx - (b.w * 0.5F);
    const float bx1 = b.cx + (b.w * 0.5F);
    const float by0 = b.cy - (b.h * 0.5F);
    const float by1 = b.cy + (b.h * 0.5F);

    const float ix = std::max(0.F, std::min(ax1, bx1) - std::max(ax0, bx0));
    const float iy = std::max(0.F, std::min(ay1, by1) - std::max(ay0, by0));
    const float inter = ix * iy;
    const float uni = (a.w * a.h) + (b.w * b.h) - inter;
    return (uni > 1e-12F) ? (inter / uni) : 0.F;
}
} // namespace

Letterbox letterbox_fit(int src_w, int src_h, int model_size) {
    Letterbox lb;
    if (src_w <= 0 || src_h <= 0 || model_size <= 0) {
        return lb; // invalid: caller checks valid()
    }
    lb.src_w = src_w;
    lb.src_h = src_h;
    lb.model_size = model_size;
    lb.scale = std::min(static_cast<float>(model_size) / static_cast<float>(src_w),
                        static_cast<float>(model_size) / static_cast<float>(src_h));
    lb.content_w = std::min(
        model_size, static_cast<int>(std::lround(static_cast<float>(src_w) * lb.scale)));
    lb.content_h = std::min(
        model_size, static_cast<int>(std::lround(static_cast<float>(src_h) * lb.scale)));
    lb.pad_x = (model_size - lb.content_w) / 2;
    lb.pad_y = (model_size - lb.content_h) / 2;
    return lb;
}

void letterbox_forward(const Letterbox& lb, float src_nx, float src_ny, float& mx,
                       float& my) {
    if (!lb.valid()) {
        mx = src_nx;
        my = src_ny;
        return;
    }
    const float s = static_cast<float>(lb.model_size);
    mx = ((src_nx * static_cast<float>(lb.src_w) * lb.scale) +
          static_cast<float>(lb.pad_x)) /
         s;
    my = ((src_ny * static_cast<float>(lb.src_h) * lb.scale) +
          static_cast<float>(lb.pad_y)) /
         s;
}

bool letterbox_undo(const Letterbox& lb, Detection& d) {
    if (!lb.valid() || lb.scale <= 0.F) {
        return false;
    }
    const float s = static_cast<float>(lb.model_size);
    const float px = d.cx * s; // model pixels
    const float py = d.cy * s;
    // Reject a centre that fell in the padding: the network fired on the fill,
    // not on the image. Mapping it back would place a phantom target at the
    // frame edge, and the tracker has no way to tell it from a real one.
    if (px < static_cast<float>(lb.pad_x) ||
        px > static_cast<float>(lb.pad_x + lb.content_w) ||
        py < static_cast<float>(lb.pad_y) ||
        py > static_cast<float>(lb.pad_y + lb.content_h)) {
        return false;
    }
    d.cx = (px - static_cast<float>(lb.pad_x)) / lb.scale / static_cast<float>(lb.src_w);
    d.cy = (py - static_cast<float>(lb.pad_y)) / lb.scale / static_cast<float>(lb.src_h);
    // Sizes carry no offset, only the scale — and each axis normalizes against
    // its own source dimension, which is exactly what the stretch would have
    // broken.
    d.w = (d.w * s) / lb.scale / static_cast<float>(lb.src_w);
    d.h = (d.h * s) / lb.scale / static_cast<float>(lb.src_h);
    return true;
}

void nms(std::vector<Detection>& dets, float iou_threshold) {
    if (dets.size() < 2) {
        return;
    }
    std::sort(dets.begin(), dets.end(),
              [](const Detection& a, const Detection& b) { return a.score > b.score; });
    std::vector<bool> dropped(dets.size(), false);
    for (std::size_t i = 0; i < dets.size(); ++i) {
        if (dropped[i]) {
            continue;
        }
        for (std::size_t j = i + 1; j < dets.size(); ++j) {
            if (dropped[j] || dets[j].cls != dets[i].cls) {
                continue; // different classes never suppress each other
            }
            if (iou(dets[i], dets[j]) > iou_threshold) {
                dropped[j] = true;
            }
        }
    }
    std::size_t keep = 0;
    for (std::size_t i = 0; i < dets.size(); ++i) {
        if (!dropped[i]) {
            dets[keep++] = dets[i];
        }
    }
    dets.resize(keep);
}

void decode_yolov8(const float* tensor, std::size_t element_count, int num_classes,
                   int num_anchors, int model_size, float score_threshold,
                   std::vector<Detection>& out) {
    out.clear();
    if (tensor == nullptr || num_classes <= 0 || num_anchors <= 0 || model_size <= 0) {
        return;
    }
    const std::size_t rows = 4U + static_cast<std::size_t>(num_classes);
    const std::size_t anchors = static_cast<std::size_t>(num_anchors);
    // Refuse to read past the buffer: a mismatched num_classes/num_anchors would
    // otherwise walk off the tensor and produce plausible-looking garbage.
    if (element_count < rows * anchors) {
        return;
    }
    const float inv = 1.F / static_cast<float>(model_size);
    for (std::size_t a = 0; a < anchors; ++a) {
        // Highest-scoring class for this anchor.
        int best_cls = -1;
        float best_score = score_threshold;
        for (int c = 0; c < num_classes; ++c) {
            const float sc = tensor[((static_cast<std::size_t>(4 + c)) * anchors) + a];
            if (sc >= best_score) {
                best_score = sc;
                best_cls = c;
            }
        }
        if (best_cls < 0) {
            continue;
        }
        Detection d{};
        d.cx = tensor[a] * inv;                // row 0
        d.cy = tensor[anchors + a] * inv;      // row 1
        d.w = tensor[(2 * anchors) + a] * inv; // row 2
        d.h = tensor[(3 * anchors) + a] * inv; // row 3
        d.score = best_score;
        d.cls = best_cls;
        // Untrusted device tensor (P1-06): every scalar must be finite — a NaN
        // width passes `w <= 0` (NaN compares false both ways) and would ride
        // into the tracker/estimator — and inside coarse bounds. Centres may sit
        // slightly outside the model square (letterbox_undo culls), but far
        // outside or oversized is tensor garbage, not a detection.
        if (!std::isfinite(d.cx) || !std::isfinite(d.cy) || !std::isfinite(d.w) ||
            !std::isfinite(d.h) || !std::isfinite(d.score)) {
            continue;
        }
        if (d.cx < -0.5F || d.cx > 1.5F || d.cy < -0.5F || d.cy > 1.5F || d.w > 2.F ||
            d.h > 2.F) {
            continue;
        }
        if (d.w <= 0.F || d.h <= 0.F) {
            continue; // degenerate box
        }
        out.push_back(d);
    }
    // Candidate cap (P1-06): keep only the top-K by score so a garbage tensor
    // passing thousands of anchors cannot turn the O(N^2) NMS into a stall.
    const auto cap = static_cast<std::size_t>(tun::RAW_DECODE_MAX_CANDS);
    if (out.size() > cap) {
        std::nth_element(
            out.begin(), out.begin() + static_cast<std::ptrdiff_t>(cap), out.end(),
            [](const Detection& a, const Detection& b) { return a.score > b.score; });
        out.resize(cap);
    }
}

} // namespace riposte
