#include "san_slam/local_slam_node.hpp"

#include <chrono>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>

#include "san_slam/delta_encoder.hpp"

namespace san_slam {

LocalSlamNode::LocalSlamNode()
    : LocalSlamNode(rclcpp::NodeOptions())
{}

LocalSlamNode::LocalSlamNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("local_slam_node", options)
{
    declareParameters();
    readParameters();
    wireInterfaces();
    RCLCPP_INFO(get_logger(),
        "LocalSlamNode started: robot_id=%d map_topic=%s period=%.2fs",
        robot_id_, map_topic_.c_str(), publish_period_sec_);
}

void LocalSlamNode::declareParameters() {
    declare_parameter<int>("robot_id", 0);
    declare_parameter<std::string>("map_topic", "/map");
    declare_parameter<double>("publish_period_sec", 1.0);
}

void LocalSlamNode::readParameters() {
    robot_id_ = static_cast<int>(get_parameter("robot_id").as_int());
    map_topic_ = get_parameter("map_topic").as_string();
    publish_period_sec_ = get_parameter("publish_period_sec").as_double();
    // PATCH_A-ME-1 (v1.5): topic was previously "/robot_id/<id>/local/slam_delta"
    // — literal "robot_id" segment was a typo. Correct ROS 2 convention is
    // "/robot_<id>/local/slam_delta", matching the Hub subscriber side in
    // san_hub_slam/hub_slam_node.cpp.
    delta_topic_ = "/robot_" + std::to_string(robot_id_) + "/local/slam_delta";
}

void LocalSlamNode::wireInterfaces() {
    map_sub_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
        map_topic_, rclcpp::QoS(1).reliable(),
        std::bind(&LocalSlamNode::onMap, this, std::placeholders::_1));
    delta_pub_ = create_publisher<
        combat_robot_msgs::msg::SLAMLocalDelta>(
            delta_topic_, rclcpp::QoS(5).reliable());

    const auto period_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(publish_period_sec_));
    publish_timer_ = create_wall_timer(
        period_ns, std::bind(&LocalSlamNode::publishDelta, this));
}

void LocalSlamNode::onMap(nav_msgs::msg::OccupancyGrid::SharedPtr msg) {
    if (msg == nullptr) return;
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    width_ = static_cast<int>(msg->info.width);
    height_ = static_cast<int>(msg->info.height);
    resolution_m_ = msg->info.resolution;
    origin_.x = msg->info.origin.position.x;
    origin_.y = msg->info.origin.position.y;
    origin_.theta = 0.0;
    current_snapshot_ = msg->data;
    if (coverage_start_ms_ == 0) coverage_start_ms_ = nowMs();
    coverage_end_ms_ = nowMs();
}

void LocalSlamNode::injectMapForTest(
    const nav_msgs::msg::OccupancyGrid& map)
{
    auto p = std::make_shared<nav_msgs::msg::OccupancyGrid>(map);
    onMap(p);
}

void LocalSlamNode::publishDelta() {
    if (!delta_pub_) return;
    if (current_snapshot_.empty()) return;
    auto msg = buildMessage();
    delta_pub_->publish(msg);
    ++published_count_;
}

combat_robot_msgs::msg::SLAMLocalDelta
LocalSlamNode::buildDeltaForTest()
{
    return buildMessage();
}

combat_robot_msgs::msg::SLAMLocalDelta
LocalSlamNode::buildMessage()
{
    combat_robot_msgs::msg::SLAMLocalDelta msg;
    msg.header.stamp = now();
    msg.header.frame_id = "map";
    msg.robot_id = std::to_string(robot_id_);
    msg.origin = origin_;
    msg.resolution_m = resolution_m_;
    msg.coverage_start_ms = coverage_start_ms_;
    msg.coverage_end_ms = coverage_end_ms_;
    msg.timestamp_ms = nowMs();

    std::vector<int8_t> previous;
    std::vector<int8_t> current;
    int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        previous = previous_snapshot_;
        current = current_snapshot_;
        w = width_;
        h = height_;
    }

    if (current.empty() || w <= 0 || h <= 0) {
        return msg;
    }

    const std::vector<uint8_t> delta = computeDelta(previous, current);
    cv::Mat img(h, w, CV_8UC1,
                const_cast<uint8_t*>(delta.data()));
    std::vector<uint8_t> png;
    cv::imencode(".png", img, png);
    msg.occupancy_grid_delta_png = png;

    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        previous_snapshot_ = current_snapshot_;
        coverage_start_ms_ = nowMs();
    }
    return msg;
}

uint64_t LocalSlamNode::nowMs() const {
    return static_cast<uint64_t>(now().nanoseconds() / 1'000'000ll);
}

}  // namespace san_slam
