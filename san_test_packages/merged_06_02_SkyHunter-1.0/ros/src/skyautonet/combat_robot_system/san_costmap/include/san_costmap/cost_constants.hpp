// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 1 - shared cost constants + grid utilities.
//
// Match the Nav2 cost convention so a costmap produced here is
// interchangeable with one from layered_costmap_2d.

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace san_costmap
{

// Standard Nav2 cost cell values.
constexpr uint8_t COST_FREE = 0;
constexpr uint8_t COST_WARN_LOW = 100;        // slope band (cautious)
constexpr uint8_t COST_WARN_HIGH = 200;       // INSCRIBED_INFLATED
constexpr uint8_t COST_LETHAL = 254;          // do-not-traverse
constexpr uint8_t COST_UNKNOWN = 255;

// UGV physical thresholds (SDD-SWARM v1.3 §4.5).
constexpr float OBSTACLE_LETHAL_HEIGHT_M = 0.235f;      // 235 mm
constexpr float OBSTACLE_INFLATED_HEIGHT_M = 0.100f;    // 100 mm
constexpr float SLOPE_LETHAL_DEG = 30.0f;
constexpr float SLOPE_WARN_DEG = 15.0f;
constexpr float DITCH_LETHAL_WIDTH_M = 0.220f;          // 220 mm

// Default grid geometry (14 m × 14 m at 5 cm).
constexpr int DEFAULT_GRID_CELLS = 280;
constexpr float DEFAULT_RESOLUTION_M = 0.05f;

inline std::size_t cellIndex(int gx, int gy, int width)
{
  return static_cast<std::size_t>(gy) * static_cast<std::size_t>(width) +
         static_cast<std::size_t>(gx);
}

// Max-pool helper: leave existing higher cost untouched.
inline void poolMax(
  std::vector<uint8_t> & grid, std::size_t idx,
  uint8_t cost)
{
  if (cost > grid[idx]) {grid[idx] = cost;}
}

}  // namespace san_costmap
