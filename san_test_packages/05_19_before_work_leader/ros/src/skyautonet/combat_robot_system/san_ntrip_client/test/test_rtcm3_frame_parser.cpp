// SAN v1.5 Phase 2-E Turn 4 — RTCM3 frame parser tests (standalone).
//
// Coverage:
//   R1  Valid empty payload frame (preamble + zero-length + CRC)
//   R2  Valid frame with 2-byte payload, msg type extracted
//   R3  Buffer too short → nullopt, no consumption
//   R4  No preamble at start → skip 1 byte (garbage drop)
//   R5  Reserved bits non-zero → treat as corrupt, skip preamble
//   R6  CRC mismatch → skip preamble
//   R7  Garbage prefix then valid frame: 2-step parse
//   R8  Real RTCM3 Type 1004 (24-byte payload) sample frame

#include "san_ntrip_client/rtcm3_frame_parser.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

namespace san_ntrip_client {
namespace {

// CRC-24Q identical to parser internal — duplicated for test
// construction (so tests don't depend on a private function).
uint32_t testCrc24q(const uint8_t* data, size_t len) {
  uint32_t crc = 0;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint32_t>(data[i]) << 16;
    for (int b = 0; b < 8; ++b) {
      crc <<= 1;
      if (crc & 0x1000000) crc ^= 0x1864CFB;
    }
  }
  return crc & 0xFFFFFF;
}

std::vector<uint8_t> buildFrame(const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> buf;
  buf.push_back(RTCM3_PREAMBLE);
  const uint16_t hdr = static_cast<uint16_t>(payload.size() & 0x3FF);
  buf.push_back(static_cast<uint8_t>(hdr >> 8));
  buf.push_back(static_cast<uint8_t>(hdr & 0xFF));
  buf.insert(buf.end(), payload.begin(), payload.end());
  const uint32_t crc = testCrc24q(buf.data(), buf.size());
  buf.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
  buf.push_back(static_cast<uint8_t>((crc >>  8) & 0xFF));
  buf.push_back(static_cast<uint8_t>( crc        & 0xFF));
  return buf;
}

// ─── Tests ──────────────────────────────────────────────────────────────

TEST(Rtcm3, R1_ValidEmptyPayload) {
  auto buf = buildFrame({});
  size_t consumed = 0;
  auto f = parseRtcm3(buf, &consumed);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(consumed, 6u);            // 3 header + 0 payload + 3 CRC
  EXPECT_EQ(f->bytes.size(), 6u);
  EXPECT_EQ(f->message_type, 0u);     // no payload → 0
}

TEST(Rtcm3, R2_TwoBytePayloadMessageType) {
  // Message type 1004 = 0x3EC. Encoded as first 12 bits of payload.
  // 0x3EC = 0011 1110 1100 → byte0 = 0x3E, byte1 high nibble = 0xC
  std::vector<uint8_t> payload = {0x3E, 0xC0};
  auto buf = buildFrame(payload);
  size_t consumed = 0;
  auto f = parseRtcm3(buf, &consumed);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(f->message_type, 1004u);
}

TEST(Rtcm3, R3_BufferTooShortReturnsNullopt) {
  auto full = buildFrame({0x3E, 0xC0});
  // truncate to 5 bytes (need 8)
  std::vector<uint8_t> short_buf(full.begin(), full.begin() + 5);
  size_t consumed = 99;
  auto f = parseRtcm3(short_buf, &consumed);
  EXPECT_FALSE(f.has_value());
  EXPECT_EQ(consumed, 0u);    // caller waits for more bytes
}

TEST(Rtcm3, R4_NoPreambleSkipsOneByte) {
  std::vector<uint8_t> buf = {0xAA, 0xBB, 0xCC};
  size_t consumed = 0;
  auto f = parseRtcm3(buf, &consumed);
  EXPECT_FALSE(f.has_value());
  EXPECT_EQ(consumed, 1u);
}

TEST(Rtcm3, R5_ReservedBitsNonZeroCorrupt) {
  // Preamble OK, but header byte 1 has reserved upper-6 bits set
  std::vector<uint8_t> buf = {0xD3, 0xFC, 0x00, 0, 0, 0, 0, 0, 0};
  size_t consumed = 0;
  auto f = parseRtcm3(buf, &consumed);
  EXPECT_FALSE(f.has_value());
  EXPECT_EQ(consumed, 1u);    // drop preamble byte
}

TEST(Rtcm3, R6_CrcMismatchCorrupt) {
  auto buf = buildFrame({0x12, 0x34});
  buf.back() ^= 0xFF;          // flip last CRC byte
  size_t consumed = 0;
  auto f = parseRtcm3(buf, &consumed);
  EXPECT_FALSE(f.has_value());
  EXPECT_EQ(consumed, 1u);
}

TEST(Rtcm3, R7_GarbagePrefixThenFrame) {
  std::vector<uint8_t> buf = {0xAA, 0xBB};
  auto frame = buildFrame({0x3E, 0xC0});
  buf.insert(buf.end(), frame.begin(), frame.end());
  // Round 1: garbage drop
  size_t c1 = 0;
  auto r1 = parseRtcm3(buf, &c1);
  EXPECT_FALSE(r1.has_value());
  EXPECT_EQ(c1, 1u);
  buf.erase(buf.begin(), buf.begin() + c1);
  // Round 2: another garbage drop
  size_t c2 = 0;
  auto r2 = parseRtcm3(buf, &c2);
  EXPECT_FALSE(r2.has_value());
  EXPECT_EQ(c2, 1u);
  buf.erase(buf.begin(), buf.begin() + c2);
  // Round 3: valid frame
  size_t c3 = 0;
  auto r3 = parseRtcm3(buf, &c3);
  ASSERT_TRUE(r3.has_value());
  EXPECT_EQ(r3->message_type, 1004u);
}

TEST(Rtcm3, R8_RealFrame1004Sized) {
  // RTCM3 Type 1004 payload is ~24 bytes for a few satellites.
  std::vector<uint8_t> payload(24, 0);
  payload[0] = 0x3E;             // upper byte of 1004
  payload[1] = 0xC0;             // lower nibble of 1004 in high half
  auto buf = buildFrame(payload);
  size_t consumed = 0;
  auto f = parseRtcm3(buf, &consumed);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(consumed, 30u);           // 3 + 24 + 3
  EXPECT_EQ(f->message_type, 1004u);
}

}  // namespace
}  // namespace san_ntrip_client
