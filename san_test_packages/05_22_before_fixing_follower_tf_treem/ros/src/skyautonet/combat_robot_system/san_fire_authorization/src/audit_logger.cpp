// SAN v1.5 PHASE 9 — Append-only audit logger implementation.
// See audit_logger.hpp for the API contract.

#include "san_fire_authorization/audit_logger.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <ios>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace san_fire_authorization {

// 64 '0' chars — sha256 of nothing-yet, by convention.
const char kGenesisHash[] =
    "0000000000000000000000000000000000000000000000000000000000000000";

namespace {

// ─── hex helpers ────────────────────────────────────────────────────────

std::string toHexLower(const uint8_t* bytes, std::size_t n) {
  static const char kHex[] = "0123456789abcdef";
  std::string s;
  s.resize(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    s[2 * i]     = kHex[(bytes[i] >> 4) & 0x0F];
    s[2 * i + 1] = kHex[ bytes[i]       & 0x0F];
  }
  return s;
}

// ─── JSON helpers (minimal, hand-rolled) ────────────────────────────────
// We avoid pulling in nlohmann/json to keep dependencies minimal and to
// guarantee byte-exact canonical output for hashing.

std::string jsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b";  break;
      case '\f': out += "\\f";  break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x",
                        static_cast<unsigned char>(c));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

std::string jq(const std::string& key, const std::string& val) {
  return "\"" + jsonEscape(key) + "\":\"" + jsonEscape(val) + "\"";
}
std::string jnum(const std::string& key, uint64_t val) {
  return "\"" + jsonEscape(key) + "\":" + std::to_string(val);
}
std::string jnum(const std::string& key, int64_t val) {
  return "\"" + jsonEscape(key) + "\":" + std::to_string(val);
}
std::string jbool(const std::string& key, bool val) {
  return "\"" + jsonEscape(key) + "\":" + (val ? "true" : "false");
}

// ─── Build a canonical (lexicographically-ordered) JSON object ─────────
// We collect (key, formatted-field) pairs into a std::map (auto-sorted by
// key) and emit. Map gives us deterministic ordering for the hash input.
class CanonicalObjectBuilder {
public:
  void putString(const std::string& key, const std::string& val) {
    fields_[key] = jq(key, val);
  }
  void putUint(const std::string& key, uint64_t val) {
    fields_[key] = jnum(key, val);
  }
  void putInt(const std::string& key, int64_t val) {
    fields_[key] = jnum(key, val);
  }
  void putBool(const std::string& key, bool val) {
    fields_[key] = jbool(key, val);
  }
  std::string finalize() const {
    std::string out = "{";
    bool first = true;
    for (const auto& kv : fields_) {
      if (!first) out += ",";
      out += kv.second;
      first = false;
    }
    out += "}";
    return out;
  }

private:
  std::map<std::string, std::string> fields_;
};

void fillCommonFields(CanonicalObjectBuilder& b,
                      const AuditEntry& e,
                      const std::string& uuid,
                      const std::string& prev_hash) {
  // Schema version pinned for forward compatibility.
  b.putString("schema",          "san.audit/v1.5");
  b.putString("uuid",            uuid);
  b.putString("prev_hash",       prev_hash);

  b.putUint  ("timestamp_ms",    e.timestamp_ms);
  b.putUint  ("request_id",      e.request_id);
  b.putString("operator_id",     e.operator_id);

  b.putBool  ("granted",         e.granted);
  b.putString("reason",          e.reason);
  b.putString("reason_detail",   e.reason_detail);
  b.putBool  ("limp_mode_fire",  e.limp_mode_fire);

  b.putInt   ("target_lat_e7",   e.target_lat_e7);
  b.putInt   ("target_lon_e7",   e.target_lon_e7);
  b.putInt   ("target_alt_mm",   e.target_alt_mm);

  b.putUint  ("hub_term",        e.hub_term);
  b.putUint  ("leader_term",     e.leader_term);
  b.putUint  ("n_alive_robots",  e.n_alive_robots);
}

}  // namespace

// ─── ctor / dtor ────────────────────────────────────────────────────────

AuditLogger::AuditLogger(const std::string& path,
                          std::size_t rotation_bytes)
    : path_(path), rotation_bytes_(rotation_bytes) {
  // Recover the previous tail hash BEFORE opening for append, so we
  // can fail-fast on a broken chain.
  prev_hash_ = recoverTailHash(path_);

  // Open with raw POSIX semantics so we can call ::fsync(fd_) after
  // each emit() for true disk durability — std::ofstream::flush only
  // pushes to the OS page cache.
  fd_ = ::open(path_.c_str(),
               O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC,
               0640);
  if (fd_ < 0) {
    throw std::runtime_error(
        "AuditLogger: cannot open for append: " + path_ +
        " (" + std::strerror(errno) + ")");
  }

  enforceMode0640(path_);
}

