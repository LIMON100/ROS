// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 4 — RTCM3 frame parser implementation.

#include "san_ntrip_client/rtcm3_frame_parser.hpp"

#include <array>

namespace san_ntrip_client
{

namespace
{

// CRC-24Q (Qualcomm), polynomial 0x1864CFB, init 0x000000, no XOR out.
// Used by RTCM3, also GPS, OpenPGP. Table-free implementation.
uint32_t crc24q(const uint8_t * data, size_t len)
{
  uint32_t crc = 0;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint32_t>(data[i]) << 16;
    for (int b = 0; b < 8; ++b) {
      crc <<= 1;
      if (crc & 0x1000000) {crc ^= 0x1864CFB;}
    }
  }
  return crc & 0xFFFFFF;
}

}  // namespace

std::optional<Rtcm3Frame> parseRtcm3(
  const std::vector<uint8_t> & buffer,
  size_t * consumed_out)
{
  *consumed_out = 0;
  if (buffer.size() < 3) {return std::nullopt;}

  // Sync on preamble. If first byte isn't 0xD3, skip it as garbage.
  if (buffer[0] != RTCM3_PREAMBLE) {
    *consumed_out = 1;
    return std::nullopt;
  }

  // Bytes 1-2 carry the 10-bit length (lower 10 bits of byte1<<8 | byte2).
  // The upper 6 bits of byte 1 are reserved and should be 0.
  const uint16_t hdr =
    (static_cast<uint16_t>(buffer[1]) << 8) | buffer[2];
  if ((hdr & 0xFC00) != 0) {
    // Reserved bits not zero — corrupted frame; skip preamble byte
    *consumed_out = 1;
    return std::nullopt;
  }
  const uint16_t payload_len = hdr & 0x3FF;
  const size_t total_len = 3 + payload_len + 3;

  if (buffer.size() < total_len) {
    return std::nullopt;                                 // need more data
  }
  // CRC-24Q over bytes 0..(3+payload_len-1)
  const uint32_t got_crc =
    (static_cast<uint32_t>(buffer[3 + payload_len]) << 16) |
    (static_cast<uint32_t>(buffer[3 + payload_len + 1]) << 8 ) |
    static_cast<uint32_t>(buffer[3 + payload_len + 2]);
  const uint32_t exp_crc = crc24q(buffer.data(), 3 + payload_len);

  if (got_crc != exp_crc) {
    // Corrupt frame — drop preamble byte and let caller re-sync.
    *consumed_out = 1;
    return std::nullopt;
  }

  Rtcm3Frame f;
  f.bytes.assign(buffer.begin(), buffer.begin() + total_len);

  // Message type lives in the first 12 bits of payload.
  if (payload_len >= 2) {
    f.message_type =
      (static_cast<uint16_t>(buffer[3]) << 4) |
      (static_cast<uint16_t>(buffer[4]) >> 4);
  }

  *consumed_out = total_len;
  return f;
}

}  // namespace san_ntrip_client
