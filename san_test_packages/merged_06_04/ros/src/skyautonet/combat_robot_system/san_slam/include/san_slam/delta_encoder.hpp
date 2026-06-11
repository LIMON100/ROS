// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 3 - per-cell SLAM delta encoder.
//
// Converts a pair of nav_msgs/OccupancyGrid snapshots (previous +
// current, both int8 with -1 unknown / 0 free / 100 occupied) into a
// uint8 delta grid where:
//   0   = free (changed since previous)
//   127 = no change since previous
//   255 = occupied (changed since previous)
// The output is PNG-encoded so a 14 m × 14 m / 5 cm grid fits into
// ~10 kB on the mesh.

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace san_slam
{

// Sentinel value for "no change" in the delta grid.
constexpr uint8_t DELTA_NO_CHANGE = 127;
constexpr uint8_t DELTA_FREE = 0;
constexpr uint8_t DELTA_OCCUPIED = 255;

// Encode a single int8 occupancy value (Nav2 convention:
//   -1 = unknown, 0..49 = free, 50..100 = occupied)
// into the uint8 "current" pixel domain (0 / 127 / 255).
inline uint8_t encodeCurrent(int8_t v)
{
  if (v < 0) {
    return DELTA_NO_CHANGE;                  // unknown -> treated as no-change
  }
  if (v >= 50) {return DELTA_OCCUPIED;}
  return DELTA_FREE;
}

// Build a delta grid from previous + current snapshots. `previous`
// may be empty on the first call - every cell is then reported as a
// changed pixel (whatever the current value resolves to).
std::vector<uint8_t> computeDelta(
  const std::vector<int8_t> & previous,
  const std::vector<int8_t> & current);

}  // namespace san_slam
