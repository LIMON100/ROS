// SAN v1.5 — Sector allocator implementation.
//
// Algorithms align with SDD-SWARM §8.1, §8.2, §8.6 and SDD-SUR §3.

#include "san_surveillance/sector_allocator.hpp"

#include <algorithm>
#include <cmath>

namespace san_surveillance {

namespace {

// Priority codes — match SurveillanceSectorAssignment.msg
constexpr uint8_t PRI_PRIMARY      = 0;
constexpr uint8_t PRI_THREAT_FOCUS = 1;
constexpr uint8_t PRI_OVERLAP      = 2;

// Mode hint codes — match SurveillanceSectorAssignment.msg
constexpr uint8_t MODE_SWEEP = 0;
constexpr uint8_t MODE_TRACK = 1;
constexpr uint8_t MODE_FIXED = 2;

// SDD §8.2 — Leader fixed front ±30° (Pan-tilt 없음)
constexpr float LEADER_HALF_DEG = 30.0f;

// SDD §8.2 — Hub rear ±90° (centred on 180°)
constexpr float HUB_HALF_DEG    = 90.0f;

// SDD §8.6.1 — Threat focus: up to 3 followers concentrated
constexpr size_t THREAT_FOLLOWER_COUNT = 3;
constexpr float  THREAT_SECTOR_HALF_DEG = 25.0f;  // each ±25° around bearing

}  // namespace

float normalizeAngle(float deg) {
  while (deg >  180.0f) deg -= 360.0f;
  while (deg < -180.0f) deg += 360.0f;
  return deg;
}

float angularDifference(float a_deg, float b_deg) {
  float d = std::fabs(normalizeAngle(a_deg - b_deg));
  return d > 180.0f ? 360.0f - d : d;
}

namespace {

/// Build a follower sector centred on `centre_deg` with width `width_deg`.
SectorAssignment makeFollowerSector(
    uint32_t robot_id, float centre_deg, float width_deg,
    uint8_t priority = PRI_PRIMARY) {
  SectorAssignment s;
  s.robot_id        = robot_id;
  s.sector_start_deg = normalizeAngle(centre_deg - width_deg * 0.5f);
  s.sector_end_deg   = normalizeAngle(centre_deg + width_deg * 0.5f);
  s.priority        = priority;
  s.mode_hint       = MODE_SWEEP;
  return s;
}

/// Determine per-follower sector width by mode.
/// Recon:  balanced ~240/N across N followers
/// Defence: equal 45°/follower
/// Assault: narrow front 15-20°/follower
float sectorWidthForMode(SurveillanceMode mode, size_t n_followers) {
  if (n_followers == 0) return 60.0f;
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

std::vector<SectorAssignment> allocateSectors(const AllocatorInput& in) {
  std::vector<SectorAssignment> out;

  // 1. Filter alive robots; partition by role.
  std::vector<RobotInfo> leaders, hubs, followers;
  for (const auto& r : in.robots) {
    if (!r.alive) continue;
    switch (r.role) {
      case RobotRole::Leader:   leaders.push_back(r);   break;
      case RobotRole::Hub:      hubs.push_back(r);      break;
      case RobotRole::Follower: followers.push_back(r); break;
    }
  }

  // 2. Leader — fixed front ±30°. Mode hint: FIXED (no pan-tilt).
  for (const auto& l : leaders) {
    SectorAssignment s;
    s.robot_id        = l.robot_id;
    s.sector_start_deg = -LEADER_HALF_DEG;
    s.sector_end_deg   = +LEADER_HALF_DEG;
    s.priority        = PRI_PRIMARY;
    s.mode_hint       = MODE_FIXED;
    out.push_back(s);
  }

  // 3. Hub — rear ±90° (centred on 180°). Wraps around ±180.
  for (const auto& h : hubs) {
    SectorAssignment s;
    s.robot_id        = h.robot_id;
    s.sector_start_deg = +180.0f - HUB_HALF_DEG;   // 90.0
    s.sector_end_deg   = -180.0f + HUB_HALF_DEG;   // -90.0 (wraps)
    s.priority        = PRI_PRIMARY;
    s.mode_hint       = MODE_SWEEP;
    out.push_back(s);
  }

  // 4. Followers — split front 240° (-120° to +120°), excluding the
  //    leader's ±30° centre band.
  //
  // SDD §8.2: 6 followers split into 3 left-of-leader + 3 right-of-leader,
  //   skipping the leader band:
  //     left:  -55°, -105°, -155°   (centred sectors of width 50°)
  //     right: +55°, +105°, +155°
  //
  // We generalise: distribute followers symmetrically around the
  // leader band. Width per follower depends on mode.

  const size_t n = followers.size();
  if (n > 0) {
    const float per_width = sectorWidthForMode(in.mode, n);

    // Place followers symmetrically left/right of leader, starting
    // just outside ±30° and stepping outward.
    // For N followers, half go left (CCW, negative bearing), half go
    // right (CW, positive bearing). If N odd, extra one goes left.
    size_t n_right = n / 2;
    size_t n_left  = n - n_right;

    for (size_t k = 0; k < n_left; ++k) {
      // Centre: -30° - (per_width/2) - k*per_width
      const float c = -(LEADER_HALF_DEG + per_width * 0.5f
                        + static_cast<float>(k) * per_width);
      out.push_back(makeFollowerSector(
          followers[k].robot_id, c, per_width));
    }
    for (size_t k = 0; k < n_right; ++k) {
      const float c = +(LEADER_HALF_DEG + per_width * 0.5f
                        + static_cast<float>(k) * per_width);
      out.push_back(makeFollowerSector(
          followers[n_left + k].robot_id, c, per_width));
    }
  }

  // 5. Threat focus — re-prioritize up to 3 followers nearest to
  //    threat_bearing onto the threat sector.
  if (in.threat_bearing_deg && n > 0) {
    const float bearing = *in.threat_bearing_deg;
    // Score followers by angular distance from their current sector
    // centre to the threat bearing.
    struct Scored { size_t idx; float dist; };
    std::vector<Scored> scored;
    scored.reserve(out.size());
    for (size_t i = 0; i < out.size(); ++i) {
      // Skip leader (FIXED, can't track) and hub (rear) for threat focus.
      // Find the original robot to check role.
      bool is_follower = false;
      for (const auto& f : followers) {
        if (f.robot_id == out[i].robot_id) { is_follower = true; break; }
      }
      if (!is_follower) continue;

      const float centre = out[i].wrapsAround()
          ? normalizeAngle((out[i].sector_start_deg
                            + out[i].sector_end_deg + 360.0f) * 0.5f)
          : (out[i].sector_start_deg + out[i].sector_end_deg) * 0.5f;
      scored.push_back({i, angularDifference(centre, bearing)});
    }
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) {
                return a.dist < b.dist;
              });

    const size_t focus_n = std::min(THREAT_FOLLOWER_COUNT, scored.size());
    for (size_t k = 0; k < focus_n; ++k) {
      auto& s = out[scored[k].idx];
      // Concentrate sector around bearing
      s.sector_start_deg = normalizeAngle(
          bearing - THREAT_SECTOR_HALF_DEG);
      s.sector_end_deg   = normalizeAngle(
          bearing + THREAT_SECTOR_HALF_DEG);
      s.priority        = PRI_THREAT_FOCUS;
      s.mode_hint       = MODE_TRACK;
    }
  }

  return out;
}

}  // namespace san_surveillance
