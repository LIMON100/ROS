// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Sector allocator implementation.
//
// PATCH 2026-05-13: world-frame sector support + leader-yaw anchor.

#include "san_surveillance/sector_allocator.hpp"

#include <algorithm>
#include <cmath>

namespace san_surveillance
{

namespace
{

// Priority codes — match SurveillanceSectorAssignment.msg
constexpr uint8_t PRI_PRIMARY = 0;
constexpr uint8_t PRI_THREAT_FOCUS = 1;
constexpr uint8_t PRI_OVERLAP = 2;

// Mode hint codes — match SurveillanceSectorAssignment.msg
constexpr uint8_t MODE_SWEEP = 0;
constexpr uint8_t MODE_TRACK = 1;
constexpr uint8_t MODE_FIXED = 2;

// SDD §8.2 — Leader fixed front ±30° (Pan-tilt 없음)
constexpr float LEADER_HALF_DEG = 30.0f;

// SDD §8.2 — Hub rear ±90° (centred on 180°)
constexpr float HUB_HALF_DEG = 90.0f;

// SDD §8.6.1 — Threat focus: up to 3 followers concentrated
constexpr size_t THREAT_FOLLOWER_COUNT = 3;
constexpr float THREAT_SECTOR_HALF_DEG = 25.0f;   // each ±25° around bearing

// DCN-2026-026 C-1 — with 2 concurrent threat clusters the 3-follower
// budget splits by proximity, at most this many per cluster (2 + 1).
constexpr size_t THREAT_PER_CLUSTER_MULTI = 2;

/// Build a follower sector centred on `centre_deg` with width `width_deg`.
SectorAssignment makeFollowerSector(
  uint32_t robot_id, float centre_deg, float width_deg,
  SectorFrame frame,
  uint8_t priority = PRI_PRIMARY)
{
  SectorAssignment s;
  s.robot_id = robot_id;
  s.sector_start_deg = normalizeAngle(centre_deg - width_deg * 0.5f);
  s.sector_end_deg = normalizeAngle(centre_deg + width_deg * 0.5f);
  s.priority = priority;
  s.mode_hint = MODE_SWEEP;
  s.frame = frame;
  return s;
}

/// Determine per-follower sector width by mode.
float sectorWidthForMode(SurveillanceMode mode, size_t n_followers)
{
  if (n_followers == 0) {return 60.0f;}
  switch (mode) {
    case SurveillanceMode::Defence:
      return 45.0f;
    case SurveillanceMode::Assault:
      return n_followers >= 4 ? 18.0f : 25.0f;
    case SurveillanceMode::Recon:
    default:
      return 240.0f / static_cast<float>(n_followers);
  }
}

/// Symmetric split of the front 240° band (excluding the leader's ±30°)
/// for the given followers — left gets the extra one when N is odd.
/// Factored out so threat-focus can redistribute the band among the
/// non-focused survivors (DCN-2026-026 C-1).
std::vector<SectorAssignment> splitFrontBand(
  const std::vector<RobotInfo> & followers,
  SurveillanceMode mode, float front_anchor, SectorFrame frame)
{
  std::vector<SectorAssignment> out;
  const size_t n = followers.size();
  if (n == 0) {return out;}
  const float per_width = sectorWidthForMode(mode, n);
  const size_t n_right = n / 2;
  const size_t n_left = n - n_right;

  for (size_t k = 0; k < n_left; ++k) {
    const float relative_centre =
      -(LEADER_HALF_DEG + per_width * 0.5f +
      static_cast<float>(k) * per_width);
    out.push_back(
      makeFollowerSector(
        followers[k].robot_id,
        normalizeAngle(front_anchor + relative_centre), per_width, frame));
  }
  for (size_t k = 0; k < n_right; ++k) {
    const float relative_centre =
      +(LEADER_HALF_DEG + per_width * 0.5f +
      static_cast<float>(k) * per_width);
    out.push_back(
      makeFollowerSector(
        followers[n_left + k].robot_id,
        normalizeAngle(front_anchor + relative_centre), per_width, frame));
  }
  return out;
}

}  // namespace

std::vector<float> clusterThreatBearings(
  const std::vector<float> & bearings_deg,
  float merge_deg,
  std::size_t max_clusters)
{
  std::vector<float> centres;
  std::vector<std::size_t> counts;
  for (const float raw : bearings_deg) {
    const float b = normalizeAngle(raw);
    // Merge into the nearest existing cluster within merge_deg.
    int best = -1;
    float best_d = merge_deg;
    for (std::size_t c = 0; c < centres.size(); ++c) {
      const float d = angularDifference(centres[c], b);
      if (d <= best_d) {best = static_cast<int>(c); best_d = d;}
    }
    if (best >= 0) {
      // Incremental circular mean: walk the centre toward the new
      // member by the signed shortest-path fraction.
      auto & centre = centres[static_cast<std::size_t>(best)];
      auto & count = counts[static_cast<std::size_t>(best)];
      const float signed_diff = normalizeAngle(b - centre);
      centre = normalizeAngle(
        centre + signed_diff / static_cast<float>(count + 1));
      ++count;
    } else if (centres.size() < max_clusters) {
      centres.push_back(b);
      counts.push_back(1);
    }
    // Beyond the cluster cap: bearing dropped (DCN-2026-026 C-1 keeps
    // at most kMaxThreatClusters concurrent threats).
  }
  return centres;
}

std::vector<SectorAssignment> allocateSectors(const AllocatorInput & in)
{
  std::vector<SectorAssignment> out;

  // 1. Filter alive robots; partition by role.
  std::vector<RobotInfo> leaders, hubs, followers;
  for (const auto & r : in.robots) {
    if (!r.alive) {continue;}
    switch (r.role) {
      case RobotRole::Leader:   leaders.push_back(r);   break;
      case RobotRole::Hub:      hubs.push_back(r);      break;
      case RobotRole::Follower: followers.push_back(r); break;
    }
  }

  // PATCH 2026-05-13: front-anchor centre.
  // Heading frame → "front" is body +x (0°).
  // World   frame → "front" rotates to leader's world yaw at allocation
  //                  time, so sectors stay world-anchored as leader rotates.
  const float front_anchor =
    (in.output_frame == SectorFrame::World) ?
    in.leader_yaw_world_deg :
    0.0f;

  // 2. Leader — fixed front ±30° around the front anchor.
  for (const auto & l : leaders) {
    SectorAssignment s;
    s.robot_id = l.robot_id;
    s.sector_start_deg = normalizeAngle(front_anchor - LEADER_HALF_DEG);
    s.sector_end_deg = normalizeAngle(front_anchor + LEADER_HALF_DEG);
    s.priority = PRI_PRIMARY;
    s.mode_hint = MODE_FIXED;
    s.frame = in.output_frame;
    out.push_back(s);
  }

  // 3. Hub — rear ±90° (centred on front + 180°).
  const float rear_centre = normalizeAngle(front_anchor + 180.0f);
  for (const auto & h : hubs) {
    SectorAssignment s;
    s.robot_id = h.robot_id;
    s.sector_start_deg = normalizeAngle(rear_centre - HUB_HALF_DEG);
    s.sector_end_deg = normalizeAngle(rear_centre + HUB_HALF_DEG);
    s.priority = PRI_PRIMARY;
    s.mode_hint = MODE_SWEEP;
    s.frame = in.output_frame;
    out.push_back(s);
  }

  // 4. Followers — provisional symmetric split of the front 240° band
  //    (excluding leader's ±30°). Kept local: threat focus below may
  //    re-point some of them, and the survivors are then
  //    redistributed over the band (DCN-2026-026 C-1).
  const size_t n = followers.size();
  std::vector<SectorAssignment> provisional =
    splitFrontBand(followers, in.mode, front_anchor, in.output_frame);

  // 5. Threat focus — SDD §8.6.1 + DCN-2026-026 C-1 multi-threat.
  //    Raw bearings (legacy scalar folded in) merge into ≤ 2 clusters;
  //    only followers are re-pointed: 3 for a single cluster (legacy,
  //    TST A7/A10), ≤ 2 per cluster / ≤ 3 total for two clusters.
  std::vector<float> raw_bearings = in.threat_bearings_deg;
  if (in.threat_bearing_deg) {
    raw_bearings.push_back(*in.threat_bearing_deg);
  }
  const std::vector<float> clusters = clusterThreatBearings(raw_bearings);

  if (clusters.empty() || n == 0) {
    out.insert(out.end(), provisional.begin(), provisional.end());
    return out;
  }

  // 5a. Pick focused followers per cluster — greedy by angular
  //     proximity of each follower's provisional sector centre.
  const size_t per_cluster_cap =
    (clusters.size() >= 2) ? THREAT_PER_CLUSTER_MULTI : THREAT_FOLLOWER_COUNT;
  size_t budget = std::min(THREAT_FOLLOWER_COUNT, n);
  std::vector<int> focus_cluster(n, -1);     // follower idx → cluster idx
  for (size_t c = 0; c < clusters.size() && budget > 0; ++c) {
    struct Scored { size_t idx; float dist; };
    std::vector<Scored> scored;
    for (size_t i = 0; i < n; ++i) {
      if (focus_cluster[i] >= 0) {continue;}
      scored.push_back(
        {i, angularDifference(provisional[i].centreDeg(), clusters[c])});
    }
    std::sort(
      scored.begin(), scored.end(),
      [](const Scored & a, const Scored & b) {
        return a.dist < b.dist;
      });
    const size_t take = std::min({per_cluster_cap, budget, scored.size()});
    for (size_t k = 0; k < take; ++k) {
      focus_cluster[scored[k].idx] = static_cast<int>(c);
      --budget;
    }
  }

  // 5b. Survivors redistribute the front band among themselves so the
  //     perimeter stays covered (union ≥ 80%, TST A10/A11).
  std::vector<RobotInfo> survivors;
  for (size_t i = 0; i < n; ++i) {
    if (focus_cluster[i] < 0) {survivors.push_back(followers[i]);}
  }
  const std::vector<SectorAssignment> redistributed =
    splitFrontBand(survivors, in.mode, front_anchor, in.output_frame);
  out.insert(out.end(), redistributed.begin(), redistributed.end());

  // 5c. Focused followers get the threat sector of their cluster.
  for (size_t i = 0; i < n; ++i) {
    if (focus_cluster[i] < 0) {continue;}
    const float bearing = clusters[static_cast<size_t>(focus_cluster[i])];
    SectorAssignment s;
    s.robot_id = followers[i].robot_id;
    s.sector_start_deg = normalizeAngle(bearing - THREAT_SECTOR_HALF_DEG);
    s.sector_end_deg = normalizeAngle(bearing + THREAT_SECTOR_HALF_DEG);
    s.priority = PRI_THREAT_FOCUS;
    s.mode_hint = MODE_TRACK;
    s.frame = in.output_frame;
    out.push_back(s);
  }

  return out;
}

}  // namespace san_surveillance
