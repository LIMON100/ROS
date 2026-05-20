// SAN v1.5 — 360° Surveillance Sector Allocator per SDD-SWARM §8.
//
// Given an active set of robots (leader, hub, N followers), assigns
// each robot an angular sector of the 360° surveillance perimeter.
//
// SDD §8.1 / §8.2 — Default 8-robot decomposition:
//   * Leader:   front  ±30°       (fixed, no pan-tilt)
//   * F1..F6:   front 240° split  (each 50°, 10° overlap w/ HFOV 60°)
//   * Hub UGV:  rear   ±90°       (sweep)
//
// Adapts to 4..8 robots; gap-fills on follower loss (§8.6.2);
// supports threat-focus mode (§8.6.1) and recon/defence mode toggle
// (§8.6.3).
//
// Pure C++17, no rclcpp, standalone testable.

#ifndef SAN_SURVEILLANCE__SECTOR_ALLOCATOR_HPP_
#define SAN_SURVEILLANCE__SECTOR_ALLOCATOR_HPP_

#include <cstdint>
#include <optional>
#include <vector>

namespace san_surveillance {

/// A single angular sector assigned to one robot.
/// All angles in degrees, heading-frame ([-180, +180]).
/// sector_start_deg is the CCW edge (smaller angle); sector_end_deg
/// is the CW edge (larger angle). For sectors crossing the ±180 wrap
/// (rear), end may be < start — call wrapsAround() to detect.
struct SectorAssignment {
  uint32_t robot_id;
  float    sector_start_deg;
  float    sector_end_deg;
  uint8_t  priority;         // 0=primary, 1=threat_focus, 2=overlap
  uint8_t  mode_hint;        // 0=sweep, 1=track, 2=fixed

  /// True if this sector crosses the ±180° boundary (e.g. rear hub).
  bool wrapsAround() const { return sector_end_deg < sector_start_deg; }

  /// Width of the sector in degrees (always positive).
  float widthDeg() const {
    return wrapsAround()
        ? (360.0f - sector_start_deg + sector_end_deg)
        : (sector_end_deg - sector_start_deg);
  }
};

/// Role classification used during sector allocation.
enum class RobotRole : uint8_t {
  Leader = 0,      // fixed front 60°
  Hub    = 1,      // rear coverage
  Follower = 2,    // configurable sector
};

struct RobotInfo {
  uint32_t  robot_id;
  RobotRole role;
  bool      alive = true;          // gap-fill drops dead robots
};

/// Operating mode for the squadron — affects sector distribution.
enum class SurveillanceMode : uint8_t {
  Recon    = 0,    // balanced 360° coverage
  Defence  = 1,    // balanced front/rear (§8.6.3)
  Assault  = 2,    // narrow forward sectors, wide rear (§8.6.3)
};

/// Inputs to the allocator.
struct AllocatorInput {
  std::vector<RobotInfo> robots;
  SurveillanceMode       mode = SurveillanceMode::Recon;

  /// Optional threat bearing (degrees, heading-frame). When set,
  /// allocator concentrates ~3 followers near this direction per
  /// SDD §8.6.1 (Threat Sector Focus).
  std::optional<float>   threat_bearing_deg;
};

/// Compute sector assignments for all alive robots.
///
/// Algorithm:
///   1. Filter alive robots.
///   2. Leader → fixed front ±30°.
///   3. Hub    → rear ±90° (around ±180°).
///   4. Followers → split front 240° equally (range depends on count).
///   5. If threat_bearing set → re-prioritize 3 nearest followers
///      onto the same direction with PRIORITY_THREAT_FOCUS.
///   6. If mode=Assault → narrow front sectors (15-20°/follower),
///      wider rear; if Defence → equal 45°/follower.
///
/// Returns one SectorAssignment per alive robot. Order matches input
/// order of alive robots.
std::vector<SectorAssignment> allocateSectors(const AllocatorInput& in);

/// Compute the smallest absolute angular difference between two
/// headings, accounting for ±180 wrap. Result in [0, 180].
float angularDifference(float a_deg, float b_deg);

/// Normalize an angle into [-180, +180].
float normalizeAngle(float deg);

}  // namespace san_surveillance

#endif  // SAN_SURVEILLANCE__SECTOR_ALLOCATOR_HPP_
