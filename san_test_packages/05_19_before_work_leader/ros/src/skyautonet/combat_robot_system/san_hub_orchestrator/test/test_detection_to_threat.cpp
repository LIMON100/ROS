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

#include "san_hub_orchestrator/detection_to_threat.hpp"
#include "san_hub_orchestrator/threat_aggregator.hpp"

#include <gtest/gtest.h>

namespace san_hub_orchestrator {
namespace {

constexpr uint8_t CLASS_UNKNOWN = 0;
constexpr uint8_t CLASS_PERSON  = 1;
constexpr uint8_t CLASS_VEHICLE = 2;
constexpr uint8_t CLASS_DRONE   = 3;
constexpr uint8_t CLASS_WEAPON  = 4;

constexpr uint8_t TA_TYPE_DRONE_DETECTED = 5;
constexpr uint8_t TA_TYPE_OTHER          = 99;

DetectionConverterConfig defaultCfg() {
  DetectionConverterConfig cfg;
  cfg.confidence_threshold     = 0.9f;
  cfg.rgb_confidence_threshold = 0.8f;
  cfg.fused_fallback_window_s  = 1.0;
  return cfg;
}

TEST(DetectionToThreat, T1_FusedAboveThresholdEmits) {
  DetectionToThreatConverter c(defaultCfg());
  auto out = c.convert(CLASS_PERSON, 0.95f, 10, 20, 30, 40,
                       DetectionSource::Fused);
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(out->severity,    threat_severity::CRITICAL);
  EXPECT_EQ(out->threat_type, TA_TYPE_OTHER);
  EXPECT_FLOAT_EQ(out->confidence, 0.95f);
}

TEST(DetectionToThreat, T2_FusedBelowThresholdSuppressed) {
  DetectionToThreatConverter c(defaultCfg());
  auto out = c.convert(CLASS_PERSON, 0.85f, 0, 0, 0, 0,
                       DetectionSource::Fused);
  EXPECT_FALSE(out.has_value());
}

TEST(DetectionToThreat, T3_ClassMapping) {
  DetectionToThreatConverter c(defaultCfg());

  auto drone = c.convert(CLASS_DRONE, 0.99f, 0, 0, 1, 1,
                         DetectionSource::Fused);
  ASSERT_TRUE(drone.has_value());
  EXPECT_EQ(drone->severity,    threat_severity::CRITICAL);
  EXPECT_EQ(drone->threat_type, TA_TYPE_DRONE_DETECTED);

  auto weapon = c.convert(CLASS_WEAPON, 0.99f, 0, 0, 1, 1,
                          DetectionSource::Fused);
  ASSERT_TRUE(weapon.has_value());
  EXPECT_EQ(weapon->severity,    threat_severity::FATAL);
  EXPECT_EQ(weapon->threat_type, TA_TYPE_OTHER);

  auto vehicle = c.convert(CLASS_VEHICLE, 0.99f, 0, 0, 1, 1,
                           DetectionSource::Fused);
  ASSERT_TRUE(vehicle.has_value());
  EXPECT_EQ(vehicle->severity, threat_severity::WARNING);

  auto unknown = c.convert(CLASS_UNKNOWN, 0.99f, 0, 0, 1, 1,
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
  auto out = c.convert(CLASS_PERSON, 0.85f, 0, 0, 1, 1,
                       DetectionSource::RgbFallback);
  EXPECT_TRUE(out.has_value());
  // 0.75 below both gates
  auto low = c.convert(CLASS_PERSON, 0.75f, 0, 0, 1, 1,
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
  auto out = c.convert(CLASS_DRONE, 0.97f, 11, 22, 33, 44,
                       DetectionSource::Fused);
  ASSERT_TRUE(out.has_value());
  // Spot-check that key fields are present (JSON shape stable enough
  // for L5 measure_latency.py to parse).
  EXPECT_NE(out->detail.find("\"source\":\"fused\""), std::string::npos);
  EXPECT_NE(out->detail.find("\"class\":\"drone\""), std::string::npos);
  EXPECT_NE(out->detail.find("\"bbox\":[11,22,33,44]"),
            std::string::npos);

  auto rgb = c.convert(CLASS_PERSON, 0.85f, 0, 0, 1, 1,
                       DetectionSource::RgbFallback);
  ASSERT_TRUE(rgb.has_value());
  EXPECT_NE(rgb->detail.find("\"source\":\"rgb_fallback\""),
            std::string::npos);
}

TEST(DetectionToThreat, T9_KoreanBannerFormat) {
  DetectionToThreatConverter c(defaultCfg());
  auto out = c.convert(CLASS_PERSON, 0.92f, 0, 0, 1, 1,
                       DetectionSource::Fused);
  ASSERT_TRUE(out.has_value());
  // Korean prefix + class + conf
  EXPECT_NE(out->message_ko.find("위협 검출"), std::string::npos);
  EXPECT_NE(out->message_ko.find("person"),    std::string::npos);
  EXPECT_NE(out->message_ko.find("0.92"),      std::string::npos);
}

}  // namespace
}  // namespace san_hub_orchestrator
