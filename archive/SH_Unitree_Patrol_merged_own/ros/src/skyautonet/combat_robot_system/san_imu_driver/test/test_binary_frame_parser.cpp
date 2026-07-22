// SAN v1.5 Phase 2-E Turn 5 — Binary IMU frame parser tests.
//
// Pure-logic standalone gtest, no ROS / serial needed.
//
// Coverage:
//   B1  Valid XorByte frame (1-byte sync, 1-byte len)
//   B2  Valid Sum16 frame (1-byte sync, 1-byte len)
//   B3  Valid 2-byte sync frame (e.g. 0xFA 0xFF preamble)
//   B4  Valid 2-byte length (large payload)
//   B5  Buffer too short → nullopt, no consumption
//   B6  Wrong sync byte → consumed=1 (drop one byte)
//   B7  Wrong 2nd sync byte (when two_byte_sync) → consumed=1
//   B8  Payload length exceeds max_payload → consumed=1
//   B9  Bad XOR checksum → consumed=1
//   B10 Bad Sum16 checksum → consumed=1

#include "san_imu_driver/binary_frame_parser.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

namespace san_imu_driver {
namespace {

uint8_t  xorCs(const std::vector<uint8_t>& v) {
  uint8_t c = 0; for (auto b : v) c ^= b; return c;
}
uint16_t sumCs(const std::vector<uint8_t>& v) {
  uint16_t s = 0; for (auto b : v) s = static_cast<uint16_t>(s + b); return s;
}

// ─── B1: Valid XorByte single-sync frame ────────────────────────────────

TEST(BinaryFrameParser, B1_ValidXorFrame) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0   = 0xFA;
  cfg.checksum_kind = ChecksumKind::XorByte;
  cfg.max_payload   = 64;

  std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> csum_region = {0x04};
  csum_region.insert(csum_region.end(), payload.begin(), payload.end());
  const uint8_t cs = xorCs(csum_region);

  std::vector<uint8_t> frame = {0xFA, 0x04};
  frame.insert(frame.end(), payload.begin(), payload.end());
  frame.push_back(cs);

  size_t consumed = 0;
  auto r = parseBinaryFrame(frame, cfg, &consumed);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(consumed, frame.size());
  EXPECT_EQ(r->payload, payload);
}

// ─── B2: Valid Sum16 frame ─────────────────────────────────────────────

TEST(BinaryFrameParser, B2_ValidSum16Frame) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0   = 0xAA;
  cfg.checksum_kind = ChecksumKind::Sum16;

  std::vector<uint8_t> payload = {0x10, 0x20, 0x30};
  std::vector<uint8_t> csum_region = {0x03};
  csum_region.insert(csum_region.end(), payload.begin(), payload.end());
  const uint16_t cs = sumCs(csum_region);

  std::vector<uint8_t> frame = {0xAA, 0x03};
  frame.insert(frame.end(), payload.begin(), payload.end());
  frame.push_back(static_cast<uint8_t>((cs >> 8) & 0xFF));
  frame.push_back(static_cast<uint8_t>(cs & 0xFF));

  size_t consumed = 0;
  auto r = parseBinaryFrame(frame, cfg, &consumed);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(consumed, frame.size());
  EXPECT_EQ(r->payload, payload);
}

// ─── B3: Two-byte sync ─────────────────────────────────────────────────

TEST(BinaryFrameParser, B3_TwoByteSync) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0    = 0xFA;
  cfg.sync_byte_1    = 0xFF;
  cfg.two_byte_sync  = true;
  cfg.checksum_kind  = ChecksumKind::XorByte;

  std::vector<uint8_t> payload = {0x55};
  std::vector<uint8_t> csum_region = {0x01, 0x55};
  std::vector<uint8_t> frame = {0xFA, 0xFF, 0x01, 0x55, xorCs(csum_region)};

  size_t consumed = 0;
  auto r = parseBinaryFrame(frame, cfg, &consumed);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(consumed, 5u);
  EXPECT_EQ(r->payload.size(), 1u);
  EXPECT_EQ(r->payload[0], 0x55);
}

// ─── B4: Two-byte length (large payload) ───────────────────────────────

