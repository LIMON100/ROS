// SAN v1.3 PHASE 1 - threshold conformance tests.
//
// Verifies the v1.3 UGV thresholds map onto the correct cost values:
//   * Obstacle:  100 mm => INSCRIBED (200), 235 mm => LETHAL (254)
//   * Slope:     <30°   => COST_WARN_LOW band, >=30° => LETHAL
//   * Ditch:     >=220 mm => LETHAL

#include <gtest/gtest.h>

#include "san_costmap/cost_constants.hpp"
#include "san_costmap/obstacle_layer_v13.hpp"
#include "san_costmap/traversability_layer.hpp"

#include "test_helpers.hpp"

using namespace san_costmap;
using namespace san_costmap_test;

namespace {

int cellAt(float x_m, float resolution_m = 0.05f) {
    return static_cast<int>(x_m / resolution_m);
}

}  // namespace

class ObstacleLayerTest : public ::testing::Test {
protected:
    ObstacleLayerV13 layer_{DEFAULT_GRID_CELLS, DEFAULT_GRID_CELLS,
                              DEFAULT_RESOLUTION_M};
};

TEST_F(ObstacleLayerTest, BelowInflated_NoCost) {
    auto cloud = makeBlock(/*top_m=*/0.060f);   // 60 mm
    layer_.setInputCloud(cloud);
    layer_.updateBounds();
    // Centered at 7.0 m -> cell (140, 140).
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_FREE);
}

TEST_F(ObstacleLayerTest, At100mm_Inflated) {
    auto cloud = makeBlock(/*top_m=*/0.100f);
    layer_.setInputCloud(cloud);
    layer_.updateBounds();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_WARN_HIGH);
}

TEST_F(ObstacleLayerTest, At200mm_StillInflated) {
    auto cloud = makeBlock(/*top_m=*/0.200f);
    layer_.setInputCloud(cloud);
    layer_.updateBounds();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_WARN_HIGH);
}

TEST_F(ObstacleLayerTest, At235mm_BecomesLethal) {
    auto cloud = makeBlock(/*top_m=*/0.235f);
    layer_.setInputCloud(cloud);
    layer_.updateBounds();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_LETHAL);
}

TEST_F(ObstacleLayerTest, Above235mm_StaysLethal) {
    auto cloud = makeBlock(/*top_m=*/0.270f);
    layer_.setInputCloud(cloud);
    layer_.updateBounds();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_LETHAL);
}

class TraversabilityLayerTest : public ::testing::Test {
protected:
    TraversabilityLayer layer_{DEFAULT_GRID_CELLS, DEFAULT_GRID_CELLS,
                                 DEFAULT_RESOLUTION_M};
};

TEST_F(TraversabilityLayerTest, Slope25deg_Cautious) {
    auto cloud = makeSlope(/*slope_deg=*/25.0f);
    layer_.setGroundCloud(cloud);
    layer_.updateCosts();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_WARN_LOW);
}

TEST_F(TraversabilityLayerTest, Slope30deg_Lethal) {
    auto cloud = makeSlope(/*slope_deg=*/30.5f);
    layer_.setGroundCloud(cloud);
    layer_.updateCosts();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_LETHAL);
}

TEST_F(TraversabilityLayerTest, Slope10deg_Free) {
    auto cloud = makeSlope(/*slope_deg=*/10.0f);
    layer_.setGroundCloud(cloud);
    layer_.updateCosts();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_FREE);
}

TEST_F(TraversabilityLayerTest, Ditch150mm_BelowLethalThreshold) {
    auto cloud = makeGroundWithDitch(/*width_m=*/0.150f);
    layer_.setGroundCloud(cloud);
    layer_.updateCosts();
    // The ditch is below the 220 mm lethal width - cell stays free.
    EXPECT_LT(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_LETHAL);
}

TEST_F(TraversabilityLayerTest, Ditch220mm_BecomesLethal) {
    auto cloud = makeGroundWithDitch(/*width_m=*/0.220f);
    layer_.setGroundCloud(cloud);
    layer_.updateCosts();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_LETHAL);
}

TEST_F(TraversabilityLayerTest, Ditch300mm_StaysLethal) {
    auto cloud = makeGroundWithDitch(/*width_m=*/0.300f);
    layer_.setGroundCloud(cloud);
    layer_.updateCosts();
    EXPECT_EQ(layer_.cost(cellAt(7.0f), cellAt(7.0f)), COST_LETHAL);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
