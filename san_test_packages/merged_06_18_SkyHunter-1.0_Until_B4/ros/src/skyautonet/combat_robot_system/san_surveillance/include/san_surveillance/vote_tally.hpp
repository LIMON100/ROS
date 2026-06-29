// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// DCN-2026-026 C-3 — k-of-n 교전 합의 집계 (pure logic).
//
// HMAC-verified TargetConfirmation votes land here; FireSolution asks
// how many DISTINCT robots confirmed a given threat cluster within the
// agreement window. A vote binds to a cluster by bearing proximity
// (≤ bind_deg — same 15° convention as the C-1 threat clusters); the
// track_id keeps per-robot vote replacement stable across re-detections.
//
// No rclcpp — standalone testable.

#ifndef SAN_SURVEILLANCE__VOTE_TALLY_HPP_
#define SAN_SURVEILLANCE__VOTE_TALLY_HPP_

#include <cstdint>
#include <vector>

namespace san_surveillance
{

class VoteTally
{
public:
  explicit VoteTally(uint32_t window_ms = 1500, float bind_deg = 15.0f)
  : window_ms_(window_ms), bind_deg_(bind_deg) {}

  /// Record a verified vote. A newer vote from the same robot for the
  /// same track replaces the old one (latest fix wins, memory bounded
  /// by robots × tracks in the window).
  void record(
    uint32_t robot_id, uint32_t track_id, float bearing_deg,
    uint64_t now_ms);

  /// Distinct robots with a fresh (≤ window) vote bound (≤ bind_deg)
  /// to the cluster bearing. This is the k of k-of-n.
  uint8_t countFor(float cluster_bearing_deg, uint64_t now_ms) const;

  /// Drop expired votes (called opportunistically by record()).
  void prune(uint64_t now_ms);

  std::size_t sizeForTest() const {return votes_.size();}

private:
  struct Vote
  {
    uint32_t robot_id;
    uint32_t track_id;
    float bearing_deg;
    uint64_t t_ms;
  };

  uint32_t window_ms_;
  float bind_deg_;
  std::vector<Vote> votes_;
};

}  // namespace san_surveillance

#endif  // SAN_SURVEILLANCE__VOTE_TALLY_HPP_
