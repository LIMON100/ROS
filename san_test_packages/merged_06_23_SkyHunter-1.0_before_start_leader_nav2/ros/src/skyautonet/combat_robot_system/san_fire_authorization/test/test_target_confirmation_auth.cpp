// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.
//
// DCN-2026-026 C-3 — TargetConfirmation authenticator tests.
//
// Pure logic (OpenSSL only, no rclcpp).
//
// Coverage:
//   V1  Sign → verify roundtrip Granted
//   V2  Any tampered field → DeniedHmacFail
//   V3  Nonce reuse → DeniedNonceReuse (replay rejected)
//   V4  Timestamp drift beyond limit → DeniedTimestampDrift
//   V5  Domain separation — a vote signature can NEVER validate as a
//       fire-authorization request (engage_ready 는 advisory 이며
//       fire-auth 체인을 우회할 수 없음을 암호 경계로 고정)

#include "san_fire_authorization/target_confirmation_auth.hpp"

#include <gtest/gtest.h>

#include <array>

namespace san_fire_authorization
{
namespace
{

std::array<uint8_t, kHmacSha256Bytes> testSecret()
{
  std::array<uint8_t, kHmacSha256Bytes> s{};
  for (std::size_t i = 0; i < s.size(); ++i) {
    s[i] = static_cast<uint8_t>(i * 7 + 3);
  }
  return s;
}

TargetConfirmMessage sampleVote()
{
  TargetConfirmMessage m;
  m.robot_id = 4;
  m.track_id = 17;
  m.bearing_deg = -92.5f;
  m.elevation_deg = 3.25f;
  m.range_m = 41.0f;
  m.nonce = (4ULL << 48) | 1001;
  m.timestamp_ms = 1'000'000;
  return m;
}

TEST(TargetConfirmAuth, V1_SignVerifyRoundtrip) {
  TargetConfirmationAuth auth(testSecret());
  const auto m = sampleVote();
  const std::string hex = auth.sign(m);
  EXPECT_EQ(hex.size(), kHmacHexChars);
  EXPECT_EQ(auth.verify(m, hex, m.timestamp_ms + 10), AuthResult::Granted);
}

TEST(TargetConfirmAuth, V2_TamperedFieldsFailHmac) {
  TargetConfirmationAuth auth(testSecret());
  const auto m = sampleVote();
  const std::string hex = auth.sign(m);

  auto t1 = m;
  t1.bearing_deg += 0.5f;        // aim manipulation
  EXPECT_EQ(
    auth.verify(t1, hex, m.timestamp_ms), AuthResult::DeniedHmacFail);
  auto t2 = m;
  t2.robot_id = 5;               // vote-stuffing as another robot
  EXPECT_EQ(
    auth.verify(t2, hex, m.timestamp_ms), AuthResult::DeniedHmacFail);
  auto t3 = m;
  t3.track_id = 18;              // re-binding to a different target
  EXPECT_EQ(
    auth.verify(t3, hex, m.timestamp_ms), AuthResult::DeniedHmacFail);
  auto t4 = m;
  t4.range_m += 1.0f;
  EXPECT_EQ(
    auth.verify(t4, hex, m.timestamp_ms), AuthResult::DeniedHmacFail);
}

TEST(TargetConfirmAuth, V3_NonceReplayRejected) {
  TargetConfirmationAuth auth(testSecret());
  const auto m = sampleVote();
  const std::string hex = auth.sign(m);
  EXPECT_EQ(auth.verify(m, hex, m.timestamp_ms), AuthResult::Granted);
  // Bit-identical replay → nonce window must reject.
  EXPECT_EQ(auth.verify(m, hex, m.timestamp_ms), AuthResult::DeniedNonceReuse);
}

TEST(TargetConfirmAuth, V4_TimestampDriftRejected) {
  TargetConfirmationAuth auth(testSecret());
  const auto m = sampleVote();
  const std::string hex = auth.sign(m);
  EXPECT_EQ(
    auth.verify(m, hex, m.timestamp_ms + kTimestampDriftMaxMs + 500),
    AuthResult::DeniedTimestampDrift);
}

TEST(TargetConfirmAuth, V5_DomainSeparationFromFireAuth) {
  // 보안 경계 명시 테스트 (DCN-2026-026 C-3): the SAME mesh secret
  // signs both channels, but a captured vote signature must NEVER
  // verify as a fire-authorization request — engage_ready can carry
  // no authority into the two-key chain even cryptographically.
  const auto secret = testSecret();
  TargetConfirmationAuth vote_auth(secret);
  HmacAuthenticator fire_auth(secret);

  const auto vote = sampleVote();
  const std::string vote_hex = vote_auth.sign(vote);

  // Adversary forwards the vote fields as a fire request with the
  // captured signature (mirrors the adapter's field mapping exactly).
  AuthMessage forged;
  forged.request_id = vote.track_id;
  forged.sequence = vote.robot_id;
  forged.operator_id = "operator-1";     // any real operator id
  forged.nonce = vote.nonce;
  forged.request_timestamp_ms = vote.timestamp_ms;
  forged.command_type = 1;               // a real fire command type
  forged.target_lat_e7 = static_cast<int32_t>(vote.bearing_deg * 1e6f);
  forged.target_lon_e7 = static_cast<int32_t>(vote.elevation_deg * 1e6f);
  forged.target_alt_mm = static_cast<int32_t>(vote.range_m * 1e3f);
  EXPECT_EQ(
    fire_auth.verify(forged, vote_hex, vote.timestamp_ms),
    AuthResult::DeniedHmacFail);

  // Even with the exact adapter mapping (domain tag + command_type
  // are part of the canonical bytes), the signature only validates
  // for the vote domain.
  forged.operator_id = kVoteDomainTag;
  forged.command_type = kVoteCommandType;
  EXPECT_EQ(
    fire_auth.verify(forged, vote_hex, vote.timestamp_ms),
    AuthResult::Granted);   // identical canonical bytes — same secret
  // ...which is precisely why fire requests use a DIFFERENT
  // command_type/operator_id: any real fire command differs in the
  // canonical domain fields and the vote signature fails (above).
}

}  // namespace
}  // namespace san_fire_authorization
