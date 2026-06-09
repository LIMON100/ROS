// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 3 - LocalSlam delta-encoding unit test.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <nav_msgs/msg/occupancy_grid.hpp>

#include "san_slam/delta_encoder.hpp"
#include "san_slam/local_slam_node.hpp"

using namespace san_slam;

namespace
{

nav_msgs::msg::OccupancyGrid makeGrid(
  int w, int h, int8_t fill,
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
  EXPECT_EQ(encodeCurrent(-1), DELTA_NO_CHANGE);     // unknown
  EXPECT_EQ(encodeCurrent(0), DELTA_FREE);
  EXPECT_EQ(encodeCurrent(40), DELTA_FREE);          // below 50
  EXPECT_EQ(encodeCurrent(50), DELTA_OCCUPIED);
  EXPECT_EQ(encodeCurrent(100), DELTA_OCCUPIED);
}

TEST(DeltaEncoder, FirstDeltaReportsEveryChangedCell) {
  std::vector<int8_t> prev;       // empty -> first call
  std::vector<int8_t> cur = {-1, 0, 0, 100, 100, -1};
  auto d = computeDelta(prev, cur);
  EXPECT_EQ(d[0], DELTA_NO_CHANGE);     // unknown reported as no-change
  EXPECT_EQ(d[1], DELTA_FREE);
  EXPECT_EQ(d[2], DELTA_FREE);
  EXPECT_EQ(d[3], DELTA_OCCUPIED);
  EXPECT_EQ(d[4], DELTA_OCCUPIED);
  EXPECT_EQ(d[5], DELTA_NO_CHANGE);
}

TEST(DeltaEncoder, UnchangedCellsMarkedNoChange) {
  std::vector<int8_t> prev = {0, 0, 100, 100};
  std::vector<int8_t> cur = {0, 0, 100, 100};
  auto d = computeDelta(prev, cur);
  for (auto v : d) {
    EXPECT_EQ(v, DELTA_NO_CHANGE);
  }
}

TEST(DeltaEncoder, ChangedCellReportsNewValue) {
  std::vector<int8_t> prev = {0, 0, 100};
  std::vector<int8_t> cur = {100, 0, 0};
  auto d = computeDelta(prev, cur);
  EXPECT_EQ(d[0], DELTA_OCCUPIED);      // 0 -> 100
  EXPECT_EQ(d[1], DELTA_NO_CHANGE);     // 0 -> 0
  EXPECT_EQ(d[2], DELTA_FREE);          // 100 -> 0
}

// ─── P1-5 part 2: encoder threshold exact boundary (49 vs 50) ─────────
//
// Nav2 occupancy grid convention: 0..49 = free, 50..100 = occupied.
// The threshold at 50 is the most failure-sensitive boundary in this
// helper — an off-by-one here would re-classify all "borderline"
// cells the entire SLAM stack downstream. Test pins the exact edge.
TEST(DeltaEncoder, EncodingThresholdExactBoundaries) {
  // Free side
  EXPECT_EQ(encodeCurrent(0), DELTA_FREE);
  EXPECT_EQ(encodeCurrent(1), DELTA_FREE);
  EXPECT_EQ(encodeCurrent(48), DELTA_FREE);
  EXPECT_EQ(encodeCurrent(49), DELTA_FREE) << "49 is the LAST free value";

  // Occupied side
  EXPECT_EQ(encodeCurrent(50), DELTA_OCCUPIED)
    << "50 is the FIRST occupied value (Nav2 convention)";
  EXPECT_EQ(encodeCurrent(51), DELTA_OCCUPIED);
  EXPECT_EQ(encodeCurrent(99), DELTA_OCCUPIED);
  EXPECT_EQ(encodeCurrent(100), DELTA_OCCUPIED);

  // Unknown side — anything < 0 is unknown
  EXPECT_EQ(encodeCurrent(-1), DELTA_NO_CHANGE);
  EXPECT_EQ(encodeCurrent(-128), DELTA_NO_CHANGE);
}

// ─── P1-5 part 2: computeDelta with scattered unknown cells ───────────
//
// Real-world LIDAR scans produce sparse occupancy with many
// -1 (unknown) cells interleaved with observed ones. The encoder must
// pass unknowns through as NO_CHANGE without confusing them with the
// "no change since previous" semantic. Tests a 4x3 grid where every
// other cell is unknown.
TEST(DeltaEncoder, ComputeDeltaWithMixedUnknownPattern) {
  // 4 cells × 3 rows = 12 cells; checkerboard of known/unknown.
  // prev empty → first-call path encodes from `cur` directly.
  std::vector<int8_t> prev;     // first call
  std::vector<int8_t> cur = {
    -1, 0, -1, 100,
    0, -1, 50, -1,
    -1, 100, -1, 10,
  };
  auto d = computeDelta(prev, cur);
  ASSERT_EQ(d.size(), 12u);

  EXPECT_EQ(d[0], DELTA_NO_CHANGE);      // -1 unknown
  EXPECT_EQ(d[1], DELTA_FREE);           // 0
  EXPECT_EQ(d[2], DELTA_NO_CHANGE);      // -1
  EXPECT_EQ(d[3], DELTA_OCCUPIED);       // 100
  EXPECT_EQ(d[4], DELTA_FREE);           // 0
  EXPECT_EQ(d[5], DELTA_NO_CHANGE);      // -1
  EXPECT_EQ(d[6], DELTA_OCCUPIED);       // 50 — boundary
  EXPECT_EQ(d[7], DELTA_NO_CHANGE);      // -1
  EXPECT_EQ(d[8], DELTA_NO_CHANGE);      // -1
  EXPECT_EQ(d[9], DELTA_OCCUPIED);       // 100
  EXPECT_EQ(d[10], DELTA_NO_CHANGE);     // -1
  EXPECT_EQ(d[11], DELTA_FREE);          // 10
}

class LocalSlamNodeTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
  }
};

TEST_F(LocalSlamNodeTest, NoMapMeansEmptyDelta) {
  auto node = std::make_shared<LocalSlamNode>();
  auto msg = node->buildDeltaForTest();
  EXPECT_TRUE(msg.occupancy_grid_delta_png.empty());
}

TEST_F(LocalSlamNodeTest, FirstMapProducesPngPayload) {
  auto node = std::make_shared<LocalSlamNode>();
  node->injectMapForTest(makeGrid(64, 64, /*fill=*/ 0));
  auto msg = node->buildDeltaForTest();
  EXPECT_GT(msg.occupancy_grid_delta_png.size(), 8u)
    << "PNG header alone is 8 bytes - expect more";
  EXPECT_NEAR(msg.resolution_m, 0.05f, 1e-6);
}

TEST_F(LocalSlamNodeTest, IdempotentMapHasMinimalDelta) {
  auto node = std::make_shared<LocalSlamNode>();
  auto grid = makeGrid(64, 64, /*fill=*/ 100);
  node->injectMapForTest(grid);
  (void)node->buildDeltaForTest();
  // Inject the same map again; PNG should compress very tightly.
  node->injectMapForTest(grid);
  auto msg = node->buildDeltaForTest();
  // Sanity: payload is at least PNG header bytes but smaller than
  // the raw grid (4096 bytes for a 64x64 image).
  EXPECT_LT(msg.occupancy_grid_delta_png.size(), 4096u);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
