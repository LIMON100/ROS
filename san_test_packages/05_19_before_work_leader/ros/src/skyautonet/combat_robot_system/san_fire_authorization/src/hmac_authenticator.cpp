// SAN v1.5 PHASE 9 — HMAC-SHA256 authenticator implementation.
// See hmac_authenticator.hpp for the API and rationale.

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

namespace san_fire_authorization {

namespace {

// Big-endian write helpers — keep canonicalization platform-independent.
void writeUint32BE(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
  out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((v >> 8)  & 0xFF));
  out.push_back(static_cast<uint8_t>( v        & 0xFF));
}

void writeUint64BE(std::vector<uint8_t>& out, uint64_t v) {
  for (int i = 7; i >= 0; --i) {
    out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
  }
}

void writeInt32BE(std::vector<uint8_t>& out, int32_t v) {
  writeUint32BE(out, static_cast<uint32_t>(v));
}

// Hex (lowercase) encode 32 bytes → 64 chars.
std::string toHex(const uint8_t* bytes, std::size_t n) {
  static const char kHex[] = "0123456789abcdef";
  std::string s;
  s.resize(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    s[2 * i]     = kHex[(bytes[i] >> 4) & 0x0F];
    s[2 * i + 1] = kHex[ bytes[i]       & 0x0F];
  }
  return s;
}

// Decode 64 lowercase-hex chars → 32 bytes. Returns false on any
// non-hex character or wrong length.
bool fromHex(std::string_view hex,
             std::array<uint8_t, kHmacSha256Bytes>& out) {
  if (hex.size() != kHmacHexChars) {
    return false;
  }
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (std::size_t i = 0; i < kHmacSha256Bytes; ++i) {
    const int hi = nibble(hex[2 * i]);
    const int lo = nibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

}  // namespace

// ─── ctors ──────────────────────────────────────────────────────────────

HmacAuthenticator::HmacAuthenticator(const std::string& secret_path)
    : secret_(loadSecret(secret_path)) {
}

HmacAuthenticator::HmacAuthenticator(
    const std::array<uint8_t, kHmacSha256Bytes>& secret)
    : secret_(secret) {
}

std::array<uint8_t, kHmacSha256Bytes>
HmacAuthenticator::loadSecret(const std::string& path) {
  // Permission check first: SDD-SWARM v1.5 §10 mandates 0400.
  struct stat st;
  if (::stat(path.c_str(), &st) != 0) {
    throw std::runtime_error(
        "HmacAuthenticator: cannot stat secret file: " + path);
  }
  // Only owner-read allowed. Group/other any-bit set is a failure.
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
  f.read(reinterpret_cast<char*>(out.data()), kHmacSha256Bytes);
  if (!f || f.gcount() != static_cast<std::streamsize>(kHmacSha256Bytes)) {
    throw std::runtime_error(
        "HmacAuthenticator: short read on secret file: " + path);
  }
  return out;
}

// ─── canonicalization ───────────────────────────────────────────────────

std::vector<uint8_t>
HmacAuthenticator::canonicalize(const AuthMessage& msg) {
  // Concatenation order matches the FireAuthorizationRequest.msg
  // documented HMAC input. Fixed-width big-endian throughout, with the
  // variable-length operator_id length-prefixed (uint32).
  std::vector<uint8_t> out;
  out.reserve(64 + msg.operator_id.size());
  writeUint32BE(out, msg.request_id);
  writeUint32BE(out, msg.sequence);
  writeUint32BE(out, static_cast<uint32_t>(msg.operator_id.size()));
  out.insert(out.end(),
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

std::string HmacAuthenticator::sign(const AuthMessage& msg) const {
  const auto buf = canonicalize(msg);
  std::array<uint8_t, kHmacSha256Bytes> digest{};
  unsigned int out_len = 0;
  HMAC(EVP_sha256(),
       secret_.data(), static_cast<int>(secret_.size()),
       buf.data(), buf.size(),
       digest.data(), &out_len);
  if (out_len != kHmacSha256Bytes) {
    throw std::runtime_error(
        "HmacAuthenticator::sign: unexpected HMAC output length");
  }
  return toHex(digest.data(), digest.size());
}

AuthResult HmacAuthenticator::verify(const AuthMessage& msg,
                                     std::string_view hmac_hex,
                                     uint64_t now_ms) {
  // 1) Timestamp drift check. Use abs diff to catch both clock skew
  //    directions. Cast through int64_t to avoid uint64 underflow.
  const int64_t drift = static_cast<int64_t>(now_ms) -
                        static_cast<int64_t>(msg.request_timestamp_ms);
  if (drift >  kTimestampDriftMaxMs ||
      drift < -kTimestampDriftMaxMs) {
    return AuthResult::DeniedTimestampDrift;
  }

  // 2) HMAC signature check (constant-time compare).
  std::array<uint8_t, kHmacSha256Bytes> expected{};
  std::array<uint8_t, kHmacSha256Bytes> received{};
  // Compute expected.
  const auto buf = canonicalize(msg);
  unsigned int out_len = 0;
  HMAC(EVP_sha256(),
       secret_.data(), static_cast<int>(secret_.size()),
       buf.data(), buf.size(),
       expected.data(), &out_len);
  if (out_len != kHmacSha256Bytes) {
    return AuthResult::DeniedInternal;
  }
  // Decode received.
  if (!fromHex(hmac_hex, received)) {
    return AuthResult::DeniedHmacFail;
  }
  // CRYPTO_memcmp == constant-time; defends against timing side channels.
  if (CRYPTO_memcmp(expected.data(), received.data(),
                    kHmacSha256Bytes) != 0) {
    return AuthResult::DeniedHmacFail;
  }

  // 3) Nonce replay check. Lock, scan window, reject if seen; else
  //    insert (FIFO-evict oldest if at capacity).
  {
    std::lock_guard<std::mutex> lock(nonce_mutex_);
    for (const auto& seen : nonce_window_) {
      if (seen == msg.nonce) {
        return AuthResult::DeniedNonceReuse;
      }
    }
    if (nonce_window_.size() >= kNonceWindowSize) {
      nonce_window_.pop_front();
    }
    nonce_window_.push_back(msg.nonce);
  }

  return AuthResult::Granted;
}

std::size_t HmacAuthenticator::nonceWindowSize() const {
  std::lock_guard<std::mutex> lock(nonce_mutex_);
  return nonce_window_.size();
}

}  // namespace san_fire_authorization
