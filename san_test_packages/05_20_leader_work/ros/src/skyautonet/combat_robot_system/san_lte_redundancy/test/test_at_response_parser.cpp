// SAN v1.5 Phase 2-E Turn 3 — AT response parser unit tests.
//
// Pure-logic gtest — runs without rclcpp / serial port / modem.
// Validates the field-extraction logic that LteModemNode relies on.
//
// Coverage:
//   C1  +CREG single-field response
//   C2  +CREG 2-field response (n,stat)
//   C3  +CREG with extra trailing fields (lac, cellid)
//   C4  Malformed +CREG → nullopt
//   C5  +COPS operator extraction (quoted)
//   C6  +QCSQ all fields populated
//   C7  +QCSQ partial fields (only rat + rssi)
//   C8  +CESQ rsrp encoding (-141 + n)
//   C9  +CGPADDR valid IPv4 extraction
//   C10 +CGPADDR rejects 0.0.0.0

#include "san_lte_redundancy/at_response_parser.hpp"

#include <gtest/gtest.h>

namespace san_lte_redundancy {
namespace {

// ─── +CREG ──────────────────────────────────────────────────────────────

TEST(AtResponseParser, C1_CregSingleField) {
  // Unsolicited: just the status code
  const auto r = parseCreg("+CREG: 1");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, CregStatus::Home);
}

TEST(AtResponseParser, C2_CregTwoFields) {
  // Query response: "<n>,<stat>"
  const auto r = parseCreg("+CREG: 2,5");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, CregStatus::Roaming);
}

TEST(AtResponseParser, C3_CregWithExtraFields) {
  // Full response with LAC + cellid
  const auto r = parseCreg("+CREG: 2,1,\"2A03\",\"01A2B3C\",7");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, CregStatus::Home);
}

TEST(AtResponseParser, C4_CregMalformedReturnsNullopt) {
  EXPECT_FALSE(parseCreg("garbage").has_value());
  EXPECT_FALSE(parseCreg("+COPS: 0,0,\"foo\"").has_value());
  EXPECT_FALSE(parseCreg("+CREG: ").has_value());
  EXPECT_FALSE(parseCreg("+CREG: 99").has_value());   // out of range
}

// ─── +COPS ──────────────────────────────────────────────────────────────

TEST(AtResponseParser, C5_CopsOperatorExtraction) {
  const auto r = parseCops("+COPS: 0,0,\"KT\",7");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "KT");

  const auto r2 = parseCops("+COPS: 0,0,\"SK Telecom\"");
  ASSERT_TRUE(r2.has_value());
  EXPECT_EQ(*r2, "SK Telecom");
}

TEST(AtResponseParser, C5b_CopsMissingOperatorNullopt) {
  EXPECT_FALSE(parseCops("+COPS: 0,0").has_value());
  EXPECT_FALSE(parseCops("+COPS: 0").has_value());
  EXPECT_FALSE(parseCops("+CREG: 1").has_value());
}

// ─── +QCSQ ──────────────────────────────────────────────────────────────

TEST(AtResponseParser, C6_QcsqAllFields) {
  // Typical Quectel response in LTE
  const auto r = parseQcsq("+QCSQ: \"LTE\",-77,-105,12,-9");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->rat,      "LTE");
  EXPECT_EQ(r->rssi_dbm, -77);
  EXPECT_EQ(r->rsrp_dbm, -105);
  EXPECT_EQ(r->sinr_db,  12);
  EXPECT_EQ(r->rsrq_db,  -9);
}

TEST(AtResponseParser, C7_QcsqPartialFields) {
  // Only rat + rssi
  const auto r = parseQcsq("+QCSQ: \"NR5G\",-65");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->rat,      "NR5G");
  EXPECT_EQ(r->rssi_dbm, -65);
  EXPECT_EQ(r->rsrp_dbm, INT32_MIN);   // not present
  EXPECT_EQ(r->sinr_db,  INT32_MIN);
  EXPECT_EQ(r->rsrq_db,  INT32_MIN);
}

TEST(AtResponseParser, C7b_QcsqEmptyRatNullopt) {
  EXPECT_FALSE(parseQcsq("+QCSQ: \"\",-65").has_value());
  EXPECT_FALSE(parseQcsq("+QCSQ:").has_value());
}

// ─── +CESQ ──────────────────────────────────────────────────────────────

TEST(AtResponseParser, C8_CesqRsrpEncoding) {
  // CESQ rsrp encoded as 0..97 → -141..-44 dBm.
  // n=0  → -141 dBm
  // n=97 → -44 dBm
  // rsrq encoded as 0..34 → -19.5..-3 dB.

  // n=80 → -141+80 = -61 dBm
  // rsrq=15 → -19.5 + 7.5 = -12 dB
  const auto r = parseCesq("+CESQ: 99,99,255,255,15,80");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->rsrp_dbm, -61);
  EXPECT_EQ(r->rsrq_db,  -12);   // (int) -12.0
}

TEST(AtResponseParser, C8b_CesqRejectsOutOfRange) {
  // rsrp encoded value 99 (out of 0..97) → leave INT32_MIN
  const auto r = parseCesq("+CESQ: 99,99,255,255,99,99");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->rsrp_dbm, INT32_MIN);
  EXPECT_EQ(r->rsrq_db,  INT32_MIN);
}

// ─── +CGPADDR ───────────────────────────────────────────────────────────

TEST(AtResponseParser, C9_CgpaddrValidIpv4) {
  const auto r = parseCgpaddr("+CGPADDR: 1,\"10.64.0.42\"");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "10.64.0.42");
}

TEST(AtResponseParser, C10_CgpaddrRejectsZeroIp) {
  EXPECT_FALSE(parseCgpaddr("+CGPADDR: 1,\"0.0.0.0\"").has_value());
  EXPECT_FALSE(parseCgpaddr("+CGPADDR: 1,\"\"").has_value());
  EXPECT_FALSE(parseCgpaddr("+CGPADDR: 1").has_value());
  EXPECT_FALSE(parseCgpaddr("+CGPADDR: 1,\"not.an.ip\"").has_value());
}

}  // namespace
}  // namespace san_lte_redundancy
