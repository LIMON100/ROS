// SAN v1.5 Phase 2-E Turn 4 — NMEA parser implementation.

#include "san_rtk_gnss/nmea_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>     // [DCN-2026-006 EXT D-022] std::fmod for HDT wrap
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

namespace san_rtk_gnss {

namespace {

// Split a NMEA sentence body on commas. Body = content between '$'
// and '*'. Empty fields are preserved (e.g. "1,,3" → ["1","","3"]).
std::vector<std::string> splitFields(const std::string& body) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : body) {
    if (c == ',') {
      out.push_back(std::move(cur));
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(std::move(cur));
  return out;
}

std::optional<double> toDouble(const std::string& s) {
  if (s.empty()) return std::nullopt;
  char* end = nullptr;
  double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || *end != '\0') return std::nullopt;
  return v;
}

std::optional<long> toInt(const std::string& s) {
  if (s.empty()) return std::nullopt;
  char* end = nullptr;
  errno = 0;
  long v = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str() || *end != '\0') return std::nullopt;
  return v;
}

uint8_t hexNibble(char c) {
  if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
  if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
  if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
  return 0xFF;
}

}  // namespace

// ─── checksum ──────────────────────────────────────────────────────────

bool nmeaChecksumOk(const std::string& sentence) {
  // Find '$' start and '*' just before the 2-char hex checksum.
  const auto dollar = sentence.find('$');
  const auto star   = sentence.find('*');
  if (dollar == std::string::npos || star == std::string::npos) return false;
  if (star <= dollar + 1) return false;
  if (star + 2 >= sentence.size()) return false;

  uint8_t expected = 0;
  for (size_t i = dollar + 1; i < star; ++i) {
    expected ^= static_cast<uint8_t>(sentence[i]);
  }

  const auto hi = hexNibble(sentence[star + 1]);
  const auto lo = hexNibble(sentence[star + 2]);
  if (hi == 0xFF || lo == 0xFF) return false;
  const uint8_t got = static_cast<uint8_t>((hi << 4) | lo);
  return expected == got;
}

// ─── ddmm.mmmm → decimal degrees ────────────────────────────────────────

std::optional<double> parseDmToDeg(const std::string& token,
                                    const std::string& hemi) {
  if (token.empty()) return std::nullopt;

  // Find decimal point — degrees are everything before the last 2
  // digits before the dot (or end-of-string if no dot).
  const auto dot = token.find('.');
  const size_t minutes_start =
      (dot == std::string::npos) ? token.size() : dot;
  if (minutes_start < 2) return std::nullopt;

  // Degrees = integer part of token[0:minutes_start-2]
  const auto deg_str = token.substr(0, minutes_start - 2);
  const auto min_str = token.substr(minutes_start - 2);

  auto deg = toInt(deg_str);
  auto min = toDouble(min_str);
  if (!deg || !min) return std::nullopt;
  if (*min < 0.0 || *min >= 60.0) return std::nullopt;

  double result = static_cast<double>(*deg) + (*min / 60.0);
  // Hemisphere: 'N' / 'E' positive, 'S' / 'W' negative
  if (!hemi.empty() && (hemi[0] == 'S' || hemi[0] == 'W')) {
    result = -result;
  }
  return result;
}

// ─── GGA ────────────────────────────────────────────────────────────────

std::optional<GgaResult> parseGga(const std::string& sentence) {
  if (!nmeaChecksumOk(sentence)) return std::nullopt;

  // Extract body (between '$' and '*')
  const auto dollar = sentence.find('$');
  const auto star   = sentence.find('*');
  if (dollar == std::string::npos || star == std::string::npos) return std::nullopt;
  const auto body = sentence.substr(dollar + 1, star - dollar - 1);

  // Talker + sentence type: e.g. "GPGGA", "GNGGA"
  // Check the last 3 chars before the first comma are "GGA".
  const auto comma1 = body.find(',');
  if (comma1 == std::string::npos || comma1 < 5) return std::nullopt;
  if (body.substr(comma1 - 3, 3) != "GGA") return std::nullopt;

  const auto fields = splitFields(body);
  // Expected layout (indices into fields[]):
  //  [0] talker+type   "GPGGA"
  //  [1] UTC hhmmss.ss
  //  [2] lat ddmm.mmmm
  //  [3] N/S
  //  [4] lon dddmm.mmmm
  //  [5] E/W
  //  [6] fix_quality
  //  [7] num_satellites
  //  [8] HDOP
  //  [9] altitude MSL
  //  [10] altitude units (M)
  //  [11] geoid separation
  //  [12] geoid units (M)
  //  [13] DGPS age (s, optional)
  //  [14] DGPS station id (optional)
  if (fields.size() < 10) return std::nullopt;

  GgaResult r;

  // Time
  if (fields[1].size() >= 6) {
    auto hh = toInt(fields[1].substr(0, 2));
    auto mm = toInt(fields[1].substr(2, 2));
    auto ss = toInt(fields[1].substr(4, 2));
    if (hh) r.hh = static_cast<uint8_t>(*hh);
    if (mm) r.mm = static_cast<uint8_t>(*mm);
    if (ss) r.ss = static_cast<uint8_t>(*ss);
  }

  // Lat/Lon are required
  auto lat = parseDmToDeg(fields[2], fields[3]);
  auto lon = parseDmToDeg(fields[4], fields[5]);
  if (!lat || !lon) return std::nullopt;
  r.latitude_deg  = *lat;
  r.longitude_deg = *lon;

  // Fix quality
  if (auto q = toInt(fields[6])) {
    if (*q < 0 || *q > 8) return std::nullopt;
    r.fix_type = static_cast<FixType>(*q);
  } else {
    return std::nullopt;
  }

  // Num satellites
  if (auto n = toInt(fields[7])) {
    r.num_satellites_used = static_cast<uint8_t>(std::clamp<long>(*n, 0, 40));
  }
  // HDOP
  if (auto h = toDouble(fields[8])) {
    r.hdop = static_cast<float>(*h);
  }
  // Altitude (MSL)
  if (auto a = toDouble(fields[9])) {
    r.altitude_m = *a;
  }
  // Geoid separation
  if (fields.size() > 11) {
    if (auto g = toDouble(fields[11])) {
      r.geoid_separation_m = *g;
    }
  }
  // DGPS / RTK age (s)
  if (fields.size() > 13) {
    if (auto t = toDouble(fields[13])) {
      r.rtk_correction_age_s = static_cast<float>(*t);
    }
  }
  // Reference station id — last field before "*HH"
  if (fields.size() > 14) {
    if (auto rs = toInt(fields[14])) {
      r.reference_station_id = static_cast<uint16_t>(
          std::clamp<long>(*rs, 0, 65535));
    }
  }

  return r;
}

