// SAN v1.5 Phase 2-E Turn 4 — RTCM3 frame parser (pure logic).
//
// RTCM3 frame format (RTCM 10403.2):
//   byte 0    : preamble 0xD3
//   byte 1-2  : 6 reserved bits + 10-bit payload length (big-endian)
//   byte 3..  : payload (length bytes)
//   last 3    : CRC24Q
//
// Total frame = 3 (header) + length + 3 (CRC) bytes.
//
// This parser does framing only — it does NOT decode message contents.
// Downstream consumer (the receiver via serial inject) does that.

#ifndef SAN_NTRIP_CLIENT__RTCM3_FRAME_PARSER_HPP_
#define SAN_NTRIP_CLIENT__RTCM3_FRAME_PARSER_HPP_

#include <cstdint>
#include <optional>
#include <vector>

namespace san_ntrip_client {

constexpr uint8_t RTCM3_PREAMBLE = 0xD3;

struct Rtcm3Frame {
  std::vector<uint8_t> bytes;   // full frame including header + CRC
  uint16_t             message_type = 0;
};

/// Parse the next RTCM3 frame from a buffer.
///
/// Returns:
///   * std::nullopt   — buffer doesn't yet contain a full frame
///                       (caller should call again after more bytes)
///   * Rtcm3Frame     — a valid frame; consume buffer[0..consumed-1]
///   * Garbage skip   — sets `consumed` to the number of leading
///                       garbage bytes to drop before retrying
///
/// Caller pattern:
///   while (true) {
///     auto r = parseRtcm3(buffer, &consumed);
///     if (!r) { if (consumed) buffer.erase(0, consumed); break; }
///     out.push_back(*r);
///     buffer.erase(0, consumed);
///   }
std::optional<Rtcm3Frame> parseRtcm3(
    const std::vector<uint8_t>& buffer,
    size_t* consumed_out);

}  // namespace san_ntrip_client

#endif  // SAN_NTRIP_CLIENT__RTCM3_FRAME_PARSER_HPP_
