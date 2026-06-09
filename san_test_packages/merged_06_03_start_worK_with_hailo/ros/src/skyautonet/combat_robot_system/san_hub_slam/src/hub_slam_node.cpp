// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_hub_slam/hub_slam_node.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <regex>
#include <string>
#include <vector>

namespace san_hub_slam
{

namespace
{
// [SLAM-1] Map the string robot_id ("1".."8" per IDS §5.10; tolerant of a
// "robot_3" form too) onto the uint32 node id used by the pose graph. Falls
// back to a stable hash if the id carries no digits, so two distinct
// non-numeric ids never collide onto the same node.
uint32_t robotIdToNode(const std::string & robot_id)
{
  uint32_t n = 0;
  bool any = false;
  for (const char c : robot_id) {
    if (c >= '0' && c <= '9') {
      n = n * 10u + static_cast<uint32_t>(c - '0');
      any = true;
    }
  }
  if (any) {return n;}
  return static_cast<uint32_t>(
    std::hash<std::string>{}(robot_id) & 0x7fffffffu);
}
}  // namespace

HubSlamNode::HubSlamNode()
: HubSlamNode(rclcpp::NodeOptions())
{}

HubSlamNode::HubSlamNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("hub_slam_node", options)
{
  declareParameters();
  readParameters();
  aggregator_.setGeometry(width_, height_, resolution_m_);
  aggregator_.setGlobalOrigin(global_origin_x_, global_origin_y_);
  wireInterfaces();
  RCLCPP_INFO(
    get_logger(),
    "HubSlamNode started: agg=%.1fs disc=%.1fs grid=%dx%d @ %.2fm "
    "max_robots=%d (dynamic discovery)",
    aggregation_period_sec_, discovery_period_sec_,
    width_, height_, resolution_m_, max_robots_);
}

void HubSlamNode::declareParameters()
{
  declare_parameter<double>("aggregation_period_sec", 5.0);
  declare_parameter<int>("max_robots", 8);
  declare_parameter<double>("discovery_period_sec", 1.0);
  declare_parameter<int>("width", 280);
  declare_parameter<int>("height", 280);
  declare_parameter<double>("resolution_m", 0.05);
  // [SLAM-1] World coordinates of the shared global grid's cell (0,0).
  // Default (0,0): the grid's lower-left corner sits at the world origin
  // (the legacy single-robot-at-origin convention). For SLAM producers
  // that center their map on the world origin, set these to the grid's
  // lower-left world corner, e.g. -(width*resolution_m)/2.
  declare_parameter<double>("global_origin_x", 0.0);
  declare_parameter<double>("global_origin_y", 0.0);
  declare_parameter<int>("pose_graph_max_iterations", 50);
  declare_parameter<double>("pose_graph_convergence_threshold", 1e-6);
  // [DCN-2026-006 EXT — source deep analysis §4.2 / §9.3] Periodic
  // vote-tally reset for the D-021 Bayesian aggregator. uint16_t
  // votes saturate at 65,535 — at 4 robots × 12 publish/min that's
  // a ~24 h budget per cell. Demo Day (9/4) and Phase-7 stability
  // runs are well inside the budget, but long-form 24 h+ tests
  // benefit from a periodic clear() so saturated cells don't get
  // pinned. Default 0 disables the timer (production sites that
  // expect <24 h sessions can leave it off and avoid the periodic
  // map-blink).
  declare_parameter<double>("vote_reset_period_sec", 0.0);

  // [SLAM-1 follow-up] Inter-robot loop-closure detection.
  //   loop_closure_period_sec > 0 enables a timer that pairwise-matches the
  //     per-robot accumulated submaps and PUBLISHES the result as a
  //     diagnostic (always safe — read-only). 0 disables detection.
  //   loop_closure_enabled gates ACTIVE correction: only when true does a
  //     confident match become a pose-graph edge that bends the merge.
  //     Default false because a bad loop-closure edge corrupts the whole
  //     graph and the matcher has only been validated on synthetic data.
  declare_parameter<bool>("loop_closure_enabled", false);
  declare_parameter<double>("loop_closure_period_sec", 10.0);
  declare_parameter<double>("loop_closure_search_xy_m", 0.5);
  declare_parameter<double>("loop_closure_search_theta_deg", 15.0);
  declare_parameter<int>("loop_closure_min_overlap_cells", 60);
  declare_parameter<double>("loop_closure_confident_score", 0.70);
  declare_parameter<double>("submap_stale_sec", 30.0);
}

void HubSlamNode::readParameters()
{
  aggregation_period_sec_ =
    get_parameter("aggregation_period_sec").as_double();
  max_robots_ = get_parameter("max_robots").as_int();
  discovery_period_sec_ =
    get_parameter("discovery_period_sec").as_double();
  width_ = get_parameter("width").as_int();
  height_ = get_parameter("height").as_int();
  resolution_m_ = static_cast<float>(
    get_parameter("resolution_m").as_double());
  global_origin_x_ = get_parameter("global_origin_x").as_double();
  global_origin_y_ = get_parameter("global_origin_y").as_double();
  vote_reset_period_sec_ =
    get_parameter("vote_reset_period_sec").as_double();

  PoseGraphOptimizerParams pg;
  pg.max_iterations =
    get_parameter("pose_graph_max_iterations").as_int();
  pg.convergence_threshold =
    get_parameter("pose_graph_convergence_threshold").as_double();
  pose_graph_ = PoseGraphOptimizer(pg);

  loop_closure_enabled_ = get_parameter("loop_closure_enabled").as_bool();
  loop_closure_period_sec_ =
    get_parameter("loop_closure_period_sec").as_double();
  OverlapMatchParams mp;
  mp.search_xy_m = get_parameter("loop_closure_search_xy_m").as_double();
  mp.search_theta_rad =
    get_parameter("loop_closure_search_theta_deg").as_double() *
    M_PI / 180.0;
  mp.min_overlap_cells =
    get_parameter("loop_closure_min_overlap_cells").as_int();
  mp.confident_score =
    get_parameter("loop_closure_confident_score").as_double();
  matcher_ = OverlapMatcher(mp);

  submap_stale_sec_ = get_parameter("submap_stale_sec").as_double();
}

void HubSlamNode::wireInterfaces()
{
  aggregate_pub_ = create_publisher<AggregatedMap>(
    "/global/slam_aggregated",
    rclcpp::QoS(1).reliable().transient_local());

  // [DCN-2026-006 EXT D-026] Audit publisher.
  audit_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
    "/diagnostics/hub_slam_audit",
    rclcpp::QoS(rclcpp::KeepLast(5)).reliable());

