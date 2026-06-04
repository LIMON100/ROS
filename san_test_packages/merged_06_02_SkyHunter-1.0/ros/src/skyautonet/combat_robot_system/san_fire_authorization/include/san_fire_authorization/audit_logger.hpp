// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — Append-only audit logger (PATCHED 2026-05-13).
//
// DCN-2026-001 D-004: 모든 발사 인가 결정은 영구 감사 기록 의무.
//
// PATCH 2026-05-13 (Fire deep-dive review):
//   * Switched from std::ofstream → POSIX ::open/::write + fsync.
//     The previous code did std::ofstream::flush() which only pushes
//     to the OS page cache. A crash could lose 10+ entries — the DCN
//     guarantee of "최대 마지막 1 entry 손실" was not met. Now every
//     emit() does ::write + ::fsync (or O_SYNC) so a successful
//     return implies durable storage.
//   * verifyChain() is now called automatically on startup. If the
//     existing tail file fails verification, the constructor throws
//     fail-closed — refuses to extend a tampered/broken chain.
//   * Genesis-prev guard: if the file is empty AND a non-genesis
//     prev_hash is provided by the caller, the constructor accepts
//     it (continuation from a rotated-out predecessor).
//
// Chain rule (unchanged):
//   entry[N].prev_hash = entry[N-1].self_hash
//   entry[N].self_hash = sha256(canonical_json(entry[N] without self_hash))
//   entry[0].prev_hash = GENESIS_HASH (64 0-chars) for the very first file
//
// Thread-safety: 단일 mutex 가 모든 emit/verify 보호. 다중 producer 안전.

#ifndef SAN_FIRE_AUTHORIZATION__AUDIT_LOGGER_HPP_
#define SAN_FIRE_AUTHORIZATION__AUDIT_LOGGER_HPP_

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace san_fire_authorization
{

extern const char kGenesisHash[];

inline constexpr std::size_t kRotationBytesDefault = 10u * 1024u * 1024u;

struct AuditEntry
{
  uint64_t timestamp_ms = 0;
  uint32_t request_id = 0;
  std::string operator_id;

  bool granted = false;
  std::string reason;
  std::string reason_detail;
  bool limp_mode_fire = false;

  int32_t target_lat_e7 = 0;
  int32_t target_lon_e7 = 0;
  int32_t target_alt_mm = 0;

  uint32_t hub_term = 0;
  uint32_t leader_term = 0;
  uint32_t n_alive_robots = 0;
};

struct AuditEmitResult
{
  bool ok = false;
  std::string uuid;
  std::string self_hash;
};

struct AuditVerifyResult
{
  bool valid = false;
  std::size_t last_line_no = 0;
  std::string tail_self_hash;
  std::string error;
};

class AuditLogger
{
public:
  /// Open `path` for append. Forces mode 0640. Scans the tail to
  /// recover the last self_hash so the chain continues across restarts.
  ///
  /// PATCH 2026-05-13: now calls verifyChain() on existing content;
  /// throws std::runtime_error if the existing chain is broken.
  /// fail-closed by design — extending a tampered audit log silently
  /// would defeat the tamper-evidence property.
  AuditLogger(
    const std::string & path,
    std::size_t rotation_bytes = kRotationBytesDefault);

  ~AuditLogger();

  /// Append one entry with **durable** write (PATCH 2026-05-13).
  /// Returns ok=true ONLY after fsync confirms the bytes are on stable
  /// storage. On any failure, ok=false and dropped_count_ is incremented.
  AuditEmitResult emit(const AuditEntry & entry);

  static AuditVerifyResult verifyChain(
    const std::string & path,
    const std::string & expected_prev_hash);

  std::size_t droppedCount() const;
  std::string prevHash() const;

  static std::string generateUuidV4();
  static std::string sha256Hex(const std::string & data);
  static std::string toCanonicalJsonForHashing(
    const AuditEntry & entry,
    const std::string & uuid,
    const std::string & prev_hash);

private:
  static std::string toJsonLine(
    const AuditEntry & entry,
    const std::string & uuid,
    const std::string & prev_hash,
    const std::string & self_hash);

  static std::string recoverTailHash(const std::string & path);

  static void enforceMode0640(const std::string & path);

  /// Open the audit file with POSIX flags suitable for durable append.
  /// Returns the file descriptor or -1 on failure. PATCH 2026-05-13.
  static int openForAppend(const std::string & path);

  mutable std::mutex mutex_;
  std::string path_;
  int fd_ = -1;                            // POSIX fd (was std::ofstream)
  std::string prev_hash_;
  std::size_t dropped_count_ = 0;
  std::size_t rotation_bytes_;
};

}  // namespace san_fire_authorization

#endif  // SAN_FIRE_AUTHORIZATION__AUDIT_LOGGER_HPP_
