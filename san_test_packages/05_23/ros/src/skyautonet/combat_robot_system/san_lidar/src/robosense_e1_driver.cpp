#include "san_lidar/robosense_e1_driver.hpp"

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl_conversions/pcl_conversions.h>

#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <diagnostic_msgs/msg/key_value.hpp>

namespace san_lidar {

using diagnostic_msgs::msg::DiagnosticArray;
using diagnostic_msgs::msg::DiagnosticStatus;
using diagnostic_msgs::msg::KeyValue;
using combat_robot_msgs::msg::ThreatAlert;

RobosenseE1Driver::RobosenseE1Driver()
    : RobosenseE1Driver(rclcpp::NodeOptions())
{}

RobosenseE1Driver::RobosenseE1Driver(const rclcpp::NodeOptions& options)
    : rclcpp::Node("robosense_e1_driver", options)
{
    declareParameters();
    readParameters();
    wireInterfaces();

    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    RCLCPP_INFO(get_logger(),
        "RobosenseE1Driver up: mount=(%.2fm, %.2fm) input=%s",
        mount_height_m_, mount_forward_m_, input_topic_.c_str());
}

void RobosenseE1Driver::declareParameters() {
    declare_parameter<double>("mount_height_m", 0.50);
    declare_parameter<double>("mount_forward_m", 0.25);
    declare_parameter<std::string>("input_topic", "/rslidar_points");
    declare_parameter<std::string>("ground_topic", "/san/lidar/ground");
    declare_parameter<std::string>("obstacle_topic", "/san/lidar/obstacles");

    declare_parameter<int>("ransac_max_iterations", 1000);
    declare_parameter<double>("ransac_distance_threshold_m", 0.05);
    declare_parameter<double>("prefilter_z_min_m", -0.5);
    declare_parameter<double>("prefilter_z_max_m",  3.0);

    // [DCN-2026-006 EXT D-017] severity escalation threshold.
    declare_parameter<int>("ransac_critical_after_fails", 5);
}

void RobosenseE1Driver::readParameters() {
    mount_height_m_  = get_parameter("mount_height_m").as_double();
    mount_forward_m_ = get_parameter("mount_forward_m").as_double();
    input_topic_     = get_parameter("input_topic").as_string();
    ground_topic_    = get_parameter("ground_topic").as_string();
    obstacle_topic_  = get_parameter("obstacle_topic").as_string();

    GroundSegmenterParams gp;
    gp.ransac_max_iterations =
        get_parameter("ransac_max_iterations").as_int();
    gp.ransac_distance_threshold_m =
        static_cast<float>(
            get_parameter("ransac_distance_threshold_m").as_double());
    gp.prefilter_z_min_m =
        static_cast<float>(get_parameter("prefilter_z_min_m").as_double());
    gp.prefilter_z_max_m =
        static_cast<float>(get_parameter("prefilter_z_max_m").as_double());
    segmenter_.setParams(gp);

    critical_after_fails_ = static_cast<uint32_t>(std::max<int64_t>(
        1, get_parameter("ransac_critical_after_fails").as_int()));
}

void RobosenseE1Driver::wireInterfaces() {
    // [Sanitizer-hardening] Single MutuallyExclusive callback group
    // binding the pointcloud sub + 1 Hz diag timer so they cannot run
    // concurrently under MTE. publishThreatAlert() runs from the sub
    // callback, so it inherits the same serialization.
    cb_group_ = create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);
    rclcpp::SubscriptionOptions sub_opts;
    sub_opts.callback_group = cb_group_;

    sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
        input_topic_, rclcpp::SensorDataQoS(),
        std::bind(&RobosenseE1Driver::onPointCloud, this,
                  std::placeholders::_1),
        sub_opts);
        
    ground_pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(
        ground_topic_, rclcpp::SensorDataQoS());
    obstacle_pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(
        obstacle_topic_, rclcpp::SensorDataQoS());
    slope_pub_ = create_publisher<std_msgs::msg::Float32>(
        "/san/lidar/ground_slope_deg",
        rclcpp::QoS(5).reliable());

    // [DCN-2026-006 EXT D-017]
    diag_pub_ = create_publisher<DiagnosticArray>(
        "/diagnostics",
        rclcpp::QoS(10).reliable());
    threat_pub_ = create_publisher<ThreatAlert>(
        "/swarm/threat_alert_raw",
        rclcpp::QoS(50).reliable());
    
    swarm_sub_ = create_subscription<geometry_msgs::msg::PoseArray>(
        "/swarm/poses", 10,
        [this](geometry_msgs::msg::PoseArray::SharedPtr msg) {
            std::lock_guard<std::mutex> lock(swarm_mutex_);
            latest_swarm_poses_ = *msg;
        });

    diag_timer_ = create_wall_timer(
        std::chrono::seconds(1),
        std::bind(&RobosenseE1Driver::publishDiagnostics, this),
        cb_group_);
}