  // [SLAM-1 follow-up] Loop-closure diagnostics publisher.
  loop_closure_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
    "/diagnostics/hub_slam_loop_closure",
    rclcpp::QoS(rclcpp::KeepLast(5)).reliable());

  const auto publish_period_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::duration<double>(aggregation_period_sec_));
  publish_timer_ = create_wall_timer(
    publish_period_ns,
    std::bind(&HubSlamNode::publishAggregate, this));

  // Phase 7 deferred: dynamic producer discovery. Fire one
  // discovery cycle immediately so subscriptions exist before the
  // first publish tick when producers are already up, then poll
  // at discovery_period_sec to catch newcomers.
  discoverProducerTopics();
  const auto discovery_period_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::duration<double>(discovery_period_sec_));
  discovery_timer_ = create_wall_timer(
    discovery_period_ns,
    std::bind(&HubSlamNode::discoverProducerTopics, this));

  // [SLAM-1 follow-up] Loop-closure detection timer. Always read-only
  // (publishes diagnostics); only adds pose-graph edges when
  // loop_closure_enabled. period <= 0 disables detection entirely.
  if (loop_closure_period_sec_ > 0.0) {
    const auto lc_period_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(loop_closure_period_sec_));
    loop_closure_timer_ = create_wall_timer(
      lc_period_ns, std::bind(&HubSlamNode::runLoopClosure, this));
  }

  // [DCN-2026-006 EXT — source deep analysis §4.2 / §9.3] Optional
  // periodic vote-tally reset. > 0 enables a wall timer that calls
  // aggregator_.clear() so the uint16_t vote counters cannot saturate
  // in long-form 24 h+ runs. Default 0 disables the timer.
  if (vote_reset_period_sec_ > 0.0) {
    const auto reset_period_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(vote_reset_period_sec_));
    vote_reset_timer_ = create_wall_timer(
      reset_period_ns, [this]() {
        RCLCPP_INFO(
          get_logger(),
          "Periodic vote-tally reset (every %.0f s) — saturation guard",
          vote_reset_period_sec_);
        // [Sanitizer-hardening] aggregator_ is otherwise touched only
        // under mutex_ (onDelta / publishAggregate / buildMessage).
        // Without this lock the reset timer would race against an
        // in-flight applyDelta and corrupt the vote counters mid-
        // iteration under MultiThreadedExecutor.
        std::lock_guard<std::mutex> lock(mutex_);
        aggregator_.clear();
      });
  }
}

