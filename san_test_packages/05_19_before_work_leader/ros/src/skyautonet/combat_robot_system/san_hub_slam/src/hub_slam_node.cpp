#include "san_hub_slam/hub_slam_node.hpp"

#include <chrono>
#include <regex>

namespace san_hub_slam {

HubSlamNode::HubSlamNode()
    : HubSlamNode(rclcpp::NodeOptions())
{}

HubSlamNode::HubSlamNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("hub_slam_node", options)
{
    declareParameters();
    readParameters();
    aggregator_.setGeometry(width_, height_, resolution_m_);
    wireInterfaces();
    RCLCPP_INFO(get_logger(),
        "HubSlamNode started: agg=%.1fs disc=%.1fs grid=%dx%d @ %.2fm "
        "max_robots=%d (dynamic discovery)",
        aggregation_period_sec_, discovery_period_sec_,
        width_, height_, resolution_m_, max_robots_);
}

void HubSlamNode::declareParameters() {
    declare_parameter<double>("aggregation_period_sec", 5.0);
    declare_parameter<int>("max_robots", 8);
    declare_parameter<double>("discovery_period_sec", 1.0);
    declare_parameter<int>("width", 280);
    declare_parameter<int>("height", 280);
    declare_parameter<double>("resolution_m", 0.05);
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
}

void HubSlamNode::readParameters() {
    aggregation_period_sec_ =
        get_parameter("aggregation_period_sec").as_double();
    max_robots_   = get_parameter("max_robots").as_int();
    discovery_period_sec_ =
        get_parameter("discovery_period_sec").as_double();
    width_        = get_parameter("width").as_int();
    height_       = get_parameter("height").as_int();
    resolution_m_ = static_cast<float>(
        get_parameter("resolution_m").as_double());
    vote_reset_period_sec_ =
        get_parameter("vote_reset_period_sec").as_double();

    PoseGraphOptimizerParams pg;
    pg.max_iterations =
        get_parameter("pose_graph_max_iterations").as_int();
    pg.convergence_threshold =
        get_parameter("pose_graph_convergence_threshold").as_double();
    pose_graph_ = PoseGraphOptimizer(pg);
}

void HubSlamNode::wireInterfaces() {
    aggregate_pub_ = create_publisher<AggregatedMap>(
        "/global/slam_aggregated",
        rclcpp::QoS(1).reliable().transient_local());

    // [DCN-2026-006 EXT D-026] Audit publisher.
    audit_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
        "/diagnostics/hub_slam_audit",
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

    // [DCN-2026-006 EXT — source deep analysis §4.2 / §9.3] Optional
    // periodic vote-tally reset. > 0 enables a wall timer that calls
    // aggregator_.clear() so the uint16_t vote counters cannot saturate
    // in long-form 24 h+ runs. Default 0 disables the timer.
    if (vote_reset_period_sec_ > 0.0) {
        const auto reset_period_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::duration<double>(vote_reset_period_sec_));
        vote_reset_timer_ = create_wall_timer(reset_period_ns, [this]() {
            RCLCPP_INFO(get_logger(),
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

std::size_t HubSlamNode::deltaSubscriptionCount() const {
    return delta_subs_.size();
}

std::vector<std::string> HubSlamNode::filterDeltaTopics(
    const std::map<std::string,
                   std::vector<std::string>>& topic_names_and_types)
{
    // Match /robot_<digits>/local/slam_delta — namespace-only form,
    // not the global node-qualified form.
    static const std::regex kPattern(
        R"(^/robot_\d+/local/slam_delta$)");
    std::vector<std::string> out;
    for (const auto& [name, types] : topic_names_and_types) {
        if (!std::regex_match(name, kPattern)) continue;
        bool right_type = false;
        for (const auto& t : types) {
            if (t == "combat_robot_msgs/msg/SLAMLocalDelta") {
                right_type = true;
                break;
            }
        }
        if (right_type) out.push_back(name);
    }
    return out;
}

void HubSlamNode::discoverProducerTopics() {
    const auto graph = get_topic_names_and_types();
    const auto candidates = filterDeltaTopics(graph);

    rclcpp::QoS qos(5);
    qos.reliable();

    for (const auto& topic : candidates) {
        if (delta_subs_.find(topic) != delta_subs_.end()) continue;
        if (static_cast<int>(delta_subs_.size()) >= max_robots_) {
            RCLCPP_WARN(get_logger(),
                "max_robots=%d reached; ignoring new delta topic '%s' "
                "(raise max_robots to subscribe)",
                max_robots_, topic.c_str());
            break;
        }
        auto sub = create_subscription<LocalDelta>(
            topic, qos,
            std::bind(&HubSlamNode::onDelta, this,
                      std::placeholders::_1));
        delta_subs_.emplace(topic, sub);
        RCLCPP_INFO(get_logger(),
            "dynamic-sub: '%s' (total %zu / cap %d)",
            topic.c_str(), delta_subs_.size(), max_robots_);
    }
}

void HubSlamNode::onDelta(LocalDelta::SharedPtr msg) {
    if (msg == nullptr) return;
    std::lock_guard<std::mutex> lock(mutex_);
    aggregator_.applyDelta(msg->robot_id, msg->occupancy_grid_delta_png);
}

void HubSlamNode::injectDeltaForTest(const LocalDelta& msg) {
    auto p = std::make_shared<LocalDelta>(msg);
    onDelta(p);
}

void HubSlamNode::publishAggregate() {
    if (!aggregate_pub_) return;
    auto msg = buildMessage();
    aggregate_pub_->publish(msg);
    ++published_count_;

    // [DCN-2026-006 EXT D-026] Audit emit. Republished every
    // aggregation_period_sec along with the master grid.
    if (!audit_pub_) return;
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
    const auto contributing  = snap.contributing_cells;
    const auto mismatched    = snap.mismatch_cells;
    // mismatch_ratio = mismatched / contributing (clamped to [0,1]).
    const double mismatch_ratio = (contributing > 0)
        ? static_cast<double>(mismatched) /
          static_cast<double>(contributing)
        : 0.0;

    // Severity thresholds (tunable later via params if needed).
    //   ≥ 10% mismatch → ERROR (alignment drift suspected)
    //   ≥  5% mismatch → WARN
    //   otherwise       → OK
    if      (mismatch_ratio >= 0.10) { ds.level = DS::ERROR;
                                       ds.message = "high SLAM disagreement"; }
    else if (mismatch_ratio >= 0.05) { ds.level = DS::WARN;
                                       ds.message = "moderate SLAM disagreement"; }
    else                             { ds.level = DS::OK;
                                       ds.message = "ok"; }

    auto add = [&](const std::string& k, const std::string& v) {
        diagnostic_msgs::msg::KeyValue kv;
        kv.key = k; kv.value = v;
        ds.values.push_back(kv);
    };
    add("contributing_robots",
        std::to_string(snap.contributing_robots));
    add("contributing_cells", std::to_string(contributing));
    add("mismatch_cells",     std::to_string(mismatched));
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.4f", mismatch_ratio);
        add("mismatch_ratio", buf);
    }
    add("width",         std::to_string(snap.width));
    add("height",        std::to_string(snap.height));

    da.status.push_back(ds);
    audit_pub_->publish(da);
}

combat_robot_msgs::msg::SLAMAggregatedMap
HubSlamNode::publishAggregateForTest()
{
    return buildMessage();
}

combat_robot_msgs::msg::SLAMAggregatedMap HubSlamNode::buildMessage() {
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
    msg.width_cells  = static_cast<uint32_t>(snap.width);
    msg.height_cells = static_cast<uint32_t>(snap.height);
    msg.resolution_m = snap.resolution_m;

    msg.origin.x = 0.0;
    msg.origin.y = 0.0;
    msg.origin.theta = 0.0;
    msg.timestamp_ms = nowMs();
    return msg;
}

uint64_t HubSlamNode::nowMs() const {
    // Static-analysis hardening: nanoseconds() returns rcl_time_point_value_t
    // (int64_t). Before clock init or under sim-time with use_sim_time=true
    // but no /clock yet published, the value can be 0 or negative — an
    // unconditional cast to uint64_t would wrap a negative to ~2^64.
    const auto ns = now().nanoseconds();
    return ns > 0 ? static_cast<uint64_t>(ns / 1'000'000ll) : 0ull;
}

}  // namespace san_hub_slam
