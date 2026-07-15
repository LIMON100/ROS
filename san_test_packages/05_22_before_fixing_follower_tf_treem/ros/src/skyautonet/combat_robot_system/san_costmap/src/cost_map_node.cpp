#include "san_costmap/cost_map_node.hpp"

#include <pcl_conversions/pcl_conversions.h>

#include <chrono>

using namespace std::chrono_literals;

namespace san_costmap {

CostMapNode::CostMapNode()
    : CostMapNode(rclcpp::NodeOptions())
{}

CostMapNode::CostMapNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("cost_map_node", options),
      last_latency_s_(0.0)
{
    declareParameters();
    readParameters();

    publisher_.setRobotId(robot_id_str_);
    publisher_.setGeometry(width_, height_,
                            static_cast<float>(resolution_m_),
                            static_cast<float>(origin_x_),
                            static_cast<float>(origin_y_));

    wireInterfaces();

    RCLCPP_INFO(get_logger(),
        "CostMapNode started: %dx%d @ %.2fm, KPP target %.1fs",
        width_, height_, resolution_m_, kpp_latency_target_s_);
}

void CostMapNode::declareParameters() {
    declare_parameter<int>("width", DEFAULT_GRID_CELLS);
    declare_parameter<int>("height", DEFAULT_GRID_CELLS);
    declare_parameter<double>("resolution_m", DEFAULT_RESOLUTION_M);
    declare_parameter<double>("origin_x", 0.0);
    declare_parameter<double>("origin_y", 0.0);
    declare_parameter<double>("kpp_latency_target_s", 5.0);
    // robot_id is declared as int to match the squadron-wide
    // convention. The previous string declaration caused
    // InvalidParameterTypeException when squadron.yaml's `robot_id: 0`
    // override (an integer in YAML) hit declare_parameter<std::string>
    // — surfaced once SOP-CI-001 §3 stopped masking TST S20-1.
    declare_parameter<int>("robot_id", 0);
    declare_parameter<std::string>("obstacle_topic", "/san/lidar/obstacles");
    declare_parameter<std::string>("ground_topic", "/san/lidar/ground");
    declare_parameter<int>("update_period_ms", 500);   // ★ v1.5.1 (DCN-2026-003 D-002, 2026-05-13): 1000 → 500 ms (1Hz → 2Hz)
    declare_parameter<int>("publish_period_ms", 100);
}

void CostMapNode::readParameters() {
    width_ = get_parameter("width").as_int();
    height_ = get_parameter("height").as_int();
    resolution_m_ = get_parameter("resolution_m").as_double();
    origin_x_ = get_parameter("origin_x").as_double();
    origin_y_ = get_parameter("origin_y").as_double();
    kpp_latency_target_s_ =
        get_parameter("kpp_latency_target_s").as_double();
    robot_id_str_ = std::to_string(get_parameter("robot_id").as_int());
}

void CostMapNode::wireInterfaces() {
    const auto obstacle_topic =
        get_parameter("obstacle_topic").as_string();
    const auto ground_topic =
        get_parameter("ground_topic").as_string();

    obstacle_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
        obstacle_topic, rclcpp::SensorDataQoS(),
        std::bind(&CostMapNode::onObstacleCloud, this,
                  std::placeholders::_1));
    ground_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
        ground_topic, rclcpp::SensorDataQoS(),
        std::bind(&CostMapNode::onGroundCloud, this,
                  std::placeholders::_1));

    cost_map_pub_ = create_publisher<
        combat_robot_msgs::msg::CostMapUpdate>(
            // PATCH_A-ME-1 (v1.5): topic was "/robot_id/<id>/local/cost_map"
            // — literal "robot_id" was a typo. Correct: "/robot_<id>/local/cost_map".
            "/robot_" + robot_id_str_ + "/local/cost_map",
            rclcpp::QoS(1).reliable());
    operator_alert_pub_ = create_publisher<std_msgs::msg::String>(
        "/operator_alert", rclcpp::QoS(5).reliable());

    const int update_ms   = get_parameter("update_period_ms").as_int();
    const int publish_ms  = get_parameter("publish_period_ms").as_int();
    update_timer_ = create_wall_timer(
        std::chrono::milliseconds(update_ms),
        std::bind(&CostMapNode::runUpdate, this));
    publish_timer_ = create_wall_timer(
        std::chrono::milliseconds(publish_ms),
        std::bind(&CostMapNode::publishCached, this));
}