void RobosenseE1Driver::onPointCloud(
    sensor_msgs::msg::PointCloud2::SharedPtr msg)
{
    if (msg == nullptr) return;
    auto pcl_cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    pcl::fromROSMsg(*msg, *pcl_cloud);

    // Apply mount offset (translation only; rotation handled upstream).
    for (auto& p : pcl_cloud->points) {
        p.x += static_cast<float>(mount_forward_m_);
        p.z += static_cast<float>(mount_height_m_);
    }

    std::vector<std::pair<double, double>> local_teammates;
    {
        std::lock_guard<std::mutex> lock(swarm_mutex_);
        geometry_msgs::msg::TransformStamped tf_map_to_base;
        try { 
            tf_map_to_base = tf_buffer_->lookupTransform("base_footprint", "map", tf2::TimePointZero); 
            for (const auto& pose : latest_swarm_poses_.poses) {
                geometry_msgs::msg::PoseStamped p_in, p_out;
                p_in.header.frame_id = "map"; p_in.pose = pose;
                tf2::doTransform(p_in, p_out, tf_map_to_base);
                double d2 = p_out.pose.position.x*p_out.pose.position.x + p_out.pose.position.y*p_out.pose.position.y;
                if (d2 > 0.5) local_teammates.push_back({p_out.pose.position.x, p_out.pose.position.y});
            }
        } catch(...) {}
    }

    pcl::PointCloud<pcl::PointXYZI>::Ptr filtered_cloud(new pcl::PointCloud<pcl::PointXYZI>);
    double filter_radius_sq = 1.0 * 1.0; // 1m stealth radius
    
    // Grid variables for your Void Guard
    const double GRID_RES = 0.20;
    const double GRID_SIZE_M = 40.0;
    const int GRID_CELLS = int(GRID_SIZE_M / GRID_RES); 
    struct GridCell { float min_z = 999.0f; float max_z = -999.0f; int point_count = 0; };
    std::vector<GridCell> grid(GRID_CELLS * GRID_CELLS);

    for (const auto& p : pcl_cloud->points) {
        bool is_teammate = false;
        for (const auto& t : local_teammates) {
            double dx = p.x - t.first;
            double dy = p.y - t.second;
            if ((dx * dx + dy * dy) < filter_radius_sq) { is_teammate = true; break; }
        }
        if (!is_teammate) {
            filtered_cloud->push_back(p);
            // Populate grid for cliff detection
            int gx = int((p.x + (GRID_SIZE_M / 2.0)) / GRID_RES);
            int gy = int((p.y + (GRID_SIZE_M / 2.0)) / GRID_RES);
            if (gx >= 0 && gx < GRID_CELLS && gy >= 0 && gy < GRID_CELLS) {
                int idx = gy * GRID_CELLS + gx;
                grid[idx].point_count++;
            }
        }
    }

    // Void Guard (Cliff Detection)
    int empty_cells = 0, total_cells = 0;
    int check_start_x = int((1.0 + (GRID_SIZE_M / 2.0)) / GRID_RES);
    int check_end_x   = int((2.5 + (GRID_SIZE_M / 2.0)) / GRID_RES);
    int center_y      = int((0.0 + (GRID_SIZE_M / 2.0)) / GRID_RES);
    for (int x = check_start_x; x < check_end_x; ++x) {
        for (int y = center_y - 3; y <= center_y + 3; ++y) {
            int idx = y * GRID_CELLS + x;
            total_cells++;
            if (grid[idx].point_count < 2) empty_cells++;
        }
    }

    if (total_cells > 0 && (float)empty_cells / total_cells > 0.70) {
        // Inject Virtual Wall
        for (double vy = -1.0; vy <= 1.0; vy += 0.1) {
            for (double vz = 0.0; vz <= 1.0; vz += 0.1) {
                pcl::PointXYZI vw;
                vw.x = 1.5; vw.y = vy; vw.z = vz; vw.intensity = 255.0;
                filtered_cloud->push_back(vw);
            }
        }
    }

    // Pass the erased/modified cloud to the client's segmenter
    const auto seg = segmenter_.segment(filtered_cloud);

    // const auto seg = segmenter_.segment(pcl_cloud);
    last_ground_count_   = seg.ground_points   ? seg.ground_points->size()   : 0;
    last_obstacle_count_ = seg.obstacle_points ? seg.obstacle_points->size() : 0;
    last_slope_deg_      = seg.slope_deg;

    // [DCN-2026-006 EXT D-017] Failure ladder.
    if (!seg.valid) {
        ++consecutive_fails_;
        publishThreatAlert(seg.fail_reason);
    } else {
        consecutive_fails_ = 0;
    }

    if (ground_pub_ && seg.ground_points) {
        sensor_msgs::msg::PointCloud2 out;
        pcl::toROSMsg(*seg.ground_points, out);
        out.header = msg->header;
        ground_pub_->publish(out);
    }
    if (obstacle_pub_ && seg.obstacle_points) {
        sensor_msgs::msg::PointCloud2 out;
        pcl::toROSMsg(*seg.obstacle_points, out);
        out.header = msg->header;
        obstacle_pub_->publish(out);
    }
    if (slope_pub_) {
        std_msgs::msg::Float32 s;
        s.data = seg.slope_deg;
        slope_pub_->publish(s);
    }
}