TEST(BinaryFrameParser, B4_TwoByteLength) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0     = 0xFA;
  cfg.two_byte_length = true;
  cfg.max_payload     = 1024;
  cfg.checksum_kind   = ChecksumKind::XorByte;

  std::vector<uint8_t> payload(300, 0x7A);   // 300 bytes
  std::vector<uint8_t> csum_region = {0x01, 0x2C};  // 0x012C = 300
  csum_region.insert(csum_region.end(), payload.begin(), payload.end());

  std::vector<uint8_t> frame = {0xFA, 0x01, 0x2C};
  frame.insert(frame.end(), payload.begin(), payload.end());
  frame.push_back(xorCs(csum_region));

  size_t consumed = 0;
  auto r = parseBinaryFrame(frame, cfg, &consumed);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(consumed, 1u + 2u + 300u + 1u);
  EXPECT_EQ(r->payload.size(), 300u);
}

// ─── B5: Buffer too short ──────────────────────────────────────────────

TEST(BinaryFrameParser, B5_BufferTooShort) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0 = 0xFA;

  std::vector<uint8_t> short_buf = {0xFA, 0x04, 0x01, 0x02};   // missing 2 bytes
  size_t consumed = 99;
  auto r = parseBinaryFrame(short_buf, cfg, &consumed);
  EXPECT_FALSE(r.has_value());
  EXPECT_EQ(consumed, 0u);
}

// ─── B6: Wrong sync byte → drop ────────────────────────────────────────

TEST(BinaryFrameParser, B6_WrongSyncByteDropOne) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0 = 0xFA;

  std::vector<uint8_t> buf = {0xCC, 0xFA, 0x01, 0x55, 0x54};
  size_t consumed = 0;
  auto r = parseBinaryFrame(buf, cfg, &consumed);
  EXPECT_FALSE(r.has_value());
  EXPECT_EQ(consumed, 1u);
}

// ─── B7: Wrong 2nd sync byte (with two_byte_sync) ──────────────────────

TEST(BinaryFrameParser, B7_WrongSecondSyncByte) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0   = 0xFA;
  cfg.sync_byte_1   = 0xFF;
  cfg.two_byte_sync = true;

  std::vector<uint8_t> buf = {0xFA, 0xEE, 0x01, 0x00, 0x00};
  size_t consumed = 0;
  auto r = parseBinaryFrame(buf, cfg, &consumed);
  EXPECT_FALSE(r.has_value());
  EXPECT_EQ(consumed, 1u);  // drop only the first sync byte
}

// ─── B8: Payload length exceeds max ────────────────────────────────────

TEST(BinaryFrameParser, B8_PayloadLengthExceedsMax) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0 = 0xFA;
  cfg.max_payload = 16;

  std::vector<uint8_t> buf = {0xFA, 0xFF, /* len=255 > max=16 */
                              0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0};
  size_t consumed = 0;
  auto r = parseBinaryFrame(buf, cfg, &consumed);
  EXPECT_FALSE(r.has_value());
  EXPECT_EQ(consumed, 1u);
}

// ─── B9: Bad XOR checksum ──────────────────────────────────────────────

TEST(BinaryFrameParser, B9_BadXorChecksum) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0   = 0xFA;
  cfg.checksum_kind = ChecksumKind::XorByte;

  // Correct payload XOR would be 0x01^0x02 = 0x03; we use 0xFF.
  std::vector<uint8_t> buf = {0xFA, 0x02, 0x01, 0x02, 0xFF};
  size_t consumed = 0;
  auto r = parseBinaryFrame(buf, cfg, &consumed);
  EXPECT_FALSE(r.has_value());
  EXPECT_EQ(consumed, 1u);
}

// ─── B10: Bad Sum16 checksum ───────────────────────────────────────────

TEST(BinaryFrameParser, B10_BadSum16Checksum) {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0   = 0xAA;
  cfg.checksum_kind = ChecksumKind::Sum16;

  std::vector<uint8_t> buf = {0xAA, 0x02, 0x01, 0x02, 0xFF, 0xFF};
  size_t consumed = 0;
  auto r = parseBinaryFrame(buf, cfg, &consumed);
  EXPECT_FALSE(r.has_value());
  EXPECT_EQ(consumed, 1u);
}

}  // namespace
}  // namespace san_imu_driver
