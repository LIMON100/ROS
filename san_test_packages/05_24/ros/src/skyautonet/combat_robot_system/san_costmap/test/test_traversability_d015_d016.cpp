// SAN v1.5.2 - DCN-2026-006 EXT D-015 + D-016 regression tests.
//
// D-015: cellH must return std::nullopt for out-of-bounds or empty
//        cells so computeLocalSlope cannot fabricate a phantom slope
//        from z=0 boundary cells.
// D-016: detectDitchWidth must not report a ditch when the empty
//        cells are occluded by an obstacle.

#include <gtest/gtest.h>

#include "san_costmap/traversability_layer.hpp"
#include "san_costmap/cost_constants.hpp"
#include "./test_helpers.hpp"

using san_costmap::TraversabilityLayer;
using san_costmap::COST_FREE;
using san_costmap::COST_LETHAL;
using san_costmap::COST_WARN_LOW;

namespace {

// Build a flat ground patch that fills a circular region of radius
// `radius_m` centred at the grid origin. Cells outside the radius are
// left empty so we can probe boundary behaviour with cellH.
pcl::PointCloud<pcl::PointXYZI>::Ptr makeCircularGround(
    float radius_m, float cx_m, float cy_m, float resolution_m)
{
    auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    const float step = resolution_m / 2.0f;
    for (float x = cx_m - radius_m; x <= cx_m + radius_m; x += step) {
        for (float y = cy_m - radius_m; y <= cy_m + radius_m; y += step) {
            const float dx = x - cx_m;
            const float dy = y - cy_m;
            if (dx * dx + dy * dy > radius_m * radius_m) continue;
            pcl::PointXYZI p;
            p.x = x; p.y = y; p.z = 0.0f; p.intensity = 100.0f;
            cloud->points.push_back(p);
        }
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    cloud->is_dense = true;
    return cloud;
}

// Build a tall obstacle column at (cx_m, cy_m).
pcl::PointCloud<pcl::PointXYZI>::Ptr makeObstacleColumn(
    float cx_m, float cy_m, float radius_m, float top_m)
{
    auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    for (float x = cx_m - radius_m; x <= cx_m + radius_m; x += 0.025f) {
        for (float y = cy_m - radius_m; y <= cy_m + radius_m; y += 0.025f) {
            for (float z = 0.10f; z <= top_m; z += 0.05f) {
                pcl::PointXYZI p;
                p.x = x; p.y = y; p.z = z; p.intensity = 200.0f;
                cloud->points.push_back(p);
            }
        }
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    cloud->is_dense = true;
    return cloud;
}

}  // namespace

// ─── D-015: boundary cells must not fabricate a slope ─────────────
//
// Before the patch, a perfectly flat ground patch whose extent does
// not cover the full grid produced false LETHAL cells along the
// boundary because cellH() defaulted to z=0 for missing samples and
// the central-difference operator picked up a 0 -> finite-z edge.
TEST(TraversabilityLayer_D015, BoundaryNoPhantomLethal) {
    TraversabilityLayer layer;
    constexpr int W = 100;
    constexpr int H = 100;
    layer.setGeometry(W, H, 0.10f, 0.0f, 0.0f);

    // Flat ground at z = 0.5 m covers only the central disc - the
    // outer ring of grid cells has no ground samples.
    auto ground = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    const float cx = (W / 2) * 0.10f;
    const float cy = (H / 2) * 0.10f;
    const float radius = 3.0f;       // 3 m radius covered
    for (float x = cx - radius; x <= cx + radius; x += 0.05f) {
        for (float y = cy - radius; y <= cy + radius; y += 0.05f) {
            const float dx = x - cx;
            const float dy = y - cy;
            if (dx * dx + dy * dy > radius * radius) continue;
            pcl::PointXYZI p;
            p.x = x; p.y = y; p.z = 0.5f; p.intensity = 1.0f;
            ground->points.push_back(p);
        }
    }
    ground->width = ground->points.size();
    ground->height = 1;
    layer.setGroundCloud(ground);

    layer.updateCosts();

    // Cells near the outer disc boundary (transition from samples to
    // no samples) must not be LETHAL. The patch should treat unknown
    // neighbours as "skip" rather than "z=0".
    const int near_edge_gx = static_cast<int>((cx + radius - 0.20f) / 0.10f);
    const int near_edge_gy = static_cast<int>(cy / 0.10f);
    EXPECT_NE(layer.cost(near_edge_gx, near_edge_gy), COST_LETHAL)
        << "Boundary cell near the ground-disc edge must not be LETHAL";

    // Grid corner: zero samples in both 5x5 windows -> slope cannot
    // be computed, should remain FREE.
    EXPECT_EQ(layer.cost(0, 0), COST_FREE);
    EXPECT_EQ(layer.cost(W - 1, H - 1), COST_FREE);
}

TEST(TraversabilityLayer_D015, FlatGroundProducesZeroSlope) {
    // Centre of a flat ground patch must still report ~0 deg slope.
    TraversabilityLayer layer;
    constexpr int W = 100;
    constexpr int H = 100;
    layer.setGeometry(W, H, 0.10f, 0.0f, 0.0f);
    auto ground = makeCircularGround(4.0f, 5.0f, 5.0f, 0.10f);
    layer.setGroundCloud(ground);
    layer.updateCosts();

    const float slope_center =
        layer.computeLocalSlopeForTest(50, 50);
    EXPECT_LT(slope_center, 1.0f)
        << "Flat ground centre must yield near-zero slope";
}

// ─── D-016: occluded cells must not be counted as a ditch ─────────
//
// Before the patch, the empty-cell run scan in detectDitchWidth
// counted any cell with count_grid_[idx] == 0. A tall stand-off
// obstacle creates a shadow of empty ground cells behind it, which
// the layer wrongly reported as a ditch (LETHAL cost), forcing the
// planner to detour around safe terrain.
TEST(TraversabilityLayer_D016, ObstacleShadowIsNotADitch) {
    TraversabilityLayer layer;
    constexpr int W = 100;
    constexpr int H = 100;
    layer.setGeometry(W, H, 0.10f, 0.0f, 0.0f);

    // Flat ground patch with a clear gap along +x from gx=60 to gx=63
    // (40 cm gap) - but the gap is filled with an obstacle column,
    // not a ditch.
    auto ground = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    for (float x = 0.0f; x <= 10.0f; x += 0.05f) {
        for (float y = 4.5f; y <= 5.5f; y += 0.05f) {
            // Skip the gap region.
            if (x >= 6.0f && x <= 6.3f) continue;
            pcl::PointXYZI p;
            p.x = x; p.y = y; p.z = 0.0f; p.intensity = 1.0f;
            ground->points.push_back(p);
        }
    }
    ground->width = ground->points.size();
    ground->height = 1;

    auto obstacle = makeObstacleColumn(6.15f, 5.0f, 0.15f, 1.0f);

    layer.setGroundCloud(ground);
    layer.setObstacleCloud(obstacle);   // D-016: provide obstacle data
    layer.updateCosts();

    // The 40 cm "empty" run between the ground patches is shadowed
    // by the obstacle column. Without D-016 it would have been
    // reported as ditch_m = 0.40 > lethal 0.22 -> LETHAL.
    const float ditch_at_obstacle =
        layer.detectDitchWidthForTest(61, 50);
    EXPECT_LT(ditch_at_obstacle, 0.05f)
        << "Cells occluded by an obstacle must not register as ditch; "
        << "got " << ditch_at_obstacle << " m";
}

TEST(TraversabilityLayer_D016, RealDitchStillDetected) {
    // Regression guard: D-016 must NOT mask genuine ditches. Provide
    // an empty obstacle cloud so every empty cell is treated as a
    // true gap (the legacy behaviour).
    TraversabilityLayer layer;
    constexpr int W = 100;
    constexpr int H = 100;
    layer.setGeometry(W, H, 0.10f, 0.0f, 0.0f);

    auto ground = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    for (float x = 0.0f; x <= 10.0f; x += 0.05f) {
        for (float y = 4.5f; y <= 5.5f; y += 0.05f) {
            if (x >= 6.0f && x <= 6.3f) continue;   // real ditch
            pcl::PointXYZI p;
            p.x = x; p.y = y; p.z = 0.0f; p.intensity = 1.0f;
            ground->points.push_back(p);
        }
    }
    ground->width = ground->points.size();
    ground->height = 1;

    auto empty_obstacle =
        std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    empty_obstacle->width = 0;
    empty_obstacle->height = 1;
    empty_obstacle->is_dense = true;

    layer.setGroundCloud(ground);
    layer.setObstacleCloud(empty_obstacle);
    layer.updateCosts();

    const float ditch_real = layer.detectDitchWidthForTest(61, 50);
    EXPECT_GE(ditch_real, 0.20f)
        << "Real ditch must still register; got " << ditch_real << " m";
}
