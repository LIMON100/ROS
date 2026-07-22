// SAN v1.3 PHASE 1 - KPP coverage test (≥ 7 m forward).

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "san_costmap/cost_map_node.hpp"
#include "test_helpers.hpp"

class CostMapKppCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(CostMapKppCoverageTest, GridIsFourteenMetersWide) {
    auto node = std::make_shared<san_costmap::CostMapNode>();
    EXPECT_EQ(node->width(), 280);
    EXPECT_EQ(node->height(), 280);
}

TEST_F(CostMapKppCoverageTest, MessageEncodesFullGrid) {
    auto node = std::make_shared<san_costmap::CostMapNode>();
    auto obstacle = san_costmap_test::makeBlock(0.10f);
    auto ground   = san_costmap_test::makeFlatGroundFull();

    auto msg = node->buildOneShotForTest(obstacle, ground);
    EXPECT_EQ(msg.width_cells, 280u);
    EXPECT_EQ(msg.height_cells, 280u);
    EXPECT_NEAR(msg.resolution_m, 0.05f, 1e-6);
    EXPECT_GT(msg.cost_grid_png.size(), 8u)
        << "PNG header magic alone is 8 bytes - we should have more";
}

TEST_F(CostMapKppCoverageTest, ForwardSevenMetersIsInsideGrid) {
    auto node = std::make_shared<san_costmap::CostMapNode>();
    auto obstacle = san_costmap_test::makeBlock(0.10f);
    auto ground   = san_costmap_test::makeFlatGroundFull();
    auto msg = node->buildOneShotForTest(obstacle, ground);

    // Robot at (0, 0), grid origin at (0, 0). Forward 7 m -> x=7.
    const int gx_forward7 =
        static_cast<int>(7.0f / msg.resolution_m);
    const int gy_center = static_cast<int>(7.0f / msg.resolution_m);
    EXPECT_LT(gx_forward7, static_cast<int>(msg.width_cells));
    EXPECT_LT(gy_center, static_cast<int>(msg.height_cells));
    EXPECT_GE(gx_forward7, 0);
    EXPECT_GE(gy_center, 0);
}

TEST_F(CostMapKppCoverageTest, LethalCountReflectsObstacle) {
    auto node = std::make_shared<san_costmap::CostMapNode>();
    auto obstacle = san_costmap_test::makeBlock(/*top_m=*/0.30f,
                                                  /*w=*/0.30f,
                                                  /*d=*/0.30f);
    auto ground = san_costmap_test::makeFlatGroundFull();
    auto msg = node->buildOneShotForTest(obstacle, ground);
    EXPECT_GT(msg.lethal_count, 0u)
        << "30 cm cube should produce some lethal cells";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
