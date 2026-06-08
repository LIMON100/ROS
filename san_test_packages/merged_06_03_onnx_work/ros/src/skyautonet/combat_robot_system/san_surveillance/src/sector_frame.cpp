// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Surveillance sector frame helpers — implementation.

#include "san_surveillance/sector_frame.hpp"

#include <cmath>

namespace san_surveillance
{

const char * toString(SectorFrame f)
{
  switch (f) {
    case SectorFrame::Heading: return "Heading";
    case SectorFrame::World:   return "World";
  }
  return "?";
}

// ─── Angle utilities ────────────────────────────────────────────────────

float normalizeAngle(float deg)
{
  while (deg > 180.0f) {deg -= 360.0f;}
  while (deg < -180.0f) {deg += 360.0f;}
  return deg;
}

float angularDifference(float a_deg, float b_deg)
{
  const float d = std::fabs(normalizeAngle(a_deg - b_deg));
  return d > 180.0f ? 360.0f - d : d;
}

// ─── DriveClassifier ────────────────────────────────────────────────────

DriveClassifier::DriveClassifier()
: cfg_(Config{}) {}
DriveClassifier::DriveClassifier(Config cfg)
: cfg_(cfg) {}

DriveClassifier::State DriveClassifier::update(const MotionSnapshot & motion)
{
  const bool above_enter =
    motion.linear_speed_mps >= cfg_.enter_drive_mps ||
    motion.angular_speed_dps >= cfg_.enter_drive_dps;
  const bool below_exit =
    motion.linear_speed_mps < cfg_.exit_drive_mps &&
    motion.angular_speed_dps < cfg_.exit_drive_dps;

  if (state_ == State::Patrol) {
    if (above_enter) {
      state_ = State::Drive;
      ++transitions_;
    }
  } else {  // Drive
    if (below_exit) {
      state_ = State::Patrol;
      ++transitions_;
    }
  }
  return state_;
}

}  // namespace san_surveillance
