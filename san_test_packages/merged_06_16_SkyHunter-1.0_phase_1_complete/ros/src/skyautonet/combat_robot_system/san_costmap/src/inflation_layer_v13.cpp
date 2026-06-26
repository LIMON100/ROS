// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_costmap/inflation_layer_v13.hpp"

#include <algorithm>
#include <cmath>

namespace san_costmap
{

InflationLayerV13::InflationLayerV13(
  int width, int height,
  float resolution_m)
: width_(width), height_(height), resolution_m_(resolution_m)
{}

void InflationLayerV13::setGeometry(
  int width, int height,
  float resolution_m)
{
  width_ = width;
  height_ = height;
  resolution_m_ = resolution_m;
}

void InflationLayerV13::inflate(std::vector<uint8_t> & master) const
{
  const int radius_cells = static_cast<int>(
    std::ceil(inflation_radius_m_ / resolution_m_));
  const auto orig = master;

  for (int gy = 0; gy < height_; ++gy) {
    for (int gx = 0; gx < width_; ++gx) {
      const auto idx = cellIndex(gx, gy, width_);
      if (orig[idx] != COST_LETHAL) {continue;}

      for (int dy = -radius_cells; dy <= radius_cells; ++dy) {
        const int ny = gy + dy;
        if (ny < 0 || ny >= height_) {continue;}
        for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
          const int nx = gx + dx;
          if (nx < 0 || nx >= width_) {continue;}
          const float dist_m = std::sqrt(
            static_cast<float>(dx * dx + dy * dy)) *
            resolution_m_;
          if (dist_m > inflation_radius_m_) {continue;}

          // Exponential decay: cost(d) = (LETHAL-1) * exp(-k * d).
          const float val = static_cast<float>(COST_LETHAL - 1) *
            std::exp(-cost_scaling_factor_ * dist_m);
          const uint8_t cost = static_cast<uint8_t>(
            std::clamp(
              static_cast<int>(val), 0,
              static_cast<int>(COST_LETHAL - 1)));
          const auto nidx = cellIndex(nx, ny, width_);
          if (cost > master[nidx] &&
            master[nidx] != COST_LETHAL)
          {
            master[nidx] = cost;
          }
        }
      }
    }
  }
}

}  // namespace san_costmap
