// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 — DCN-2026-006 EXT D-018/D-019 unit tests.
//
// Exercises the postprocess_helpers in isolation:
//   D-018: fastSigmoid numerical safety
//   D-019: validateYoloHead channel layout sanity

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "human_detector/postprocess_helpers.hpp"

using human_detector::postprocess::fastSigmoid;
using human_detector::postprocess::validateYoloHead;
using human_detector::postprocess::kOk;
using human_detector::postprocess::kErrChanNotDivisible;
using human_detector::postprocess::kErrNonPositiveClasses;
using human_detector::postprocess::kErrTooManyClasses;

// ─── D-018: fastSigmoid ────────────────────────────────────────────────

TEST(FastSigmoidD018, KnownValues) {
  EXPECT_NEAR(fastSigmoid(0.0f), 0.5f, 1e-6f);
  EXPECT_NEAR(fastSigmoid(1.0f), 0.7310585f, 1e-5f);
  EXPECT_NEAR(fastSigmoid(-1.0f), 0.2689414f, 1e-5f);
  EXPECT_NEAR(fastSigmoid(5.0f), 0.99330714f, 1e-5f);
  EXPECT_NEAR(fastSigmoid(-5.0f), 0.00669285f, 1e-5f);
}

TEST(FastSigmoidD018, MonotonicAcrossRange) {
  // Strict monotonicity within the unsaturated range.
  float prev = fastSigmoid(-10.0f);
  for (float x = -10.0f; x <= 10.0f; x += 0.5f) {
    const float y = fastSigmoid(x);
    EXPECT_GE(y, prev);
    prev = y;
  }
}

TEST(FastSigmoidD018, ClampsExtremesNoOverflow) {
  // Inputs far beyond the natural numerical range. The pre-fix
  // implementation: x = -1000 → exp(1000) → +inf → result 0.0 OK,
  // but x = +1000 already gave 1.0f. The risk was on the negative
  // side where INT8 quant overrun produced floats below -88, and
  // on NaN inputs.
  EXPECT_TRUE(std::isfinite(fastSigmoid(-1000.0f)));
  EXPECT_TRUE(std::isfinite(fastSigmoid(1000.0f)));
  EXPECT_NEAR(fastSigmoid(-1000.0f), 0.0f, 1e-5f);    // clamped to -30, ~9e-14
  EXPECT_NEAR(fastSigmoid(1000.0f), 1.0f, 1e-5f);     // clamped to +30
}

TEST(FastSigmoidD018, NaNReturnsNeutralMidpoint) {
  // The critical guarantee: NaN input must NOT propagate through
  // the post-process arithmetic. We return 0.5 — neutral — which
  // never trips any reasonable conf_threshold and so the detection
  // is silently dropped rather than poisoning NMS.
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float r = fastSigmoid(nan);
  EXPECT_FALSE(std::isnan(r));
  EXPECT_NEAR(r, 0.5f, 1e-6f);
}

TEST(FastSigmoidD018, InfinityIsHandled) {
  const float pinf = std::numeric_limits<float>::infinity();
  const float ninf = -pinf;
  // +inf clamps to +30 → ~1.0
  EXPECT_NEAR(fastSigmoid(pinf), 1.0f, 1e-5f);
  // -inf clamps to -30 → ~0.0
  EXPECT_NEAR(fastSigmoid(ninf), 0.0f, 1e-5f);
}

TEST(FastSigmoidD018, ResultAlwaysInUnitInterval) {
  for (float x = -100.0f; x <= 100.0f; x += 1.5f) {
    const float y = fastSigmoid(x);
    EXPECT_GE(y, 0.0f);
    EXPECT_LE(y, 1.0f);
  }
}

// ─── D-019: validateYoloHead ───────────────────────────────────────────

TEST(ValidateYoloHeadD019, HappyPathCOCO) {
  // YOLOv5 COCO: 3 anchors × (5 + 80) = 255 channels per head.
  EXPECT_EQ(validateYoloHead(255, 3), kOk);
}

TEST(ValidateYoloHeadD019, HappyPathSingleClass) {
  // Custom mannequin model: 3 anchors × (5 + 1) = 18 channels.
  EXPECT_EQ(validateYoloHead(18, 3), kOk);
}

TEST(ValidateYoloHeadD019, ChannelNotDivisibleByAnchors) {
  // 256 ÷ 3 → non-integer. Anchor/model mismatch.
  EXPECT_EQ(validateYoloHead(256, 3), kErrChanNotDivisible);
}

TEST(ValidateYoloHeadD019, NonPositiveClasses) {
  // chan=15, anchors=3 → n_per_anchor=5 → n_classes=0.
  EXPECT_EQ(validateYoloHead(15, 3), kErrNonPositiveClasses);
  // chan=9, anchors=3 → n_per_anchor=3 → n_classes=-2.
  EXPECT_EQ(validateYoloHead(9, 3), kErrNonPositiveClasses);
}

TEST(ValidateYoloHeadD019, TooManyClassesRejected) {
  // chan=3 × (5 + 1000) = 3015 → n_classes=1000 > 80.
  // Strong signal: wrong model file loaded.
  EXPECT_EQ(validateYoloHead(3015, 3), kErrTooManyClasses);
}

TEST(ValidateYoloHeadD019, CustomMaxExpectedClasses) {
  // If caller raises the cap (e.g. running an experimental 200-class
  // model), the validator should accept what was previously rejected.
  EXPECT_EQ(validateYoloHead(3015, 3, /*max=*/ 2000), kOk);
}

TEST(ValidateYoloHeadD019, DegenerateInputsRejected) {
  EXPECT_EQ(validateYoloHead(0, 3), kErrChanNotDivisible);
  EXPECT_EQ(validateYoloHead(255, 0), kErrChanNotDivisible);
  EXPECT_EQ(validateYoloHead(-3, 3), kErrChanNotDivisible);
}
