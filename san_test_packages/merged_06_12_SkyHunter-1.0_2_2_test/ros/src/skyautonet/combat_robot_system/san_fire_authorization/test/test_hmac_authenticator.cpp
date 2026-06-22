// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — HmacAuthenticator unit tests.
//
// Coverage:
//   * RFC 4231 HMAC-SHA256 known-answer vector (Test Case 1)
//   * sign/verify round-trip (granted)
//   * tampered signature (denied: HmacFail)
//   * nonce reuse within sliding window (denied: NonceReuse)
//   * timestamp drift > 1000 ms (denied: TimestampDrift)
//   * sliding-window eviction at capacity (oldest nonce reusable)
//   * loadSecret rejects wrong size / wrong permissions

#include "san_fire_authorization/hmac_authenticator.hpp"

#include <gtest/gtest.h>

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace san_fire_authorization
{
namespace
{

// ─── helpers ────────────────────────────────────────────────────────────

std::array<uint8_t, kHmacSha256Bytes> makeSecret(uint8_t fill = 0x42)
{
  std::array<uint8_t, kHmacSha256Bytes> s{};
  s.fill(fill);
  return s;
}

AuthMessage makeMsg(
  uint64_t nonce = 12345,
  uint64_t ts_ms = 1'700'000'000'000ULL)
{
  AuthMessage m;
  m.request_id = 7;
  m.sequence = 1;
  m.operator_id = "op_taegeun";
  m.nonce = nonce;
  m.request_timestamp_ms = ts_ms;
  m.command_type = 2;              // TWO_KEY_KEY2_CONFIRM
  m.target_lat_e7 = 374200000;         // 37.42°N
  m.target_lon_e7 = 1270000000;        // 127.0°E
  m.target_alt_mm = 250;
  return m;
}

// ─── tests ──────────────────────────────────────────────────────────────

TEST(HmacAuthenticatorTest, Rfc4231TestCase1KnownAnswer) {
  // RFC 4231 §4.2 Test Case 1:
  //   Key  : 0x0b * 20 bytes (padded to 32 with 0x00 for our 32-byte key path)
  //   Data : "Hi There" (8 bytes)
  //   HMAC-SHA256: b0344c61d8db38535ca8afceaf0bf12b
  //                881dc200c9833da726e9376c2e32cff7
  //
  // We test the underlying OpenSSL HMAC primitive directly (no
  // AuthMessage) to confirm the library is doing what we expect. The
  // higher-level AuthMessage path is exercised by round-trip tests.
  std::array<uint8_t, 20> key_20{};
  key_20.fill(0x0b);
  const std::string data = "Hi There";
  const std::string expected_hex =
    "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7";

  std::array<uint8_t, kHmacSha256Bytes> digest{};
  unsigned int out_len = 0;
  HMAC(
    EVP_sha256(),
    key_20.data(), static_cast<int>(key_20.size()),
    reinterpret_cast<const uint8_t *>(data.data()), data.size(),
    digest.data(), &out_len);

  ASSERT_EQ(out_len, kHmacSha256Bytes);
  static const char kHex[] = "0123456789abcdef";
  std::string got;
  got.resize(kHmacSha256Bytes * 2);
  for (std::size_t i = 0; i < kHmacSha256Bytes; ++i) {
    got[2 * i] = kHex[(digest[i] >> 4) & 0x0F];
    got[2 * i + 1] = kHex[digest[i] & 0x0F];
  }
  EXPECT_EQ(got, expected_hex)
    << "RFC 4231 Test Case 1 HMAC-SHA256 mismatch — OpenSSL primitive broken";
}

TEST(HmacAuthenticatorTest, SignVerifyRoundTripGranted) {
  HmacAuthenticator auth(makeSecret());
  const auto msg = makeMsg();
  const auto hex = auth.sign(msg);
  EXPECT_EQ(hex.size(), kHmacHexChars);

  const auto result = auth.verify(msg, hex, msg.request_timestamp_ms);
  EXPECT_EQ(result, AuthResult::Granted);
  EXPECT_EQ(auth.nonceWindowSize(), 1u);
}

TEST(HmacAuthenticatorTest, TamperedSignatureDenied) {
  HmacAuthenticator auth(makeSecret());
  const auto msg = makeMsg();
  auto hex = auth.sign(msg);
  // Flip one character.
  hex[5] = (hex[5] == 'a') ? 'b' : 'a';

  const auto result = auth.verify(msg, hex, msg.request_timestamp_ms);
  EXPECT_EQ(result, AuthResult::DeniedHmacFail);
  EXPECT_EQ(auth.nonceWindowSize(), 0u)
    << "Failed verify must NOT consume the nonce slot";
}

TEST(HmacAuthenticatorTest, WrongLengthHmacHexDenied) {
  HmacAuthenticator auth(makeSecret());
  const auto msg = makeMsg();
  // Truncate the hex string.
  std::string bad_hex = auth.sign(msg).substr(0, 60);

  const auto result = auth.verify(msg, bad_hex, msg.request_timestamp_ms);
  EXPECT_EQ(result, AuthResult::DeniedHmacFail);
}

TEST(HmacAuthenticatorTest, NonceReuseDenied) {
  HmacAuthenticator auth(makeSecret());
  const auto msg = makeMsg(/*nonce=*/ 0xDEAD'BEEFULL);
  const auto hex = auth.sign(msg);
  ASSERT_EQ(
    auth.verify(msg, hex, msg.request_timestamp_ms),
    AuthResult::Granted);

  // Resend identical request — same nonce.
  const auto result = auth.verify(msg, hex, msg.request_timestamp_ms);
  EXPECT_EQ(result, AuthResult::DeniedNonceReuse);
  EXPECT_EQ(auth.nonceWindowSize(), 1u);
}

TEST(HmacAuthenticatorTest, TimestampDriftDenied) {
  HmacAuthenticator auth(makeSecret());
  const auto msg = makeMsg();
  const auto hex = auth.sign(msg);

  // Receive 1500 ms after the operator stamped it.
  const uint64_t now_ms = msg.request_timestamp_ms + 1500;
  const auto result = auth.verify(msg, hex, now_ms);
  EXPECT_EQ(result, AuthResult::DeniedTimestampDrift);
}

TEST(HmacAuthenticatorTest, NegativeDriftAlsoDenied) {
  HmacAuthenticator auth(makeSecret());
  const auto msg = makeMsg();
  const auto hex = auth.sign(msg);

  // Operator clock 1500 ms ahead of gate.
  const uint64_t now_ms = msg.request_timestamp_ms - 1500;
  const auto result = auth.verify(msg, hex, now_ms);
  EXPECT_EQ(result, AuthResult::DeniedTimestampDrift);
}

TEST(HmacAuthenticatorTest, NonceWindowEvictsOldestAtCapacity) {
  HmacAuthenticator auth(makeSecret());
  const uint64_t base_nonce = 100'000ULL;

  // Fill the window with kNonceWindowSize unique nonces.
  for (std::size_t i = 0; i < kNonceWindowSize; ++i) {
    auto msg = makeMsg(/*nonce=*/ base_nonce + i);
    const auto hex = auth.sign(msg);
    ASSERT_EQ(
      auth.verify(msg, hex, msg.request_timestamp_ms),
      AuthResult::Granted);
  }
  EXPECT_EQ(auth.nonceWindowSize(), kNonceWindowSize);

  // Push one more — evicts the oldest (base_nonce + 0).
  {
    auto msg = makeMsg(/*nonce=*/ base_nonce + kNonceWindowSize);
    const auto hex = auth.sign(msg);
    ASSERT_EQ(
      auth.verify(msg, hex, msg.request_timestamp_ms),
      AuthResult::Granted);
  }
  EXPECT_EQ(auth.nonceWindowSize(), kNonceWindowSize);

  // The evicted nonce should now be re-usable (no longer in window).
  {
    auto msg = makeMsg(/*nonce=*/ base_nonce + 0);
    const auto hex = auth.sign(msg);
    EXPECT_EQ(
      auth.verify(msg, hex, msg.request_timestamp_ms),
      AuthResult::Granted);
  }
}

TEST(HmacAuthenticatorTest, LoadSecretRejectsWrongSize) {
  // Create a temp file with the wrong size.
  char tmpl[] = "/tmp/san_fire_secret_bad_XXXXXX";
  int fd = ::mkstemp(tmpl);
  ASSERT_GE(fd, 0);
  const std::string path = tmpl;
  ::close(fd);

  std::ofstream f(path, std::ios::binary);
  for (int i = 0; i < 16; ++i) {
    f.put('\0');                                // only 16 bytes
  }
  f.close();
  ::chmod(path.c_str(), 0400);

  EXPECT_THROW(
      {
        HmacAuthenticator auth(path);
      }, std::runtime_error);

  std::remove(path.c_str());
}

TEST(HmacAuthenticatorTest, LoadSecretRejectsLoosePermissions) {
  char tmpl[] = "/tmp/san_fire_secret_loose_XXXXXX";
  int fd = ::mkstemp(tmpl);
  ASSERT_GE(fd, 0);
  const std::string path = tmpl;
  ::close(fd);

  std::ofstream f(path, std::ios::binary);
  for (std::size_t i = 0; i < kHmacSha256Bytes; ++i) {
    f.put(0x42);
  }
  f.close();
  ::chmod(path.c_str(), 0644);   // world-readable — must reject

  EXPECT_THROW(
      {
        HmacAuthenticator auth(path);
      }, std::runtime_error);

  std::remove(path.c_str());
}

TEST(HmacAuthenticatorTest, LoadSecretAcceptsTightPermissions) {
  char tmpl[] = "/tmp/san_fire_secret_good_XXXXXX";
  int fd = ::mkstemp(tmpl);
  ASSERT_GE(fd, 0);
  const std::string path = tmpl;
  ::close(fd);

  std::ofstream f(path, std::ios::binary);
  for (std::size_t i = 0; i < kHmacSha256Bytes; ++i) {
    f.put(0x42);
  }
  f.close();
  ::chmod(path.c_str(), 0400);

  EXPECT_NO_THROW(
      {
        HmacAuthenticator auth(path);
      });

  std::remove(path.c_str());
}

// ═══════════════════════════════════════════════════════════════════════
// PATCH 2026-05-13 — new tests covering deep-dive fixes
// ═══════════════════════════════════════════════════════════════════════

// ─── PH1 (★ C1 fix): HMAC verified BEFORE timestamp ────────────────────
// Goal: a request with a wildly stale timestamp AND a bad signature
// must return DeniedHmacFail (not DeniedTimestampDrift). The previous
// implementation returned DeniedTimestampDrift, which leaked
// information to unauthenticated attackers and risked corrupting
// audit logs with attacker-controlled timestamps.
TEST(PatchHmac, PH1_HmacCheckedBeforeTimestamp) {
  std::array<uint8_t, 32> secret{};
  for (size_t i = 0; i < 32; ++i) {
    secret[i] = static_cast<uint8_t>(i);
  }
  HmacAuthenticator auth(secret);

  AuthMessage m;
  m.request_id = 1;
  m.nonce = 1;
  m.request_timestamp_ms = 1000;     // 1.0s in request

  // Bad signature (64 'a's — valid hex, wrong value).
  const std::string bad_sig(64, 'a');
  // Gate clock 1000s later → drift > 1s → would be DeniedTimestampDrift
  // under the old order.
  const uint64_t now_ms = 1000 + 1'000'000;
  EXPECT_EQ(
    auth.verify(m, bad_sig, now_ms),
    AuthResult::DeniedHmacFail);
}

// ─── PH2 (★ C2 fix): nonce hashset O(1) reuse detection ────────────────
TEST(PatchHmac, PH2_NonceHashsetDetectsReuse) {
  std::array<uint8_t, 32> secret{};
  HmacAuthenticator auth(secret);

  AuthMessage m;
  m.request_id = 1;
  m.nonce = 42;
  m.request_timestamp_ms = 1000;
  const auto sig = auth.sign(m);

  EXPECT_EQ(auth.verify(m, sig, 1100), AuthResult::Granted);
  // Replay — same nonce.
  EXPECT_EQ(auth.verify(m, sig, 1200), AuthResult::DeniedNonceReuse);
  // Different nonce — passes.
  m.nonce = 43;
  const auto sig2 = auth.sign(m);
  EXPECT_EQ(auth.verify(m, sig2, 1300), AuthResult::Granted);
}

// ─── PH3 (★ M7 fix): lowercase-only hex (uppercase rejected) ───────────
TEST(PatchHmac, PH3_LowercaseHexOnly) {
  std::array<uint8_t, 32> secret{};
  HmacAuthenticator auth(secret);

  AuthMessage m;
  m.request_id = 1;
  m.nonce = 1;
  m.request_timestamp_ms = 1000;
  const auto sig_lower = auth.sign(m);
  EXPECT_EQ(auth.verify(m, sig_lower, 1100), AuthResult::Granted);

  // Replay defeat aside, change ONE char to uppercase.
  m.nonce = 2;
  const auto sig_lower_2 = auth.sign(m);
  std::string sig_mixed = sig_lower_2;
  // Find first alpha char and uppercase it.
  for (auto & c : sig_mixed) {
    if (c >= 'a' && c <= 'f') {c = static_cast<char>(c - 32); break;}
  }
  // Mixed-case signature must be rejected even though hex value matches.
  EXPECT_EQ(
    auth.verify(m, sig_mixed, 1200),
    AuthResult::DeniedHmacFail);
}

}  // namespace
}  // namespace san_fire_authorization
