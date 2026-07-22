// SAN v1.5.2 DCN-2026-010 D-028 — Detection → ThreatAlert converter
// (pure logic, no ROS deps).
//
// Two responsibilities, split out from the rclcpp node so they can be
// unit-tested in isolation:
//   1. Per-detection conversion: bbox + class_id + confidence → severity,
//      threat_type, message_ko, JSON detail.
//   2. Fused/RGB source arbitration: prefer EO+IR fused; fall back to
//      RGB-only after a configurable dropout window.
//
// 권원:
//   * DCN-2026-010 §3.2 (detection_to_threat_node spec)
//   * SkyHunter v1.5.2 Algorithm Inventory v1.1 Addendum

#ifndef SAN_HUB_ORCHESTRATOR__DETECTION_TO_THREAT_HPP_
#define SAN_HUB_ORCHESTRATOR__DETECTION_TO_THREAT_HPP_

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

namespace san_hub_orchestrator {

// Source tag carried on ThreatOutput.detail (JSON) for L5 debugging
// and downstream Android UI hinting.
enum class DetectionSource { Fused, RgbFallback };

// Single converted threat (mirrors combat_robot_msgs/ThreatAlert
// fields used by detection_to_threat_node). Independent of the
// generated message header so the converter is testable without
// rosidl_default_runtime.
struct ConvertedThreat {
  uint8_t     severity;        // ThreatAlert::SEVERITY_*
  uint8_t     threat_type;     // ThreatAlert::TYPE_*
  float       confidence;      // copied through 0..1
  std::string message_ko;      // operator-facing Korean
  std::string detail;          // JSON-encoded: source, class_id, bbox
};

struct DetectionConverterConfig {
  // Fused-stream gate (Scenario B B2 ≥ 0.9)
  float confidence_threshold = 0.9f;
  // RGB-only fallback uses a slightly lower gate so a noisy RGB pass
  // can still cover for a thermal dropout.
  float rgb_confidence_threshold = 0.8f;
  // How long (s) since the last fused detection before RGB fallback
  // is allowed. Spec: ≤ 1s dropout → primary still authoritative.
  double fused_fallback_window_s = 1.0;
};

class DetectionToThreatConverter {
 public:
  explicit DetectionToThreatConverter(
      DetectionConverterConfig cfg = DetectionConverterConfig{});

  
  // --- ADDED TO FIX STD::ATOMIC REASSIGNMENT ERROR ---
  DetectionToThreatConverter& operator=(const DetectionToThreatConverter& other) {
      cfg_ = other.cfg_;
      last_fused_ms_.store(other.last_fused_ms_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      return *this;
  }
  DetectionToThreatConverter& operator=(DetectionToThreatConverter&& other) noexcept {
      cfg_ = std::move(other.cfg_);
      last_fused_ms_.store(other.last_fused_ms_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      return *this;
  }

  // Record receipt of a fused-stream batch so RGB callbacks can
  // tell whether they're authoritative or suppressed.
  void markFusedReceived(uint64_t ts_ms);

  // True when RGB callbacks should publish (fused has been silent
  // for longer than `fused_fallback_window_s`, OR never observed).
  bool shouldUseRgb(uint64_t now_ms) const;

  // Convert one detection. Returns the populated ConvertedThreat on
  // success, or std::nullopt when the detection is below threshold
  // for the given source.
  std::optional<ConvertedThreat> convert(
      uint8_t  class_id,
      float    confidence,
      uint32_t bbox_x1, uint32_t bbox_y1,
      uint32_t bbox_x2, uint32_t bbox_y2,
      DetectionSource source) const;

  const DetectionConverterConfig& config() const { return cfg_; }
  uint64_t lastFusedMs() const {
    return last_fused_ms_.load(std::memory_order_relaxed);
  }

 private:
  DetectionConverterConfig cfg_;
  // 0 = never seen. Updated by markFusedReceived from the fused-stream
  // callback and read by shouldUseRgb from the RGB callback — different
  // threads under MultiThreadedExecutor, and a 64-bit value is not
  // guaranteed atomic on 32-bit ARM. std::atomic gives us the torn-read
  // protection that the two-callback design implicitly needed.
  std::atomic<uint64_t> last_fused_ms_{0};
};

// Pure helpers exposed for tests / reuse.
uint8_t classToSeverity(uint8_t class_id);
uint8_t classToThreatType(uint8_t class_id);
const char* classToLabel(uint8_t class_id);

}  // namespace san_hub_orchestrator

#endif  // SAN_HUB_ORCHESTRATOR__DETECTION_TO_THREAT_HPP_
