// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — 9 Formation geometry implementations.
//
// PATCH 2026-05-13 (Formation deep-dive review):
//   * Box ring-out is now 4-way symmetric (was always pushing to
//     the same NE quadrant after the first 5 slots).
//   * Diamond ring-out adds all 4 corners at the SAME ring radius
//     before stepping out (was advancing radius mid-ring).
//   * Documented coordinate convention explicitly:
//       leader at origin, heading = +x, y+ = LEFT (ROS convention).

#include "san_formation/formations.hpp"

#include <array>
#include <cmath>
#include <random>

namespace san_formation
{

namespace
{

constexpr float DEG2RAD = 0.0174532925f;
constexpr float SQRT2_2 = 0.70710678f;   // sqrt(2)/2

// ─── Column: 1열 종대 (협로) ────────────────────────────────────────────
// Leader at origin, followers behind at y = 0, x = -k*d (k=1..N-1).
std::vector<SlotXY> column(size_t n, float d)
{
  std::vector<SlotXY> out;
  out.reserve(n);
  for (size_t k = 0; k < n; ++k) {
    out.push_back({-static_cast<float>(k) * d, 0.0f});
  }
  return out;
}

// ─── Line: 1열 횡대 (탐색) ─────────────────────────────────────────────
// Leader at origin, followers spread left/right alternating starting LEFT.
// PATCH 2026-05-13: start LEFT (y+) first to match ROS y+=left convention.
// k=0 leader, k=1 left (+y), k=2 right (-y), k=3 far left, k=4 far right...
std::vector<SlotXY> line(size_t n, float d)
{
  std::vector<SlotXY> out;
  out.reserve(n);
  out.push_back({0.0f, 0.0f});
  int offset = 1;
  bool left = true;                          // start LEFT (ROS y+ = left)
  while (out.size() < n) {
    const float y = left ? offset * d : -offset * d;
    out.push_back({0.0f, y});
    if (!left) {++offset;}
    left = !left;
  }
  return out;
}

// ─── V-Shape: 정찰 default (θ 표준 60°) ─────────────────────────────────
// Leader at origin, arms branching BACK (-x) at half-angle θ/2 on left
// (+y) and right (-y) alternately at spacing d.
std::vector<SlotXY> vShape(size_t n, float d, float theta_deg)
{
  std::vector<SlotXY> out;
  out.reserve(n);
  out.push_back({0.0f, 0.0f});                  // leader
  const float half = (theta_deg * 0.5f) * DEG2RAD;
  const float cos_h = std::cos(half);
  const float sin_h = std::sin(half);
  int level = 1;
  bool left = true;                              // start LEFT
  while (out.size() < n) {
    const float r = level * d;
    const float x = -r * cos_h;
    const float y = left ? r * sin_h : -r * sin_h;
    out.push_back({x, y});
    if (!left) {++level;}
    left = !left;
  }
  return out;
}

// ─── Diamond: 4 corners + center ─────────────────────────────────────────
// PATCH 2026-05-13: ring-out now fills ALL 4 corners at the same ring
// before stepping out. Previously k++ inside the loop advanced the
// radius for every corner, producing a spiral instead of concentric
// diamonds.
std::vector<SlotXY> diamond(size_t n, float d)
{
  const float h = d * SQRT2_2;
  std::vector<SlotXY> out = {
    {0.0f, 0.0f},       // 0: leader / center
    {h, h},             // 1: front-left (FL)   — x+, y+
    {h, -h},            // 2: front-right (FR)  — x+, y-
    {-h, h},            // 3: rear-left (RL)
    {-h, -h},           // 4: rear-right (RR)
  };
  // Concentric rings at r = 1.5d, 2.0d, 2.5d, ...
  // Each ring contributes up to 4 corners before stepping out.
  size_t ring = 0;
  while (out.size() < n) {
    const float r = (1.5f + 0.5f * static_cast<float>(ring)) * d * SQRT2_2;
    const std::array<SlotXY, 4> corners = {{
      {r, r}, {r, -r}, {-r, r}, {-r, -r},
    }};
    for (const auto & c : corners) {
      if (out.size() >= n) {break;}
      out.push_back(c);
    }
    ++ring;
  }
  out.resize(n);
  return out;
}

// ─── Echelon: 45° offset (좌/우) ─────────────────────────────────────────
std::vector<SlotXY> echelon(size_t n, float d, bool right)
{
  std::vector<SlotXY> out;
  out.reserve(n);
  const float h = d * SQRT2_2;
  for (size_t k = 0; k < n; ++k) {
    // y_local = +k*d/√2 (left)  OR  -k*d/√2 (right) — SDD §7.1 row 5
    // PATCH 2026-05-13: y+ = LEFT (ROS convention), so EchelonLeft
    // increments y POSITIVELY. Previously the sign was inverted.
    const float x = -static_cast<float>(k) * h;
    const float y = right ? -static_cast<float>(k) * h :
      static_cast<float>(k) * h;
    out.push_back({x, y});
  }
  return out;
}

// ─── Box: 정사각 + center ────────────────────────────────────────────────
// 5 slots: 4 corners (FL, FR, RL, RR) + 1 center (leader).
// PATCH 2026-05-13: ring-out is now 4-way symmetric. Previously every
// extra slot beyond 5 went to (+r, +r) only.
std::vector<SlotXY> box(size_t n, float d)
{
  std::vector<SlotXY> out = {
    {0.0f, 0.0f},                     // 0: leader (center)
    {d * 0.5f, d * 0.5f},             // 1: FL  (x+, y+)
    {d * 0.5f, -d * 0.5f},            // 2: FR
    {-d * 0.5f, d * 0.5f},            // 3: RL
    {-d * 0.5f, -d * 0.5f},           // 4: RR
  };
  size_t ring = 0;
  while (out.size() < n) {
    // Expand the box outward at radius (1.0 + 0.5*ring) * d.
    const float r = (1.0f + 0.5f * static_cast<float>(ring)) * d;
    const std::array<SlotXY, 4> corners = {{
      {r, r}, {r, -r}, {-r, r}, {-r, -r},
    }};
    for (const auto & c : corners) {
      if (out.size() >= n) {break;}
      out.push_back(c);
    }
    ++ring;
  }
  out.resize(n);
  return out;
}

// ─── Vee-Inverted: V형이 leader 후방으로 (매복) ────────────────────────
// Leader at front, V arms FORWARD (+x) instead of backward.
std::vector<SlotXY> veeInverted(size_t n, float d, float theta_deg)
{
  std::vector<SlotXY> out;
  out.reserve(n);
  out.push_back({0.0f, 0.0f});
  const float half = (theta_deg * 0.5f) * DEG2RAD;
  int level = 1;
  bool left = true;
  while (out.size() < n) {
    const float r = level * d;
    // Forward (+x) instead of backward
    const float x = r * std::cos(half);
    const float y = left ? r * std::sin(half) : -r * std::sin(half);
    out.push_back({x, y});
    if (!left) {++level;}
    left = !left;
  }
  return out;
}

// ─── Free-Spread: deterministic pseudo-random within radius d ──────────
std::vector<SlotXY> freeSpread(size_t n, float d)
{
  std::vector<SlotXY> out;
  out.reserve(n);
  out.push_back({0.0f, 0.0f});                  // leader anchor
  std::mt19937 rng(static_cast<uint32_t>(n));
  std::uniform_real_distribution<float> rdist(0.5f * d, d);
  std::uniform_real_distribution<float> adist(0.0f, 2.0f * 3.14159265f);
  while (out.size() < n) {
    const float r = rdist(rng);
    const float a = adist(rng);
    out.push_back({r * std::cos(a), r * std::sin(a)});
  }
  return out;
}

}  // namespace

// ─── Preset table per SDD §7.2 ──────────────────────────────────────────

Preset getPreset(uint8_t preset_id)
{
  switch (preset_id) {
    case PRESET_NARROW_PASSAGE: return {"narrow_passage", 3.0f, 40.0f};
    case PRESET_RECON_DEFENCE:  return {"recon_defence", 5.0f, 90.0f};
    case PRESET_WIDE_RECON:     return {"wide_recon", 7.0f, 120.0f};
    case PRESET_ASSAULT:        return {"assault", 15.0f, 60.0f};
    default:                    return {"", 0.0f, 0.0f};
  }
}

// ─── Dispatch ──────────────────────────────────────────────────────────

std::vector<SlotXY> generateSlots(
  Formation form, size_t n, float d, float theta_deg)
{
  if (n == 0) {return {};}
  switch (form) {
    case Formation::Column:        return column(n, d);
    case Formation::Line:          return line(n, d);
    case Formation::VShape:        return vShape(n, d, theta_deg);
    case Formation::Diamond:       return diamond(n, d);
    case Formation::EchelonLeft:   return echelon(n, d, /*right=*/ false);
    case Formation::EchelonRight:  return echelon(n, d, /*right=*/ true);
    case Formation::Box:           return box(n, d);
    case Formation::VeeInverted:   return veeInverted(n, d, theta_deg);
    case Formation::FreeSpread:    return freeSpread(n, d);
  }
  return column(n, d);    // unreachable
}

Formation fromMessageId(uint8_t id)
{
  if (id >= 1 && id <= 9) {return static_cast<Formation>(id);}
  return Formation::Column;
}

}  // namespace san_formation
