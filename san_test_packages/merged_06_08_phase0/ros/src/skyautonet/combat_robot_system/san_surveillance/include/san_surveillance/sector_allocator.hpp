// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — 360° Surveillance Sector Allocator per SDD-SWARM §8.
//
// Given an active set of robots (leader, hub, N followers), assigns
// each robot an angular sector of the 360° surveillance perimeter.
//
// PATCH 2026-05-13 (deep-dive review):
//   * Sectors can be emitted in either Heading OR World frame.
//   * Leader heading drives world-frame anchor (so sectors don't
//     spin when leader rotates).
//   * normalizeAngle / angularDifference moved to sector_frame.hpp
//     (single source of truth across the package).
//
// SDD §8.1 / §8.2 — Default 8-robot decomposition:
//   * Leader:   front  ±30°       (fixed, no pan-tilt)
//   * F1..F6:   front 240° split  (each 50°, 10° overlap w/ HFOV 60°)
//   * Hub UGV:  rear   ±90°       (sweep)
//
// Adapts to 4..8 robots; gap-fills on follower loss (§8.6.2);
// supports threat-focus mode (§8.6.1) and recon/defence/assault toggle
// (§8.6.3).
//
// Pure C++17, no rclcpp, standalone testable.

#ifndef SAN_SURVEILLANCE__SECTOR_ALLOCATOR_HPP_
#define SAN_SURVEILLANCE__SECTOR_ALLOCATOR_HPP_

#include <cstdint>
#include <optional>
#include <vector>

#include "san_surveillance/sector_frame.hpp"

namespace san_surveillance
{

/// A single angular sector assigned to one robot.
/// All angles in degrees, in the frame indicated by `frame`.
/// sector_start_deg is the CCW edge (smaller angle); sector_end_deg
/// is the CW edge (larger angle). For sectors crossing the ±180 wrap
/// (rear), end may be < start — call wrapsAround() to detect.
struct SectorAssignment
{
  uint32_t robot_id;
  float sector_start_deg;
  float sector_end_deg;
  uint8_t priority;              // 0=primary, 1=threat_focus, 2=overlap
  uint8_t mode_hint;             // 0=sweep, 1=track, 2=fixed
  SectorFrame frame = SectorFrame::Heading;   // ★ PATCH 2026-05-13

  /// True if this sector crosses the ±180° boundary (e.g. rear hub).
  bool wrapsAround() const {return sector_end_deg < sector_start_deg;}

  /// Width of the sector in degrees (always positive).
  float widthDeg() const
  {
    return wrapsAround() ?
           (360.0f - sector_start_deg + sector_end_deg) :
           (sector_end_deg - sector_start_deg);
  }

  /// Centre of the sector (handling wrap).
  float centreDeg() const
  {
    if (wrapsAround()) {
      return normalizeAngle(
        (sector_start_deg + sector_end_deg +
        360.0f) * 0.5f);
    }
    return (sector_start_deg + sector_end_deg) * 0.5f;
  }
};

/// Role classification used during sector allocation.
enum class RobotRole : uint8_t
{
  Leader = 0,      // fixed front 60°
  Hub    = 1,      // rear coverage
  Follower = 2,    // configurable sector
};

struct RobotInfo
{
  uint32_t robot_id;
  RobotRole role;
  bool alive = true;               // gap-fill drops dead robots

  /// World-frame yaw of this robot (degrees, ±180 wrap). Used only
  /// when allocator is asked to emit world-frame sectors so the node
  /// can transform back for the body-mounted pan-tilt.
  /// PATCH 2026-05-13.
  float yaw_world_deg = 0.0f;
};

/// Operating mode for the squadron — affects sector distribution.
enum class SurveillanceMode : uint8_t
{
  Recon    = 0,    // balanced 360° coverage
  Defence  = 1,    // balanced front/rear (§8.6.3)
  Assault  = 2,    // narrow forward sectors, wide rear (§8.6.3)
};

/// Inputs to the allocator.
struct AllocatorInput
{
  std::vector<RobotInfo> robots;
  SurveillanceMode mode = SurveillanceMode::Recon;

  /// Optional threat bearing. Frame must match `output_frame` (i.e.
  /// world-frame threat → world-frame sectors).
  // std::optional<float> threat_bearing_deg;
  std::vector<float> threat_bearings_deg;

  /// PATCH 2026-05-13: requested output frame.
  /// SectorFrame::Heading — sectors are relative to each robot's own
  ///   heading. The same numeric value means the same place in body
  ///   frame for every robot. Use when stopped (방어/경계 모드).
  /// SectorFrame::World — sectors are absolute. Every robot covers
  ///   the SAME absolute slice of the world. Use when driving
  ///   (공격 모드 — 절대 360° 분담 유지).
  SectorFrame output_frame = SectorFrame::Heading;

  /// Leader's world yaw (used only when output_frame == World as the
  /// anchor for the front sector). Default 0 = world +x.
  float leader_yaw_world_deg = 0.0f;
};

/// Compute sector assignments for all alive robots.
///
/// PATCH 2026-05-13: when output_frame == World, the sectors are
/// rotated by leader_yaw_world_deg so the "front sector" always
/// points where the leader was facing at allocation time. As the
/// leader subsequently rotates, the sectors stay anchored — every
/// robot keeps covering the same world slice via its world-frame
/// pan-tilt command.
std::vector<SectorAssignment> allocateSectors(const AllocatorInput & in);

}  // namespace san_surveillance

#endif  // SAN_SURVEILLANCE__SECTOR_ALLOCATOR_HPP_