AuditLogger::~AuditLogger() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (fd_ >= 0) {
    // Best-effort final fsync so a graceful shutdown durably flushes
    // any in-flight entry. Errors are intentionally ignored here —
    // the dtor is on the exit path.
    ::fsync(fd_);
    ::close(fd_);
    fd_ = -1;
  }
}

// ─── emit ───────────────────────────────────────────────────────────────

AuditEmitResult AuditLogger::emit(const AuditEntry& entry) {
  AuditEmitResult res;
  const std::string uuid = generateUuidV4();

  std::lock_guard<std::mutex> lock(mutex_);

  // 1. Build the canonical (hash-input) JSON without self_hash.
  const std::string canonical =
      toCanonicalJsonForHashing(entry, uuid, prev_hash_);

  // 2. Compute self_hash = sha256(canonical).
  const std::string self_hash = sha256Hex(canonical);

  // 3. Build the full JSON line (canonical + self_hash, still ordered).
  const std::string line = toJsonLine(entry, uuid, prev_hash_, self_hash);

  // 4. Write + fsync atomically. Only advance prev_hash_ on success.
  //    ::write(O_APPEND) is atomic for buffers ≤ PIPE_BUF on POSIX,
  //    and our lines are well under that. ::fsync forces the page
  //    cache out to the storage device — true durability per
  //    DCN-2026-001 D-004 audit obligation.
  const std::string out = line + "\n";
  const ssize_t want = static_cast<ssize_t>(out.size());
  const ssize_t got  = ::write(fd_, out.data(), out.size());
  if (got != want) {
    ++dropped_count_;
    return res;  // ok == false, uuid empty
  }
  if (::fsync(fd_) != 0) {
    // Write reached the kernel but not the platter. Treat as drop —
    // a crash now would lose the line and break the chain.
    ++dropped_count_;
    return res;
  }

  prev_hash_ = self_hash;
  res.ok = true;
  res.uuid = uuid;
  res.self_hash = self_hash;
  return res;
}

// ─── chain verification ────────────────────────────────────────────────

AuditVerifyResult AuditLogger::verifyChain(
    const std::string& path, const std::string& expected_prev_hash) {
  AuditVerifyResult r;
  std::ifstream in(path);
  if (!in) {
    r.error = "cannot open: " + path;
    return r;
  }

  std::string line;
  std::string prev = expected_prev_hash;
  std::size_t line_no = 0;

  while (std::getline(in, line)) {
    if (line.empty()) {
      ++line_no;
      continue;
    }

    // Extract prev_hash, self_hash, uuid via naive string-scan.
    // We rely on the canonical lexicographic field order produced by
    // toCanonicalJsonForHashing — see schema field positions. To stay
    // robust against unknown ordering at read time, scan for keys.
    auto extractStr = [&line](const std::string& key) -> std::string {
      const std::string needle = "\"" + key + "\":\"";
      const auto pos = line.find(needle);
      if (pos == std::string::npos) return std::string();
      const auto start = pos + needle.size();
      const auto end = line.find('"', start);
      if (end == std::string::npos) return std::string();
      return line.substr(start, end - start);
    };

    const std::string this_prev = extractStr("prev_hash");
    const std::string this_self = extractStr("self_hash");
    const std::string this_uuid = extractStr("uuid");

    if (this_prev.size() != 64 || this_self.size() != 64 ||
        this_uuid.empty()) {
      r.error = "line " + std::to_string(line_no) +
                ": missing or malformed prev_hash/self_hash/uuid";
      r.last_line_no = line_no;
      return r;
    }

    if (this_prev != prev) {
      r.error = "line " + std::to_string(line_no) +
                ": prev_hash mismatch (expected " + prev +
                ", got " + this_prev + ")";
      r.last_line_no = line_no;
      return r;
    }

    // Reconstruct the canonical hash input by stripping the self_hash
    // field. Since the line was produced with deterministic ordering,
    // we can strip the `,"self_hash":"..."` token by string search.
    const std::string sh_needle = ",\"self_hash\":\"" + this_self + "\"";
    const auto sh_pos = line.find(sh_needle);
    if (sh_pos == std::string::npos) {
      // try as the first field (no leading comma)
      const std::string sh_alt = "\"self_hash\":\"" + this_self + "\",";
      const auto sh_pos2 = line.find(sh_alt);
      if (sh_pos2 == std::string::npos) {
        r.error = "line " + std::to_string(line_no) +
                  ": cannot locate self_hash for verification";
        r.last_line_no = line_no;
        return r;
      }
    }
    std::string canonical = line;
    if (sh_pos != std::string::npos) {
      canonical.erase(sh_pos, sh_needle.size());
    }

    const std::string computed = sha256Hex(canonical);
    if (computed != this_self) {
      r.error = "line " + std::to_string(line_no) +
                ": self_hash mismatch (computed " + computed +
                ", recorded " + this_self + ")";
      r.last_line_no = line_no;
      return r;
    }

    prev = this_self;
    ++line_no;
  }

  r.valid = true;
  r.last_line_no = (line_no == 0) ? 0 : (line_no - 1);
  r.tail_self_hash = prev;
  return r;
}

