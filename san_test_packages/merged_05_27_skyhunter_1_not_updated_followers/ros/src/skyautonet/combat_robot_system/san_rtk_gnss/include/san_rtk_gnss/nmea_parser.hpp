// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 4 — NMEA 0183 parser (pure logic).
//
// Parses the NMEA sentences the RTK GNSS node consumes:
//   $GxGGA — Global Positioning System Fix Data (primary fix info)
//   $GxGSA — DOP and active satellites
//
// Pure C++17, no external deps. Fully standalone testable.
//
// References:
//   * NMEA 0183 v4.10 §5.3.20 (GGA)
//   * NMEA 0183 v4.10 §5.3.21 (GSA)
//   * u-blox F9P Interface Manual (talker-id GN/GP/GA/GL/GB/GI)

#ifndef SAN_RTK_GNSS__NMEA_PARSER_HPP_
#define SAN_RTK_GNSS__NMEA_PARSER_HPP_

#include <cstdint>
#include <optional>
#include <string>

namespace san_rtk_gnss
{

/// NMEA GGA fix-quality values (sentence field 6).
/// 1:1 maps to RtkFixStatus.FIX_* constants.
enum class FixType : uint8_t
{
  No          = 0,
  Auto2D      = 1,
  Dgps        = 2,
  Pps         = 3,
  RtkFix      = 4,
  RtkFloat    = 5,
  Estimated   = 6,
  Manual      = 7,
  Simulated   = 8,
};

struct GgaResult
{
  // UTC time of fix as HHMMSS.ss (truncated to seconds in this struct)
  uint8_t hh = 0;
  uint8_t mm = 0;
  uint8_t ss = 0;
  // Position
  double latitude_deg = 0.0;
  double longitude_deg = 0.0;
  FixType fix_type = FixType::No;
  uint8_t num_satellites_used = 0;
  float hdop = 0.0f;
  double altitude_m = 0.0;                       // MSL
  double geoid_separation_m = 0.0;               // WGS84 - MSL
  float rtk_correction_age_s = 0.0f;             // 0 if absent
  uint16_t reference_station_id = 0;
};

struct GsaResult
{
  uint8_t mode = 0;                // 1=no-fix, 2=2D, 3=3D
  uint8_t num_used = 0;
  float pdop = 0.0f;
  float hdop = 0.0f;
  float vdop = 0.0f;
};

/// [DCN-2026-006 EXT D-022] $GxHDT — dual-antenna true heading.
///
/// Field-1 — heading in degrees True (0..360)
/// Field-2 — literal 'T' (True north reference)
///
/// Heading is provided by a dual-antenna RTK receiver that derives
/// it from the baseline vector between two GNSS antennas mounted
/// fore-aft on the platform. Accuracy is on the order of 0.1°
/// (vs IMU magnetometer's 1-3° in ferrous-rich environments) and
/// the value is absolute — no drift, no calibration drift, no
/// initial-yaw alignment problem.
///
/// This sentence is published by u-blox F9R/F9H and Septentrio
/// PolaRx/Trimble BD-series multi-antenna boards. Single-antenna
/// receivers do not emit it; absence is non-fatal.
///
/// KPP-1 impact: localization heading absolute reference for
/// 50 m+ follower offsets where IMU heading drift accumulates
/// to >4 m lateral error within 60 s without an absolute reset.
struct HdtResult
{
  float heading_deg = 0.0f;   // 0 .. < 360
  bool valid = false;         // true if 'T' qualifier present
};

/// Verify NMEA sentence has correct *HH (XOR of bytes between '$' and '*').
/// Lines without a '*' are rejected. Returns true on match.
bool nmeaChecksumOk(const std::string & sentence);

/// Parse "$GxGGA,..." into GgaResult. Talker prefix (GP/GN/GA/...) is
/// accepted. Returns std::nullopt on:
///   - failed checksum
///   - wrong sentence type
///   - missing required fields (lat/lon/fix_quality)
/// Optional fields (rtk_age, ref_station, geoid_sep) default to 0 when
/// absent.
std::optional<GgaResult> parseGga(const std::string & sentence);

/// Parse "$GxGSA,..." into GsaResult. Used for DOP refinement.
std::optional<GsaResult> parseGsa(const std::string & sentence);

/// [DCN-2026-006 EXT D-022] Parse "$GxHDT,heading,T*HH".
/// Returns std::nullopt on checksum failure, wrong sentence type, or
/// malformed heading. Hemisphere field MUST be 'T' (True). Heading is
/// normalized into [0, 360).
std::optional<HdtResult> parseHdt(const std::string & sentence);

/// Convert NMEA ddmm.mmmm format with hemisphere to signed decimal
/// degrees. Returns std::nullopt on malformed input.
/// Public for tests + reuse.
std::optional<double> parseDmToDeg(
  const std::string & token,
  const std::string & hemi);

}  // namespace san_rtk_gnss

#endif  // SAN_RTK_GNSS__NMEA_PARSER_HPP_
