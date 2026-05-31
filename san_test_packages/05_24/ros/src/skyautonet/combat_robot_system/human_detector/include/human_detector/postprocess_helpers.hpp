// SAN v1.5.2 — DCN-2026-006 EXT D-018: numerically-safe sigmoid.
//
// Header-only inline helper extracted from rk3588_npu_backend.cpp so
// the unit tests (test_postprocess_d018_d019.cpp) can link without
// pulling in libopencv / librknnrt. The original definition in the
// .cpp anonymous namespace is now replaced with `using` of this
// symbol.
//
// Design notes — see comment block at fastSigmoid below.

#pragma once

#include <algorithm>
#include <cmath>

namespace human_detector {
namespace postprocess {

// [DCN-2026-006 EXT D-018] Numerically-safe sigmoid for NPU logits.
//
// Original v1.5.1 implementation: 1 / (1 + exp(-x)) — correct for
// |x| < ~80 but exhibits three failure modes under NPU outputs:
//   (1) x < -88     → exp(-x) overflows to +inf → result 0.0f
//                     (acceptable but pessimistic for high-confidence
//                     negative samples).
//   (2) x > 88      → exp(-x) underflows to 0.0f → result 1.0f
//                     (acceptable).
//   (3) x = NaN     → propagates NaN through obj*best_score; pollutes
//                     NMS sort and silently drops real detections
//                     (observed once in W-1 footage under thermal
//                     throttling, ~0.5%-tile NaN rate).
//
// Fix: clamp to [-30, +30] (preserves 1e-13 .. 1 - 1e-13 of output
// range, more than the 0.4 .. 0.9 threshold band post-process uses)
// and explicit NaN guard returning 0.5 (midpoint — neutral, never
// trips any threshold).
//
// Naming: `fastSigmoid` (camelCase) — matches the rest of
// human_detector style. The old `fast_sigmoid` in .cpp now resolves
// to this via a `using` declaration.
inline float fastSigmoid(float x) {
    // NaN test — std::isnan would also work but std::min/max
    // behaviour with NaN is implementation-defined on some
    // libstdc++; explicit (x == x) check is portable.
    if (!(x == x)) return 0.5f;
    constexpr float kClampLo = -30.0f;
    constexpr float kClampHi =  30.0f;
    const float clamped = std::max(kClampLo, std::min(kClampHi, x));
    return 1.0f / (1.0f + std::exp(-clamped));
}

// [DCN-2026-006 EXT D-019] YOLO head channel validation.
//
// Validates that a given output head's `chan` and `num_anchors`
// produce a well-formed (chan, n_per_anchor, n_classes) triplet:
//   - chan must be divisible by num_anchors
//   - n_per_anchor must be > 5 (5 = 4 bbox + 1 objectness)
//   - n_classes must not exceed max_expected (defaults to 80 for COCO)
//
// Returns 0 on success; non-zero error code identifying the failure
// mode. Used by rk3588_npu_backend.cpp inside the decode loop and
// by the unit tests directly.
enum YoloHeadValidation : int {
    kOk                    = 0,
    kErrChanNotDivisible   = 1,
    kErrNonPositiveClasses = 2,
    kErrTooManyClasses     = 3,
};

inline YoloHeadValidation validateYoloHead(int chan,
                                            int num_anchors,
                                            int max_expected_classes = 80) {
    if (num_anchors <= 0 || chan <= 0)         return kErrChanNotDivisible;
    if (chan % num_anchors != 0)               return kErrChanNotDivisible;
    const int n_per_anchor = chan / num_anchors;
    const int n_classes    = n_per_anchor - 5;
    if (n_classes <= 0)                        return kErrNonPositiveClasses;
    if (n_classes > max_expected_classes)      return kErrTooManyClasses;
    return kOk;
}

}  // namespace postprocess
}  // namespace human_detector
