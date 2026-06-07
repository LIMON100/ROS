// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_hub_comm/passphrase_generator.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

#if defined(__linux__)
#  include <sys/random.h>
#  include <sys/syscall.h>
#endif

namespace san_hub_comm
{

namespace
{

// ★ PATCH 2026-05-13 (HC5): rejection-sampling-free 62-char map.
// 256 = 4 * 64; bytes 0..247 map cleanly to [0..61] via /4. The
// remaining 8 byte values (248..255) are dropped and resampled.
constexpr const char kCharset[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789";
constexpr std::size_t kCharsetSize = sizeof(kCharset) - 1;
constexpr std::uint8_t kRejectThreshold =
  static_cast<std::uint8_t>(256 - (256 % kCharsetSize));    // = 248

// Fill `out` with `n` CSPRNG bytes. Tries getrandom(2) first, then
// /dev/urandom. Returns true on success; false leaves out untouched.
bool fillCsprngBytes(std::uint8_t * out, std::size_t n)
{
#if defined(__linux__) && defined(SYS_getrandom)
  std::size_t got = 0;
  while (got < n) {
    ssize_t r = ::syscall(SYS_getrandom, out + got, n - got, 0);
    if (r < 0) {
      if (errno == EINTR) {continue;}
      break;           // fall through to /dev/urandom
    }
    got += static_cast<std::size_t>(r);
  }
  if (got == n) {return true;}
#endif
  // Fallback: /dev/urandom.
  int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {return false;}
  std::size_t got2 = 0;
  while (got2 < n) {
    ssize_t r = ::read(fd, out + got2, n - got2);
    if (r < 0) {
      if (errno == EINTR) {continue;}
      ::close(fd);
      return false;
    }
    if (r == 0) {
      ::close(fd);
      return false;
    }
    got2 += static_cast<std::size_t>(r);
  }
  ::close(fd);
  return true;
}

}  // namespace

PassphraseGenerator::PassphraseGenerator(std::size_t len)
: len_(len)
{
  if (len_ < kMinLen || len_ > kMaxLen) {
    throw std::invalid_argument(
            "passphrase length must satisfy SRT 10..79 range");
  }
  // ★ PATCH 2026-05-13 (HC5): fail-fast if no CSPRNG.
  if (!isCsprngAvailable()) {
    throw std::runtime_error(
            "PassphraseGenerator: no CSPRNG available (getrandom and "
            "/dev/urandom both unusable); refusing to produce a weak "
            "passphrase for AES-128 video encryption");
  }
}

bool PassphraseGenerator::isCsprngAvailable()
{
  std::uint8_t probe[4];
  return fillCsprngBytes(probe, sizeof(probe));
}

std::string PassphraseGenerator::generate()
{
  std::string out;
  out.reserve(len_);

  // Buffered drain — refill in 64-byte chunks. With rejection rate
  // ≤ 8/256 ≈ 3.1 %, we expect ~33 bytes consumed per 32-char
  // passphrase; a 64-byte buffer covers it comfortably.
  constexpr std::size_t kBufSize = 64;
  std::uint8_t buf[kBufSize];
  std::size_t buf_pos = kBufSize;     // force initial fill

  while (out.size() < len_) {
    if (buf_pos >= kBufSize) {
      if (!fillCsprngBytes(buf, kBufSize)) {
        // Should never happen after constructor probe, but
        // a runtime failure (mid-life FD exhaustion) must
        // not silently weaken the passphrase.
        throw std::runtime_error(
                "PassphraseGenerator: CSPRNG fill failed mid-generate");
      }
      buf_pos = 0;
    }
    const std::uint8_t b = buf[buf_pos++];
    // ★ PATCH 2026-05-13 (HC5): rejection sampling — uniform.
    if (b >= kRejectThreshold) {continue;}
    out.push_back(kCharset[b % kCharsetSize]);
  }
  return out;
}

}  // namespace san_hub_comm
