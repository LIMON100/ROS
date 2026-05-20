// SAN v1.5.2 DCN-2026-010 D-028 — Detection → ThreatAlert converter impl.

#include "san_hub_orchestrator/detection_to_threat.hpp"

#include "san_hub_orchestrator/threat_aggregator.hpp"  // threat_severity::*

#include <cstdio>

namespace san_hub_orchestrator {

// ThreatAlert constants (mirrored — see threat_aggregator.hpp for the
// same pattern; we don't pull combat_robot_msgs here to keep this
// library standalone-testable).
namespace {
constexpr uint8_t TA_TYPE_DRONE_DETECTED = 5;
constexpr uint8_t TA_TYPE_OTHER          = 99;

// Detection.msg CLASS_* values (kept in sync with the .msg defn).
constexpr uint8_t CLASS_UNKNOWN = 0;
constexpr uint8_t CLASS_PERSON  = 1;
constexpr uint8_t CLASS_VEHICLE = 2;
constexpr uint8_t CLASS_DRONE   = 3;
constexpr uint8_t CLASS_WEAPON  = 4;
constexpr uint8_t CLASS_ANIMAL  = 5;
}  // namespace

uint8_t classToSeverity(uint8_t class_id) {
  switch (class_id) {
    case CLASS_WEAPON:  return threat_severity::FATAL;
    case CLASS_DRONE:   return threat_severity::CRITICAL;
    case CLASS_PERSON:  return threat_severity::CRITICAL;
    case CLASS_VEHICLE: return threat_severity::WARNING;
    default:            return threat_severity::INFO;
  }
}

uint8_t classToThreatType(uint8_t class_id) {
  // The existing ThreatAlert taxonomy was built for system-level
  // failures (SBC_FAILED, RTK_LOST, ...). Only DRONE_DETECTED maps
  // to a perception class; everything else folds into OTHER and
  // carries class info in `detail`.
  return (class_id == CLASS_DRONE)
      ? TA_TYPE_DRONE_DETECTED
      : TA_TYPE_OTHER;
}

const char* classToLabel(uint8_t class_id) {
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

void DetectionToThreatConverter::markFusedReceived(uint64_t ts_ms) {
  last_fused_ms_.store(ts_ms, std::memory_order_relaxed);
}

bool DetectionToThreatConverter::shouldUseRgb(uint64_t now_ms) const {
  const uint64_t last = last_fused_ms_.load(std::memory_order_relaxed);
  if (last == 0) return true;   // never seen → RGB authoritative
  const auto window_ms = static_cast<uint64_t>(
      cfg_.fused_fallback_window_s * 1000.0);
  // Guard against now < last_fused (clock skew on tests etc.).
  if (now_ms <= last) return false;
  return (now_ms - last) > window_ms;
}

std::optional<ConvertedThreat> DetectionToThreatConverter::convert(
    uint8_t  class_id,
    float    confidence,
    uint32_t bbox_x1, uint32_t bbox_y1,
    uint32_t bbox_x2, uint32_t bbox_y2,
    DetectionSource source) const {
  const float gate = (source == DetectionSource::Fused)
      ? cfg_.confidence_threshold
      : cfg_.rgb_confidence_threshold;
  if (confidence < gate) return std::nullopt;

  ConvertedThreat out;
  out.severity    = classToSeverity(class_id);
  out.threat_type = classToThreatType(class_id);
  out.confidence  = confidence;

  const char* label = classToLabel(class_id);
  // Korean operator banner. Demo Day Scenario B expects this on the
  // Android App UI; keep the format compact but informative.
  char msg[128];
  std::snprintf(msg, sizeof(msg),
                "위협 검출: %s (conf=%.2f)", label, confidence);
  out.message_ko = msg;

  // JSON detail — consumed by L5 measure_latency.py and any future
  // operator log viewer. Manually formatted to avoid pulling nlohmann.
  char detail[256];
  std::snprintf(detail, sizeof(detail),
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

}  // namespace san_hub_orchestrator
