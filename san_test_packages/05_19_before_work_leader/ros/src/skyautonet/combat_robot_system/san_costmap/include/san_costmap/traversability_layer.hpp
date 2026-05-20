// SAN v1.5.2 PHASE 7 - traversability layer (local slope + ditch).
//
// Operates on the ground cloud emitted by san_lidar. For each grid
// cell we:
//   1. compute the local slope (deg) from a 5×5 ground-point patch
//   2. measure the ditch width by counting consecutive empty cells
//      along the predominant scan direction
// then map onto the v1.3 cost values:
//   slope >= 30°   -> LETHAL          (climb limit)
//   slope >= 15°   -> COST_WARN_LOW   (cautious, slow down)
//   ditch >= 220mm -> LETHAL          (ditch crossing limit)
//
// DCN-2026-006 EXT (v1.5.2):
//   D-015 cellH boundary fix    : grid-edge / empty cells no longer
//                                  generate phantom slopes (false
//                                  LETHAL at 5 km/h on perimeter).
//   D-016 ditch occlusion fix   : cells shadowed by an obstacle are
//                                  excluded from the empty-run scan
//                                  so a tall stand-off is not
//                                  reported as a ditch.

#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "san_costmap/cost_constants.hpp"

namespace san_costmap {

class TraversabilityLayer {
public:
    TraversabilityLayer(int width = DEFAULT_GRID_CELLS,
                         int height = DEFAULT_GRID_CELLS,
                         float resolution_m = DEFAULT_RESOLUTION_M);

    void setGeometry(int width, int height, float resolution_m,
                      float origin_x = 0.0f, float origin_y = 0.0f);

    void setGroundCloud(
        pcl::PointCloud<pcl::PointXYZI>::ConstPtr cloud) {
        ground_ = std::move(cloud);
    }

    // [DCN-2026-006 EXT D-016] obstacle cloud is needed to suppress
    // false ditch detections in cells shadowed by an obstacle. Optional
    // - if no obstacle cloud is provided the legacy (occlusion-blind)
    // behaviour is retained for backward compatibility.
    void setObstacleCloud(
        pcl::PointCloud<pcl::PointXYZI>::ConstPtr cloud) {
        obstacle_ = std::move(cloud);
    }

    // Refresh the grid from the latest ground cloud.
    void updateCosts();

    uint8_t cost(int gx, int gy) const;
    const std::vector<uint8_t>& grid() const { return grid_; }
    int width()  const { return width_; }
    int height() const { return height_; }
    float resolution_m() const { return resolution_m_; }

    void setLethalSlopeDeg(float d) { lethal_slope_deg_ = d; }
    void setWarnSlopeDeg(float d)   { warn_slope_deg_ = d; }
    void setLethalDitchWidthM(float m) { lethal_ditch_width_m_ = m; }

    // Test-only seam: expose the per-cell heights derived from the
    // ground cloud so the slope/ditch helpers can be unit-tested in
    // isolation.
    float computeLocalSlopeForTest(int gx, int gy) const {
        return computeLocalSlope(gx, gy);
    }
    float detectDitchWidthForTest(int gx, int gy) const {
        return detectDitchWidth(gx, gy);
    }

private:
    // [DCN-2026-006 EXT cleanup §5.1] In-class defaults guard against
    // a future refactor that adds another ctor without re-initializing
    // these fields; the current ctors explicitly set them so behaviour
    // is unchanged.
    int width_ = 0;
    int height_ = 0;
    float resolution_m_ = 0.0f;
    float origin_x_ = 0.0f;
    float origin_y_ = 0.0f;
    float lethal_slope_deg_      = SLOPE_LETHAL_DEG;
    float warn_slope_deg_        = SLOPE_WARN_DEG;
    float lethal_ditch_width_m_  = DITCH_LETHAL_WIDTH_M;

    pcl::PointCloud<pcl::PointXYZI>::ConstPtr ground_;
    pcl::PointCloud<pcl::PointXYZI>::ConstPtr obstacle_;   // D-016
    std::vector<uint8_t> grid_;
    std::vector<float> height_grid_;       // mean z per cell
    std::vector<int>   count_grid_;        // ground sample count
    std::vector<int>   obstacle_count_grid_;  // D-016: obstacle sample count

    void rasterizeGround();
    void rasterizeObstacles();             // D-016
    float computeLocalSlope(int gx, int gy) const;
    float detectDitchWidth(int gx, int gy) const;

    // [DCN-2026-006 EXT D-015] cellH returns nullopt for cells that
    // are outside the grid or have no ground samples, so the slope
    // calculation can skip them instead of assuming z=0.
    std::optional<float> cellH(int gx, int gy) const;
};

}  // namespace san_costmap
