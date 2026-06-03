// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

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

namespace
{

// Build a raw uint8 delta of size width*height with all 127 except
// the cells listed in `changes` (each pair = (index, value)).
std::vector<uint8_t> makeDeltaRaw(
  int w, int h,
  const std::vector<std::pair<int, uint8_t>> & changes)
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
  const auto & g = agg.globalGrid();
  EXPECT_EQ(g.size(), 256u);
  for (auto v : g) {
    EXPECT_EQ(v, GLOBAL_UNKNOWN);
  }
}

TEST(MultirobotAggregator, RawDeltaUpdatesMatchingCells) {
  MultirobotAggregator agg(4, 4);
  auto delta = makeDeltaRaw(
    4, 4, {
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
  for (auto v : agg.globalGrid()) {
    EXPECT_EQ(v, GLOBAL_UNKNOWN);
  }
}

TEST(MultirobotAggregator, ContributingRobotsIsUniqueIds) {
  MultirobotAggregator agg(4, 4);
  auto delta = makeDeltaRaw(4, 4, {{0, GLOBAL_FREE}});
  agg.applyDeltaRaw("1", delta);
  agg.applyDeltaRaw("2", delta);
  agg.applyDeltaRaw("1", delta);      // duplicate
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
  EXPECT_EQ(agg.globalGrid()[3], GLOBAL_FREE);       // live reflects mutation
  EXPECT_EQ(snap.grid[3], GLOBAL_UNKNOWN);           // snapshot was pre-mutation
  EXPECT_EQ(snap.grid[0], GLOBAL_OCCUPIED);          // pre-mutation cell unchanged
}

TEST(MultirobotAggregator, StaticEncodePngMatchesInstanceEncode) {
  // The instance encodeGlobalPng() must produce the same bytes as
  // the static encodePng(grid, w, h) helper called on the same
  // grid, since the lock-free fast path uses the static form.
  MultirobotAggregator agg(8, 8);
  auto delta = makeDeltaRaw(8, 8, {{0, GLOBAL_OCCUPIED}, {7, GLOBAL_FREE}});
  agg.applyDeltaRaw("x", delta);

  auto via_instance = agg.encodeGlobalPng();
  auto via_static = MultirobotAggregator::encodePng(
    agg.globalGrid(), agg.width(), agg.height());
  EXPECT_EQ(via_instance, via_static);
}

TEST(MultirobotAggregator, StaticEncodePngRejectsGeometryMismatch) {
  std::vector<uint8_t> grid(10, GLOBAL_UNKNOWN);     // 10 cells
  auto buf = MultirobotAggregator::encodePng(grid, 4, 4);     // wants 16
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
  MultirobotAggregator agg(100000, 100000);     // > kMaxAxisCells
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
  auto delta = makeDeltaRaw(
    8, 8, {
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

class HubSlamNodeTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
  }
};

TEST_F(HubSlamNodeTest, FiveSecondPeriodIsTheDefault) {
  auto node = std::make_shared<HubSlamNode>();
  EXPECT_DOUBLE_EQ(node->aggregationPeriodSec(), 5.0);
}

// ─── Phase-7 audit F5: aggregation-period timing edge cases ─────────
//
// The Hub publishes /global/slam_aggregated every aggregation_period_sec
// (default 5s, DCN-2026-006). Robots publish their local deltas
// asynchronously. The audit raised concerns about deltas arriving
// "just before" vs "just after" the timer fire, especially with
// inter-robot clock skew. These tests pin two invariants:
//
//   1. ParamOverride — operators can tune aggregation_period_sec via
//      ros parameter; the configured value is respected end-to-end
//      (regression guard against hardcoded 5.0 sneaking back in).
//   2. BoundaryDeltaInjected — a delta injected BEFORE publishAggregate
//      MUST appear in that publish's contributing_robots count;
//      deltas injected AFTER publishAggregate but BEFORE the next
//      publish must appear in the NEXT one (no silent drops).
//   3. CarryOverAcrossPublishes — votes accumulated in publish N are
//      NOT cleared on publish; subsequent deltas extend the master
//      grid (DCN-2026-006 §4.2 — clear only via vote_reset_period_sec).

TEST_F(HubSlamNodeTest, F5_AggregationPeriodParamOverrideRespected) {
  rclcpp::NodeOptions opts;
  opts.parameter_overrides(
  {
    rclcpp::Parameter("aggregation_period_sec", 0.5)
  });
  auto node = std::make_shared<HubSlamNode>(opts);
  EXPECT_DOUBLE_EQ(node->aggregationPeriodSec(), 0.5)
    << "Operator-tuned aggregation_period_sec (e.g. for bench "
    "testing) must override the 5.0 default.";
}

TEST_F(HubSlamNodeTest, F5_DeltaInjectedBeforePublishAppearsImmediately) {
  auto node = std::make_shared<HubSlamNode>();

  // Build a synthetic delta from robot 7 (a follower not seen yet).
  combat_robot_msgs::msg::SLAMLocalDelta delta;
  delta.robot_id = "7";
  delta.resolution_m = 0.05f;
  MultirobotAggregator src(280, 280);
  src.applyDeltaRaw("src", makeDeltaRaw(280, 280, {{0, GLOBAL_FREE}}));
  delta.occupancy_grid_delta_png = src.encodeGlobalPng();

  // Inject — simulates a delta arriving "just before" the timer fires.
  node->injectDeltaForTest(delta);

  // Publish immediately (no spin needed — direct accessor).
  auto msg = node->publishAggregateForTest();
  EXPECT_EQ(msg.contributing_robots, 1u)
    << "Delta injected BEFORE publishAggregate must appear in "
    "that publish's contributing_robots count — no silent "
    "drop at the boundary.";
}

TEST_F(HubSlamNodeTest, F5_DeltaInjectedAfterPublishAccumulatesIntoNext) {
  auto node = std::make_shared<HubSlamNode>();

  // Round 1: robot 7 contributes.
  combat_robot_msgs::msg::SLAMLocalDelta d1;
  d1.robot_id = "7";
  d1.resolution_m = 0.05f;
  {
    MultirobotAggregator src(280, 280);
    src.applyDeltaRaw(
      "src", makeDeltaRaw(
        280, 280,
        {{0, GLOBAL_FREE}}));
    d1.occupancy_grid_delta_png = src.encodeGlobalPng();
  }
  node->injectDeltaForTest(d1);
  auto msg1 = node->publishAggregateForTest();
  ASSERT_EQ(msg1.contributing_robots, 1u);

  // Round 2: robot 4 arrives AFTER publish 1, BEFORE publish 2.
  // It should accumulate alongside robot 7 (votes persist).
  combat_robot_msgs::msg::SLAMLocalDelta d2;
  d2.robot_id = "4";
  d2.resolution_m = 0.05f;
  {
    MultirobotAggregator src(280, 280);
    src.applyDeltaRaw(
      "src", makeDeltaRaw(
        280, 280,
        {{1, GLOBAL_OCCUPIED}}));
    d2.occupancy_grid_delta_png = src.encodeGlobalPng();
  }
  node->injectDeltaForTest(d2);

  auto msg2 = node->publishAggregateForTest();
  EXPECT_EQ(msg2.contributing_robots, 2u)
    << "Delta from robot 4 arriving between publish 1 and "
    "publish 2 must accumulate into publish 2 (vote counts "
    "from publish 1 are NOT reset — DCN-2026-006 §4.2 says "
    "clear only via vote_reset_period_sec, not on publish).";
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
  graph["/robot_2/local/other"] =
  {"combat_robot_msgs/msg/SLAMLocalDelta"};
  auto matched = san_hub_slam::HubSlamNode::filterDeltaTopics(graph);
  EXPECT_EQ(matched.size(), 2u);
}

TEST_F(HubSlamNodeTest, BuildMessageReflectsContributingRobots) {
  auto node = std::make_shared<HubSlamNode>();
  auto & agg = node->aggregator();

  auto delta = makeDeltaRaw(
    agg.width(), agg.height(),
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

// ─── [SLAM-1 follow-up] Loop-closure wiring ────────────────────────────
//
// Builds two robots whose accumulated submaps observe the same occupied
// structure but with a small origin bias, then drives runLoopClosure().
// With correction disabled (default) no pose-graph edge is added; with it
// enabled a confident match becomes an edge.

namespace
{
// A full (non-sparse) GxG occupancy grid: FREE field with one OCCUPIED
// rectangle, PNG-encoded for a SLAMLocalDelta payload.
combat_robot_msgs::msg::SLAMLocalDelta makeRectDelta(
  const std::string & id, int g, double ox, double oy, float res,
  int x0, int x1, int y0, int y1)
{
  std::vector<uint8_t> grid(static_cast<std::size_t>(g) * g, GLOBAL_FREE);
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      grid[static_cast<std::size_t>(y) * g + x] = GLOBAL_OCCUPIED;
    }
  }
  combat_robot_msgs::msg::SLAMLocalDelta d;
  d.robot_id = id;
  d.resolution_m = res;
  d.origin.x = ox;
  d.origin.y = oy;
  d.origin.theta = 0.0;
  d.occupancy_grid_delta_png = MultirobotAggregator::encodePng(grid, g, g);
  d.timestamp_ms = 1000;
  return d;
}
}  // namespace

TEST_F(HubSlamNodeTest, LoopClosureDisabledAddsNoEdges) {
  auto node = std::make_shared<HubSlamNode>();     // default: disabled
  ASSERT_FALSE(node->loopClosureEnabled());

  node->injectDeltaForTest(makeRectDelta("1", 30, 0.0, 0.0, 0.1f,
    9, 20, 10, 17));
  node->injectDeltaForTest(makeRectDelta("2", 30, 0.2, -0.1, 0.1f,
    9, 20, 10, 17));
  ASSERT_EQ(node->submapCount(), 2u);

  node->runLoopClosureForTest();
  EXPECT_EQ(node->poseGraphEdgeCount(), 0u)
    << "detection-only mode must never add pose-graph edges";
}

TEST_F(HubSlamNodeTest, LoopClosureEnabledAddsEdgeOnConfidentMatch) {
  rclcpp::NodeOptions opts;
  opts.parameter_overrides({rclcpp::Parameter("loop_closure_enabled", true)});
  auto node = std::make_shared<HubSlamNode>(opts);
  ASSERT_TRUE(node->loopClosureEnabled());

  // Same occupied rectangle, robot 2 biased by (+0.2, -0.1) m.
  node->injectDeltaForTest(makeRectDelta("1", 30, 0.0, 0.0, 0.1f,
    9, 20, 10, 17));
  node->injectDeltaForTest(makeRectDelta("2", 30, 0.2, -0.1, 0.1f,
    9, 20, 10, 17));
  ASSERT_EQ(node->submapCount(), 2u);

  node->runLoopClosureForTest();
  EXPECT_GE(node->poseGraphEdgeCount(), 1u)
    << "a confident overlap with correction enabled must add an edge";
}

TEST_F(HubSlamNodeTest, LoopClosureNoEdgeWhenStructureDisagrees) {
  rclcpp::NodeOptions opts;
  opts.parameter_overrides({rclcpp::Parameter("loop_closure_enabled", true)});
  auto node = std::make_shared<HubSlamNode>(opts);

  // Rectangles > search window apart → no confident closure → no edge.
  node->injectDeltaForTest(makeRectDelta("1", 30, 0.0, 0.0, 0.1f,
    2, 9, 2, 9));
  node->injectDeltaForTest(makeRectDelta("2", 30, 0.0, 0.0, 0.1f,
    20, 27, 20, 27));
  ASSERT_EQ(node->submapCount(), 2u);

  node->runLoopClosureForTest();
  EXPECT_EQ(node->poseGraphEdgeCount(), 0u);
}

TEST_F(HubSlamNodeTest, StaleSubmapsArePruned) {
  auto node = std::make_shared<HubSlamNode>();
  const int64_t t0 = node->now().nanoseconds();

  node->injectDeltaForTest(makeRectDelta("1", 30, 0.0, 0.0, 0.1f,
    9, 20, 10, 17));
  node->injectDeltaForTest(makeRectDelta("2", 30, 0.2, -0.1, 0.1f,
    9, 20, 10, 17));
  ASSERT_EQ(node->submapCount(), 2u);

  // "Now" still near capture time → nothing stale (default 30 s window).
  node->pruneStaleSubmapsForTest(t0);
  EXPECT_EQ(node->submapCount(), 2u);

  // 31 s later → both submaps exceed the 30 s window and are dropped.
  node->pruneStaleSubmapsForTest(t0 + static_cast<int64_t>(31e9));
  EXPECT_EQ(node->submapCount(), 0u);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
