// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Cost-map view (pure C++17, ROS-agnostic).
//
// PATCH 2026-05-13 (Reroute deep-dive review):
//   The v1.5.0 release shipped lateral_evasion.hpp + reroute_node.hpp
//   + 3 source files + test_reroute.cpp all including this header,
//   but the header file ITSELF was missing from the package. The
//   package therefore did not compile from a clean checkout. This
//   file restores the API contract that the rest of the package
//   already presumes.
//
// API (reconstructed from existing consumers):
//   * CostMapView      — plain-data view of a uint8 cost grid
//   * Cost constants   — COST_FREE / COST_INFLATED_LOW / COST_LETHAL /
//                        COST_UNKNOWN — match nav2 conventions
//   * costAt(x, y)     — world-frame lookup with OOB → COST_UNKNOWN
//
// Cost semantics (matches Nav2 / Costmap_2D):
//   0      FREE        — clear, traversable
//   1-49   LOW_COST    — preferred but soft cost
//   50-99  INFLATED    — inside inflation radius, slow / avoid
//   100-252 HIGH_COST  — close to obstacle, prefer to avoid
//   253    INSCRIBED   — inside robot footprint, treat as lethal
//   254    LETHAL      — definitive obstacle
//   255    UNKNOWN     — no information

#ifndef SAN_REROUTE_PLANNER__COST_MAP_VIEW_HPP_
#define SAN_REROUTE_PLANNER__COST_MAP_VIEW_HPP_

#include <cstdint>
#include <cstddef>
#include <vector>

namespace san_reroute_planner
{

// ─── Cost constants (nav2-compatible) ───────────────────────────────────
inline constexpr uint8_t COST_FREE = 0;
inline constexpr uint8_t COST_INFLATED_LOW = 50;
inline constexpr uint8_t COST_INSCRIBED = 253;
inline constexpr uint8_t COST_LETHAL = 254;
inline constexpr uint8_t COST_UNKNOWN = 255;

/// Plain-data cost grid + geometry. The grid is row-major:
///   index = y * width + x  with  (0, 0) at the origin.
///
/// Thread-safety: a CostMapView is a value type. Multiple producers
/// must serialise WRITES. Readers can hold a const reference safely
/// as long as the writer is excluded.
struct CostMapView
{
  // ─── geometry ─────────────────────────────────────────────────────
  uint32_t width = 0;               // cells in x
  uint32_t height = 0;              // cells in y
  float resolution_m = 0.0f;        // metres per cell
  float origin_x_m = 0.0f;          // world-frame x of cell (0,0) corner
  float origin_y_m = 0.0f;          // world-frame y of cell (0,0) corner

  // ─── cost cells ───────────────────────────────────────────────────
  std::vector<uint8_t> grid;        // size == width * height

  /// True iff geometry is consistent and grid is the right size.
  bool valid() const
  {
    if (width == 0 || height == 0) {return false;}
    if (resolution_m <= 0.0f) {return false;}
    const std::size_t n =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    return grid.size() == n;
  }

  /// World-frame point inside the grid (half-open: [origin, origin+size)).
  bool inBounds(float x_m, float y_m) const
  {
    if (!valid()) {return false;}
    if (x_m < origin_x_m || y_m < origin_y_m) {return false;}
    const float w_m = static_cast<float>(width) * resolution_m;
    const float h_m = static_cast<float>(height) * resolution_m;
    return (x_m < origin_x_m + w_m) && (y_m < origin_y_m + h_m);
  }

  /// World-frame cost lookup. Returns COST_UNKNOWN for any OOB.
  /// PATCH 2026-05-13: OOB explicitly returns COST_UNKNOWN (was
  /// "implementation defined" in the missing header — V2 test
  /// expects UNKNOWN, so we standardise on it).
  uint8_t costAt(float x_m, float y_m) const
  {
    if (!inBounds(x_m, y_m)) {return COST_UNKNOWN;}
    const std::uint32_t gx = static_cast<std::uint32_t>(
      (x_m - origin_x_m) / resolution_m);
    const std::uint32_t gy = static_cast<std::uint32_t>(
      (y_m - origin_y_m) / resolution_m);
    if (gx >= width || gy >= height) {return COST_UNKNOWN;}
    return grid[static_cast<std::size_t>(gy) * width + gx];
  }
};

}  // namespace san_reroute_planner

#endif  // SAN_REROUTE_PLANNER__COST_MAP_VIEW_HPP_