// [DCN-2026-006 EXT D-017] Surface RANSAC failures as ThreatAlert so
// the operator / hub aggregator sees them. Rate-limited (>=500 ms
// between alerts) to keep the bus tidy on persistent failures.
void RobosenseE1Driver::publishThreatAlert(
    GroundSegmenterFailReason reason)
{
    if (!threat_pub_) return;
    const auto now_steady = std::chrono::steady_clock::now();
    const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(
        now_steady - last_threat_at_).count();
    if (since < 500) return;
    last_threat_at_ = now_steady;

    ThreatAlert msg;
    msg.header.stamp = now();
    msg.header.frame_id = "base_link";
    msg.threat_type = ThreatAlert::TYPE_OBSTACLE_BLOCKED;
    msg.severity = (consecutive_fails_ >= critical_after_fails_)
        ? ThreatAlert::SEVERITY_CRITICAL
        : ThreatAlert::SEVERITY_WARNING;
    msg.source_robot_id = get_namespace();
    msg.peer_id = "";
    if (msg.severity == ThreatAlert::SEVERITY_CRITICAL) {
        msg.message_ko =
            "지면 인식 실패 — 라이다 ground plane 미검출 (CRITICAL)";
    } else {
        msg.message_ko =
            "지면 인식 일시 실패 — 라이다 RANSAC 확인 필요";
    }
    msg.detail = std::string("{\"reason\":\"") + toString(reason)
        + "\",\"consecutive\":" + std::to_string(consecutive_fails_)
        + ",\"fail_total\":" + std::to_string(segmenter_.failCount())
        + ",\"success_total\":" + std::to_string(segmenter_.successCount())
        + "}";
    msg.timestamp_ms = static_cast<uint64_t>(
        now().nanoseconds() / 1'000'000ULL);
    msg.instance_count = consecutive_fails_;
    threat_pub_->publish(msg);

    RCLCPP_WARN(get_logger(),
        "RANSAC fail (reason=%s, consecutive=%u, total_fail=%lu)",
        toString(reason), consecutive_fails_,
        static_cast<unsigned long>(segmenter_.failCount()));
}

void RobosenseE1Driver::publishDiagnostics() {
    if (!diag_pub_) return;

    DiagnosticArray arr;
    arr.header.stamp = now();
    DiagnosticStatus st;
    st.name = "san_lidar/ground_segmenter";
    st.hardware_id = "robosense_e1";

    const uint64_t fail = segmenter_.failCount();
    const uint64_t ok   = segmenter_.successCount();
    const bool currently_failing = consecutive_fails_ > 0;
    if (currently_failing) {
        st.level = (consecutive_fails_ >= critical_after_fails_)
            ? DiagnosticStatus::ERROR
            : DiagnosticStatus::WARN;
        st.message = "RANSAC failing";
    } else {
        st.level = DiagnosticStatus::OK;
        st.message = "OK";
    }

    auto kv = [](const std::string& k, const std::string& v) {
        KeyValue p; p.key = k; p.value = v; return p;
    };
    st.values.push_back(kv("consecutive_fails",
        std::to_string(consecutive_fails_)));
    st.values.push_back(kv("fail_total", std::to_string(fail)));
    st.values.push_back(kv("success_total", std::to_string(ok)));
    st.values.push_back(kv("last_slope_deg",
        std::to_string(last_slope_deg_)));
    arr.status.push_back(st);
    diag_pub_->publish(arr);
}

}  // namespace san_lidar
