// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// DCN-2026-026 C-2 — Encircle combat trigger + lifecycle.
//
// Pure-logic gate / state machine for the formation's threat-response
// Encircle maneuver. The node feeds qualifying ThreatAlerts (already
// world-localized from the REPORTING robot's pose — never the leader's,
// see threatWorldXY) and operator confirm/release events; this decides
// when the swarm is in combat and where the encircle anchor sits.
//
//   Idle ──qualifying threat──▶ PendingConfirm ──operator──▶ Active
//     ▲                            │(auto_engage: skip)        │
//     └──── Cooldown ◀── TTL expiry / operator release ────────┘
//
// Gate (ALL must hold — DCN-2026-026 C-2, ratified 2026-06-10):
//   1. severity ≥ CRITICAL and threat_type ∈ {DRONE_DETECTED, OTHER}
//      (system alerts — battery/comm/SBC — can NEVER trigger combat),
//   2. confidence ≥ min_confidence (parsed from the alert detail JSON),
//   3. has_position with a usable range (world point computable),
//   4. operator confirmation (auto_engage=true opt-in skips it).
// Lifecycle: TTL refresh on every qualifying alert; release only via
// TTL expiry or operator release; Cooldown blocks re-entry (anti-
// flicker hysteresis). Non-qualifying alerts never clear combat.
//
// No rclcpp — standalone testable.

#ifndef SAN_FORMATION__ENCIRCLE_COMBAT_HPP_
#define SAN_FORMATION__ENCIRCLE_COMBAT_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace san_formation
{

// Mirror combat_robot_msgs/ThreatAlert constants (pure-logic library —
// no generated headers; same pattern as san_hub_orchestrator).
namespace threat_alert
{
constexpr uint8_t SEVERITY_CRITICAL = 2;
constexpr uint8_t TYPE_DRONE_DETECTED = 5;
constexpr uint8_t TYPE_OTHER = 99;     // perception person/vehicle class
}

struct EncircleConfig
{
  float min_confidence = 0.9f;     // gate #2
  bool auto_engage = false;        // true = skip operator confirm (opt-in)
  uint32_t ttl_ms = 10000;         // combat decays without fresh threat
  uint32_t reentry_block_ms = 5000;  // Cooldown — anti-flicker hysteresis
};

enum class EncirclePhase : uint8_t
{
  Idle = 0,
  PendingConfirm = 1,   // gate passed, waiting for the operator 1-tap
  Active = 2,           // encircle slots override the formation
  Cooldown = 3,         // recently released — re-entry blocked
};

/// Gate #1/#2/#3 — pure predicate (operator confirm is the state
/// machine's job). `confidence` is nullopt when the alert detail
/// carried none — fails the gate (unknown confidence ≠ confident).
bool passesEncircleGate(
  uint8_t severity, uint8_t threat_type,
  std::optional<float> confidence, bool has_position, float range_m,
  float min_confidence);

/// Extract "confidence":<float> from the machine-readable alert
/// `detail` JSON (hand-formatted by detection_to_threat — parsed
/// without a JSON dependency, same project convention).
std::optional<float> parseConfidenceFromDetail(const std::string & detail);

/// Parse a ThreatAlert.source_robot_id of the form "robot_3" / "3"
/// into the numeric robot id. Non-robot reporters ("hub",
/// "perception") return nullopt — their pose is unknown to formation.
std::optional<uint32_t> parseRobotIdString(const std::string & source);

/// Threat world position from the REPORTING robot's pose + the alert's
/// world-frame bearing/range (DCN-2026-010). Using the leader's pose
/// here was the original defbb64 defect (encircled the wrong point).
std::pair<float, float> threatWorldXY(
  float reporter_x, float reporter_y, float bearing_deg, float range_m);

class EncircleCombat
{
public:
  explicit EncircleCombat(EncircleConfig cfg = EncircleConfig{})
  : cfg_(cfg) {}

  /// Feed one already-world-localized, gate-checked threat. Refreshes
  /// the TTL + anchor when in PendingConfirm/Active; arms the trigger
  /// from Idle. Ignored during Cooldown (hysteresis). Returns true if
  /// the phase changed.
  bool onQualifiedThreat(float world_x, float world_y, uint64_t now_ms);

  /// Operator "포위 승인" 1-tap — PendingConfirm → Active.
  bool onOperatorConfirm(uint64_t now_ms);

  /// Operator release — PendingConfirm/Active → Cooldown.
  bool onOperatorRelease(uint64_t now_ms);

  /// Time-driven decay: Active/PendingConfirm past TTL → Cooldown;
  /// Cooldown past reentry_block_ms → Idle. Returns true on change.
  bool tick(uint64_t now_ms);

  EncirclePhase phase() const {return phase_;}

  /// Anchor (threat world point) — valid in PendingConfirm/Active.
  std::optional<std::pair<float, float>> anchor() const
  {
    if (phase_ == EncirclePhase::PendingConfirm ||
      phase_ == EncirclePhase::Active)
    {
      return std::make_pair(anchor_x_, anchor_y_);
    }
    return std::nullopt;
  }

  const EncircleConfig & config() const {return cfg_;}

private:
  EncircleConfig cfg_;
  EncirclePhase phase_{EncirclePhase::Idle};
  float anchor_x_{0.0f};
  float anchor_y_{0.0f};
  uint64_t last_threat_ms_{0};
  uint64_t cooldown_since_ms_{0};
};

}  // namespace san_formation

#endif  // SAN_FORMATION__ENCIRCLE_COMBAT_HPP_
