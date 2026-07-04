// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — HMAC-SHA256 authenticator (PATCHED 2026-05-13).
//
// DCN-2026-001 D-004: 모든 FireAuthorizationRequest 는 HMAC-SHA256
// 으로 서명되며, 게이트는 다음 3 단계 검증을 수행:
//   1) 서명 일치 (CRYPTO_memcmp, 상수-시간 비교)    ← ★ MUST be FIRST
//   2) timestamp drift ≤ 1000 ms (replay attack 방지)
//   3) nonce 미사용 (sliding window)
//
// PATCH 2026-05-13 (Fire deep-dive review):
//   * Verify order changed: HMAC FIRST, then timestamp, then nonce.
//     The previous order (timestamp → HMAC → nonce) leaked information
//     to an attacker who could distinguish DeniedTimestampDrift from
//     DeniedHmacFail without knowing the secret. Now nothing is
//     classified until the signature proves the message authentic.
//   * Nonce window backed by unordered_set + FIFO deque (was O(N)
//     linear scan on deque). Reduces timing variance and DoS surface.
//   * fromHex() rejects upper-case input (canonical lowercase only).
//     The signer emits lowercase, so accepting upper-case made the
//     same logical signature have two on-wire forms — a minor
//     non-canonicalisation that complicates audit-replay analysis.
//   * fromHex() is now constant-time (full-string scan, branchless
//     nibble decode) — eliminates timing leak on malformed hex.
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
#include <unordered_set>
#include <vector>

namespace san_fire_authorization
{

inline constexpr std::size_t kHmacSha256Bytes = 32;
inline constexpr std::size_t kHmacHexChars = 64;
inline constexpr std::size_t kNonceWindowSize = 64;
inline constexpr int64_t kTimestampDriftMaxMs = 1000;

enum class AuthResult : uint8_t
{
  Granted              = 0,
  DeniedHmacFail       = 1,
  DeniedNonceReuse     = 2,
  DeniedTimestampDrift = 3,
  DeniedInternal       = 99,
};

struct AuthMessage
{
  uint32_t request_id = 0;
  uint32_t sequence = 0;
  std::string operator_id;
  uint64_t nonce = 0;
  uint64_t request_timestamp_ms = 0;
  uint8_t command_type = 0;
  int32_t target_lat_e7 = 0;
  int32_t target_lon_e7 = 0;
  int32_t target_alt_mm = 0;
};

class HmacAuthenticator
{
public:
  explicit HmacAuthenticator(const std::string & secret_path);
  explicit HmacAuthenticator(
    const std::array<uint8_t, kHmacSha256Bytes> & secret);

  /// PATCH 2026-05-13: verification order is HMAC → timestamp → nonce.
  /// Previously timestamp came first, leaking unauthenticated rejection
  /// signal to attackers and risking audit-log corruption via tampered
  /// timestamps.
  AuthResult verify(
    const AuthMessage & msg,
    std::string_view hmac_hex,
    uint64_t now_ms);

  std::string sign(const AuthMessage & msg) const;

  std::size_t nonceWindowSize() const;

private:
  static std::vector<uint8_t> canonicalize(const AuthMessage & msg);
  static std::array<uint8_t, kHmacSha256Bytes>
  loadSecret(const std::string & path);

  std::array<uint8_t, kHmacSha256Bytes> secret_{};

  // PATCH 2026-05-13: O(1) average nonce check via hashset +
  // bounded FIFO eviction.
  mutable std::mutex nonce_mutex_;
  std::unordered_set<uint64_t> nonce_set_;
  std::deque<uint64_t> nonce_fifo_;           // for ordered eviction
};

}  // namespace san_fire_authorization

#endif  // SAN_FIRE_AUTHORIZATION__HMAC_AUTHENTICATOR_HPP_
