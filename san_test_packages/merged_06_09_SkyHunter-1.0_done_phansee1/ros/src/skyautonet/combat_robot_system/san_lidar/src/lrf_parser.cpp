// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 7 — LRF parser implementation.

#include "san_lidar/lrf_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace san_lidar
{

namespace
{

std::string trim(const std::string & s)
{
  auto isws = [](unsigned char c) {return std::isspace(c);};
  auto a = std::find_if_not(s.begin(), s.end(), isws);
  auto b = std::find_if_not(s.rbegin(), s.rend(), isws).base();
  return (a < b) ? std::string(a, b) : std::string();
}

std::vector<std::string> splitCommas(const std::string & s)
{
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ',') {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

std::optional<float> toFloat(const std::string & s)
{
  const auto t = trim(s);
  if (t.empty()) {return std::nullopt;}
  char * end = nullptr;
  float v = std::strtof(t.c_str(), &end);
  if (end == t.c_str() || *end != '\0') {return std::nullopt;}
  // Reject NaN / +Inf / -Inf — strtof happily parses "nan", "inf",
  // "INFINITY" etc. Letting these through would propagate to range
  // / strength fields and downstream consumers (reroute planner,
  // fire authorization) that do not defend against non-finite
  // values.
  if (!std::isfinite(v)) {return std::nullopt;}
  return v;
}

}  // namespace

std::optional<LrfSample> parseLrfLine(
  const std::string & line, float max_range_m)
{
  const auto trimmed = trim(line);
  if (trimmed.empty()) {return std::nullopt;}

  const auto fields = splitCommas(trimmed);
  if (fields.empty()) {return std::nullopt;}

  // First field must be numeric range.
  auto range = toFloat(fields[0]);
  if (!range) {return std::nullopt;}

  LrfSample s;
  s.range_m = *range;

  // Validity rules:
  //   * range <= 0           → no return / invalid
  //   * range > max_range_m  → out of range / invalid
  //   * otherwise            → valid
  if (s.range_m <= 0.05f || s.range_m > max_range_m) {
    s.valid = false;
  } else {
    s.valid = true;
  }

  // Optional strength field — second value, normalize if obviously
  // 0..255 (Lightware reports 0..255) or 0..100 (others).
  if (fields.size() >= 2) {
    if (auto str = toFloat(fields[1])) {
      float v = *str;
      if (v > 1.0f && v <= 100.0f) {
        s.return_strength = v / 100.0f;
      } else if (v > 100.0f && v <= 255.0f) {
        s.return_strength = v / 255.0f;
      } else if (v >= 0.0f && v <= 1.0f) {
        s.return_strength = v;
      } else {
        s.return_strength = 0.9f;   // unexpected — fallback default
      }
    } else {
      s.return_strength = 0.9f;
    }
  } else {
    // No strength reported (LWnav style) — use default
    s.return_strength = 0.9f;
  }

  return s;
}

}  // namespace san_lidar