std::size_t HubSlamNode::deltaSubscriptionCount() const
{
  return delta_subs_.size();
}

std::vector<std::string> HubSlamNode::filterDeltaTopics(
  const std::map<std::string,
  std::vector<std::string>> & topic_names_and_types)
{
  // Match /robot_<digits>/local/slam_delta — namespace-only form,
  // not the global node-qualified form.
  static const std::regex kPattern(
    R"(^/robot_\d+/local/slam_delta$)");
  std::vector<std::string> out;
  for (const auto & [name, types] : topic_names_and_types) {
    if (!std::regex_match(name, kPattern)) {continue;}
    bool right_type = false;
    for (const auto & t : types) {
      if (t == "combat_robot_msgs/msg/SLAMLocalDelta") {
        right_type = true;
        break;
      }
    }
    if (right_type) {out.push_back(name);}
  }
  return out;
}

void HubSlamNode::discoverProducerTopics()
{
  const auto graph = get_topic_names_and_types();
  const auto candidates = filterDeltaTopics(graph);

  rclcpp::QoS qos(5);
  qos.reliable();

  for (const auto & topic : candidates) {
    if (delta_subs_.find(topic) != delta_subs_.end()) {continue;}
    if (static_cast<int>(delta_subs_.size()) >= max_robots_) {
      RCLCPP_WARN(
        get_logger(),
        "max_robots=%d reached; ignoring new delta topic '%s' "
        "(raise max_robots to subscribe)",
        max_robots_, topic.c_str());
      break;
    }
    auto sub = create_subscription<LocalDelta>(
      topic, qos,
      std::bind(
        &HubSlamNode::onDelta, this,
        std::placeholders::_1));
    delta_subs_.emplace(topic, sub);
    RCLCPP_INFO(
      get_logger(),
      "dynamic-sub: '%s' (total %zu / cap %d)",
      topic.c_str(), delta_subs_.size(), max_robots_);
  }
}

void HubSlamNode::onDelta(LocalDelta::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  std::lock_guard<std::mutex> lock(mutex_);

  // [SLAM-1] Pose-graph-aligned merge. Register this robot's self-reported
  // grid origin (SLAMLocalDelta.origin) as its pose-graph prior, run the
  // optimizer (a no-op that returns the prior unchanged when there are no
  // inter-robot loop-closure edges or g2o is unavailable), then project the
  // delta into the shared global grid at the refined pose and the delta's
  // own resolution. This places/rotates each robot's delta in world frame
  // rather than overlaying it cell-for-cell.
  //
  // Falls back to the legacy cell-for-cell overlay when the producer has
  // not populated resolution_m (== 0) or when projection fails to decode,
  // so an older san_slam build still merges.
  if (msg->resolution_m > 0.0f) {
    const uint32_t node_id = robotIdToNode(msg->robot_id);
    pose_graph_.setNodePrior(
      node_id,
      Pose2D{msg->origin.x, msg->origin.y, msg->origin.theta});
    pose_graph_.optimize();

    // Decode the delta ONCE here; both the per-robot submap accumulation
    // and the world-frame projection reuse the decoded grid (avoids a
    // second cv::imdecode of the same payload under the lock).
    std::vector<uint8_t> dgrid;
    int dw = 0;
    int dh = 0;
    if (MultirobotAggregator::decodePng(
        msg->occupancy_grid_delta_png, dgrid, dw, dh))
    {
      // [SLAM-1 follow-up] Fold the delta's known cells into this robot's
      // accumulated submap (a single delta is too sparse to loop-close on).
      accumulateSubmap(
        msg->robot_id, node_id, dgrid, dw, dh,
        msg->resolution_m, msg->timestamp_ms);

      const Pose2D p = pose_graph_.getPose(node_id);
      if (aggregator_.applyDeltaAtDecoded(
          msg->robot_id, dgrid, dw, dh,
          p.x, p.y, p.theta, msg->resolution_m))
      {
        return;
      }
    }
  }

  // Legacy cell-for-cell fallback (producer without resolution_m, or the
  // pose-aware projection above failed to decode).
  // [SLAM-2] applyDelta returns false when the delta is empty/undecodable
  // or its decoded grid dims don't match the hub grid (width_ x height_).
  // Previously such deltas vanished silently — a robot whose local SLAM map
  // isn't exactly the hub geometry would contribute nothing with no
  // diagnostic. Surface it (throttled).
  if (!aggregator_.applyDelta(msg->robot_id, msg->occupancy_grid_delta_png)) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "onDelta: dropped delta from robot '%s' (empty/undecodable or grid "
      "dims != hub %dx%d); it contributes nothing to the aggregate",
      msg->robot_id.c_str(), width_, height_);
  }
}

