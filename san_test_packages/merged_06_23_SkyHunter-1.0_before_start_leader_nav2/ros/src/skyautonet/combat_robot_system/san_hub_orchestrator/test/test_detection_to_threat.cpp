// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 DCN-2026-010 D-028 — DetectionToThreatConverter tests.
//
// Pure-logic state machine, no ROS deps.
//
// Coverage:
//   T1  Fused detection at conf ≥ threshold → emit
//   T2  Fused detection below threshold → no emit
//   T3  Class → severity / threat_type mapping (drone, person, weapon, ...)
//   T4  RGB fallback suppressed while fused is fresh (≤ 1 s)
//   T5  RGB fallback engages after fused dropout > 1 s
//   T6  RGB fallback uses lower (rgb_confidence_threshold) gate
//   T7  RGB callbacks before any fused are authoritative (cold-start)
//   T8  Detail JSON encodes source + class + bbox + confidence
//   T9  Korean banner message format
//   G1..G7  computeGeo() monocular geolocation + class height priors

#include "san_hub_orchestrator/detection_to_threat.hpp"
#include "san_hub_orchestrator/threat_aggregator.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace san_hub_orchestrator
{
namespace
{

constexpr uint8_t CLASS_UNKNOWN = 0;
constexpr uint8_t CLASS_PERSON = 1;
constexpr uint8_t CLASS_VEHICLE = 2;
constexpr uint8_t CLASS_DRONE = 3;
constexpr uint8_t CLASS_WEAPON = 4;
constexpr uint8_t CLASS_ANIMAL = 5;

constexpr uint8_t TA_TYPE_DRONE_DETECTED = 5;
constexpr uint8_t TA_TYPE_OTHER = 99;

DetectionConverterConfig defaultCfg()
{
  DetectionConverterConfig cfg;
  cfg.confidence_threshold = 0.9f;
  cfg.rgb_confidence_threshold = 0.8f;
  cfg.fused_fallback_window_s = 1.0;
  return cfg;
}

TEST(DetectionToThreat, T1_FusedAboveThresholdEmits) {
  DetectionToThreatConverter c(defaultCfg());
  auto out = c.convert(
    CLASS_PERSON, 0.95f, 10, 20, 30, 40,
    DetectionSource::Fused);
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(out->severity, threat_severity::CRITICAL);
  EXPECT_EQ(out->threat_type, TA_TYPE_OTHER);
  EXPECT_FLOAT_EQ(out->confidence, 0.95f);
}

TEST(DetectionToThreat, T2_FusedBelowThresholdSuppressed) {
  DetectionToThreatConverter c(defaultCfg());
  auto out = c.convert(
    CLASS_PERSON, 0.85f, 0, 0, 0, 0,
    DetectionSource::Fused);
  EXPECT_FALSE(out.has_value());
}

TEST(DetectionToThreat, T3_ClassMapping) {
  DetectionToThreatConverter c(defaultCfg());

  auto drone = c.convert(
    CLASS_DRONE, 0.99f, 0, 0, 1, 1,
    DetectionSource::Fused);
  ASSERT_TRUE(drone.has_value());
  EXPECT_EQ(drone->severity, threat_severity::CRITICAL);
  EXPECT_EQ(drone->threat_type, TA_TYPE_DRONE_DETECTED);

  auto weapon = c.convert(
    CLASS_WEAPON, 0.99f, 0, 0, 1, 1,
    DetectionSource::Fused);
  ASSERT_TRUE(weapon.has_value());
  EXPECT_EQ(weapon->severity, threat_severity::FATAL);
  EXPECT_EQ(weapon->threat_type, TA_TYPE_OTHER);

  auto vehicle = c.convert(
    CLASS_VEHICLE, 0.99f, 0, 0, 1, 1,
    DetectionSource::Fused);
  ASSERT_TRUE(vehicle.has_value());
  EXPECT_EQ(vehicle->severity, threat_severity::WARNING);

  auto unknown = c.convert(
    CLASS_UNKNOWN, 0.99f, 0, 0, 1, 1,
    DetectionSource::Fused);
  ASSERT_TRUE(unknown.has_value());
  EXPECT_EQ(unknown->severity, threat_severity::INFO);
}

TEST(DetectionToThreat, T4_RgbSuppressedWhileFusedFresh) {
  DetectionToThreatConverter c(defaultCfg());
  c.markFusedReceived(1000);
  // 500 ms later — fused still authoritative.
  EXPECT_FALSE(c.shouldUseRgb(1500));
  // Exactly at the window boundary (1.0s) — still suppressed.
  EXPECT_FALSE(c.shouldUseRgb(2000));
}

TEST(DetectionToThreat, T5_RgbEngagesAfterFusedDropout) {
  DetectionToThreatConverter c(defaultCfg());
  c.markFusedReceived(1000);
  // 1.5 s later → past the 1.0 s fallback window.
  EXPECT_TRUE(c.shouldUseRgb(2500));
}

TEST(DetectionToThreat, T6_RgbUsesLowerGate) {
  DetectionToThreatConverter c(defaultCfg());
  // 0.85 < 0.9 fused gate, but ≥ 0.8 RGB gate → emit
  auto out = c.convert(
    CLASS_PERSON, 0.85f, 0, 0, 1, 1,
    DetectionSource::RgbFallback);
  EXPECT_TRUE(out.has_value());
  // 0.75 below both gates
  auto low = c.convert(
    CLASS_PERSON, 0.75f, 0, 0, 1, 1,
    DetectionSource::RgbFallback);
  EXPECT_FALSE(low.has_value());
}

TEST(DetectionToThreat, T7_RgbAuthoritativeOnColdStart) {
  DetectionToThreatConverter c(defaultCfg());
  // No markFusedReceived() yet → RGB should be authoritative
  EXPECT_TRUE(c.shouldUseRgb(0));
  EXPECT_TRUE(c.shouldUseRgb(123456));
  EXPECT_EQ(c.lastFusedMs(), 0u);
}

TEST(DetectionToThreat, T8_DetailJsonIncludesFieldsAndSource) {
  DetectionToThreatConverter c(defaultCfg());
  auto out = c.convert(
    CLASS_DRONE, 0.97f, 11, 22, 33, 44,
    DetectionSource::Fused);
  ASSERT_TRUE(out.has_value());
  // Spot-check that key fields are present (JSON shape stable enough
  // for L5 measure_latency.py to parse).
  EXPECT_NE(out->detail.find("\"source\":\"fused\""), std::string::npos);
  EXPECT_NE(out->detail.find("\"class\":\"drone\""), std::string::npos);
  EXPECT_NE(
    out->detail.find("\"bbox\":[11,22,33,44]"),
    std::string::npos);

  auto rgb = c.convert(
    CLASS_PERSON, 0.85f, 0, 0, 1, 1,
    DetectionSource::RgbFallback);
  ASSERT_TRUE(rgb.has_value());
  EXPECT_NE(
    rgb->detail.find("\"source\":\"rgb_fallback\""),
    std::string::npos);
}

TEST(DetectionToThreat, T9_KoreanBannerFormat) {
  DetectionToThreatConverter c(defaultCfg());
  auto out = c.convert(
    CLASS_PERSON, 0.92f, 0, 0, 1, 1,
    DetectionSource::Fused);
  ASSERT_TRUE(out.has_value());
  // Korean prefix + class + conf
  EXPECT_NE(out->message_ko.find("위협 검출"), std::string::npos);
  EXPECT_NE(out->message_ko.find("person"), std::string::npos);
  EXPECT_NE(out->message_ko.find("0.92"), std::string::npos);
}

// ─── computeGeo() — monocular geolocation (G1..G7) ─────────────────────

TEST(ComputeGeo, G1_CentrePixelBearingIsYawPlusPan) {
  // Target dead-centre: camera-relative azimuth/elevation are 0, so the
  // world bearing is exactly robot_yaw + gimbal_pan.
  auto g = computeGeo(
    320.0, 240.0, 320.0, 240.0, 550.0,
    0.5 /*yaw*/, 0.25 /*pan*/, 0.1 /*tilt*/,
    10.0 /*depth*/, 100.0, 1.7);
  ASSERT_TRUE(g.has_position);
  EXPECT_NEAR(g.bearing_deg, (0.5 + 0.25) * 180.0 / M_PI, 1e-3);
  EXPECT_NEAR(g.elevation_deg, 0.1 * 180.0 / M_PI, 1e-3);
  EXPECT_NEAR(g.range_m, 10.0, 1e-4);
}

TEST(ComputeGeo, G2_RightOfCentreDecreasesBearing) {
  // ENU/CCW convention: a target right of the image centre is clockwise
  // of boresight → smaller bearing.
  auto g = computeGeo(
    420.0, 240.0, 320.0, 240.0, 550.0,
    0.0, 0.0, 0.0, 5.0, 100.0, 1.7);
  ASSERT_TRUE(g.has_position);
  EXPECT_LT(g.bearing_deg, 0.0f);
  EXPECT_NEAR(g.bearing_deg, std::atan2(-100.0, 550.0) * 180.0 / M_PI, 1e-3);
}

TEST(ComputeGeo, G3_DepthZeroFallsBackToHeightPrior) {
  // Pinhole: range = focal * H / h_px = 550 * 1.7 / 110 = 8.5 m.
  auto g = computeGeo(
    320.0, 240.0, 320.0, 240.0, 550.0,
    0.0, 0.0, 0.0, 0.0 /*no depth*/, 110.0, 1.7);
  ASSERT_TRUE(g.has_position);
  EXPECT_NEAR(g.range_m, 8.5, 1e-3);
}

TEST(ComputeGeo, G4_NoHeightPriorRangeUnknownBearingValid) {
  // CLASS_UNKNOWN/WEAPON have no height prior → range 0 ("unknown" per
  // ThreatAlert.msg) but the angular fix is still published.
  auto g = computeGeo(
    400.0, 240.0, 320.0, 240.0, 550.0,
    0.0, 0.0, 0.0, 0.0, 110.0, 0.0 /*no prior*/);
  ASSERT_TRUE(g.has_position);
  EXPECT_FLOAT_EQ(g.range_m, 0.0f);
  EXPECT_NE(g.bearing_deg, 0.0f);
}

TEST(ComputeGeo, G5_InvalidFocalNoPosition) {
  auto g = computeGeo(
    320.0, 240.0, 320.0, 240.0, 0.0 /*bad focal*/,
    0.0, 0.0, 0.0, 10.0, 100.0, 1.7);
  EXPECT_FALSE(g.has_position);
}

TEST(ComputeGeo, G6_BearingWrapsIntoPlusMinus180) {
  // yaw + pan beyond +pi must wrap into (-180, 180].
  auto g = computeGeo(
    320.0, 240.0, 320.0, 240.0, 550.0,
    3.0, 1.0, 0.0, 5.0, 100.0, 1.7);
  ASSERT_TRUE(g.has_position);
  EXPECT_GE(g.bearing_deg, -180.0f);
  EXPECT_LE(g.bearing_deg, 180.0f);
  EXPECT_NEAR(g.bearing_deg, (3.0 + 1.0 - 2.0 * M_PI) * 180.0 / M_PI, 1e-3);
}

TEST(ComputeGeo, G7_ClassHeightPriors) {
  // Person/vehicle/drone/animal carry priors; weapon/unknown do not, so
  // their monocular range stays unknown instead of person-height-derived.
  EXPECT_DOUBLE_EQ(classToRealHeightM(CLASS_PERSON), 1.7);
  EXPECT_GT(classToRealHeightM(CLASS_VEHICLE), 0.0);
  EXPECT_GT(classToRealHeightM(CLASS_ANIMAL), 0.0);
  EXPECT_LT(classToRealHeightM(CLASS_DRONE), 1.0);   // not person-sized
  EXPECT_DOUBLE_EQ(classToRealHeightM(CLASS_WEAPON), 0.0);
  EXPECT_DOUBLE_EQ(classToRealHeightM(CLASS_UNKNOWN), 0.0);
}

}  // namespace
}  // namespace san_hub_orchestrator
