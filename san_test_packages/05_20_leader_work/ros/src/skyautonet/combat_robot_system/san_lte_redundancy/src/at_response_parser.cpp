// SAN v1.5 Phase 2-E Turn 3 — AT response parser implementation.

#include "san_lte_redundancy/at_response_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace san_lte_redundancy {

namespace {

// Split comma-separated fields, respecting "quoted" strings.
// Quotes are stripped from output. Empty fields are preserved.
std::vector<std::string> splitCsvFields(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  bool in_quote = false;
  for (char c : s) {
    if (c == '"') {
      in_quote = !in_quote;
      continue;        // strip the quotes from output
    }
    if (c == ',' && !in_quote) {
      out.push_back(cur);
      cur.clear();
      continue;
    }
    cur.push_back(c);
  }
  out.push_back(cur);
  return out;
}

// Trim ASCII whitespace from both ends.
std::string trim(const std::string& s) {
  auto isws = [](unsigned char c) { return std::isspace(c); };
  auto a = std::find_if_not(s.begin(), s.end(), isws);
  auto b = std::find_if_not(s.rbegin(), s.rend(), isws).base();
  return (a < b) ? std::string(a, b) : std::string();
}

// Parse signed integer; returns std::nullopt on empty or non-numeric.
std::optional<int32_t> toInt(const std::string& s) {
  const auto t = trim(s);
  if (t.empty()) return std::nullopt;
  char* end = nullptr;
  errno = 0;
  long v = std::strtol(t.c_str(), &end, 10);
  if (end == t.c_str() || *end != '\0') return std::nullopt;
  if (v < INT32_MIN || v > INT32_MAX) return std::nullopt;
  return static_cast<int32_t>(v);
}

// Strip a "+TAG: " prefix from a response line. Returns the body
// (after the colon-space) or the original line if no tag found.
std::string stripTag(const std::string& line, const std::string& tag) {
  const auto pos = line.find(tag);
  if (pos == std::string::npos) return line;
  auto colon = line.find(':', pos);
  if (colon == std::string::npos) return line;
  return trim(line.substr(colon + 1));
}

}  // namespace

// ─── +CREG ──────────────────────────────────────────────────────────────

std::optional<CregStatus> parseCreg(const std::string& line) {
  // Accept either "+CREG: <n>,<stat>" or unsolicited "+CREG: <stat>"
  const auto body = stripTag(line, "+CREG");
  if (body == line) return std::nullopt;   // no +CREG tag

  const auto fields = splitCsvFields(body);
  if (fields.empty()) return std::nullopt;

  // Decide which field carries <stat>. If 1 field, that's it; if 2+,
  // the second is <stat>.
  const auto pick = (fields.size() == 1) ? fields[0] : fields[1];
  const auto v = toInt(pick);
  if (!v) return std::nullopt;
  const int s = *v;
  if (s < 0 || s > 5) return std::nullopt;
  return static_cast<CregStatus>(s);
}

// ─── +COPS ──────────────────────────────────────────────────────────────

std::optional<std::string> parseCops(const std::string& line) {
  const auto body = stripTag(line, "+COPS");
  if (body == line) return std::nullopt;

  const auto fields = splitCsvFields(body);
  // Operator name is the third field (mode,format,"<oper>",...).
  if (fields.size() < 3) return std::nullopt;
  const auto oper = trim(fields[2]);
  if (oper.empty()) return std::nullopt;
  return oper;
}

// ─── +QCSQ ──────────────────────────────────────────────────────────────

std::optional<QcsqResult> parseQcsq(const std::string& line) {
  const auto body = stripTag(line, "+QCSQ");
  if (body == line) return std::nullopt;

  const auto fields = splitCsvFields(body);
  // Need at least the RAT label.
  if (fields.empty()) return std::nullopt;

  QcsqResult r;
  r.rat = trim(fields[0]);
  if (r.rat.empty()) return std::nullopt;

  auto setIfPresent = [&fields](std::size_t idx, int32_t& out) {
    if (idx >= fields.size()) return;
    auto v = toInt(fields[idx]);
    if (v) out = *v;
  };
  setIfPresent(1, r.rssi_dbm);
  setIfPresent(2, r.rsrp_dbm);
  setIfPresent(3, r.sinr_db);
  setIfPresent(4, r.rsrq_db);
  return r;
}

// ─── +CESQ ──────────────────────────────────────────────────────────────

std::optional<CesqResult> parseCesq(const std::string& line) {
  const auto body = stripTag(line, "+CESQ");
  if (body == line) return std::nullopt;

  const auto fields = splitCsvFields(body);
  if (fields.size() < 6) return std::nullopt;

  CesqResult r;
  // rsrq: field 4 (encoded 0..34; -19.5 + n*0.5 dB)
  if (auto v = toInt(fields[4])) {
    if (*v >= 0 && *v <= 34) {
      // Convert to nearest integer dB.
      const double db = -19.5 + (*v) * 0.5;
      r.rsrq_db = static_cast<int32_t>(db);
    }
  }
  // rsrp: field 5 (encoded 0..97; -141 + n dBm)
  if (auto v = toInt(fields[5])) {
    if (*v >= 0 && *v <= 97) {
      r.rsrp_dbm = -141 + *v;
    }
  }
  return r;
}

// ─── +CGPADDR ───────────────────────────────────────────────────────────

std::optional<std::string> parseCgpaddr(const std::string& line) {
  const auto body = stripTag(line, "+CGPADDR");
  if (body == line) return std::nullopt;

  const auto fields = splitCsvFields(body);
  if (fields.size() < 2) return std::nullopt;
  const auto ip = trim(fields[1]);
  // Reject empty / "0.0.0.0".
  if (ip.empty() || ip == "0.0.0.0") return std::nullopt;
  // Basic shape check: dotted quad.
  static const std::regex re("^[0-9]{1,3}(\\.[0-9]{1,3}){3}$");
  if (!std::regex_match(ip, re)) return std::nullopt;
  return ip;
}

}  // namespace san_lte_redundancy
