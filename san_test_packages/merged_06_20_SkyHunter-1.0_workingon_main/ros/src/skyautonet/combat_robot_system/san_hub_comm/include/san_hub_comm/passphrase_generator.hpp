// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 2-E - SRT AES-128 passphrase generator.
//
// SRT's `passphrase` URI parameter accepts 10-79 ASCII bytes; the
// actual AES-128 key is derived internally. We pick 32 random
// alphanumeric chars per stream (≈190 bits of entropy), regenerated
// on every START transition so a captured operator session can't
// be replayed.
//
// PATCH 2026-05-13 (HC5): use getrandom(2) on Linux for guaranteed
// CSPRNG output. std::random_device's quality is implementation-
// defined (some libstdc++ builds fall back to a PRNG seeded from
// time(2) when /dev/urandom is unavailable, which is unacceptable
// for a passphrase that gates AES-128 video).
//
// Fallback: when getrandom is missing (very old glibc, non-Linux),
// we read directly from /dev/urandom. Both paths are CSPRNG. The
// constructor raises if neither is available.

#pragma once

#include <cstddef>
#include <string>

namespace san_hub_comm
{

class PassphraseGenerator
{
public:
  static constexpr std::size_t kDefaultLen = 32;
  static constexpr std::size_t kMinLen = 10;       // SRT lower bound
  static constexpr std::size_t kMaxLen = 79;       // SRT upper bound

  explicit PassphraseGenerator(std::size_t len = kDefaultLen);

  // Generate a fresh passphrase. Returns a kLen-character string of
  // alphanumeric characters [A-Za-z0-9].
  std::string generate();

  std::size_t length() const {return len_;}

  // ★ PATCH 2026-05-13 (HC5): test seam — true iff backing CSPRNG
  // is verified to be cryptographically secure (getrandom / urandom).
  // Exposed for diagnostic logging; production should assert this is
  // true at startup.
  static bool isCsprngAvailable();

private:
  std::size_t len_;
};

}  // namespace san_hub_comm
