// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 — DCN-2026-006 EXT D-022 NMEA HDT parser tests.
//
// Verifies parseHdt() against happy path, talker variants, invalid
// qualifier, malformed numbers, NaN guard, and 360° wrap.

#include "san_rtk_gnss/nmea_parser.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <string>

namespace san_rtk_gnss
{
namespace
{

// Build a valid $...HDT line with checksum computed from body.
std::string makeHdt(const std::string & body)
{
  uint8_t cs = 0;
  for (char c : body) {
    cs ^= static_cast<uint8_t>(c);
  }
  char buf[8];
  std::snprintf(buf, sizeof(buf), "*%02X", cs);
  return "$" + body + buf;
}

// ─── Happy path ────────────────────────────────────────────────────────

TEST(NmeaHdtD022, BasicHeadingGP) {
  // u-blox style GP talker
  const std::string s = makeHdt("GPHDT,123.45,T");
  const auto r = parseHdt(s);
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(r->valid);
  EXPECT_NEAR(r->heading_deg, 123.45f, 1e-4f);
}

TEST(NmeaHdtD022, AcceptsGNTalker) {
  const std::string s = makeHdt("GNHDT,45.0,T");
  const auto r = parseHdt(s);
  ASSERT_TRUE(r.has_value());
  EXPECT_NEAR(r->heading_deg, 45.0f, 1e-4f);
}

TEST(NmeaHdtD022, AcceptsHETalker) {
  // Septentrio sometimes uses HE (heading-only) talker.
  const std::string s = makeHdt("HEHDT,0.0,T");
  const auto r = parseHdt(s);
  ASSERT_TRUE(r.has_value());
  EXPECT_NEAR(r->heading_deg, 0.0f, 1e-4f);
}

TEST(NmeaHdtD022, HeadingZeroAndJustUnder360) {
  {
    const auto r = parseHdt(makeHdt("GPHDT,0.000,T"));
    ASSERT_TRUE(r.has_value());
    EXPECT_NEAR(r->heading_deg, 0.0f, 1e-4f);
  }
  {
    const auto r = parseHdt(makeHdt("GPHDT,359.999,T"));
    ASSERT_TRUE(r.has_value());
    EXPECT_NEAR(r->heading_deg, 359.999f, 1e-3f);
  }
}

// ─── 360° wrap ─────────────────────────────────────────────────────────

TEST(NmeaHdtD022, ExactlyThreeSixtyWrapsToZero) {
  const auto r = parseHdt(makeHdt("GPHDT,360.0,T"));
  ASSERT_TRUE(r.has_value());
  EXPECT_NEAR(r->heading_deg, 0.0f, 1e-4f);
}

TEST(NmeaHdtD022, OverflowWrap) {
  // 720° = two full turns → 0°
  const auto r = parseHdt(makeHdt("GPHDT,720.0,T"));
  ASSERT_TRUE(r.has_value());
  EXPECT_NEAR(r->heading_deg, 0.0f, 1e-4f);
}

TEST(NmeaHdtD022, NegativeHeadingWrapsPositive) {
  // -90° should wrap to 270°. (Unusual on the wire — defensive.)
  const auto r = parseHdt(makeHdt("GPHDT,-90.0,T"));
  ASSERT_TRUE(r.has_value());
  EXPECT_NEAR(r->heading_deg, 270.0f, 1e-4f);
}

// ─── Rejections ────────────────────────────────────────────────────────

TEST(NmeaHdtD022, NonTQualifierRejected) {
  // Some legacy receivers emit 'M' for magnetic heading.
  const auto r = parseHdt(makeHdt("GPHDT,123.45,M"));
  EXPECT_FALSE(r.has_value())
    << "Magnetic 'M' qualifier must be rejected — we only fuse "
    << "true-north 'T' from dual-antenna RTK.";
}

TEST(NmeaHdtD022, WrongSentenceTypeRejected) {
  const auto r = parseHdt(makeHdt("GPGGA,123.45,T"));
  EXPECT_FALSE(r.has_value());
}

TEST(NmeaHdtD022, ChecksumMismatchRejected) {
  // Hand-crafted bad checksum.
  EXPECT_FALSE(parseHdt("$GPHDT,123.45,T*00").has_value());
}

TEST(NmeaHdtD022, MalformedHeadingRejected) {
  const auto r = parseHdt(makeHdt("GPHDT,not_a_number,T"));
  EXPECT_FALSE(r.has_value());
}

TEST(NmeaHdtD022, EmptyHeadingRejected) {
  const auto r = parseHdt(makeHdt("GPHDT,,T"));
  EXPECT_FALSE(r.has_value());
}

TEST(NmeaHdtD022, MissingTQualifierRejected) {
  // Only two fields (GPHDT + heading), no qualifier.
  const auto r = parseHdt(makeHdt("GPHDT,123.45"));
  EXPECT_FALSE(r.has_value());
}

}  // namespace
}  // namespace san_rtk_gnss
