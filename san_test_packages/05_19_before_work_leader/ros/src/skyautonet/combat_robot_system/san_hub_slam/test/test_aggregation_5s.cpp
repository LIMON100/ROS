// SAN v1.3 PHASE 3 - hub aggregation unit test.
//
// Drives MultirobotAggregator + HubSlamNode through their test
// entry points: builds synthetic deltas, applies them onto the
// master grid, and verifies the published SLAMAggregatedMap shape.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/slam_local_delta.hpp>

#include "san_hub_slam/aggregator.hpp"
#include "san_hub_slam/hub_slam_node.hpp"

using namespace san_hub_slam;

namespace {

// Build a raw uint8 delta of size width*height with all 127 except
// the cells listed in `changes` (each pair = (index, value)).
std::vector<uint8_t> makeDeltaRaw(
    int w, int h,
    const std::vector<std::pair<int, uint8_t>>& changes)
{
    std::vector<uint8_t> grid(static_cast<std::size_t>(w) * h, 127);
    for (auto [idx, v] : changes) {
        if (idx >= 0 && idx < static_cast<int>(grid.size())) {
            grid[idx] = v;
        }
    }
    return grid;
}

}  // namespace

TEST(MultirobotAggregator, InitialGridIsUnknown) {
    MultirobotAggregator agg(16, 16);
    const auto& g = agg.globalGrid();
    EXPECT_EQ(g.size(), 256u);
    for (auto v : g) EXPECT_EQ(v, GLOBAL_UNKNOWN);
}

TEST(MultirobotAggregator, RawDeltaUpdatesMatchingCells) {
    MultirobotAggregator agg(4, 4);
    auto delta = makeDeltaRaw(4, 4, {
        {0, GLOBAL_FREE},
        {5, GLOBAL_OCCUPIED},
    });
    EXPECT_TRUE(agg.applyDeltaRaw("3", delta));
    // [D-021] Votes deferred to recomputeGlobal(); call it explicitly
    // before reading the master grid.
    agg.recomputeGlobal();
    EXPECT_EQ(agg.globalGrid()[0], GLOBAL_FREE);
    EXPECT_EQ(agg.globalGrid()[5], GLOBAL_OCCUPIED);
    EXPECT_EQ(agg.globalGrid()[1], GLOBAL_UNKNOWN);
}

TEST(MultirobotAggregator, NoChangeCellsAreSkipped) {
    MultirobotAggregator agg(4, 4);
    std::vector<uint8_t> grid(16, 127);
    EXPECT_TRUE(agg.applyDeltaRaw("3", grid));
    for (auto v : agg.globalGrid()) EXPECT_EQ(v, GLOBAL_UNKNOWN);
}

TEST(MultirobotAggregator, ContributingRobotsIsUniqueIds) {
    MultirobotAggregator agg(4, 4);
    auto delta = makeDeltaRaw(4, 4, {{0, GLOBAL_FREE}});
    agg.applyDeltaRaw("1", delta);
    agg.applyDeltaRaw("2", delta);
    agg.applyDeltaRaw("1", delta);    // duplicate
    EXPECT_EQ(agg.contributingRobotCount(), 2u);
}

TEST(MultirobotAggregator, GeometryMismatchFails) {
    MultirobotAggregator agg(4, 4);
    std::vector<uint8_t> grid(15, 127);
    EXPECT_FALSE(agg.applyDeltaRaw("1", grid));
}

