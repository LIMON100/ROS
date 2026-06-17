// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — Append-only audit logger (PATCHED 2026-05-13).

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
#include <variant>
#include <vector>

namespace san_fire_authorization
{

const char kGenesisHash[] =
  "0000000000000000000000000000000000000000000000000000000000000000";

namespace
{

std::string toHexLower(const uint8_t * bytes, std::size_t n)
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

std::string jsonEscape(const std::string & s)
{
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
          std::snprintf(
            buf, sizeof(buf), "\\u%04x",
            static_cast<unsigned char>(c));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

std::string jq(const std::string & key, const std::string & val)
{
  return "\"" + jsonEscape(key) + "\":\"" + jsonEscape(val) + "\"";
}
std::string jnum(const std::string & key, uint64_t val)
{
  return "\"" + jsonEscape(key) + "\":" + std::to_string(val);
}
std::string jnum(const std::string & key, int64_t val)
{
  return "\"" + jsonEscape(key) + "\":" + std::to_string(val);
}
std::string jbool(const std::string & key, bool val)
{
  return "\"" + jsonEscape(key) + "\":" + (val ? "true" : "false");
}

class CanonicalObjectBuilder
{
public:
  void putString(const std::string & key, const std::string & val)
  {
    fields_[key] = jq(key, val);
  }
  void putUint(const std::string & key, uint64_t val)
  {
    fields_[key] = jnum(key, val);
  }
  void putInt(const std::string & key, int64_t val)
  {
    fields_[key] = jnum(key, val);
  }
  void putBool(const std::string & key, bool val)
  {
    fields_[key] = jbool(key, val);
  }
  std::string finalize() const
  {
    std::string out = "{";
    bool first = true;
    for (const auto & kv : fields_) {
      if (!first) {out += ",";}
      out += kv.second;
      first = false;
    }
    out += "}";
    return out;
  }

private:
  std::map<std::string, std::string> fields_;
};

void fillCommonFields(
  CanonicalObjectBuilder & b,
  const AuditEntry & e,
  const std::string & uuid,
  const std::string & prev_hash)
{
  b.putString("schema", "san.audit/v1.5");
  b.putString("uuid", uuid);
  b.putString("prev_hash", prev_hash);

  b.putUint("timestamp_ms", e.timestamp_ms);
  b.putUint("request_id", e.request_id);
  b.putString("operator_id", e.operator_id);

  b.putBool("granted", e.granted);
  b.putString("reason", e.reason);
  b.putString("reason_detail", e.reason_detail);
  b.putBool("limp_mode_fire", e.limp_mode_fire);

  b.putInt("target_lat_e7", e.target_lat_e7);
  b.putInt("target_lon_e7", e.target_lon_e7);
  b.putInt("target_alt_mm", e.target_alt_mm);

  b.putUint("hub_term", e.hub_term);
  b.putUint("leader_term", e.leader_term);
  b.putUint("n_alive_robots", e.n_alive_robots);
}

}  // namespace

// ─── ctor / dtor ────────────────────────────────────────────────────────

AuditLogger::AuditLogger(
  const std::string & path,
  std::size_t rotation_bytes)
: path_(path), rotation_bytes_(rotation_bytes)
{
  // PATCH 2026-05-13: verify the existing chain BEFORE accepting it.
  // Fail-closed on broken chain — extending tampered audit silently
  // would defeat the tamper-evidence property of D-004.
  struct stat st;
  if (::stat(path_.c_str(), &st) == 0 && st.st_size > 0) {
    const auto vr = verifyChain(path_, kGenesisHash);
    if (!vr.valid) {
      throw std::runtime_error(
              "AuditLogger: existing chain verification failed at " +
              path_ + " — refusing to extend (line " +
              std::to_string(vr.last_line_no) + ": " + vr.error + ")");
    }
    prev_hash_ = vr.tail_self_hash;
  } else {
    prev_hash_ = kGenesisHash;
  }

  // PATCH 2026-05-13: POSIX open with O_APPEND for atomic writes.
  fd_ = openForAppend(path_);
  if (fd_ < 0) {
    throw std::runtime_error(
            "AuditLogger: cannot open for append: " + path_ +
            " (" + std::strerror(errno) + ")");
  }
  enforceMode0640(path_);
}

AuditLogger::~AuditLogger()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (fd_ >= 0) {
    ::fsync(fd_);
    ::close(fd_);
    fd_ = -1;
  }
}

int AuditLogger::openForAppend(const std::string & path)
{
  // O_APPEND      : atomic concurrent appends from same process
  // O_WRONLY      : we never read; reads go through verifyChain()
  // O_CREAT       : first-time creation OK
  // 0640          : owner rw + group r (matches enforceMode0640)
  return ::open(
    path.c_str(),
    O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC,
    0640);
}

// ─── emit ───────────────────────────────────────────────────────────────

AuditEmitResult AuditLogger::emit(const AuditEntry & entry)
{
  AuditEmitResult res;
  const std::string uuid = generateUuidV4();

  std::lock_guard<std::mutex> lock(mutex_);
  if (fd_ < 0) {
    ++dropped_count_;
    return res;
  }

  // 1. Canonical JSON (hash input) without self_hash.
  const std::string canonical =
    toCanonicalJsonForHashing(entry, uuid, prev_hash_);
  // 2. Self hash.
  const std::string self_hash = sha256Hex(canonical);
  // 3. Full line.
  const std::string line =
    toJsonLine(entry, uuid, prev_hash_, self_hash) + "\n";

  // 4. PATCH 2026-05-13: durable write — ::write + ::fsync.
  //    Both must succeed before we advance prev_hash and return ok=true.
  const char * data = line.data();
  std::size_t remaining = line.size();
  while (remaining > 0) {
    const ssize_t n = ::write(fd_, data, remaining);
    if (n < 0) {
      if (errno == EINTR) {continue;}
      ++dropped_count_;
      return res;
    }
    if (n == 0) {
      // 0-byte write — treat as failure (unusual for files).
      ++dropped_count_;
      return res;
    }
    data += n;
    remaining -= static_cast<std::size_t>(n);
  }
  // ★ PATCH: real fsync (was missing — the Phase-2-D TODO).
  if (::fsync(fd_) != 0) {
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
  const std::string & path, const std::string & expected_prev_hash)
{
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

    auto extractStr = [&line](const std::string & key) -> std::string {
        const std::string needle = "\"" + key + "\":\"";
        const auto pos = line.find(needle);
        if (pos == std::string::npos) {return std::string();}
        const auto start = pos + needle.size();
        const auto end = line.find('"', start);
        if (end == std::string::npos) {return std::string();}
        return line.substr(start, end - start);
      };

    const std::string this_prev = extractStr("prev_hash");
    const std::string this_self = extractStr("self_hash");
    const std::string this_uuid = extractStr("uuid");

    if (this_prev.size() != 64 || this_self.size() != 64 ||
      this_uuid.empty())
    {
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

    const std::string sh_needle = ",\"self_hash\":\"" + this_self + "\"";
    const auto sh_pos = line.find(sh_needle);
    std::string canonical = line;
    if (sh_pos != std::string::npos) {
      canonical.erase(sh_pos, sh_needle.size());
    } else {
      const std::string sh_alt = "\"self_hash\":\"" + this_self + "\",";
      const auto sh_pos2 = line.find(sh_alt);
      if (sh_pos2 == std::string::npos) {
        r.error = "line " + std::to_string(line_no) +
          ": cannot locate self_hash for verification";
        r.last_line_no = line_no;
        return r;
      }
      canonical.erase(sh_pos2, sh_alt.size());
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

std::size_t AuditLogger::droppedCount() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return dropped_count_;
}

std::string AuditLogger::prevHash() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return prev_hash_;
}

std::string AuditLogger::generateUuidV4()
{
  std::array<uint8_t, 16> b{};
  if (RAND_bytes(b.data(), static_cast<int>(b.size())) != 1) {
    throw std::runtime_error(
            "AuditLogger::generateUuidV4: RAND_bytes failed");
  }
  b[6] = (b[6] & 0x0F) | 0x40;  // version 4
  b[8] = (b[8] & 0x3F) | 0x80;  // variant RFC 4122

  const std::string hex = toHexLower(b.data(), b.size());
  return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" +
         hex.substr(12, 4) + "-" + hex.substr(16, 4) + "-" +
         hex.substr(20, 12);
}

std::string AuditLogger::sha256Hex(const std::string & data)
{
  std::array<uint8_t, SHA256_DIGEST_LENGTH> digest{};
  unsigned int len = 0;

  EVP_MD_CTX * ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    throw std::runtime_error("sha256Hex: EVP_MD_CTX_new failed");
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
    EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
    EVP_DigestFinal_ex(ctx, digest.data(), &len) != 1)
  {
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("sha256Hex: EVP digest pipeline failed");
  }
  EVP_MD_CTX_free(ctx);
  return toHexLower(digest.data(), len);
}

std::string AuditLogger::toCanonicalJsonForHashing(
  const AuditEntry & e,
  const std::string & uuid,
  const std::string & prev_hash)
{
  CanonicalObjectBuilder b;
  fillCommonFields(b, e, uuid, prev_hash);
  return b.finalize();
}

std::string AuditLogger::toJsonLine(
  const AuditEntry & e,
  const std::string & uuid,
  const std::string & prev_hash,
  const std::string & self_hash)
{
  CanonicalObjectBuilder b;
  fillCommonFields(b, e, uuid, prev_hash);
  b.putString("self_hash", self_hash);
  return b.finalize();
}

std::string AuditLogger::recoverTailHash(const std::string & path)
{
  std::ifstream in(path);
  if (!in) {
    return kGenesisHash;
  }
  std::string line, last_nonempty;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      last_nonempty = line;
    }
  }
  if (last_nonempty.empty()) {
    return kGenesisHash;
  }
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

void AuditLogger::enforceMode0640(const std::string & path)
{
  if (::chmod(path.c_str(), 0640) != 0) {
    throw std::runtime_error(
            "AuditLogger::enforceMode0640: chmod failed: " + path +
            " (" + std::strerror(errno) + ")");
  }
}

}  // namespace san_fire_authorization
