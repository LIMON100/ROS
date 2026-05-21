// SAN v1.3 PHASE 3 - LocalSlam delta-encoding unit test.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <nav_msgs/msg/occupancy_grid.hpp>

#include "san_slam/delta_encoder.hpp"
#include "san_slam/local_slam_node.hpp"

using namespace san_slam;

namespace {

nav_msgs::msg::OccupancyGrid makeGrid(int w, int h, int8_t fill,
                                       float resolution = 0.05f)
{
    nav_msgs::msg::OccupancyGrid g;
    g.info.width = w;
    g.info.height = h;
    g.info.resolution = resolution;
    g.info.origin.position.x = 0.0;
    g.info.origin.position.y = 0.0;
    g.data.assign(static_cast<std::size_t>(w) * h, fill);
    return g;
}

}  // namespace

TEST(DeltaEncoder, EncodeCurrentMapping) {
    EXPECT_EQ(encodeCurrent(-1), DELTA_NO_CHANGE);   // unknown
    EXPECT_EQ(encodeCurrent(0),  DELTA_FREE);
    EXPECT_EQ(encodeCurrent(40), DELTA_FREE);        // below 50
    EXPECT_EQ(encodeCurrent(50), DELTA_OCCUPIED);
    EXPECT_EQ(encodeCurrent(100), DELTA_OCCUPIED);
}

TEST(DeltaEncoder, FirstDeltaReportsEveryChangedCell) {
    std::vector<int8_t> prev;     // empty -> first call
    std::vector<int8_t> cur = {-1, 0, 0, 100, 100, -1};
    auto d = computeDelta(prev, cur);
    EXPECT_EQ(d[0], DELTA_NO_CHANGE);   // unknown reported as no-change
    EXPECT_EQ(d[1], DELTA_FREE);
    EXPECT_EQ(d[2], DELTA_FREE);
    EXPECT_EQ(d[3], DELTA_OCCUPIED);
    EXPECT_EQ(d[4], DELTA_OCCUPIED);
    EXPECT_EQ(d[5], DELTA_NO_CHANGE);
}

TEST(DeltaEncoder, UnchangedCellsMarkedNoChange) {
    std::vector<int8_t> prev = {0, 0, 100, 100};
    std::vector<int8_t> cur  = {0, 0, 100, 100};
    auto d = computeDelta(prev, cur);
    for (auto v : d) EXPECT_EQ(v, DELTA_NO_CHANGE);
}

TEST(DeltaEncoder, ChangedCellReportsNewValue) {
    std::vector<int8_t> prev = {0,   0,   100};
    std::vector<int8_t> cur  = {100, 0,   0};
    auto d = computeDelta(prev, cur);
    EXPECT_EQ(d[0], DELTA_OCCUPIED);    // 0 -> 100
    EXPECT_EQ(d[1], DELTA_NO_CHANGE);   // 0 -> 0
    EXPECT_EQ(d[2], DELTA_FREE);        // 100 -> 0
}

class LocalSlamNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(LocalSlamNodeTest, NoMapMeansEmptyDelta) {
    auto node = std::make_shared<LocalSlamNode>();
    auto msg = node->buildDeltaForTest();
    EXPECT_TRUE(msg.occupancy_grid_delta_png.empty());
}

TEST_F(LocalSlamNodeTest, FirstMapProducesPngPayload) {
    auto node = std::make_shared<LocalSlamNode>();
    node->injectMapForTest(makeGrid(64, 64, /*fill=*/0));
    auto msg = node->buildDeltaForTest();
    EXPECT_GT(msg.occupancy_grid_delta_png.size(), 8u)
        << "PNG header alone is 8 bytes - expect more";
    EXPECT_NEAR(msg.resolution_m, 0.05f, 1e-6);
}

TEST_F(LocalSlamNodeTest, IdempotentMapHasMinimalDelta) {
    auto node = std::make_shared<LocalSlamNode>();
    auto grid = makeGrid(64, 64, /*fill=*/100);
    node->injectMapForTest(grid);
    (void)node->buildDeltaForTest();
    // Inject the same map again; PNG should compress very tightly.
    node->injectMapForTest(grid);
    auto msg = node->buildDeltaForTest();
    // Sanity: payload is at least PNG header bytes but smaller than
    // the raw grid (4096 bytes for a 64x64 image).
    EXPECT_LT(msg.occupancy_grid_delta_png.size(), 4096u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