void HubSlamNode::accumulateSubmap(
  const std::string & robot_id, uint32_t node_id,
  const std::vector<uint8_t> & dgrid, int dw, int dh,
  float resolution_m, uint64_t stamp_ms)
{
  AccumSubmap & acc = submaps_[robot_id];
  acc.node_id = node_id;
  acc.resolution_m = resolution_m;
  acc.stamp_ms = stamp_ms;
  acc.last_seen_ns = now().nanoseconds();     // hub clock — skew-free
  if (acc.grid.size() != dgrid.size()) {
    // First delta (or a geometry change): seed the accumulation.
    acc.grid = dgrid;
    acc.width = dw;
    acc.height = dh;
    return;
  }
  // Overwrite only the cells the delta actually changed (FREE/OCCUPIED);
  // GLOBAL_UNKNOWN (127) means "no change since previous delta".
  for (std::size_t k = 0; k < dgrid.size(); ++k) {
    if (dgrid[k] != GLOBAL_UNKNOWN) {acc.grid[k] = dgrid[k];}
  }
}

void HubSlamNode::injectDeltaForTest(const LocalDelta & msg)
{
  auto p = std::make_shared<LocalDelta>(msg);
  onDelta(p);
}

void HubSlamNode::publishAggregate()
{
  if (!aggregate_pub_) {return;}
  auto msg = buildMessage();
  aggregate_pub_->publish(msg);
  ++published_count_;

  // [DCN-2026-006 EXT D-026] Audit emit. Republished every
  // aggregation_period_sec along with the master grid.
  if (!audit_pub_) {return;}
  MultirobotAggregator::GridSnapshot snap;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    snap = aggregator_.snapshot();
  }
  diagnostic_msgs::msg::DiagnosticArray da;
  da.header.stamp = now();
  da.header.frame_id = "hub_slam_node";

  diagnostic_msgs::msg::DiagnosticStatus ds;
  ds.name = "hub_slam_aggregator/audit";
  ds.hardware_id = "hub";

  using DS = diagnostic_msgs::msg::DiagnosticStatus;
  const auto contributing = snap.contributing_cells;
  const auto mismatched = snap.mismatch_cells;
  // mismatch_ratio = mismatched / contributing (clamped to [0,1]).
  const double mismatch_ratio = (contributing > 0) ?
    static_cast<double>(mismatched) /
    static_cast<double>(contributing) :
    0.0;

  // Severity thresholds (tunable later via params if needed).
  //   ≥ 10% mismatch → ERROR (alignment drift suspected)
  //   ≥  5% mismatch → WARN
  //   otherwise       → OK
  if (mismatch_ratio >= 0.10) {
    ds.level = DS::ERROR;
    ds.message = "high SLAM disagreement";
  } else if (mismatch_ratio >= 0.05) {
    ds.level = DS::WARN;
    ds.message = "moderate SLAM disagreement";
  } else {
    ds.level = DS::OK;
    ds.message = "ok";
  }

  auto add = [&](const std::string & k, const std::string & v) {
      diagnostic_msgs::msg::KeyValue kv;
      kv.key = k; kv.value = v;
      ds.values.push_back(kv);
    };
  add(
    "contributing_robots",
    std::to_string(snap.contributing_robots));
  add("contributing_cells", std::to_string(contributing));
  add("mismatch_cells", std::to_string(mismatched));
  {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", mismatch_ratio);
    add("mismatch_ratio", buf);
  }
  add("width", std::to_string(snap.width));
  add("height", std::to_string(snap.height));

  da.status.push_back(ds);
  audit_pub_->publish(da);
}

