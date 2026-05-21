// SAN v1.5 Phase 2-E Turn 4 — NMEA parser unit tests.
//
// Pure-logic gtest, no ROS / serial needed.
//
// Coverage:
//   N1  XOR checksum match
//   N2  XOR checksum mismatch
//   N3  Missing '*' rejected
//   N4  ddmm.mmmm → decimal degrees (N hemisphere)
//   N5  ddmm.mmmm → decimal degrees (S/W hemisphere)
//   N6  ddmm.mmmm rejects invalid minutes (≥60)
//   N7  GGA real-world u-blox F9P sample (RTK_FIX)
//   N8  GGA RTK_FLOAT sample
//   N9  GGA NO_FIX sample
//   N10 GGA with empty optional fields (geoid sep, RTK age)
//   N11 GGA with talker GN (multi-GNSS) accepted
//   N12 GGA wrong sentence type (GLL) rejected
//   N13 GGA malformed checksum rejected
//   N14 GSA 3D fix with 8 satellites
//   N15 GSA no-fix mode 1 rejected fields default 0

#include "san_rtk_gnss/nmea_parser.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>

namespace san_rtk_gnss {
namespace {

// ─── Checksum ───────────────────────────────────────────────────────────

TEST(NmeaChecksum, N1_ChecksumMatch) {
  // Real u-blox sample: "$GNGGA,121234.00,3733.5300,N,12658.7800,E,4,12,0.8,55.3,M,18.5,M,2.0,0001*HH"
  // Compute expected XOR: between $ and * exclusive.
  const std::string body =
      "GNGGA,121234.00,3733.5300,N,12658.7800,E,4,12,0.8,55.3,M,18.5,M,2.0,0001";
  uint8_t cs = 0;
  for (char c : body) cs ^= static_cast<uint8_t>(c);
  char hex[3];
  std::snprintf(hex, sizeof(hex), "%02X", cs);
  const std::string sentence = "$" + body + "*" + std::string(hex);

  EXPECT_TRUE(nmeaChecksumOk(sentence));
}

TEST(NmeaChecksum, N2_ChecksumMismatch) {
  // Same body but bad checksum
  EXPECT_FALSE(nmeaChecksumOk("$GNGGA,1,2,3*FF"));
}

TEST(NmeaChecksum, N3_MissingStarRejected) {
  EXPECT_FALSE(nmeaChecksumOk("$GNGGA,1,2,3"));
  EXPECT_FALSE(nmeaChecksumOk(""));
  EXPECT_FALSE(nmeaChecksumOk("garbage*5C"));
}

// ─── ddmm.mmmm conversion ───────────────────────────────────────────────

TEST(DmToDeg, N4_NorthHemisphere) {
  // 3733.5300 N = 37° + 33.53/60 = 37.55883°
  auto v = parseDmToDeg("3733.5300", "N");
  ASSERT_TRUE(v.has_value());
  EXPECT_NEAR(*v, 37.558833, 1e-5);
}

TEST(DmToDeg, N5_SouthAndWestHemispheres) {
  auto s = parseDmToDeg("3733.5300", "S");
  auto w = parseDmToDeg("12658.7800", "W");
  ASSERT_TRUE(s.has_value());
  ASSERT_TRUE(w.has_value());
  EXPECT_NEAR(*s, -37.558833, 1e-5);
  EXPECT_NEAR(*w, -126.979666, 1e-5);
}

TEST(DmToDeg, N6_RejectsInvalidMinutes) {
  // Minutes 60.0+ is invalid
  EXPECT_FALSE(parseDmToDeg("3760.0000", "N").has_value());
  EXPECT_FALSE(parseDmToDeg("", "N").has_value());
}

// ─── GGA — helper for valid checksum ────────────────────────────────────

static std::string withChecksum(const std::string& body) {
  uint8_t cs = 0;
  for (char c : body) cs ^= static_cast<uint8_t>(c);
  char hex[3];
  std::snprintf(hex, sizeof(hex), "%02X", cs);
  return "$" + body + "*" + std::string(hex);
}

// ─── GGA cases ──────────────────────────────────────────────────────────

TEST(NmeaParseGga, N7_RtkFixRealSample) {
  // Quality 4 = RTK Fix
  const auto sent = withChecksum(
      "GNGGA,121234.00,3733.5300,N,12658.7800,E,4,12,0.8,55.3,M,18.5,M,2.0,0001");
  const auto r = parseGga(sent);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->hh, 12);
  EXPECT_EQ(r->mm, 12);
  EXPECT_EQ(r->ss, 34);
  EXPECT_NEAR(r->latitude_deg,  37.558833,  1e-5);
  EXPECT_NEAR(r->longitude_deg, 126.979666, 1e-5);
  EXPECT_EQ(r->fix_type, FixType::RtkFix);
  EXPECT_EQ(r->num_satellites_used, 12);
  EXPECT_FLOAT_EQ(r->hdop, 0.8f);
  EXPECT_DOUBLE_EQ(r->altitude_m, 55.3);
  EXPECT_DOUBLE_EQ(r->geoid_separation_m, 18.5);
  EXPECT_FLOAT_EQ(r->rtk_correction_age_s, 2.0f);
  EXPECT_EQ(r->reference_station_id, 1u);
}

TEST(NmeaParseGga, N8_RtkFloatSample) {
  const auto sent = withChecksum(
      "GPGGA,000000.00,3733.5300,N,12658.7800,E,5,10,1.2,55.3,M,18.5,M,,");
  const auto r = parseGga(sent);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->fix_type, FixType::RtkFloat);
  EXPECT_EQ(r->num_satellites_used, 10);
  EXPECT_FLOAT_EQ(r->rtk_correction_age_s, 0.0f);  // empty field
}

