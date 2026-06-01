// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 3 - Hub UGV SLAM aggregator node.
//
// Subscribes to /robot_*/local/slam_delta via topic-graph
// discovery (Phase 7 deferred: was hardcoded /robot_{1..max}/...).
// New producers are picked up automatically within
// discovery_period_sec; robot ids are not constrained to 1..N.
// max_robots remains a soft cap on the total number of concurrent
// subscriptions.
//
// Each delta is applied onto a global occupancy grid. Every
// aggregation_period_sec the grid is PNG-encoded and published as
// SLAMAggregatedMap on /global/slam_aggregated for the cost-map
// static layer and the operator console.

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/slam_local_delta.hpp>
#include <combat_robot_msgs/msg/slam_aggregated_map.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>

#include "san_hub_slam/aggregator.hpp"
#include "san_hub_slam/pose_graph_optimizer.hpp"
#include "san_hub_slam/overlap_matcher.hpp"

namespace san_hub_slam
{

class HubSlamNode : public rclcpp::Node
{
public:
  HubSlamNode();
  explicit HubSlamNode(const rclcpp::NodeOptions & options);

  // Test accessors.
  MultirobotAggregator & aggregator() {return aggregator_;}
  PoseGraphOptimizer & poseGraph() {return pose_graph_;}
  double aggregationPeriodSec() const {return aggregation_period_sec_;}
  uint32_t publishedAggregateCount() const
  {
    return published_count_;
  }

  // Test entry points.
  void injectDeltaForTest(
    const combat_robot_msgs::msg::SLAMLocalDelta & msg);
  combat_robot_msgs::msg::SLAMAggregatedMap publishAggregateForTest();

  // Test entry: count of currently held delta subscriptions.
  std::size_t deltaSubscriptionCount() const;

  // [SLAM-1 follow-up] Loop-closure test entry points.
  void runLoopClosureForTest() {runLoopClosure();}
  std::size_t submapCount() const {return submaps_.size();}
  std::size_t poseGraphEdgeCount() const {return pose_graph_.edgeCount();}
  bool loopClosureEnabled() const {return loop_closure_enabled_;}
  void pruneStaleSubmapsForTest(int64_t now_ns)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pruneStaleSubmaps(now_ns);
  }

  // Pure helper, exposed for unit testing — given a list of topic
  // names + types, returns the subset that look like SLAM-delta
  // publishers (/robot_*/local/slam_delta).
  static std::vector<std::string> filterDeltaTopics(
    const std::map<std::string,
    std::vector<std::string>> & topic_names_and_types);

private:
  double aggregation_period_sec_ = 5.0;
  int max_robots_ = 8;
  double discovery_period_sec_ = 1.0;
  int width_ = 280;
  int height_ = 280;
  float resolution_m_ = 0.05f;
  // [SLAM-1] World coords of global grid cell (0,0); published in the
  // SLAMAggregatedMap origin so consumers can place the shared grid.
  double global_origin_x_ = 0.0;
  double global_origin_y_ = 0.0;
  // [DCN-2026-006 EXT — source deep analysis §4.2] Periodic reset of
  // the D-021 vote tally. 0 disables (the default for sessions under
  // ~24 h where uint16_t saturation isn't reachable).
  double vote_reset_period_sec_ = 0.0;

  MultirobotAggregator aggregator_;
  PoseGraphOptimizer pose_graph_;

  // [SLAM-1 follow-up] Inter-robot loop-closure detection.
  OverlapMatcher matcher_;
  bool loop_closure_enabled_ = false;        // gate on active correction
  double loop_closure_period_sec_ = 10.0;    // detection/diagnostic cadence

  // Per-robot ACCUMULATED local occupancy submap. SLAMLocalDelta carries
  // only the cells changed since the previous delta, so a single delta is
  // too sparse to match on; we fold each delta's known cells into a
  // persistent grid keyed by robot_id and match on the accumulation.
  struct AccumSubmap
  {
    uint32_t node_id = 0;
    std::vector<uint8_t> grid;     // row-major, FREE/UNKNOWN/OCCUPIED
    int width = 0;
    int height = 0;
    float resolution_m = 0.0f;
    uint64_t stamp_ms = 0;         // producer wall-clock (from the delta)
    int64_t last_seen_ns = 0;      // hub receipt time (skew-free pruning)
  };
  std::map<std::string, AccumSubmap> submaps_;
  // Drop submaps from robots not heard from in submap_stale_sec; a stale
  // submap (departed/crashed robot) must not keep producing loop-closure
  // matches. 0 disables pruning. Pruned during the loop-closure cycle.
  double submap_stale_sec_ = 30.0;

  mutable std::mutex mutex_;
  uint32_t sequence_ = 0;
  uint32_t published_count_ = 0;

  using LocalDelta = combat_robot_msgs::msg::SLAMLocalDelta;
  using AggregatedMap = combat_robot_msgs::msg::SLAMAggregatedMap;

  // Phase 7 deferred (dynamic producer discovery): keyed by topic
  // name so the discovery loop can dedupe on rediscover.
  std::unordered_map<std::string,
    rclcpp::Subscription<LocalDelta>::SharedPtr> delta_subs_;
  rclcpp::Publisher<AggregatedMap>::SharedPtr aggregate_pub_;

  // [DCN-2026-006 EXT D-026] Publish per-publish-cycle audit on
  // /diagnostics/hub_slam_audit so the operator sees aggregation
  // health: contributing robots, contributing cells, mismatch
  // count + mismatch ratio.
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr
    audit_pub_;

  // [SLAM-1 follow-up] Per-pair loop-closure diagnostics, published on
  // /diagnostics/hub_slam_loop_closure each detection cycle.
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr
    loop_closure_pub_;

  rclcpp::TimerBase::SharedPtr publish_timer_;
  rclcpp::TimerBase::SharedPtr discovery_timer_;
  rclcpp::TimerBase::SharedPtr loop_closure_timer_;
  // [DCN-2026-006 EXT — source deep analysis §4.2] Vote-tally reset
  // timer. Nullptr when vote_reset_period_sec_ == 0 (the default).
  rclcpp::TimerBase::SharedPtr vote_reset_timer_;

  void declareParameters();
  void readParameters();
  void wireInterfaces();
  void discoverProducerTopics();

  void onDelta(LocalDelta::SharedPtr msg);
  void accumulateSubmap(
    const std::string & robot_id, uint32_t node_id,
    const std::vector<uint8_t> & dgrid, int dw, int dh,
    float resolution_m, uint64_t stamp_ms);
  void pruneStaleSubmaps(int64_t now_ns);     // assumes mutex_ held
  void publishAggregate();
  void runLoopClosure();
  AggregatedMap buildMessage();

  uint64_t nowMs() const;
};

}  // namespace san_hub_slam