TEST(MultirobotAggregator, SnapshotIsDeepCopy) {
    // Phase 7 deferred / R-3 — snapshot() must return an independent
    // copy so the producer thread can keep writing to the aggregator
    // while a consumer encodes the PNG outside the lock.
    MultirobotAggregator agg(4, 4);
    auto delta = makeDeltaRaw(4, 4, {{0, GLOBAL_OCCUPIED}});
    agg.applyDeltaRaw("a", delta);

    auto snap = agg.snapshot();
    EXPECT_EQ(snap.width, 4);
    EXPECT_EQ(snap.height, 4);
    EXPECT_FLOAT_EQ(snap.resolution_m, agg.resolution_m());
    EXPECT_EQ(snap.contributing_robots, 1u);
    ASSERT_EQ(snap.grid.size(), 16u);
    EXPECT_EQ(snap.grid[0], GLOBAL_OCCUPIED);

    // Mutate the master grid AFTER the snapshot; the snapshot must not
    // see the change.
    // [D-021] Mutate a *different* cell post-snapshot: the original
    // last-write-wins test re-wrote cell 0 from OCCUPIED → FREE, but
    // under Bayesian voting that becomes a 1:1 tie which preserves the
    // previous master (anti-flicker rule). Using a fresh cell keeps the
    // snapshot-independence assertion intact without touching the rule.
    auto delta2 = makeDeltaRaw(4, 4, {{3, GLOBAL_FREE}});
    agg.applyDeltaRaw("b", delta2);
    agg.recomputeGlobal();
    EXPECT_EQ(agg.globalGrid()[3], GLOBAL_FREE);     // live reflects mutation
    EXPECT_EQ(snap.grid[3], GLOBAL_UNKNOWN);         // snapshot was pre-mutation
    EXPECT_EQ(snap.grid[0], GLOBAL_OCCUPIED);        // pre-mutation cell unchanged
}

TEST(MultirobotAggregator, StaticEncodePngMatchesInstanceEncode) {
    // The instance encodeGlobalPng() must produce the same bytes as
    // the static encodePng(grid, w, h) helper called on the same
    // grid, since the lock-free fast path uses the static form.
    MultirobotAggregator agg(8, 8);
    auto delta = makeDeltaRaw(8, 8, {{0, GLOBAL_OCCUPIED}, {7, GLOBAL_FREE}});
    agg.applyDeltaRaw("x", delta);

    auto via_instance = agg.encodeGlobalPng();
    auto via_static   = MultirobotAggregator::encodePng(
        agg.globalGrid(), agg.width(), agg.height());
    EXPECT_EQ(via_instance, via_static);
}

TEST(MultirobotAggregator, StaticEncodePngRejectsGeometryMismatch) {
    std::vector<uint8_t> grid(10, GLOBAL_UNKNOWN);   // 10 cells
    auto buf = MultirobotAggregator::encodePng(grid, 4, 4);   // wants 16
    EXPECT_TRUE(buf.empty());
}

// Static-analysis hardening — geometry clamp (PR #134)
TEST(MultirobotAggregator, NegativeDimensionsAreClampedToEmpty) {
    MultirobotAggregator agg(-1, -1);
    EXPECT_EQ(agg.globalGrid().size(), 0u);
    EXPECT_EQ(agg.width(), 0);
    EXPECT_EQ(agg.height(), 0);
}

TEST(MultirobotAggregator, AbsurdlyLargeDimensionsRejected) {
    MultirobotAggregator agg(100000, 100000);   // > kMaxAxisCells
    EXPECT_EQ(agg.globalGrid().size(), 0u);
}

TEST(MultirobotAggregator, SetGeometryClampsNegative) {
    MultirobotAggregator agg(8, 8);
    EXPECT_EQ(agg.globalGrid().size(), 64u);
    agg.setGeometry(-3, 4, 0.05f);
    EXPECT_EQ(agg.globalGrid().size(), 0u);
}

TEST(MultirobotAggregator, PngRoundTrip) {
    MultirobotAggregator agg(8, 8);
    auto delta = makeDeltaRaw(8, 8, {
        {0, GLOBAL_OCCUPIED},
        {63, GLOBAL_FREE},
    });
    // Need a real PNG payload; use the aggregator itself to encode
    // a temporary grid the same size.
    MultirobotAggregator src(8, 8);
    src.applyDeltaRaw("src", delta);
    const auto png = src.encodeGlobalPng();
    EXPECT_GT(png.size(), 8u);

    // Apply the PNG to a fresh aggregator.
    // NOTE: src.encodeGlobalPng() encoded its global grid (with
    // UNKNOWN sentinel 127 everywhere except our two changes), so the
    // round-trip aggregator should pick up the two known cells.
    EXPECT_TRUE(agg.applyDelta("src", png));
    // [D-021] Force vote → master grid translation before reading.
    agg.recomputeGlobal();
    EXPECT_EQ(agg.globalGrid()[0], GLOBAL_OCCUPIED);
    EXPECT_EQ(agg.globalGrid()[63], GLOBAL_FREE);
}

class HubSlamNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(HubSlamNodeTest, FiveSecondPeriodIsTheDefault) {
    auto node = std::make_shared<HubSlamNode>();
    EXPECT_DOUBLE_EQ(node->aggregationPeriodSec(), 5.0);
}

