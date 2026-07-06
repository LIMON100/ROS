// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 5 — Generic binary IMU frame parser.
//
// Many serial IMUs share a common framing pattern:
//   [SYNC]   1-2 byte preamble (0xFA, 0x55AA, etc.)
//   [LEN]    1-2 byte payload length
//   [PAYLOAD] N bytes of IMU data
//   [CSUM]   1-2 byte checksum (XOR or 16-bit sum)
//
// Concrete IMU models (BMI088, VectorNav VN-100, Xsens MTi-3, etc.) only
// differ in the sync byte(s), length encoding, checksum algorithm, and
// payload layout. This parser handles the *framing* — sync detection,
// length read, full-frame buffering, and checksum verification — and
// returns the payload to the caller, who decodes it for their specific
// IMU model.
//
// Standalone testable: pure C++17, no ROS / serial dep.

#ifndef SAN_IMU_DRIVER__BINARY_FRAME_PARSER_HPP_
#define SAN_IMU_DRIVER__BINARY_FRAME_PARSER_HPP_

#include <cstdint>
#include <optional>
#include <vector>

namespace san_imu_driver
{

/// Checksum algorithm. Most IMUs use one of two:
///   * XorByte:  XOR over all bytes including sync (e.g. Xsens MT)
///   * Sum16:    16-bit modular sum over [LEN..PAYLOAD] (e.g. VectorNav)
enum class ChecksumKind
{
  XorByte,
  Sum16,
};

struct BinaryFrameConfig
{
  uint8_t sync_byte_0 = 0xFA;
  uint8_t sync_byte_1 = 0x00;          // 0 = single-byte sync
  bool two_byte_sync = false;
  bool two_byte_length = false;           // false = 1-byte length
  ChecksumKind checksum_kind = ChecksumKind::XorByte;
  // Maximum allowed payload length — bounds runaway frames.
  size_t max_payload = 256;
};

struct BinaryFrame
{
  std::vector<uint8_t> payload;   // contents only (no sync/len/csum)
};

/// Try to parse the next complete frame from `buffer`. On success
/// returns the frame; sets `consumed_out` to the number of bytes to
/// drop from the front of `buffer`. On "need more data", returns
/// std::nullopt with `consumed_out = 0`. On garbage prefix, returns
/// std::nullopt with `consumed_out = 1` (drop one byte and retry).
///
/// Same usage pattern as the RTCM3 parser from Turn 4.
std::optional<BinaryFrame> parseBinaryFrame(
  const std::vector<uint8_t> & buffer,
  const BinaryFrameConfig & cfg,
  size_t * consumed_out);

}  // namespace san_imu_driver

#endif  // SAN_IMU_DRIVER__BINARY_FRAME_PARSER_HPP_
