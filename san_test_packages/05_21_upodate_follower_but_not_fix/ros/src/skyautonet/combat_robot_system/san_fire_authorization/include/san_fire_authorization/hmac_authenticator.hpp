// SAN v1.5 PHASE 9 — HMAC-SHA256 authenticator.
//
// DCN-2026-001 D-004: 모든 FireAuthorizationRequest 는 HMAC-SHA256
// 으로 서명되며, 게이트는 다음 3 단계 검증을 수행:
//   1) 서명 일치 (CRYPTO_memcmp, 상수-시간 비교)
//   2) nonce 미사용 (64-slot sliding window)
//   3) timestamp drift ≤ 1000 ms (replay attack 방지)
//
// Mesh shared secret 은 /etc/san/mesh_secret.bin 에서 로드 (32 bytes,
// mode 0400). 본 모듈은 ROS 2 비의존 — gtest 로 단위 검증 가능.
//
// 권원:
//   * SAN-IDS-CMD-001 v1.5 §3.5 FireAuthorizationRequest
//   * SAN-SDD-SWARM-001 v1.5 §5.7.2.1 (Limp Mode 발사 정책 Option A)
//   * RFC 4231 (HMAC-SHA256 test vectors)

#ifndef SAN_FIRE_AUTHORIZATION__HMAC_AUTHENTICATOR_HPP_
#define SAN_FIRE_AUTHORIZATION__HMAC_AUTHENTICATOR_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace san_fire_authorization {

/// HMAC-SHA256 output is 32 bytes; hex-encoded as 64 chars.
inline constexpr std::size_t kHmacSha256Bytes = 32;
inline constexpr std::size_t kHmacHexChars    = 64;

/// Replay-protection: sliding window size for nonce-seen ring buffer.
inline constexpr std::size_t kNonceWindowSize = 64;

/// Maximum allowed clock drift between request timestamp and gate
/// receipt, in milliseconds. SDD-SWARM v1.5 §10 standard.
inline constexpr int64_t kTimestampDriftMaxMs = 1000;

/// Result of an authentication check, mirroring the FireAuthorizationResponse
/// REASON_* codes so the node can forward without translation.
enum class AuthResult : uint8_t {
  Granted             = 0,
  DeniedHmacFail      = 1,
  DeniedNonceReuse    = 2,
  DeniedTimestampDrift = 3,
  DeniedInternal      = 99,
};

/// Canonical fields that get HMAC'd. Caller fills these from the
/// inbound FireAuthorizationRequest.
struct AuthMessage {
  uint32_t request_id        = 0;
  uint32_t sequence          = 0;
  std::string operator_id;
  uint64_t nonce             = 0;
  uint64_t request_timestamp_ms = 0;
  uint8_t  command_type      = 0;
  int32_t  target_lat_e7     = 0;
  int32_t  target_lon_e7     = 0;
  int32_t  target_alt_mm     = 0;
};

/// Thread-safe HMAC-SHA256 authenticator with built-in replay protection.
///
/// Construction loads the mesh shared secret. If the secret file is
/// missing or has the wrong size/permissions, construction throws
/// std::runtime_error — fail-closed by design.
class HmacAuthenticator {
public:
  /// Construct with the mesh secret loaded from a file (binary, 32 bytes).
  /// Throws std::runtime_error on any I/O / size / permission failure.
  explicit HmacAuthenticator(const std::string& secret_path);

  /// Construct directly from a 32-byte secret. Intended for unit tests
  /// (RFC 4231 known-answer tests) — production callers use the
  /// secret_path overload.
  explicit HmacAuthenticator(const std::array<uint8_t, kHmacSha256Bytes>& secret);

  /// Verify an inbound request. Returns Granted on success; one of
  /// DeniedHmacFail / DeniedNonceReuse / DeniedTimestampDrift otherwise.
  ///
  /// On Granted, the nonce is recorded in the sliding window so a
  /// retransmission of the same request will fail with DeniedNonceReuse.
  ///
  /// `now_ms` is the gate's clock at receipt; pass time-since-epoch ms.
  AuthResult verify(const AuthMessage& msg,
                    std::string_view hmac_hex,
                    uint64_t now_ms);

  /// Compute the HMAC-SHA256 hex string for the given message.
  /// Pure function — does not mutate the nonce window. Used by the
  /// operator-side signing path AND by verify() internally.
  std::string sign(const AuthMessage& msg) const;

  /// For tests + diagnostics: how many nonces are currently remembered.
  std::size_t nonceWindowSize() const;

private:
  /// Canonical byte serialization of AuthMessage. Used as the HMAC input.
  /// Big-endian fixed-width encoding so all platforms agree.
  static std::vector<uint8_t> canonicalize(const AuthMessage& msg);

  /// Load the secret from `path`. Validates size == 32 + permission 0400.
  static std::array<uint8_t, kHmacSha256Bytes>
  loadSecret(const std::string& path);

  std::array<uint8_t, kHmacSha256Bytes> secret_{};
  mutable std::mutex nonce_mutex_;
  std::deque<uint64_t> nonce_window_;   // FIFO, max kNonceWindowSize
};

}  // namespace san_fire_authorization

#endif  // SAN_FIRE_AUTHORIZATION__HMAC_AUTHENTICATOR_HPP_