// Phase 7 deferred — dynamic producer discovery -----------------------
// filterDeltaTopics() is a pure static helper; cover the matching
// regex without spinning a real graph.

TEST(HubSlamDiscovery, FilterAcceptsCanonicalRobotPath) {
    std::map<std::string, std::vector<std::string>> graph;
    graph["/robot_3/local/slam_delta"] =
        {"combat_robot_msgs/msg/SLAMLocalDelta"};
    auto matched = san_hub_slam::HubSlamNode::filterDeltaTopics(graph);
    ASSERT_EQ(matched.size(), 1u);
    EXPECT_EQ(matched[0], "/robot_3/local/slam_delta");
}

TEST(HubSlamDiscovery, FilterAcceptsMultiDigitRobotId) {
    std::map<std::string, std::vector<std::string>> graph;
    graph["/robot_42/local/slam_delta"] =
        {"combat_robot_msgs/msg/SLAMLocalDelta"};
    auto matched = san_hub_slam::HubSlamNode::filterDeltaTopics(graph);
    EXPECT_EQ(matched.size(), 1u);
}

TEST(HubSlamDiscovery, FilterRejectsWrongType) {
    std::map<std::string, std::vector<std::string>> graph;
    graph["/robot_1/local/slam_delta"] =
        {"std_msgs/msg/String"};
    auto matched = san_hub_slam::HubSlamNode::filterDeltaTopics(graph);
    EXPECT_TRUE(matched.empty());
}

TEST(HubSlamDiscovery, FilterRejectsForeignNamespace) {
    std::map<std::string, std::vector<std::string>> graph;
    graph["/robot_1/foo/slam_delta"] =
        {"combat_robot_msgs/msg/SLAMLocalDelta"};
    graph["/foreign/local/slam_delta"] =
        {"combat_robot_msgs/msg/SLAMLocalDelta"};
    graph["/swarm/slam_delta"] =
        {"combat_robot_msgs/msg/SLAMLocalDelta"};
    auto matched = san_hub_slam::HubSlamNode::filterDeltaTopics(graph);
    EXPECT_TRUE(matched.empty());
}

TEST(HubSlamDiscovery, FilterMatchesMultipleRobots) {
    std::map<std::string, std::vector<std::string>> graph;
    graph["/robot_1/local/slam_delta"] =
        {"combat_robot_msgs/msg/SLAMLocalDelta"};
    graph["/robot_2/local/slam_delta"] =
        {"combat_robot_msgs/msg/SLAMLocalDelta"};
    graph["/robot_2/local/other"]     =
        {"combat_robot_msgs/msg/SLAMLocalDelta"};
    auto matched = san_hub_slam::HubSlamNode::filterDeltaTopics(graph);
    EXPECT_EQ(matched.size(), 2u);
}

TEST_F(HubSlamNodeTest, BuildMessageReflectsContributingRobots) {
    auto node = std::make_shared<HubSlamNode>();
    auto& agg = node->aggregator();

    auto delta = makeDeltaRaw(agg.width(), agg.height(),
                              {{0, GLOBAL_OCCUPIED}});
    EXPECT_TRUE(agg.applyDeltaRaw("3", delta));
    EXPECT_TRUE(agg.applyDeltaRaw("5", delta));

    auto msg = node->publishAggregateForTest();
    EXPECT_EQ(msg.contributing_robots, 2u);
    EXPECT_EQ(msg.width_cells, 280u);
    EXPECT_EQ(msg.height_cells, 280u);
    EXPECT_GT(msg.occupancy_grid_png.size(), 8u);
}

TEST_F(HubSlamNodeTest, InjectDeltaForwardsToAggregator) {
    auto node = std::make_shared<HubSlamNode>();

    combat_robot_msgs::msg::SLAMLocalDelta delta;
    delta.robot_id = "4";
    delta.resolution_m = 0.05f;
    // Build a PNG payload of the right shape so applyDelta accepts it.
    MultirobotAggregator src(280, 280);
    auto raw = makeDeltaRaw(280, 280, {{0, GLOBAL_FREE}});
    src.applyDeltaRaw("src", raw);
    delta.occupancy_grid_delta_png = src.encodeGlobalPng();

    node->injectDeltaForTest(delta);
    EXPECT_EQ(node->aggregator().contributingRobotCount(), 1u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