void HubSlamNode::pruneStaleSubmaps(int64_t now_ns)
{
  if (submap_stale_sec_ <= 0.0) {return;}
  const int64_t max_age_ns =
    static_cast<int64_t>(submap_stale_sec_ * 1e9);
  for (auto it = submaps_.begin(); it != submaps_.end(); ) {
    if (now_ns - it->second.last_seen_ns > max_age_ns) {
      it = submaps_.erase(it);
    } else {
      ++it;
    }
  }
}

// [SLAM-1 follow-up] Pairwise loop-closure detection over the per-robot
// accumulated submaps. Always publishes a diagnostic; only mutates the
// pose graph (adds edges + re-optimizes) when loop_closure_enabled.
void HubSlamNode::runLoopClosure()
{
  using DS = diagnostic_msgs::msg::DiagnosticStatus;

  struct Entry
  {
    std::string id;
    uint32_t node;
    Submap map;     // map.origin_* is the snapshot pose at capture time
  };

  // Phase 1 (under lock): snapshot the per-robot submaps + current poses
  // into local copies, then RELEASE the lock. The pairwise match below is
  // O(pairs × candidates × samples) and must not run under mutex_ or it
  // would stall onDelta ingestion under a MultiThreadedExecutor — same
  // snapshot-then-work-outside-lock discipline as publishAggregate().
  std::vector<Entry> entries;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // Drop submaps from robots that have gone silent before matching, so a
    // stale map can't seed a spurious loop closure.
    pruneStaleSubmaps(now().nanoseconds());
    if (submaps_.size() < 2) {return;}
    entries.reserve(submaps_.size());
    for (const auto & [id, acc] : submaps_) {
      if (acc.grid.empty() || acc.resolution_m <= 0.0f) {continue;}
      Submap s;
      s.grid = acc.grid;
      s.width = acc.width;
      s.height = acc.height;
      s.resolution_m = acc.resolution_m;
      const Pose2D p = pose_graph_.getPose(acc.node_id);
      s.origin_x = p.x;
      s.origin_y = p.y;
      s.origin_theta = p.theta;
      entries.push_back({id, acc.node_id, std::move(s)});
    }
  }
  if (entries.size() < 2) {return;}

  // Phase 2 (no lock): match every pair, build the diagnostic, and collect
  // the confident corrections to apply. Uses the snapshot poses only — no
  // shared state is touched here.
  struct Correction
  {
    uint32_t node_i;
    Pose2D pose_i;
    uint32_t node_j;
    Pose2D pose_j_corrected;
  };
  std::vector<Correction> corrections;

  diagnostic_msgs::msg::DiagnosticArray da;
  da.header.stamp = now();
  da.header.frame_id = "hub_slam_node";

  std::size_t overlapping = 0;
  std::size_t confident_n = 0;

  for (std::size_t i = 0; i < entries.size(); ++i) {
    for (std::size_t j = i + 1; j < entries.size(); ++j) {
      const OverlapMatch r = matcher_.match(entries[i].map, entries[j].map);
      if (!r.has_overlap) {continue;}
      ++overlapping;
      if (r.confident) {++confident_n;}

      bool applied = false;
      if (loop_closure_enabled_ && r.confident) {
        const Pose2D pi{
          entries[i].map.origin_x, entries[i].map.origin_y,
          entries[i].map.origin_theta};
        const Pose2D pj_corrected{
          entries[j].map.origin_x + r.dx,
          entries[j].map.origin_y + r.dy,
          entries[j].map.origin_theta + r.dtheta};
        corrections.push_back(
          {entries[i].node, pi, entries[j].node, pj_corrected});
        applied = true;
      }

      DS ds;
      ds.name = "loop_closure/" + entries[i].id + "_" + entries[j].id;
      ds.hardware_id = "hub";
      // A confident match that is NOT being applied (correction disabled)
      // is surfaced as WARN so the operator knows a correction is available.
      if (r.confident && !loop_closure_enabled_) {
        ds.level = DS::WARN;
        ds.message = "loop closure available (correction disabled)";
      } else if (applied) {
        ds.level = DS::OK;
        ds.message = "loop closure applied";
      } else {
        ds.level = DS::OK;
        ds.message = "overlap, no confident closure";
      }
      auto add = [&](const std::string & k, const std::string & v) {
          diagnostic_msgs::msg::KeyValue kv;
          kv.key = k; kv.value = v;
          ds.values.push_back(kv);
        };
      char buf[48];
      std::snprintf(buf, sizeof(buf), "%.3f", r.overlap_area_m2);
      add("overlap_area_m2", buf);
      add("overlap_cells", std::to_string(r.overlap_cells));
      std::snprintf(buf, sizeof(buf), "%.4f", r.score_identity);
      add("score_identity", buf);
      std::snprintf(buf, sizeof(buf), "%.4f", r.best_score);
      add("best_score", buf);
      std::snprintf(buf, sizeof(buf), "%.3f", r.dx);
      add("correction_dx_m", buf);
      std::snprintf(buf, sizeof(buf), "%.3f", r.dy);
      add("correction_dy_m", buf);
      std::snprintf(buf, sizeof(buf), "%.2f", r.dtheta * 180.0 / M_PI);
      add("correction_dtheta_deg", buf);
      add("confident", r.confident ? "true" : "false");
      add("applied", applied ? "true" : "false");
      da.status.push_back(ds);
    }
  }

  // Phase 3 (under lock): rebuild the edge set from the confident matches
  // and re-optimize. Edges are cleared first so stale constraints do not
  // accumulate; priors are preserved.
  if (loop_closure_enabled_) {
    std::lock_guard<std::mutex> lock(mutex_);
    pose_graph_.clearEdges();
    for (const auto & c : corrections) {
      pose_graph_.addEdge(
        PoseGraphOptimizer::relativeEdge(
          c.node_i, c.pose_i, c.node_j, c.pose_j_corrected));
    }
    if (!corrections.empty()) {pose_graph_.optimize();}
  }

  // Summary status.
  DS summary;
  summary.name = "loop_closure/summary";
  summary.hardware_id = "hub";
  summary.level = (confident_n > 0 && !loop_closure_enabled_) ?
    DS::WARN : DS::OK;
  summary.message = loop_closure_enabled_ ?
    "loop closure active" : "detection only (correction disabled)";
  auto adds = [&](const std::string & k, const std::string & v) {
      diagnostic_msgs::msg::KeyValue kv;
      kv.key = k; kv.value = v;
      summary.values.push_back(kv);
    };
  adds("robots", std::to_string(entries.size()));
  adds("overlapping_pairs", std::to_string(overlapping));
  adds("confident_pairs", std::to_string(confident_n));
  adds("applied_pairs", std::to_string(corrections.size()));
  adds("enabled", loop_closure_enabled_ ? "true" : "false");
  da.status.push_back(summary);

  if (loop_closure_pub_) {loop_closure_pub_->publish(da);}
}

