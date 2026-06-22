// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — 9 Formation slot generation per SDD-SWARM §7.1.
//
// For a given formation, number of robots N, preset spacing d, and
// spread angle theta, generate N (x, y) slot positions in the LEADER
// LOCAL FRAME (leader at origin, heading +x).
//
// Pure C++17, no rclcpp, fully standalone testable.

#ifndef SAN_FORMATION__FORMATIONS_HPP_
#define SAN_FORMATION__FORMATIONS_HPP_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace san_formation
{

/// 9 formation types per SDD-SWARM §7.1.
/// Numeric values match FormationStatus.msg / SlotAssignment.msg
/// formation_id field.
enum class Formation : uint8_t
{
  Column        = 1,   // 1열 종대
  Line          = 2,   // 1열 횡대
  VShape        = 3,   // V형 (정찰 default)
  Diamond       = 4,   // 4 corners 전방향 경계
  EchelonLeft   = 5,   // 45° 좌측
  EchelonRight  = 6,   // 45° 우측
  Box           = 7,   // 정사각 + center
  VeeInverted   = 8,   // 매복 (Leader 후방)
  FreeSpread    = 9,   // 자유 산개
  Encircle      = 10,
};

/// 4 operational presets per SDD-SWARM §7.2.
struct Preset
{
  std::string name;
  float spacing_d_m;
  float spread_theta_deg;
};

constexpr uint8_t PRESET_NARROW_PASSAGE = 1;
constexpr uint8_t PRESET_RECON_DEFENCE = 2;
constexpr uint8_t PRESET_WIDE_RECON = 3;
constexpr uint8_t PRESET_ASSAULT = 4;

/// Look up a preset by ID. Returns std::nullopt-equivalent (empty name) on unknown ID.
Preset getPreset(uint8_t preset_id);

/// (x, y) in metres, leader's local frame (heading +x).
struct SlotXY
{
  float x;
  float y;
};

/// Generate N slot positions for the given formation/preset combo.
///
/// Args:
///   form    — Formation enum
///   n       — number of robots (incl. leader). Must be >= 2 for
///              VShape, EchelonLeft/Right, VeeInverted (which require
///              left/right arms). Box requires exactly 5 slots (4
///              corners + center) and rounds up if n < 5.
///   d_m     — spacing in metres (3.0/5.0/7.0/15.0 typical)
///   theta_deg — spread angle for V-shaped formations (40/60/90/120)
///
/// Returns: vector<SlotXY> length n. Index 0 is the LEADER slot
/// (origin for most formations).
///
/// Determinism: same input → same output (no randomness, even
/// FreeSpread uses a fixed seed of N for reproducibility in tests).
std::vector<SlotXY> generateSlots(
  Formation form,
  size_t n,
  float d_m,
  float theta_deg);

/// Convert formation enum to its 8-bit message-field id (1..9).
inline uint8_t toMessageId(Formation f)
{
  return static_cast<uint8_t>(f);
}

/// Convert message id (1..9) back to Formation. Returns
/// Formation::Column for unknown ids (safe fallback).
Formation fromMessageId(uint8_t id);

}  // namespace san_formation

#endif  // SAN_FORMATION__FORMATIONS_HPP_
