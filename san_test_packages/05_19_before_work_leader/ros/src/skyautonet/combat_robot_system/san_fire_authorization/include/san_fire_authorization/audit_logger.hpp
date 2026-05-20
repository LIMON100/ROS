// SAN v1.5 PHASE 9 — Append-only audit logger with hash chain.
//
// DCN-2026-001 D-004: 모든 발사 인가 결정은 영구 감사 기록 의무.
// 본 로거는 다음 보장을 제공:
//   1) Append-only — 기존 entry 불변
//   2) Tamper-evident — sha256 hash chain (prev_hash → self_hash)
//   3) UUIDv4 — 각 entry 외부 시스템 join key
//   4) Atomic line write — fsync per entry; crash 시 마지막 entry 손실만
//   5) File mode 0640 — owner rw + group r, world none
//   6) Rotation — 10 MB per file, 외부 logrotate.d 가 처리 (count = 7000)
//
// Chain rule:
//   entry[N].prev_hash = entry[N-1].self_hash
//   entry[N].self_hash = sha256(canonical_json(entry[N] without self_hash))
//   entry[0].prev_hash = GENESIS_HASH (64 0-chars)
//
// 권원:
//   * SAN-SDD-SWARM-001 v1.5 §5.7.2.1 (Limp Mode 발사 정책 — audit 의무화)
//   * SAN-OPS-SOP-001   v1.5 §7.11    (limp_mode_fire flag 의무)
//   * SAN-IDS-CMD-001   v1.5 §3.5     (FireAuthorizationResponse.audit_log_uuid)
//   * core/audit_log.py (v1.4 Python prototype 동일 패턴)
//
// Thread-safety: 단일 mutex 가 모든 emit/verify 보호. 다중 producer 안전.

#ifndef SAN_FIRE_AUTHORIZATION__AUDIT_LOGGER_HPP_
#define SAN_FIRE_AUTHORIZATION__AUDIT_LOGGER_HPP_

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace san_fire_authorization {

/// Initial prev_hash for the first entry of the very first file.
/// 64 '0' chars = 32 zero bytes hex. Matches core/audit_log.py.
extern const char kGenesisHash[];

/// Default rotation threshold per file (10 MB — matches Python).
inline constexpr std::size_t kRotationBytesDefault = 10u * 1024u * 1024u;

/// Reason codes mirror FireAuthorizationResponse REASON_* values.
/// Stored as string in JSON so analysts don't need a code table.
struct AuditEntry {
  // ─── identity ─────────────────────────────────────────────────────
  uint64_t    timestamp_ms     = 0;   // unix epoch ms
  uint32_t    request_id       = 0;
  std::string operator_id;

  // ─── decision ─────────────────────────────────────────────────────
  bool        granted          = false;
  std::string reason;                  // "GRANTED" | "HMAC_FAIL" | ...
  std::string reason_detail;           // optional short human note
  bool        limp_mode_fire   = false; // DCN-2026-001 D-004 flag

  // ─── target geometry ──────────────────────────────────────────────
  int32_t     target_lat_e7    = 0;
  int32_t     target_lon_e7    = 0;
  int32_t     target_alt_mm    = 0;

  // ─── swarm state context ──────────────────────────────────────────
  uint32_t    hub_term         = 0;
  uint32_t    leader_term      = 0;
  uint32_t    n_alive_robots   = 0;

  // ─── auto-filled by emit() ────────────────────────────────────────
  // uuid, prev_hash, self_hash are populated by AuditLogger::emit and
  // returned to the caller via the AuditEmitResult.
};

/// Return value of emit(). `uuid` non-empty iff the write hit the disk.
struct AuditEmitResult {
  bool        ok           = false;
  std::string uuid;            // empty on failure
  std::string self_hash;       // empty on failure
};

/// Chain verification result for one file.
struct AuditVerifyResult {
  bool        valid          = false;
  std::size_t last_line_no   = 0;   // 0-based; last successfully verified line
  std::string tail_self_hash;       // for threading into the next file
  std::string error;                // populated when valid == false
};

class AuditLogger {
public:
  /// Open `path` for append. Forces mode 0640. Scans the tail to
  /// recover the last self_hash so the chain continues across restarts.
  ///
  /// Throws std::runtime_error on:
  ///   - cannot open / cannot chmod
  ///   - existing file with broken chain (refuses to continue)
  AuditLogger(const std::string& path,
              std::size_t rotation_bytes = kRotationBytesDefault);

  ~AuditLogger();

  /// Append one entry. Returns the assigned uuid + self_hash on success.
  /// On failure (disk full, FS readonly, etc.) returns ok=false and
  /// increments dropped_count_. Caller MUST NOT crash on failure.
  AuditEmitResult emit(const AuditEntry& entry);

  /// Verify the hash chain of `path` end-to-end. Pass the expected
  /// prev_hash for line 0 (kGenesisHash for the very first file, or the
  /// previous file's tail_self_hash when chaining across rotation).
  static AuditVerifyResult verifyChain(const std::string& path,
                                        const std::string& expected_prev_hash);

  // ─── diagnostics ──────────────────────────────────────────────────
  std::size_t droppedCount() const;
  std::string prevHash() const;

  // ─── static helpers (exposed for tests) ───────────────────────────
  /// Generate a RFC 4122 v4 UUID using OpenSSL RAND_bytes.
  static std::string generateUuidV4();

  /// Compute sha256 of `data` as lowercase hex (64 chars).
  static std::string sha256Hex(const std::string& data);

  /// Serialize one entry to canonical JSON (lexicographic key order)
  /// EXCLUDING the self_hash field. This is exactly what gets hashed.
  static std::string toCanonicalJsonForHashing(const AuditEntry& entry,
                                                const std::string& uuid,
                                                const std::string& prev_hash);

private:
  /// Build the full JSON Lines record (with self_hash included).
  static std::string toJsonLine(const AuditEntry& entry,
                                 const std::string& uuid,
                                 const std::string& prev_hash,
                                 const std::string& self_hash);

  /// Scan `path` from the end, find the last well-formed line, and
  /// return its self_hash. Returns kGenesisHash for an empty file.
  /// Throws std::runtime_error if the tail is malformed.
  static std::string recoverTailHash(const std::string& path);

  /// File mode enforcement. Throws on chmod failure.
  static void enforceMode0640(const std::string& path);

  mutable std::mutex mutex_;
  std::string path_;
  int         fd_ = -1;            // raw fd for ::write + ::fsync (durable)
  std::string prev_hash_;
  std::size_t dropped_count_ = 0;
  std::size_t rotation_bytes_;   // for diagnostics only — logrotate does the actual rotate
};

}  // namespace san_fire_authorization

#endif  // SAN_FIRE_AUTHORIZATION__AUDIT_LOGGER_HPP_