combat_robot_msgs::msg::SLAMAggregatedMap
HubSlamNode::publishAggregateForTest()
{
  return buildMessage();
}

combat_robot_msgs::msg::SLAMAggregatedMap HubSlamNode::buildMessage()
{
  AggregatedMap msg;
  msg.header.stamp = now();
  msg.header.frame_id = "map";

  // Phase 7 deferred / R-3: snapshot the aggregator state under
  // lock (cheap memcpy of the uint8 grid), then PNG-encode outside
  // the lock so heavy OpenCV imencode does not block applyDelta()
  // on subscription callbacks.
  MultirobotAggregator::GridSnapshot snap;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    snap = aggregator_.snapshot();
  }
  msg.occupancy_grid_png =
    MultirobotAggregator::encodePng(snap.grid, snap.width, snap.height);
  msg.contributing_robots =
    static_cast<uint32_t>(snap.contributing_robots);
  msg.width_cells = static_cast<uint32_t>(snap.width);
  msg.height_cells = static_cast<uint32_t>(snap.height);
  msg.resolution_m = snap.resolution_m;

  msg.origin.x = global_origin_x_;
  msg.origin.y = global_origin_y_;
  msg.origin.theta = 0.0;
  msg.timestamp_ms = nowMs();
  return msg;
}

uint64_t HubSlamNode::nowMs() const
{
  // Static-analysis hardening: nanoseconds() returns rcl_time_point_value_t
  // (int64_t). Before clock init or under sim-time with use_sim_time=true
  // but no /clock yet published, the value can be 0 or negative — an
  // unconditional cast to uint64_t would wrap a negative to ~2^64.
  const auto ns = now().nanoseconds();
  return ns > 0 ? static_cast<uint64_t>(ns / 1'000'000ll) : 0ull;
}

}  // namespace san_hub_slam
