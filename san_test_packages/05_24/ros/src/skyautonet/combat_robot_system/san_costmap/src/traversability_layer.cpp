#include "san_costmap/traversability_layer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace san_costmap {

TraversabilityLayer::TraversabilityLayer(int width, int height,
                                          float resolution_m)
    : width_(width), height_(height), resolution_m_(resolution_m),
      grid_(static_cast<std::size_t>(width) * height, COST_FREE),
      height_grid_(static_cast<std::size_t>(width) * height, 0.0f),
      count_grid_(static_cast<std::size_t>(width) * height, 0),
      obstacle_count_grid_(static_cast<std::size_t>(width) * height, 0)
{}

void TraversabilityLayer::setGeometry(int width, int height,
                                       float resolution_m,
                                       float origin_x, float origin_y)
{
    width_ = width;
    height_ = height;
    resolution_m_ = resolution_m;
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    grid_.assign(static_cast<std::size_t>(width) * height, COST_FREE);
    height_grid_.assign(static_cast<std::size_t>(width) * height, 0.0f);
    count_grid_.assign(static_cast<std::size_t>(width) * height, 0);
    obstacle_count_grid_.assign(static_cast<std::size_t>(width) * height, 0);
}

void TraversabilityLayer::rasterizeGround() {
    std::fill(height_grid_.begin(), height_grid_.end(), 0.0f);
    std::fill(count_grid_.begin(), count_grid_.end(), 0);
    if (ground_ == nullptr) return;

    for (const auto& p : ground_->points) {
        const int gx = static_cast<int>(
            (p.x - origin_x_) / resolution_m_);
        const int gy = static_cast<int>(
            (p.y - origin_y_) / resolution_m_);
        if (gx < 0 || gx >= width_) continue;
        if (gy < 0 || gy >= height_) continue;
        const auto idx = cellIndex(gx, gy, width_);
        height_grid_[idx] += p.z;
        ++count_grid_[idx];
    }
    for (std::size_t i = 0; i < height_grid_.size(); ++i) {
        if (count_grid_[i] > 0) {
            height_grid_[i] /= static_cast<float>(count_grid_[i]);
        }
    }
}

// [DCN-2026-006 EXT D-016] Rasterize obstacle cloud into a parallel
// count grid. A cell with no ground samples but >=1 obstacle sample
// is treated as occluded ground (not a ditch).
void TraversabilityLayer::rasterizeObstacles() {
    std::fill(obstacle_count_grid_.begin(),
              obstacle_count_grid_.end(), 0);
    if (obstacle_ == nullptr) return;

    for (const auto& p : obstacle_->points) {
        const int gx = static_cast<int>(
            (p.x - origin_x_) / resolution_m_);
        const int gy = static_cast<int>(
            (p.y - origin_y_) / resolution_m_);
        if (gx < 0 || gx >= width_) continue;
        if (gy < 0 || gy >= height_) continue;
        const auto idx = cellIndex(gx, gy, width_);
        ++obstacle_count_grid_[idx];
    }
}

// [DCN-2026-006 EXT D-015] Return ground height only when the cell
// is in-bounds AND has at least one ground sample. nullopt signals
// "unknown" to the caller; computeLocalSlope skips the patch instead
// of fabricating a slope from z=0.
std::optional<float> TraversabilityLayer::cellH(int gx, int gy) const {
    if (gx < 0 || gx >= width_)  return std::nullopt;
    if (gy < 0 || gy >= height_) return std::nullopt;
    const auto idx = cellIndex(gx, gy, width_);
    if (count_grid_[idx] <= 0)   return std::nullopt;
    return height_grid_[idx];
}

float TraversabilityLayer::computeLocalSlope(int gx, int gy) const {
    // [DCN-2026-006 EXT D-015] Central differences require all four
    // neighbours to have ground samples. If any neighbour is unknown,
    // we cannot compute a meaningful slope - return 0 (treated as
    // FREE by the cost mapping) rather than a phantom slope derived
    // from z=0 boundary cells.
    const auto h_xp = cellH(gx + 1, gy);
    const auto h_xn = cellH(gx - 1, gy);
    const auto h_yp = cellH(gx, gy + 1);
    const auto h_yn = cellH(gx, gy - 1);
    if (!h_xp || !h_xn || !h_yp || !h_yn) {
        return 0.0f;
    }
    const float dx = (*h_xp - *h_xn) / (2.0f * resolution_m_);
    const float dy = (*h_yp - *h_yn) / (2.0f * resolution_m_);
    const float grad = std::sqrt(dx * dx + dy * dy);
    return std::atan(grad) * 180.0f / static_cast<float>(M_PI);
}

float TraversabilityLayer::detectDitchWidth(int gx, int gy) const {
    // Scan along +/- x for a contiguous run of cells whose count is
    // zero (no ground returns) - the width in m approximates a ditch
    // perpendicular to the robot's forward motion.
    //
    // [DCN-2026-006 EXT D-016] A cell with no ground samples but with
    // obstacle samples is occluded ground, not a ditch. Exclude such
    // cells from the empty-run scan.
    auto isEmpty = [&](int x) -> bool {
        if (x < 0 || x >= width_) return false;
        const auto idx = cellIndex(x, gy, width_);
        if (count_grid_[idx] != 0) return false;
        // D-016: occluded by obstacle - not a true empty cell.
        if (obstacle_count_grid_[idx] > 0) return false;
        return true;
    };

    if (!isEmpty(gx)) return 0.0f;
    int left = gx;
    while (left > 0 && isEmpty(left - 1)) --left;
    int right = gx;
    while (right < width_ - 1 && isEmpty(right + 1)) ++right;
    // [DCN-2026-006 EXT D-015] A ditch is a gap *bounded by ground* on
    // both sides. An empty run that touches the grid edge is unknown
    // territory beyond the perception window, not a measurable ditch -
    // returning a width here would mark corner / off-disc cells LETHAL.
    if (left == 0 || right == width_ - 1) return 0.0f;
    const int n = right - left + 1;
    return static_cast<float>(n) * resolution_m_;
}

void TraversabilityLayer::updateCosts() {
    rasterizeGround();
    rasterizeObstacles();        // [DCN-2026-006 EXT D-016]
    std::fill(grid_.begin(), grid_.end(), COST_FREE);

    for (int gy = 0; gy < height_; ++gy) {
        for (int gx = 0; gx < width_; ++gx) {
            const float slope_deg = computeLocalSlope(gx, gy);
            const float ditch_m   = detectDitchWidth(gx, gy);

            uint8_t cost = COST_FREE;
            if (slope_deg >= lethal_slope_deg_) {
                cost = COST_LETHAL;
            } else if (ditch_m >= lethal_ditch_width_m_) {
                cost = COST_LETHAL;
            } else if (slope_deg >= warn_slope_deg_) {
                cost = COST_WARN_LOW;
            }
            poolMax(grid_, cellIndex(gx, gy, width_), cost);
        }
    }
}

uint8_t TraversabilityLayer::cost(int gx, int gy) const {
    if (gx < 0 || gx >= width_) return COST_UNKNOWN;
    if (gy < 0 || gy >= height_) return COST_UNKNOWN;
    return grid_[cellIndex(gx, gy, width_)];
}

}  // namespace san_costmap