void CostMapNode::onObstacleCloud(
    sensor_msgs::msg::PointCloud2::SharedPtr msg)
{
    if (msg == nullptr) return;
    std::lock_guard<std::mutex> lock(cloud_mutex_);
    latest_obstacle_msg_ = msg;
}

void CostMapNode::onGroundCloud(
    sensor_msgs::msg::PointCloud2::SharedPtr msg)
{
    if (msg == nullptr) return;
    std::lock_guard<std::mutex> lock(cloud_mutex_);
    latest_ground_msg_ = msg;
}

void CostMapNode::runUpdate() {
    const auto t0 = std::chrono::steady_clock::now();

    // Snapshot the message pointers under the mutex, then run
    // fromROSMsg WITHOUT the lock so a slow conversion doesn't block
    // the sensor callbacks. (DCN-2026-005 D-014.)
    sensor_msgs::msg::PointCloud2::SharedPtr obs_msg, gnd_msg;
    {
        std::lock_guard<std::mutex> lock(cloud_mutex_);
        obs_msg = latest_obstacle_msg_;
        gnd_msg = latest_ground_msg_;
    }
    if (obs_msg) {
        auto pcl_cloud =
            std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
        pcl::fromROSMsg(*obs_msg, *pcl_cloud);
        latest_obstacle_ = pcl_cloud;
    }
    if (gnd_msg) {
        auto pcl_cloud =
            std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
        pcl::fromROSMsg(*gnd_msg, *pcl_cloud);
        latest_ground_ = pcl_cloud;
    }

    publisher_.obstacleLayer().setInputCloud(latest_obstacle_);
    publisher_.traversabilityLayer().setGroundCloud(latest_ground_);

    publisher_.obstacleLayer().updateBounds();
    publisher_.traversabilityLayer().updateCosts();
    publisher_.compose();

    const auto t1 = std::chrono::steady_clock::now();
    const double seconds =
        std::chrono::duration<double>(t1 - t0).count();
    recordLatency(seconds);
}

void CostMapNode::publishCached() {
    if (!cost_map_pub_) return;
    auto msg = publisher_.buildMessage(++sequence_);
    msg.header.stamp = now();
    msg.header.frame_id = "base_link";
    msg.computed_at_ms = static_cast<uint64_t>(
        now().nanoseconds() / 1'000'000ll);
    msg.timestamp_ms = msg.computed_at_ms;
    cost_map_pub_->publish(msg);
    maybePublishOperatorAlert();
}

void CostMapNode::recordLatency(double seconds) {
    last_latency_s_.store(seconds);
    if (seconds > kpp_latency_target_s_) {
        RCLCPP_ERROR(get_logger(),
            "Cost Map cycle %.2fs exceeds %.1fs KPP",
            seconds, kpp_latency_target_s_);
    }
}

void CostMapNode::maybePublishOperatorAlert() {
    const auto& master = publisher_.master();
    bool any_lethal = false;
    for (auto c : master) {
        if (c == COST_LETHAL) { any_lethal = true; break; }
    }
    if (any_lethal) {
        ++consecutive_lethal_;
    } else {
        consecutive_lethal_ = 0;
    }
    if (consecutive_lethal_ >= 3 && operator_alert_pub_) {
        std_msgs::msg::String alert;
        alert.data = "COST_MAP_AVOIDANCE_FAILED: 3 consecutive lethal "
                     "publishes; manual intervention recommended";
        operator_alert_pub_->publish(alert);
    }
}

combat_robot_msgs::msg::CostMapUpdate
CostMapNode::buildOneShotForTest(
    const pcl::PointCloud<pcl::PointXYZI>::Ptr& obstacle_cloud,
    const pcl::PointCloud<pcl::PointXYZI>::Ptr& ground_cloud)
{
    latest_obstacle_ = obstacle_cloud;
    latest_ground_ = ground_cloud;
    runUpdate();
    auto msg = publisher_.buildMessage(++sequence_);
    msg.timestamp_ms = static_cast<uint64_t>(
        now().nanoseconds() / 1'000'000ll);
    msg.computed_at_ms = msg.timestamp_ms;
    return msg;
}

}  // namespace san_costmap
