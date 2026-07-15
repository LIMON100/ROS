// SAN v1.5 Phase 2-E Turn 8 — Threat alert aggregator (pure logic).
//
// Implements dedup + severity escalation + time-window aggregation for
// per-robot ThreatAlerts before hub publication. Decoupled from rclcpp
// so the logic is testable in isolation.
//
// Behavior:
//   * Identical (source_robot_id, threat_type) within `dedup_window_s`
//     fold into a single output, incrementing instance_count.
//   * If a new alert arrives with higher severity than a folded one,
//     the output's severity is promoted.
//   * After `dedup_window_s` the slot is reset.
//   * `pollReady()` returns alerts whose window has elapsed (caller
//     publishes them).

#ifndef SAN_HUB_ORCHESTRATOR__THREAT_AGGREGATOR_HPP_
#define SAN_HUB_ORCHESTRATOR__THREAT_AGGREGATOR_HPP_

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace san_hub_orchestrator {

// Mirror combat_robot_msgs/ThreatAlert constants — kept here so the
// pure-logic library doesn't depend on the generated message header.
namespace threat_severity {
  constexpr uint8_t INFO     = 0;
  constexpr uint8_t WARNING  = 1;
  constexpr uint8_t CRITICAL = 2;
  constexpr uint8_t FATAL    = 3;
}

struct ThreatInput {
  uint8_t  severity;
  uint8_t  threat_type;
  std::string source_robot_id;
  std::string peer_id;
  std::string message_ko;
  std::string detail;
  uint64_t timestamp_ms;
};

struct ThreatOutput {
  uint8_t  severity;
  uint8_t  threat_type;
  std::string source_robot_id;
  std::string peer_id;
  std::string message_ko;
  std::string detail;
  uint64_t timestamp_ms;      // last update
  uint32_t instance_count;
};

class ThreatAggregator {
public:
  /// `dedup_window_s` = how long identical alerts are folded together.
  explicit ThreatAggregator(double dedup_window_s = 5.0);

  /// Returns true when the input is a NEW (or newly-folded) alert —
  /// when this returns true, downstream may want to publish
  /// immediately for FATAL/CRITICAL.
  bool ingest(const ThreatInput& in);

  /// Returns alerts whose dedup window has elapsed (caller publishes
  /// these and forgets them).
  std::vector<ThreatOutput> pollReady(uint64_t now_ms);

  /// Returns the count of currently-active (in-window) folds.
  size_t activeCount() const { return slots_.size(); }

  /// Test helper — fold state for a key.
  std::optional<ThreatOutput> peek(
      const std::string& source_robot_id,
      uint8_t threat_type) const;

  /// Reset all state (test/restart use).
  void reset();

private:
  struct Slot {
    ThreatOutput agg;
    uint64_t     window_start_ms;
  };
  std::string makeKey(const std::string& robot_id, uint8_t type) const;

  double                                          dedup_window_s_;
  std::unordered_map<std::string, Slot>           slots_;
};

}  // namespace san_hub_orchestrator

#endif  // SAN_HUB_ORCHESTRATOR__THREAT_AGGREGATOR_HPP_
