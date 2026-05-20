// SAN v1.3 PHASE 1 - shared synthetic-cloud helpers for GTests.

#pragma once

#include <cmath>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

namespace san_costmap_test {

// Build a block of points spanning a rectangular footprint. `top_m`
// is the maximum z; the block runs from a base height of 0.05 m up.
//
// The inner z loop (z += 0.02f) does NOT reliably visit `top_m` itself
// because of float-stride truncation — e.g. for top_m = 0.100 the loop
// produces z ∈ {0.05, 0.07, 0.09} and stops before 0.11. Without an
// explicit top-row insertion, the boundary threshold tests
// (At100mm_Inflated → COST_WARN_HIGH at h >= 0.100,
//  At235mm_BecomesLethal → COST_LETHAL at h >= 0.235) silently fall
// below the inflated_height_m_ / lethal_height_m_ checks and the cell
// stays COST_FREE. Add a top row pinned to exactly top_m so each
// boundary value is represented in the cloud.
inline pcl::PointCloud<pcl::PointXYZI>::Ptr makeBlock(
    float top_m, float width_m = 0.10f, float depth_m = 0.10f,
    float cx = 7.0f, float cy = 7.0f)
{
    auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    for (float x = cx - width_m / 2; x <= cx + width_m / 2; x += 0.02f) {
        for (float y = cy - depth_m / 2; y <= cy + depth_m / 2; y += 0.02f) {
            for (float z = 0.05f; z <= top_m + 1e-3f; z += 0.02f) {
                pcl::PointXYZI p;
                p.x = x; p.y = y; p.z = z; p.intensity = 200.0f;
                cloud->points.push_back(p);
            }
            // Explicit top row at exactly top_m so >= threshold checks
            // against this value hit the boundary.
            pcl::PointXYZI top_p;
            top_p.x = x; top_p.y = y; top_p.z = top_m;
            top_p.intensity = 200.0f;
            cloud->points.push_back(top_p);
        }
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    cloud->is_dense = true;
    return cloud;
}

// Build a synthetic slope along +x axis.
inline pcl::PointCloud<pcl::PointXYZI>::Ptr makeSlope(
    float slope_deg, float extent_m = 1.0f,
    float cx = 7.0f, float cy = 7.0f)
{
    auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    const float tan_s = std::tan(
        slope_deg * static_cast<float>(M_PI) / 180.0f);
    for (float x = -extent_m; x <= extent_m; x += 0.02f) {
        for (float y = -extent_m; y <= extent_m; y += 0.02f) {
            pcl::PointXYZI p;
            p.x = cx + x; p.y = cy + y; p.z = x * tan_s;
            p.intensity = 1.0f;
            cloud->points.push_back(p);
        }
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    return cloud;
}

// Build a flat ground patch with a horizontal gap (ditch) along +y.
//
// The TraversabilityLayer reports ditch width as `empty_cell_count
// * resolution_m_` (5 cm grid). With the naive `half = ditch_width / 2`
// skip range, the boundary cells (e.g. the cells at gx = 137 and 142
// for a ditch centred at gx = 140) still pick up scan points from
// outside the skip range and remain non-empty, so the reported width
// rounds DOWN to one fewer cell than requested. For a ditch_width_m
// at exactly a non-cell-aligned boundary (e.g. 220 mm with 50 mm
// cells), this systematically undershoots the algorithm's lethal
// threshold and the test "Ditch220mm_BecomesLethal" fails because
// only 4 cells × 50 mm = 200 mm of empty are produced.
//
// Pad the skipped range by one full grid resolution (0.05 m) so the
// boundary cells become empty too. With the pad, a 220 mm input
// produces 5 empty cells (250 mm reported, just over the 220 mm
// threshold); a 150 mm input still produces 4 empty cells (200 mm
// reported, below threshold) so `Ditch150mm_BelowLethalThreshold`
// keeps passing.
inline pcl::PointCloud<pcl::PointXYZI>::Ptr makeGroundWithDitch(
    float ditch_width_m, float cx = 7.0f, float cy = 7.0f,
    float extent_m = 1.0f)
{
    auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    // Pad by one cell to account for the boundary-cell rasterization
    // effect described above. resolution constant must match the
    // TraversabilityLayer default (DEFAULT_RESOLUTION_M = 0.05).
    constexpr float kGridResolutionM = 0.05f;
    const float half = ditch_width_m / 2.0f + kGridResolutionM;
    for (float x = -extent_m; x <= extent_m; x += 0.02f) {
        if (x > -half && x < half) continue;       // skip the ditch
        for (float y = -extent_m; y <= extent_m; y += 0.02f) {
            pcl::PointXYZI p;
            p.x = cx + x; p.y = cy + y; p.z = 0.0f;
            p.intensity = 1.0f;
            cloud->points.push_back(p);
        }
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    return cloud;
}

// Build a uniformly flat ground patch covering the whole 14 m × 14 m
// area at z = 0. Useful for coverage tests.
inline pcl::PointCloud<pcl::PointXYZI>::Ptr makeFlatGroundFull(
    float origin_x = 0.0f, float origin_y = 0.0f,
    float extent_m = 14.0f)
{
    auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    for (float x = 0.0f; x <= extent_m; x += 0.10f) {
        for (float y = 0.0f; y <= extent_m; y += 0.10f) {
            pcl::PointXYZI p;
            p.x = origin_x + x; p.y = origin_y + y; p.z = 0.0f;
            p.intensity = 1.0f;
            cloud->points.push_back(p);
        }
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    return cloud;
}

}  // namespace san_costmap_test
