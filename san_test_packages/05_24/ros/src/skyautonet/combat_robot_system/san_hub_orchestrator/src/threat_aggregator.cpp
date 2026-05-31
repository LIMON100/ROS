// SAN v1.5 Phase 2-E Turn 8 — ThreatAggregator implementation.

#include "san_hub_orchestrator/threat_aggregator.hpp"

#include <algorithm>

namespace san_hub_orchestrator {

ThreatAggregator::ThreatAggregator(double dedup_window_s)
    : dedup_window_s_(dedup_window_s) {}

std::string ThreatAggregator::makeKey(
    const std::string& robot_id, uint8_t type) const {
  return robot_id + ":" + std::to_string(type);
}

bool ThreatAggregator::ingest(const ThreatInput& in) {
  const auto key = makeKey(in.source_robot_id, in.threat_type);
  auto it = slots_.find(key);

  // Window check — has the existing slot expired?
  const uint64_t window_ms =
      static_cast<uint64_t>(dedup_window_s_ * 1000.0);
  if (it != slots_.end() &&
      in.timestamp_ms >= it->second.window_start_ms + window_ms) {
    // Window elapsed before this ingest — drop the old slot and let
    // pollReady eventually retrieve it. Reset for the new alert.
    slots_.erase(it);
    it = slots_.end();
  }

  if (it == slots_.end()) {
    // New slot
    Slot s;
    s.agg.severity         = in.severity;
    s.agg.threat_type      = in.threat_type;
    s.agg.source_robot_id  = in.source_robot_id;
    s.agg.peer_id          = in.peer_id;
    s.agg.message_ko       = in.message_ko;
    s.agg.detail           = in.detail;
    s.agg.timestamp_ms     = in.timestamp_ms;
    s.agg.instance_count   = 1;
    s.window_start_ms      = in.timestamp_ms;
    slots_[key] = std::move(s);
    return true;
  }

  // Fold into existing slot
  it->second.agg.instance_count += 1;
  it->second.agg.timestamp_ms    = in.timestamp_ms;
  // Severity promotion: keep the highest seen
  if (in.severity > it->second.agg.severity) {
    it->second.agg.severity   = in.severity;
    it->second.agg.message_ko = in.message_ko;
    it->second.agg.detail     = in.detail;
    return true;        // promoted → caller should publish immediately
  }
  return false;
}

std::vector<ThreatOutput> ThreatAggregator::pollReady(uint64_t now_ms) {
  std::vector<ThreatOutput> ready;
  const uint64_t window_ms =
      static_cast<uint64_t>(dedup_window_s_ * 1000.0);

  for (auto it = slots_.begin(); it != slots_.end(); ) {
    if (now_ms >= it->second.window_start_ms + window_ms) {
      ready.push_back(it->second.agg);
      it = slots_.erase(it);
    } else {
      ++it;
    }
  }
  return ready;
}

std::optional<ThreatOutput> ThreatAggregator::peek(
    const std::string& source_robot_id, uint8_t threat_type) const {
  const auto it = slots_.find(makeKey(source_robot_id, threat_type));
  if (it == slots_.end()) return std::nullopt;
  return it->second.agg;
}

void ThreatAggregator::reset() {
  slots_.clear();
}

}  // namespace san_hub_orchestrator