TEST(NmeaParseGga, N9_NoFixSample) {
  const auto sent = withChecksum(
      "GNGGA,000000.00,3733.5300,N,12658.7800,E,0,0,99.9,0.0,M,0.0,M,,");
  const auto r = parseGga(sent);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->fix_type, FixType::No);
  EXPECT_EQ(r->num_satellites_used, 0);
}

TEST(NmeaParseGga, N10_EmptyOptionalFields) {
  // No geoid sep, no RTK age, no station id
  const auto sent = withChecksum(
      "GNGGA,121234.00,3733.5300,N,12658.7800,E,2,8,1.5,55.0,M,,,,");
  const auto r = parseGga(sent);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->fix_type, FixType::Dgps);
  EXPECT_DOUBLE_EQ(r->geoid_separation_m, 0.0);   // default
  EXPECT_FLOAT_EQ(r->rtk_correction_age_s, 0.0f); // default
  EXPECT_EQ(r->reference_station_id, 0u);          // default
}

TEST(NmeaParseGga, N11_GnTalkerAccepted) {
  const auto sent = withChecksum(
      "GNGGA,121234.00,3733.5300,N,12658.7800,E,4,12,0.8,55.3,M,18.5,M,,");
  EXPECT_TRUE(parseGga(sent).has_value());
}

TEST(NmeaParseGga, N12_WrongSentenceTypeRejected) {
  // GLL is geographic position, not GGA — should reject
  const auto sent = withChecksum(
      "GPGLL,3733.5300,N,12658.7800,E,121234.00,A");
  EXPECT_FALSE(parseGga(sent).has_value());
}

TEST(NmeaParseGga, N13_MalformedChecksumRejected) {
  // Valid body but wrong checksum (FF instead of computed)
  EXPECT_FALSE(parseGga(
      "$GNGGA,121234.00,3733.5300,N,12658.7800,E,4,12,0.8,55.3,M,18.5,M,2.0,0001*FF"
      ).has_value());
}

// ─── GSA cases ──────────────────────────────────────────────────────────

TEST(NmeaParseGsa, N14_ThreeDFixEightSatellites) {
  // 8 satellite slots used, others empty
  const auto sent = withChecksum(
      "GNGSA,A,3,01,02,03,04,05,06,07,08,,,,,1.4,0.8,1.1");
  const auto r = parseGsa(sent);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->mode, 3);            // 3D fix
  EXPECT_EQ(r->num_used, 8);
  EXPECT_FLOAT_EQ(r->pdop, 1.4f);
  EXPECT_FLOAT_EQ(r->hdop, 0.8f);
  EXPECT_FLOAT_EQ(r->vdop, 1.1f);
}

TEST(NmeaParseGsa, N15_NoFixModeOneZeroSatellites) {
  const auto sent = withChecksum(
      "GNGSA,A,1,,,,,,,,,,,,,99.9,99.9,99.9");
  const auto r = parseGsa(sent);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->mode, 1);            // no-fix
  EXPECT_EQ(r->num_used, 0);
  EXPECT_FLOAT_EQ(r->pdop, 99.9f);
}

}  // namespace
}  // namespace san_rtk_gnss