// ─── diagnostics ────────────────────────────────────────────────────────

std::size_t AuditLogger::droppedCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dropped_count_;
}

std::string AuditLogger::prevHash() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return prev_hash_;
}

// ─── static helpers ─────────────────────────────────────────────────────

std::string AuditLogger::generateUuidV4() {
  // RFC 4122 v4: 16 random bytes with version+variant bits set.
  std::array<uint8_t, 16> b{};
  if (RAND_bytes(b.data(), static_cast<int>(b.size())) != 1) {
    throw std::runtime_error(
        "AuditLogger::generateUuidV4: RAND_bytes failed");
  }
  b[6] = (b[6] & 0x0F) | 0x40;  // version 4
  b[8] = (b[8] & 0x3F) | 0x80;  // variant RFC 4122

  // 8-4-4-4-12 hex format.
  const std::string hex = toHexLower(b.data(), b.size());
  return hex.substr(0, 8)  + "-" + hex.substr(8, 4)  + "-" +
         hex.substr(12, 4) + "-" + hex.substr(16, 4) + "-" +
         hex.substr(20, 12);
}

std::string AuditLogger::sha256Hex(const std::string& data) {
  std::array<uint8_t, SHA256_DIGEST_LENGTH> digest{};
  unsigned int len = 0;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    throw std::runtime_error("sha256Hex: EVP_MD_CTX_new failed");
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
      EVP_DigestFinal_ex(ctx, digest.data(), &len) != 1) {
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("sha256Hex: EVP digest pipeline failed");
  }
  EVP_MD_CTX_free(ctx);

  return toHexLower(digest.data(), len);
}

std::string AuditLogger::toCanonicalJsonForHashing(
    const AuditEntry& e,
    const std::string& uuid,
    const std::string& prev_hash) {
  CanonicalObjectBuilder b;
  fillCommonFields(b, e, uuid, prev_hash);
  return b.finalize();
}

// ─── internal ───────────────────────────────────────────────────────────

std::string AuditLogger::toJsonLine(
    const AuditEntry& e,
    const std::string& uuid,
    const std::string& prev_hash,
    const std::string& self_hash) {
  CanonicalObjectBuilder b;
  fillCommonFields(b, e, uuid, prev_hash);
  b.putString("self_hash", self_hash);
  return b.finalize();
}

std::string AuditLogger::recoverTailHash(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    // File doesn't exist yet — that's fine, first entry uses genesis.
    return kGenesisHash;
  }

  // For now, do a linear scan. Audit files are bounded at 10 MB so
  // this is a one-shot tens-of-megabytes scan at startup — acceptable.
  std::string line, last_nonempty;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      last_nonempty = line;
    }
  }
  if (last_nonempty.empty()) {
    return kGenesisHash;
  }

  // Extract self_hash field.
  const std::string needle = "\"self_hash\":\"";
  const auto pos = last_nonempty.find(needle);
  if (pos == std::string::npos) {
    throw std::runtime_error(
        "AuditLogger::recoverTailHash: last line missing self_hash: " +
        path);
  }
  const auto start = pos + needle.size();
  const auto end = last_nonempty.find('"', start);
  if (end == std::string::npos || (end - start) != 64) {
    throw std::runtime_error(
        "AuditLogger::recoverTailHash: malformed self_hash: " + path);
  }
  return last_nonempty.substr(start, 64);
}

void AuditLogger::enforceMode0640(const std::string& path) {
  if (::chmod(path.c_str(), 0640) != 0) {
    throw std::runtime_error(
        "AuditLogger::enforceMode0640: chmod failed: " + path +
        " (" + std::strerror(errno) + ")");
  }
}

}  // namespace san_fire_authorization