// ─── GSA ────────────────────────────────────────────────────────────────

std::optional<GsaResult> parseGsa(const std::string& sentence) {
  if (!nmeaChecksumOk(sentence)) return std::nullopt;

  const auto dollar = sentence.find('$');
  const auto star   = sentence.find('*');
  if (dollar == std::string::npos || star == std::string::npos) return std::nullopt;
  const auto body = sentence.substr(dollar + 1, star - dollar - 1);

  const auto comma1 = body.find(',');
  if (comma1 == std::string::npos || comma1 < 5) return std::nullopt;
  if (body.substr(comma1 - 3, 3) != "GSA") return std::nullopt;

  const auto fields = splitFields(body);
  // Expected: [0]GxGSA [1]selection_mode(A/M) [2]mode(1/2/3)
  //           [3..14] satellite ids (12 slots)
  //           [15] PDOP [16] HDOP [17] VDOP
  if (fields.size() < 18) return std::nullopt;

  GsaResult r;
  if (auto m = toInt(fields[2])) {
    r.mode = static_cast<uint8_t>(*m);
  }
  // Count non-empty satellite-id slots.
  // GSA sentence carries up to 12 satellite IDs at indices 3..14 (inclusive).
  // Clamp the upper bound to fields.size() so a short sentence doesn't UB.
  uint8_t used = 0;
  const size_t end = std::min<size_t>(15, fields.size());
  for (size_t i = 3; i < end; ++i) {
    if (!fields[i].empty()) ++used;
  }
  r.num_used = used;

  if (auto p = toDouble(fields[15])) r.pdop = static_cast<float>(*p);
  if (auto h = toDouble(fields[16])) r.hdop = static_cast<float>(*h);
  if (auto v = toDouble(fields[17])) r.vdop = static_cast<float>(*v);
  return r;
}

// [DCN-2026-006 EXT D-022] $GxHDT — true heading from dual antenna.
//
// Wire format: "$GPHDT,HHH.HH,T*HH"
//   field[0] = "GPHDT"   (or any other talker — accept GP/GN/HE/etc.)
//   field[1] = heading degrees (0..360, allows decimal)
//   field[2] = 'T' (literal — True north reference)
//
// Returns nullopt on:
//   - checksum mismatch
//   - wrong sentence body (not "*HDT")
//   - field count < 3
//   - heading not parseable as double
//   - 'T' qualifier absent (rare — defensive)
//
// Heading is normalized into [0, 360); 360.0 → 0.0.
std::optional<HdtResult> parseHdt(const std::string& sentence) {
  if (!nmeaChecksumOk(sentence)) return std::nullopt;

  const auto dollar = sentence.find('$');
  const auto star   = sentence.find('*');
  if (dollar == std::string::npos || star == std::string::npos) {
    return std::nullopt;
  }
  const auto body = sentence.substr(dollar + 1, star - dollar - 1);

  // Body starts with talker (2) + "HDT" (3) + "," → comma at >= 5.
  const auto comma1 = body.find(',');
  if (comma1 == std::string::npos || comma1 < 5) return std::nullopt;
  if (body.substr(comma1 - 3, 3) != "HDT") return std::nullopt;

  const auto fields = splitFields(body);
  // [0] GxHDT  [1] heading  [2] T
  if (fields.size() < 3) return std::nullopt;
  if (fields[2] != "T")  return std::nullopt;

  const auto h = toDouble(fields[1]);
  if (!h) return std::nullopt;

  double heading = *h;
  if (!(heading == heading)) return std::nullopt;   // NaN guard
  // Wrap any out-of-range value into [0, 360).
  heading = std::fmod(heading, 360.0);
  if (heading < 0.0) heading += 360.0;

  HdtResult r;
  r.heading_deg = static_cast<float>(heading);
  r.valid       = true;
  return r;
}

}  // namespace san_rtk_gnss
