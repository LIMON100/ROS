// SAN v1.5 — CostMapView: pure-logic cost grid abstraction.
//
// Decoupled from CostMapUpdate.msg PNG encoding so the path-check
// and evasion algorithms can be unit-tested in isolation with raw
// uint8 grids. The rclcpp Node side decodes PNG → fills CostMapView.
//
// 권원: SDD-SWARM v1.5 §6.4 (Cost Map T1.5 회피)
//        san_costmap::cost_constants.hpp (cost cell values)

#ifndef SAN_REROUTE_PLANNER__COST_MAP_VIEW_HPP_
#define SAN_REROUTE_PLANNER__COST_MAP_VIEW_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace san_reroute_planner {

/// Standard Nav2 cost cell values (mirror of san_costmap constants).
constexpr uint8_t COST_FREE              = 0;
constexpr uint8_t COST_INFLATED_LOW      = 50;   // T1.5 entry (감속)
constexpr uint8_t COST_WARN_LOW          = 100;  // slope band
constexpr uint8_t COST_INSCRIBED         = 200;  // inflated
constexpr uint8_t COST_LETHAL            = 254;  // do-not-traverse
constexpr uint8_t COST_UNKNOWN           = 255;

/// Raw cost grid + world↔grid coordinate transform.
struct CostMapView {
  std::vector<uint8_t> grid;          // row-major, size = width × height
  uint32_t width{0};                  // cells
  uint32_t height{0};
  float    resolution_m{0.05f};       // m / cell
  float    origin_x_m{0.0f};          // world coord of grid[0, 0]
  float    origin_y_m{0.0f};

  /// True if the grid has been populated with a valid size.
  bool valid() const {
    return width > 0 && height > 0
        && grid.size() == static_cast<size_t>(width) * height;
  }

  /// Check if world point falls inside the grid bounds.
  bool inBounds(float x_m, float y_m) const {
    const float dx = x_m - origin_x_m;
    const float dy = y_m - origin_y_m;
    const int gx = static_cast<int>(dx / resolution_m);
    const int gy = static_cast<int>(dy / resolution_m);
    return gx >= 0 && gy >= 0
        && static_cast<uint32_t>(gx) < width
        && static_cast<uint32_t>(gy) < height;
  }

  /// Get cost at a world coordinate. Returns COST_UNKNOWN if out of bounds.
  uint8_t costAt(float x_m, float y_m) const {
    if (!valid()) return COST_UNKNOWN;
    const float dx = x_m - origin_x_m;
    const float dy = y_m - origin_y_m;
    const int gx = static_cast<int>(dx / resolution_m);
    const int gy = static_cast<int>(dy / resolution_m);
    if (gx < 0 || gy < 0
        || static_cast<uint32_t>(gx) >= width
        || static_cast<uint32_t>(gy) >= height) {
      return COST_UNKNOWN;
    }
    return grid[static_cast<size_t>(gy) * width + gx];
  }
};

}  // namespace san_reroute_planner

#endif  // SAN_REROUTE_PLANNER__COST_MAP_VIEW_HPP_
