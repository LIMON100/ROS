// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — HMAC-SHA256 authenticator (PATCHED 2026-05-13).
// See hmac_authenticator.hpp for fix rationale.

#include "san_fire_authorization/hmac_authenticator.hpp"

#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>

namespace san_fire_authorization
{

namespace
{

void writeUint32BE(std::vector<uint8_t> & out, uint32_t v)
{
  out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
  out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>( v & 0xFF));
}

void writeUint64BE(std::vector<uint8_t> & out, uint64_t v)
{
  for (int i = 7; i >= 0; --i) {
    out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
  }
}

void writeInt32BE(std::vector<uint8_t> & out, int32_t v)
{
  writeUint32BE(out, static_cast<uint32_t>(v));
}

std::string toHex(const uint8_t * bytes, std::size_t n)
{
  static const char kHex[] = "0123456789abcdef";
  std::string s;
  s.resize(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    s[2 * i] = kHex[(bytes[i] >> 4) & 0x0F];
    s[2 * i + 1] = kHex[bytes[i] & 0x0F];
  }
  return s;
}

// PATCH 2026-05-13: lowercase-only + constant-time scan.
//   * Scans every character even on first-mismatch — prevents timing
//     leak about which position is malformed.
//   * Rejects upper-case to enforce one-true canonical signature form
//     (matches sign()'s output).
bool fromHexStrictLowerCT(
  std::string_view hex,
  std::array<uint8_t, kHmacSha256Bytes> & out)
{
  if (hex.size() != kHmacHexChars) {
    return false;
  }
  uint32_t bad = 0;            // accumulated "any byte malformed" flag
  for (std::size_t i = 0; i < kHmacSha256Bytes; ++i) {
    const unsigned char c0 = static_cast<unsigned char>(hex[2 * i]);
    const unsigned char c1 = static_cast<unsigned char>(hex[2 * i + 1]);

    // Branchless nibble decode (lowercase only). Each nibble must be
    // in '0'..'9' OR 'a'..'f' — anything else marks bad and the
    // computed value is irrelevant.
    auto nibble = [&bad](unsigned char c) -> uint8_t {
        const uint32_t is_digit = (c >= '0') & (c <= '9');
        const uint32_t is_lo = (c >= 'a') & (c <= 'f');
        // value = (is_digit ? c-'0' : 0) | (is_lo ? c-'a'+10 : 0)
        const uint8_t v_digit = static_cast<uint8_t>((c - '0') & 0x0F);
        const uint8_t v_lo = static_cast<uint8_t>((c - 'a' + 10) & 0x0F);
        const uint8_t v = (is_digit ? v_digit : 0) | (is_lo ? v_lo : 0);
        // OR the bad flag without short-circuit
        bad |= ~(is_digit | is_lo) & 1u;
        return v;
      };
    const uint8_t hi = nibble(c0);
    const uint8_t lo = nibble(c1);
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return bad == 0;
}

}  // namespace

// ─── ctors ──────────────────────────────────────────────────────────────

HmacAuthenticator::HmacAuthenticator(const std::string & secret_path)
: secret_(loadSecret(secret_path)) {}

HmacAuthenticator::HmacAuthenticator(
  const std::array<uint8_t, kHmacSha256Bytes> & secret)
: secret_(secret) {}

std::array<uint8_t, kHmacSha256Bytes>
HmacAuthenticator::loadSecret(const std::string & path)
{
  struct stat st;
  if (::stat(path.c_str(), &st) != 0) {
    throw std::runtime_error(
            "HmacAuthenticator: cannot stat secret file: " + path);
  }
  const auto perm = st.st_mode & 0777;
  if (perm != 0400 && perm != 0600) {
    throw std::runtime_error(
            "HmacAuthenticator: secret file permissions too permissive: " +
            path + " (expected 0400 or 0600)");
  }
  if (st.st_size != static_cast<off_t>(kHmacSha256Bytes)) {
    throw std::runtime_error(
            "HmacAuthenticator: secret file wrong size (expected " +
            std::to_string(kHmacSha256Bytes) + " bytes): " + path);
  }

  std::ifstream f(path, std::ios::binary);
  if (!f) {
    throw std::runtime_error(
            "HmacAuthenticator: cannot open secret file: " + path);
  }
  std::array<uint8_t, kHmacSha256Bytes> out{};
  f.read(reinterpret_cast<char *>(out.data()), kHmacSha256Bytes);
  if (!f || f.gcount() != static_cast<std::streamsize>(kHmacSha256Bytes)) {
    throw std::runtime_error(
            "HmacAuthenticator: short read on secret file: " + path);
  }
  return out;
}

// ─── canonicalization ───────────────────────────────────────────────────

std::vector<uint8_t>
HmacAuthenticator::canonicalize(const AuthMessage & msg)
{
  std::vector<uint8_t> out;
  out.reserve(64 + msg.operator_id.size());
  writeUint32BE(out, msg.request_id);
  writeUint32BE(out, msg.sequence);
  writeUint32BE(out, static_cast<uint32_t>(msg.operator_id.size()));
  out.insert(
    out.end(),
    msg.operator_id.begin(), msg.operator_id.end());
  writeUint64BE(out, msg.nonce);
  writeUint64BE(out, msg.request_timestamp_ms);
  out.push_back(msg.command_type);
  writeInt32BE(out, msg.target_lat_e7);
  writeInt32BE(out, msg.target_lon_e7);
  writeInt32BE(out, msg.target_alt_mm);
  return out;
}

// ─── sign / verify ──────────────────────────────────────────────────────

std::string HmacAuthenticator::sign(const AuthMessage & msg) const
{
  const auto buf = canonicalize(msg);
  std::array<uint8_t, kHmacSha256Bytes> digest{};
  unsigned int out_len = 0;
  HMAC(
    EVP_sha256(),
    secret_.data(), static_cast<int>(secret_.size()),
    buf.data(), buf.size(),
    digest.data(), &out_len);
  if (out_len != kHmacSha256Bytes) {
    throw std::runtime_error(
            "HmacAuthenticator::sign: unexpected HMAC output length");
  }
  return toHex(digest.data(), digest.size());
}

// ★ PATCH 2026-05-13: verification ORDER changed to HMAC first.
AuthResult HmacAuthenticator::verify(
  const AuthMessage & msg,
  std::string_view hmac_hex,
  uint64_t now_ms)
{
  // ─ 1) HMAC verification (constant-time compare) ────────────────
  //    Compute expected HMAC FIRST, then constant-time compare.
  //    No other branch may classify the request before this passes.
  std::array<uint8_t, kHmacSha256Bytes> expected{};
  std::array<uint8_t, kHmacSha256Bytes> received{};
  const auto buf = canonicalize(msg);
  unsigned int out_len = 0;
  HMAC(
    EVP_sha256(),
    secret_.data(), static_cast<int>(secret_.size()),
    buf.data(), buf.size(),
    expected.data(), &out_len);
  if (out_len != kHmacSha256Bytes) {
    return AuthResult::DeniedInternal;
  }
  // Decode received (strict lowercase, constant-time scan).
  if (!fromHexStrictLowerCT(hmac_hex, received)) {
    return AuthResult::DeniedHmacFail;
  }
  if (CRYPTO_memcmp(
      expected.data(), received.data(),
      kHmacSha256Bytes) != 0)
  {
    return AuthResult::DeniedHmacFail;
  }

  // ─ 2) Timestamp drift check ────────────────────────────────────
  //    Now that we know the message is authentic, check freshness.
  //    A tampered timestamp would have made HMAC fail above.
  const int64_t drift = static_cast<int64_t>(now_ms) -
    static_cast<int64_t>(msg.request_timestamp_ms);
  if (drift > kTimestampDriftMaxMs ||
    drift < -kTimestampDriftMaxMs)
  {
    return AuthResult::DeniedTimestampDrift;
  }

  // ─ 3) Nonce replay check (O(1) via hashset) ────────────────────
  {
    std::lock_guard<std::mutex> lock(nonce_mutex_);
    if (nonce_set_.count(msg.nonce) != 0) {
      return AuthResult::DeniedNonceReuse;
    }
    if (nonce_fifo_.size() >= kNonceWindowSize) {
      const auto evicted = nonce_fifo_.front();
      nonce_fifo_.pop_front();
      nonce_set_.erase(evicted);
    }
    nonce_fifo_.push_back(msg.nonce);
    nonce_set_.insert(msg.nonce);
  }
  return AuthResult::Granted;
}

std::size_t HmacAuthenticator::nonceWindowSize() const
{
  std::lock_guard<std::mutex> lock(nonce_mutex_);
  return nonce_fifo_.size();
}

}  // namespace san_fire_authorization
