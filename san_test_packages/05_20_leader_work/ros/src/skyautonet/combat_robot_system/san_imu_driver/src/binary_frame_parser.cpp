// SAN v1.5 Phase 2-E Turn 5 — Binary IMU frame parser implementation.

#include "san_imu_driver/binary_frame_parser.hpp"

namespace san_imu_driver {

namespace {

uint8_t xorChecksum(const uint8_t* data, size_t len) {
  uint8_t c = 0;
  for (size_t i = 0; i < len; ++i) c ^= data[i];
  return c;
}

uint16_t sum16(const uint8_t* data, size_t len) {
  uint16_t s = 0;
  for (size_t i = 0; i < len; ++i) s = static_cast<uint16_t>(s + data[i]);
  return s;
}

}  // namespace

std::optional<BinaryFrame> parseBinaryFrame(
    const std::vector<uint8_t>& buffer,
    const BinaryFrameConfig& cfg,
    size_t* consumed_out) {
  *consumed_out = 0;

  // Need at least sync + len + 1 byte payload + checksum
  const size_t sync_len = cfg.two_byte_sync ? 2 : 1;
  const size_t len_len  = cfg.two_byte_length ? 2 : 1;
  const size_t csum_len = (cfg.checksum_kind == ChecksumKind::Sum16) ? 2 : 1;
  const size_t header_total = sync_len + len_len;
  const size_t min_total    = header_total + csum_len;

  if (buffer.size() < min_total) return std::nullopt;

  // Sync byte(s)
  if (buffer[0] != cfg.sync_byte_0) {
    *consumed_out = 1;
    return std::nullopt;
  }
  if (cfg.two_byte_sync && buffer[1] != cfg.sync_byte_1) {
    *consumed_out = 1;
    return std::nullopt;
  }

  // Length
  size_t payload_len = 0;
  if (cfg.two_byte_length) {
    payload_len = (static_cast<size_t>(buffer[sync_len    ]) << 8) |
                   static_cast<size_t>(buffer[sync_len + 1]);
  } else {
    payload_len = buffer[sync_len];
  }
  if (payload_len > cfg.max_payload) {
    *consumed_out = 1;
    return std::nullopt;
  }

  const size_t total = header_total + payload_len + csum_len;
  if (buffer.size() < total) return std::nullopt;  // need more bytes

  // Verify checksum
  // Coverage: from start of LEN field to end of PAYLOAD (excludes sync).
  // This is the most common convention (Xsens, VectorNav, etc.).
  const uint8_t* csum_region_start = buffer.data() + sync_len;
  const size_t   csum_region_len   = len_len + payload_len;

  bool ok = false;
  if (cfg.checksum_kind == ChecksumKind::XorByte) {
    const uint8_t got = buffer[total - 1];
    const uint8_t exp = xorChecksum(csum_region_start, csum_region_len);
    ok = (got == exp);
  } else { /* Sum16 */
    const uint16_t got =
        (static_cast<uint16_t>(buffer[total - 2]) << 8) |
         static_cast<uint16_t>(buffer[total - 1]);
    const uint16_t exp = sum16(csum_region_start, csum_region_len);
    ok = (got == exp);
  }
  if (!ok) {
    *consumed_out = 1;
    return std::nullopt;
  }

  BinaryFrame f;
  f.payload.assign(buffer.begin() + header_total,
                    buffer.begin() + header_total + payload_len);
  *consumed_out = total;
  return f;
}

}  // namespace san_imu_driver
