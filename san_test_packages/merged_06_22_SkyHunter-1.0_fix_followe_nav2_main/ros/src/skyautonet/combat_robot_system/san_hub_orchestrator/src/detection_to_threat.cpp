// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 DCN-2026-010 D-028 — Detection → ThreatAlert converter impl.

#include "san_hub_orchestrator/detection_to_threat.hpp"

#include "san_hub_orchestrator/threat_aggregator.hpp"  // threat_severity::*

#include <cstdio>

#include <cmath>

namespace san_hub_orchestrator
{

// ThreatAlert constants (mirrored — see threat_aggregator.hpp for the
// same pattern; we don't pull combat_robot_msgs here to keep this
// library standalone-testable).
namespace
{
constexpr uint8_t TA_TYPE_DRONE_DETECTED = 5;
constexpr uint8_t TA_TYPE_OTHER = 99;

// Detection.msg CLASS_* values (kept in sync with the .msg defn).
constexpr uint8_t CLASS_UNKNOWN = 0;
constexpr uint8_t CLASS_PERSON = 1;
constexpr uint8_t CLASS_VEHICLE = 2;
constexpr uint8_t CLASS_DRONE = 3;
constexpr uint8_t CLASS_WEAPON = 4;
constexpr uint8_t CLASS_ANIMAL = 5;
}  // namespace

uint8_t classToSeverity(uint8_t class_id)
{
  switch (class_id) {
    case CLASS_WEAPON:  return threat_severity::FATAL;
    case CLASS_DRONE:   return threat_severity::CRITICAL;
    case CLASS_PERSON:  return threat_severity::CRITICAL;
    case CLASS_VEHICLE: return threat_severity::WARNING;
    default:            return threat_severity::INFO;
  }
}

uint8_t classToThreatType(uint8_t class_id)
{
  // The existing ThreatAlert taxonomy was built for system-level
  // failures (SBC_FAILED, RTK_LOST, ...). Only DRONE_DETECTED maps
  // to a perception class; everything else folds into OTHER and
  // carries class info in `detail`.
  return (class_id == CLASS_DRONE) ?
         TA_TYPE_DRONE_DETECTED :
         TA_TYPE_OTHER;
}

const char * classToLabel(uint8_t class_id)
{
  switch (class_id) {
    case CLASS_PERSON:  return "person";
    case CLASS_VEHICLE: return "vehicle";
    case CLASS_DRONE:   return "drone";
    case CLASS_WEAPON:  return "weapon";
    case CLASS_ANIMAL:  return "animal";
    default:            return "unknown";
  }
}

DetectionToThreatConverter::DetectionToThreatConverter(
  DetectionConverterConfig cfg)
: cfg_(cfg) {}

void DetectionToThreatConverter::markFusedReceived(uint64_t ts_ms)
{
  last_fused_ms_.store(ts_ms, std::memory_order_relaxed);
}

bool DetectionToThreatConverter::shouldUseRgb(uint64_t now_ms) const
{
  const uint64_t last = last_fused_ms_.load(std::memory_order_relaxed);
  if (last == 0) {
    return true;                 // never seen → RGB authoritative
  }
  const auto window_ms = static_cast<uint64_t>(
    cfg_.fused_fallback_window_s * 1000.0);
  // Guard against now < last_fused (clock skew on tests etc.).
  if (now_ms <= last) {return false;}
  return (now_ms - last) > window_ms;
}

std::optional<ConvertedThreat> DetectionToThreatConverter::convert(
  uint8_t class_id,
  float confidence,
  uint32_t bbox_x1, uint32_t bbox_y1,
  uint32_t bbox_x2, uint32_t bbox_y2,
  DetectionSource source) const
{
  const float gate = (source == DetectionSource::Fused) ?
    cfg_.confidence_threshold :
    cfg_.rgb_confidence_threshold;
  if (confidence < gate) {return std::nullopt;}

  ConvertedThreat out;
  out.severity = classToSeverity(class_id);
  out.threat_type = classToThreatType(class_id);
  out.confidence = confidence;

  const char * label = classToLabel(class_id);
  // Korean operator banner. Demo Day Scenario B expects this on the
  // Android App UI; keep the format compact but informative.
  char msg[128];
  std::snprintf(
    msg, sizeof(msg),
    "위협 검출: %s (conf=%.2f)", label, confidence);
  out.message_ko = msg;

  // JSON detail — consumed by L5 measure_latency.py and any future
  // operator log viewer. Manually formatted to avoid pulling nlohmann.
  char detail[256];
  std::snprintf(
    detail, sizeof(detail),
    "{\"source\":\"%s\",\"class\":\"%s\",\"class_id\":%u,"
    "\"confidence\":%.4f,"
    "\"bbox\":[%u,%u,%u,%u]}",
    source == DetectionSource::Fused ? "fused" : "rgb_fallback",
    label, static_cast<unsigned>(class_id),
    confidence,
    bbox_x1, bbox_y1, bbox_x2, bbox_y2);
  out.detail = detail;

  return out;
}

namespace
{
inline double wrapPi(double a)
{
  while (a > M_PI) {a -= 2.0 * M_PI;}
  while (a < -M_PI) {a += 2.0 * M_PI;}
  return a;
}
}  // namespace

double classToRealHeightM(uint8_t class_id)
{
  // Monocular range priors. 0 = no stable height prior for the class →
  // computeGeo leaves range_m at 0 ("unknown" per ThreatAlert.msg) rather
  // than inventing a distance from a person-height assumption.
  switch (class_id) {
    case CLASS_PERSON:  return 1.7;   // standing adult
    case CLASS_VEHICLE: return 1.8;   // light tactical vehicle
    case CLASS_DRONE:   return 0.4;   // small multirotor incl. props
    case CLASS_ANIMAL:  return 0.8;
    case CLASS_WEAPON:  return 0.0;   // bbox height is pose-dependent
    default:            return 0.0;   // CLASS_UNKNOWN
  }
}

GeoResult computeGeo(
  double px, double py, double cx0, double cy0, double focal_px,
  double robot_yaw, double gimbal_pan, double gimbal_tilt,
  double depth_m, double bbox_height_px, double real_height_m)
{
  GeoResult g;
  if (focal_px <= 0.0) {return g;}
  const double az = std::atan2(cx0 - px, focal_px);
  const double el = std::atan2(cy0 - py, focal_px);
  double range = depth_m;
  if (range <= 0.0) {
    // Pinhole similar-triangles from the class height prior. No prior
    // (real_height_m <= 0) → range stays 0 (unknown); bearing is still valid.
    range = (real_height_m > 0.0 && bbox_height_px > 1.0) ?
      (focal_px * real_height_m / bbox_height_px) : 0.0;
  }
  g.bearing_deg =
    static_cast<float>(wrapPi(robot_yaw + gimbal_pan + az) * 180.0 / M_PI);
  g.elevation_deg = static_cast<float>((gimbal_tilt + el) * 180.0 / M_PI);
  g.range_m = static_cast<float>(range);
  g.has_position = true;
  return g;
}

}  // namespace san_hub_orchestrator
