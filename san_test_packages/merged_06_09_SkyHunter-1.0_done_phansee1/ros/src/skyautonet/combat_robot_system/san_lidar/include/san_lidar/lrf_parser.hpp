// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 7 — LRF (single-point Laser Range Finder) parser.
//
// Parses ASCII responses from common single-point LRFs:
//   * Lightware LW20:  "<range>,<strength>,<flags>\r\n"
//   * Lightware LWnav: "<range>\r\n"
//   * Generic:         decimal range only
//
// Pure C++17 — no ROS, no serial. Standalone testable.

#ifndef SAN_LIDAR__LRF_PARSER_HPP_
#define SAN_LIDAR__LRF_PARSER_HPP_

#include <optional>
#include <string>

namespace san_lidar
{

struct LrfSample
{
  float range_m = 0.0f;
  float return_strength = 0.0f;
  bool valid = false;
};

/// Parse a single LRF response line. Trims whitespace; tolerates
/// either "<range>" or "<range>,<strength>[,<flags>]" formats.
///
/// Behavior:
///   * Empty line                   → std::nullopt
///   * Non-numeric range            → std::nullopt
///   * Range <= 0 or > max_range_m  → valid = false (still returned)
///   * No strength field            → strength = 0.9 (default reasonable)
///   * Strength field present       → normalized to 0..1 if needed
std::optional<LrfSample> parseLrfLine(
  const std::string & line,
  float max_range_m = 200.0f);

}  // namespace san_lidar

#endif  // SAN_LIDAR__LRF_PARSER_HPP_
