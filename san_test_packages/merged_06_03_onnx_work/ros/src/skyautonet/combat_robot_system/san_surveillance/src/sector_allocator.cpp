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

}  // namespace

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

  // 4. Followers — split front 240° (excluding leader's ±30° band),
  //    placed symmetrically around the front anchor.
  const size_t n = followers.size();
  if (n > 0) {
    const float per_width = sectorWidthForMode(in.mode, n);

    // Distribute followers symmetrically:
    //   left  (CCW from front): n_left  followers
    //   right (CW from front):  n_right followers
    // If N odd, extra one goes left.
    size_t n_right = n / 2;
    size_t n_left = n - n_right;

    for (size_t k = 0; k < n_left; ++k) {
      const float relative_centre =
        -(LEADER_HALF_DEG + per_width * 0.5f +
        static_cast<float>(k) * per_width);
      const float absolute_centre = normalizeAngle(
        front_anchor + relative_centre);
      out.push_back(
        makeFollowerSector(
          followers[k].robot_id, absolute_centre, per_width, in.output_frame));
    }
    for (size_t k = 0; k < n_right; ++k) {
      const float relative_centre =
        +(LEADER_HALF_DEG + per_width * 0.5f +
        static_cast<float>(k) * per_width);
      const float absolute_centre = normalizeAngle(
        front_anchor + relative_centre);
      out.push_back(
        makeFollowerSector(
          followers[n_left + k].robot_id, absolute_centre, per_width,
          in.output_frame));
    }
  }

  // 5. Threat focus — re-prioritize up to 3 followers nearest the
  //    threat bearing onto the threat sector.
  if (in.threat_bearing_deg && n > 0) {
    const float bearing = *in.threat_bearing_deg;
    struct Scored { size_t idx; float dist; };
    std::vector<Scored> scored;
    scored.reserve(out.size());
    for (size_t i = 0; i < out.size(); ++i) {
      bool is_follower = false;
      for (const auto & f : followers) {
        if (f.robot_id == out[i].robot_id) {is_follower = true; break;}
      }
      if (!is_follower) {continue;}
      scored.push_back({i, angularDifference(out[i].centreDeg(), bearing)});
    }
    std::sort(
      scored.begin(), scored.end(),
      [](const Scored & a, const Scored & b) {
        return a.dist < b.dist;
      });

    const size_t focus_n = std::min(THREAT_FOLLOWER_COUNT, scored.size());
    for (size_t k = 0; k < focus_n; ++k) {
      auto & s = out[scored[k].idx];
      s.sector_start_deg = normalizeAngle(
        bearing - THREAT_SECTOR_HALF_DEG);
      s.sector_end_deg = normalizeAngle(
        bearing + THREAT_SECTOR_HALF_DEG);
      s.priority = PRI_THREAT_FOCUS;
      s.mode_hint = MODE_TRACK;
      // frame preserved from input
    }
  }

  return out;
}

}  // namespace san_surveillance
