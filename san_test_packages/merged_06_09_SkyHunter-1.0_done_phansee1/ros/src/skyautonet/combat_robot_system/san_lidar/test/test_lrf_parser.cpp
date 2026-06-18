// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 7 — LRF parser tests (standalone).
//
// Coverage:
//   L1  Simple "<range>" line (LWnav style) → strength=0.9 default
//   L2  Lightware LW20 "<range>,<strength>,<flags>" format
//   L3  Strength 0..1 passed through
//   L4  Strength 0..100 normalized
//   L5  Strength 0..255 normalized
//   L6  Empty/whitespace lines → nullopt
//   L7  Non-numeric range → nullopt
//   L8  Range below threshold → valid=false
//   L9  Range above max → valid=false
//   L10 Valid range in band → valid=true

#include "san_lidar/lrf_parser.hpp"
#include <gtest/gtest.h>

namespace san_lidar
{
namespace
{

TEST(LrfParser, L1_SimpleRangeOnly) {
  auto r = parseLrfLine("5.32");
  ASSERT_TRUE(r.has_value());
  EXPECT_FLOAT_EQ(r->range_m, 5.32f);
  EXPECT_FLOAT_EQ(r->return_strength, 0.9f);
  EXPECT_TRUE(r->valid);
}

TEST(LrfParser, L2_Lw20Format) {
  // Lightware LW20: "<range>,<strength>,<flags>"
  auto r = parseLrfLine("12.50,200,0");
  ASSERT_TRUE(r.has_value());
  EXPECT_FLOAT_EQ(r->range_m, 12.50f);
  // strength 200/255 ≈ 0.784
  EXPECT_NEAR(r->return_strength, 200.0f / 255.0f, 1e-4);
  EXPECT_TRUE(r->valid);
}

TEST(LrfParser, L3_StrengthAsNormalizedFloat) {
  auto r = parseLrfLine("3.0,0.85");
  ASSERT_TRUE(r.has_value());
  EXPECT_FLOAT_EQ(r->return_strength, 0.85f);
}

TEST(LrfParser, L4_StrengthAsPercent) {
  // 85 should be normalized as percent
  auto r = parseLrfLine("3.0,85");
  ASSERT_TRUE(r.has_value());
  EXPECT_NEAR(r->return_strength, 0.85f, 1e-4);
}

TEST(LrfParser, L5_StrengthAsByte) {
  // 200 should be normalized as 0..255 range
  auto r = parseLrfLine("3.0,200");
  ASSERT_TRUE(r.has_value());
  EXPECT_NEAR(r->return_strength, 200.0f / 255.0f, 1e-4);
}

TEST(LrfParser, L6_EmptyLineRejected) {
  EXPECT_FALSE(parseLrfLine("").has_value());
  EXPECT_FALSE(parseLrfLine("   ").has_value());
  EXPECT_FALSE(parseLrfLine("\r\n").has_value());
}

TEST(LrfParser, L7_NonNumericRejected) {
  EXPECT_FALSE(parseLrfLine("ERROR").has_value());
  EXPECT_FALSE(parseLrfLine("abc,1,0").has_value());
}

TEST(LrfParser, L8_BelowMinimumInvalid) {
  // range <= 0.05 → invalid (no return / too close)
  auto r = parseLrfLine("0.0");
  ASSERT_TRUE(r.has_value());
  EXPECT_FLOAT_EQ(r->range_m, 0.0f);
  EXPECT_FALSE(r->valid);

  auto r2 = parseLrfLine("0.03");
  ASSERT_TRUE(r2.has_value());
  EXPECT_FALSE(r2->valid);
}

TEST(LrfParser, L9_AboveMaxInvalid) {
  // default max = 200 m
  auto r = parseLrfLine("250.0");
  ASSERT_TRUE(r.has_value());
  EXPECT_FALSE(r->valid);
}

TEST(LrfParser, L10_ValidInBand) {
  // 0.05 m < range <= 200 m → valid
  auto r1 = parseLrfLine("0.5");
  ASSERT_TRUE(r1.has_value()); EXPECT_TRUE(r1->valid);

  auto r2 = parseLrfLine("100.0");
  ASSERT_TRUE(r2.has_value()); EXPECT_TRUE(r2->valid);

  auto r3 = parseLrfLine("200.0");
  ASSERT_TRUE(r3.has_value()); EXPECT_TRUE(r3->valid);
}

// ─── Phase 0 PR-B: NaN / Inf rejection ──────────────────────────────────
// Pre-patch strtof happily parsed "nan" / "inf" / "infinity"; the
// downstream `range <= 0.05 || range > max` check was false for NaN
// (any comparison with NaN is false) → valid=true, NaN range
// published. Reroute planner and fire-authorization do not defend
// against non-finite values.

TEST(LrfParser, L11_NanRangeRejected) {
  EXPECT_FALSE(parseLrfLine("nan").has_value());
  EXPECT_FALSE(parseLrfLine("NaN").has_value());
  EXPECT_FALSE(parseLrfLine("NAN,200,0").has_value());
}

TEST(LrfParser, L12_InfRangeRejected) {
  EXPECT_FALSE(parseLrfLine("inf").has_value());
  EXPECT_FALSE(parseLrfLine("-inf").has_value());
  EXPECT_FALSE(parseLrfLine("infinity,0.5").has_value());
}

TEST(LrfParser, L13_NanStrengthFieldFallsBackToDefault) {
  // Range itself is valid; strength is "nan" — toFloat() returns
  // nullopt → existing fallback path sets strength=0.9.
  auto r = parseLrfLine("3.0,nan");
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(r->valid);
  EXPECT_FLOAT_EQ(r->return_strength, 0.9f);
}

}  // namespace
}  // namespace san_lidar
