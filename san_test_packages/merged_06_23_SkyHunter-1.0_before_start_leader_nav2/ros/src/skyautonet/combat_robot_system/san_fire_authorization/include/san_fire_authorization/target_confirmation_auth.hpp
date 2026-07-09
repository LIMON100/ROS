// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// DCN-2026-026 C-3 — TargetConfirmation (교전 합의 투표) authenticator.
//
// Thin, domain-separated adapter over HmacAuthenticator so the vote
// channel reuses the EXACT same mesh-secret HMAC-SHA256 + timestamp-
// drift + nonce sliding-window discipline as fire authorization
// (DCN-2026-001 D-004) without duplicating any crypto code. Each
// adapter instance owns its OWN nonce window — vote traffic can never
// evict fire-authorization nonces.
//
// Domain separation: votes canonicalize with command_type =
// kVoteCommandType and operator_id = kVoteDomainTag, so a captured
// vote signature can never validate as a fire request (and vice
// versa) even under the shared secret.
//
// 보안 경계 (DCN-2026-026 C-3): 검증된 투표는 FireSolution.engage_ready
// 판정에만 쓰이는 advisory — Two-key fire-authorization 체인을
// 우회하거나 대체할 수 없다.

#ifndef SAN_FIRE_AUTHORIZATION__TARGET_CONFIRMATION_AUTH_HPP_
#define SAN_FIRE_AUTHORIZATION__TARGET_CONFIRMATION_AUTH_HPP_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "san_fire_authorization/hmac_authenticator.hpp"

namespace san_fire_authorization
{

constexpr uint8_t kVoteCommandType = 0x54;       // 'T' — domain tag
inline constexpr char kVoteDomainTag[] = "target_confirm.v1";

/// Signed fields of a combat_robot_msgs/TargetConfirmation.
struct TargetConfirmMessage
{
  uint32_t robot_id = 0;
  uint32_t track_id = 0;
  float bearing_deg = 0.0f;       // world frame
  float elevation_deg = 0.0f;
  float range_m = 0.0f;
  uint64_t nonce = 0;
  uint64_t timestamp_ms = 0;
};

class TargetConfirmationAuth
{
public:
  explicit TargetConfirmationAuth(const std::string & secret_path)
  : hmac_(secret_path) {}
  explicit TargetConfirmationAuth(
    const std::array<uint8_t, kHmacSha256Bytes> & secret)
  : hmac_(secret) {}

  std::string sign(const TargetConfirmMessage & msg) const
  {
    return hmac_.sign(toAuthMessage(msg));
  }

  /// HMAC → timestamp drift → nonce window (same order/policy as
  /// fire authorization; window is THIS instance's, not fire-auth's).
  AuthResult verify(
    const TargetConfirmMessage & msg,
    std::string_view hmac_hex,
    uint64_t now_ms)
  {
    return hmac_.verify(toAuthMessage(msg), hmac_hex, now_ms);
  }

private:
  /// Lossless field mapping into the canonical AuthMessage layout.
  /// Angles/range scale to fixed-point so the float bits are bound
  /// deterministically: bearing/elevation ×1e6 (±180° < int32 max),
  /// range ×1e3 (mm).
  static AuthMessage toAuthMessage(const TargetConfirmMessage & m)
  {
    AuthMessage a;
    a.request_id = m.track_id;
    a.sequence = m.robot_id;
    a.operator_id = kVoteDomainTag;
    a.nonce = m.nonce;
    a.request_timestamp_ms = m.timestamp_ms;
    a.command_type = kVoteCommandType;
    a.target_lat_e7 = static_cast<int32_t>(m.bearing_deg * 1e6f);
    a.target_lon_e7 = static_cast<int32_t>(m.elevation_deg * 1e6f);
    a.target_alt_mm = static_cast<int32_t>(m.range_m * 1e3f);
    return a;
  }

  HmacAuthenticator hmac_;
};

}  // namespace san_fire_authorization

#endif  // SAN_FIRE_AUTHORIZATION__TARGET_CONFIRMATION_AUTH_HPP_
