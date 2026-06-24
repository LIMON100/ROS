// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// DCN-2026-026 C-3 — k-of-n 교전 합의 집계 impl.

#include "san_surveillance/vote_tally.hpp"

#include <algorithm>

#include "san_surveillance/sector_frame.hpp"

namespace san_surveillance
{

void VoteTally::record(
  uint32_t robot_id, uint32_t track_id, float bearing_deg, uint64_t now_ms)
{
  prune(now_ms);
  for (auto & v : votes_) {
    if (v.robot_id == robot_id && v.track_id == track_id) {
      v.bearing_deg = bearing_deg;       // latest fix wins
      v.t_ms = now_ms;
      return;
    }
  }
  votes_.push_back(Vote{robot_id, track_id, bearing_deg, now_ms});
}

uint8_t VoteTally::countFor(
  float cluster_bearing_deg, uint64_t now_ms) const
{
  // Distinct robots — a robot voting for two tracks near the same
  // cluster still counts once (k-of-n is per-ROBOT cross-confirmation).
  std::vector<uint32_t> seen;
  for (const auto & v : votes_) {
    if (now_ms - v.t_ms > window_ms_) {continue;}
    if (angularDifference(v.bearing_deg, cluster_bearing_deg) > bind_deg_) {
      continue;
    }
    if (std::find(seen.begin(), seen.end(), v.robot_id) == seen.end()) {
      seen.push_back(v.robot_id);
    }
  }
  return static_cast<uint8_t>(std::min<std::size_t>(seen.size(), 255));
}

void VoteTally::prune(uint64_t now_ms)
{
  votes_.erase(
    std::remove_if(
      votes_.begin(), votes_.end(),
      [&](const Vote & v) {return now_ms - v.t_ms > window_ms_;}),
    votes_.end());
}

}  // namespace san_surveillance
