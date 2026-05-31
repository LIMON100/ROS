// SAN v1.5 — 9 Formation geometry implementations.

#include "san_formation/formations.hpp"

#include <cmath>
#include <random>

namespace san_formation {

namespace {

constexpr float DEG2RAD = 0.0174532925f;
constexpr float SQRT2_2 = 0.70710678f;   // sqrt(2)/2 ≈ 1/sqrt(2)

// ─── Column: 1열 종대 (협로) ────────────────────────────────────────────
// Leader at origin, followers behind at y = 0, x = -k*d (k=1..N-1).
std::vector<SlotXY> column(size_t n, float d) {
  std::vector<SlotXY> out;
  out.reserve(n);
  for (size_t k = 0; k < n; ++k) {
    out.push_back({-static_cast<float>(k) * d, 0.0f});
  }
  return out;
}

// ─── Line: 1열 횡대 (탐색) ─────────────────────────────────────────────
// Leader at origin, followers spread left/right alternating.
// k=0 leader, k=1 right, k=2 left, k=3 far right, k=4 far left, ...
std::vector<SlotXY> line(size_t n, float d) {
  std::vector<SlotXY> out;
  out.reserve(n);
  out.push_back({0.0f, 0.0f});
  int offset = 1;
  bool right = true;
  while (out.size() < n) {
    const float y = right ? offset * d : -offset * d;
    out.push_back({0.0f, y});
    if (!right) ++offset;
    right = !right;
  }
  return out;
}

// ─── V-Shape: 정찰 default (θ 표준 60°) ─────────────────────────────────
// Leader at origin, arms branching back at angle theta/2 from -x axis
// on left and right alternately at spacing d.
std::vector<SlotXY> vShape(size_t n, float d, float theta_deg) {
  std::vector<SlotXY> out;
  out.reserve(n);
  out.push_back({0.0f, 0.0f});                  // leader
  const float half = (theta_deg * 0.5f) * DEG2RAD;
  const float cos_h = std::cos(half);
  const float sin_h = std::sin(half);
  int level = 1;
  bool right = true;
  while (out.size() < n) {
    const float r = level * d;
    const float x = -r * cos_h;
    const float y = right ? r * sin_h : -r * sin_h;
    out.push_back({x, y});
    if (!right) ++level;
    right = !right;
  }
  return out;
}

// ─── Diamond: 4 corners + (optional) center ─────────────────────────────
// Leader at origin (center). Followers at the 4 diagonal corners at
// distance d.
std::vector<SlotXY> diamond(size_t n, float d) {
  const float h = d * SQRT2_2;
  std::vector<SlotXY> base = {
      { 0.0f,  0.0f},   // leader / center
      { h,     h   },   // front-right
      { h,    -h   },   // front-left
      {-h,     h   },   // rear-right
      {-h,    -h   },   // rear-left
  };
  // If n > 5, ring out additional slots at 1.5d on the same diagonals
  for (size_t k = 0; base.size() < n; ++k) {
    const float r = (1.5f + k * 0.5f) * d * SQRT2_2;
    base.push_back({ r,  r});
    if (base.size() >= n) break;
    base.push_back({ r, -r});
    if (base.size() >= n) break;
    base.push_back({-r,  r});
    if (base.size() >= n) break;
    base.push_back({-r, -r});
  }
  base.resize(n);
  return base;
}

// ─── Echelon: 45° offset (좌/우) ─────────────────────────────────────────
std::vector<SlotXY> echelon(size_t n, float d, bool right) {
  std::vector<SlotXY> out;
  out.reserve(n);
  const float h = d * SQRT2_2;
  for (size_t k = 0; k < n; ++k) {
    // y_local = -k*d/√2 (left)  OR  +k*d/√2 (right) — SDD §7.1 row 5
    const float x = -static_cast<float>(k) * h;
    const float y =  right ?  static_cast<float>(k) * h
                            : -static_cast<float>(k) * h;
    out.push_back({x, y});
  }
  return out;
}

// ─── Box: 정사각 + center ────────────────────────────────────────────────
// 5 slots: 4 corners (FR, FL, RR, RL) + 1 center (leader).
std::vector<SlotXY> box(size_t n, float d) {
  std::vector<SlotXY> base = {
      { 0.0f,  0.0f},
      { d * 0.5f,  d * 0.5f},
      { d * 0.5f, -d * 0.5f},
      {-d * 0.5f,  d * 0.5f},
      {-d * 0.5f, -d * 0.5f},
  };
  while (base.size() < n) {
    // ring out at multiples of d
    const float r = (static_cast<float>(base.size()) - 4) * d * 0.5f + d;
    base.push_back({ r,  r});
  }
  base.resize(n);
  return base;
}

// ─── Vee-Inverted: V형이 leader 후방으로 (매복) ────────────────────────
// Leader at front, V arms FORWARD instead of backward.
std::vector<SlotXY> veeInverted(size_t n, float d, float theta_deg) {
  std::vector<SlotXY> out;
  out.reserve(n);
  out.push_back({0.0f, 0.0f});
  const float half = (theta_deg * 0.5f) * DEG2RAD;
  int level = 1;
  bool right = true;
  while (out.size() < n) {
    const float r = level * d;
    // Forward (+x) instead of backward
    const float x =  r * std::cos(half);
    const float y = right ? r * std::sin(half) : -r * std::sin(half);
    out.push_back({x, y});
    if (!right) ++level;
    right = !right;
  }
  return out;
}

// ─── Free-Spread: deterministic pseudo-random within radius d ──────────
// Use Mersenne with seed = n so the same n always produces same layout
// (test-friendly).
std::vector<SlotXY> freeSpread(size_t n, float d) {
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

Preset getPreset(uint8_t preset_id) {
  switch (preset_id) {
    case PRESET_NARROW_PASSAGE: return {"narrow_passage", 3.0f,  40.0f};
    case PRESET_RECON_DEFENCE:  return {"recon_defence",  5.0f,  90.0f};
    case PRESET_WIDE_RECON:     return {"wide_recon",     7.0f, 120.0f};
    case PRESET_ASSAULT:        return {"assault",       15.0f,  60.0f};
    default:                    return {"",              0.0f,   0.0f};
  }
}

// ─── Dispatch ──────────────────────────────────────────────────────────

std::vector<SlotXY> generateSlots(
    Formation form, size_t n, float d, float theta_deg) {
  if (n == 0) return {};
  switch (form) {
    case Formation::Column:        return column(n, d);
    case Formation::Line:          return line(n, d);
    case Formation::VShape:        return vShape(n, d, theta_deg);
    case Formation::Diamond:       return diamond(n, d);
    case Formation::EchelonLeft:   return echelon(n, d, /*right=*/false);
    case Formation::EchelonRight:  return echelon(n, d, /*right=*/true);
    case Formation::Box:           return box(n, d);
    case Formation::VeeInverted:   return veeInverted(n, d, theta_deg);
    case Formation::FreeSpread:    return freeSpread(n, d);
  }
  return column(n, d);    // unreachable — silence compiler warning
}

Formation fromMessageId(uint8_t id) {
  if (id >= 1 && id <= 9) return static_cast<Formation>(id);
  return Formation::Column;
}

}  // namespace san_formation
