// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_costmap/obstacle_layer_v13.hpp"

#include <cmath>

namespace san_costmap
{

ObstacleLayerV13::ObstacleLayerV13(int width, int height, float resolution_m)
: width_(width), height_(height), resolution_m_(resolution_m),
  grid_(static_cast<std::size_t>(width) * height, COST_FREE)
{}

void ObstacleLayerV13::setGeometry(
  int width, int height,
  float resolution_m,
  float origin_x, float origin_y)
{
  width_ = width;
  height_ = height;
  resolution_m_ = resolution_m;
  origin_x_ = origin_x;
  origin_y_ = origin_y;
  grid_.assign(static_cast<std::size_t>(width) * height, COST_FREE);
}

void ObstacleLayerV13::updateBounds()
{
  std::fill(grid_.begin(), grid_.end(), COST_FREE);
  if (input_ == nullptr) {return;}

  for (const auto & p : input_->points) {
    // std::floor (not truncation) so points just outside the negative
    // origin edge map to cell -1 (out of bounds, skipped) instead of
    // wrongly folding into cell 0.
    const int gx = static_cast<int>(
      std::floor((p.x - origin_x_) / resolution_m_));
    const int gy = static_cast<int>(
      std::floor((p.y - origin_y_) / resolution_m_));
    if (gx < 0 || gx >= width_) {continue;}
    if (gy < 0 || gy >= height_) {continue;}

    const float h = p.z;
    uint8_t cost;
    if (h >= lethal_height_m_) {
      cost = COST_LETHAL;
    } else if (h >= inflated_height_m_) {
      cost = COST_WARN_HIGH;
    } else {
      continue;           // sub-100 mm; ignore (ground noise level)
    }
    poolMax(grid_, cellIndex(gx, gy, width_), cost);
  }
}

uint8_t ObstacleLayerV13::cost(int gx, int gy) const
{
  if (gx < 0 || gx >= width_) {return COST_UNKNOWN;}
  if (gy < 0 || gy >= height_) {return COST_UNKNOWN;}
  return grid_[cellIndex(gx, gy, width_)];
}

}  // namespace san_costmap
