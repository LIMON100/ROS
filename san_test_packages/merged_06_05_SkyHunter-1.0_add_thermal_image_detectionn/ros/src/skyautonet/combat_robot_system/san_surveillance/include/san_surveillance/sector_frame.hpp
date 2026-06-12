// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Surveillance sector frame helpers (pure C++17, no ROS).
//
// PATCH 2026-05-13 (Surveillance deep-dive review):
//   Adds the missing piece for "주행 중 360° 감시 유지" (continuous
//   360° coverage while driving).
//
// Problem with the existing implementation:
//   sector_allocator always emits sectors in the robot's HEADING frame
//   (e.g. "follower covers +55°..+105° relative to my heading"). When
//   the robot rotates or drives along a curved path, EVERY robot's
//   sector rotates with it — the squadron's absolute 360° coverage
//   collapses to "always look in the same direction relative to body".
//
// Operational requirement (사업수요신청서 §3 운용 개념 나):
//   * 방어 모드 (정지) — 진지 방어 → heading-frame OK (로봇 정지)
//   * 공격 모드 (주행) — 보병보다 선행 이동 → 절대 world-frame 분담 필요
//
// Solution: introduce SectorFrame { Heading, World } and let the
// allocator emit world-frame sectors during drive. The node then
// transforms to heading-frame just before publishing PanTiltCommand
// (since the pan-tilt hardware is body-mounted).
//
// Pure C++17, no rclcpp, gtest-friendly.

#ifndef SAN_SURVEILLANCE__SECTOR_FRAME_HPP_
#define SAN_SURVEILLANCE__SECTOR_FRAME_HPP_

#include <cstdint>

namespace san_surveillance
{

/// Frame in which sector boundaries are expressed.
enum class SectorFrame : uint8_t
{
  Heading = 0,   // relative to robot heading (default; 정지 / 경계모드)
  World   = 1,   // absolute heading-frame (true north etc; 주행 중)
};

const char * toString(SectorFrame f);

// ─── Angle utilities ────────────────────────────────────────────────────
// (Public so node + allocator + tests share one implementation.)

/// Normalize to (-180, +180].
float normalizeAngle(float deg);

/// Smallest absolute angular difference, in [0, 180].
float angularDifference(float a_deg, float b_deg);

// ─── Frame transforms ───────────────────────────────────────────────────

/// World-frame bearing → heading-frame bearing.
/// If a robot at world yaw `robot_yaw_deg` wants to look at world
/// bearing `world_deg`, it must point its pan-tilt at this many
/// degrees in its own body frame.
///
///   heading_bearing = normalizeAngle(world_bearing - robot_yaw)
inline float worldToHeading(
  float world_bearing_deg,
  float robot_yaw_deg)
{
  return normalizeAngle(world_bearing_deg - robot_yaw_deg);
}

/// Heading-frame bearing → world-frame bearing (inverse of above).
inline float headingToWorld(
  float heading_bearing_deg,
  float robot_yaw_deg)
{
  return normalizeAngle(heading_bearing_deg + robot_yaw_deg);
}

// ─── Drive/Patrol classifier ────────────────────────────────────────────

/// Inputs to the drive-vs-patrol classifier.
struct MotionSnapshot
{
  float linear_speed_mps = 0.0f;
  float angular_speed_dps = 0.0f;
};

/// Hysteresis-based classifier.
///
/// State machine:
///   PATROL ──(speed ≥ enter_drive_mps OR yaw_rate ≥ enter_drive_dps)─→ DRIVE
///   DRIVE  ──(speed < exit_drive_mps  AND yaw_rate < exit_drive_dps)─→ PATROL
///
/// The hysteresis prevents frame-flapping when the robot creeps
/// (speed oscillates near the threshold). Defaults: enter @ 0.3 m/s
/// or 5°/s (typical creep speed of tracked UGV); exit @ 0.1 m/s and
/// 2°/s (well below creep so PATROL doesn't latch).
class DriveClassifier
{
public:
  enum class State : uint8_t
  {
    Patrol = 0,    // stationary or near-stationary → heading-frame
    Drive  = 1,    // moving / rotating              → world-frame
  };

  struct Config
  {
    float enter_drive_mps = 0.3f;
    float enter_drive_dps = 5.0f;
    float exit_drive_mps = 0.1f;
    float exit_drive_dps = 2.0f;
  };

  explicit DriveClassifier(Config cfg);
  DriveClassifier();

  /// Push a motion sample. Returns the (possibly updated) state.
  State update(const MotionSnapshot & motion);

  State state() const {return state_;}

  /// Recommended sector frame for the current state.
  SectorFrame recommendedFrame() const
  {
    return (state_ == State::Drive) ? SectorFrame::World :
           SectorFrame::Heading;
  }

  uint64_t transitionCount() const {return transitions_;}

private:
  Config cfg_;
  State state_ = State::Patrol;
  uint64_t transitions_ = 0;
};

}  // namespace san_surveillance

#endif  // SAN_SURVEILLANCE__SECTOR_FRAME_HPP_
