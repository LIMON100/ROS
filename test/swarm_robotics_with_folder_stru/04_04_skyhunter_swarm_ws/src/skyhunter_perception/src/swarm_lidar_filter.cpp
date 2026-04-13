#include "skyhunter_perception/swarm_lidar_filter.hpp"

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <omp.h>
#include <cmath>

SwarmLidarFilter::SwarmLidarFilter() : Node("swarm_lidar_filter") {
    // --- 1. PARAMETERS ---
    // The stealth radius around each teammate (0.8m is usually perfect for UGV)
    this->declare_parameter<double>("stealth_radius", 0.8);
    double radius = this->get_parameter("stealth_radius").as_double();
    filter_radius_sq_ = radius * radius; // Squared for ultra-fast math

    // --- 2. TF2 SETUP ---
    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    // --- 3. PUB/SUB ---
    auto qos = rclcpp::SensorDataQoS();
    
    sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
        "/swarm/poses", 10, std::bind(&SwarmLidarFilter::swarm_callback, this, std::placeholders::_1));

    sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
        "scan/points", qos, std::bind(&SwarmLidarFilter::scan_callback, this, std::placeholders::_1));

    pub_scan_ = this->create_publisher<sensor_msgs::msg::PointCloud2>("scan/points_filtered", qos);

    RCLCPP_INFO(this->get_logger(), "Swarm LiDAR Filter Online. Stealth Radius: %.2fm", radius);
}

void SwarmLidarFilter::swarm_callback(const geometry_msgs::msg::PoseArray::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(swarm_mutex_);
    latest_swarm_poses_ = *msg;
    has_swarm_data_ = true;
}

void SwarmLidarFilter::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    // If no swarm data yet, just pass the raw cloud through so Nav2 doesn't crash
    if (!has_swarm_data_) {
        pub_scan_->publish(*msg);
        return;
    }

    // --- A. TRANSFORM SWARM POSES TO LOCAL LIDAR FRAME ---
    // Instead of doing TF on 50,000 LiDAR points, we do it on 7 robots! (O(1) vs O(N))
    std::vector<std::pair<double, double>> local_teammates;
    std::string lidar_frame = msg->header.frame_id;
    std::string swarm_frame = latest_swarm_poses_.header.frame_id;

    geometry_msgs::msg::TransformStamped tf_map_to_lidar;
    try {
        // Look up where the LiDAR is inside the global map right now
        tf_map_to_lidar = tf_buffer_->lookupTransform(
            lidar_frame, swarm_frame, tf2::TimePointZero);
    } catch (const tf2::TransformException & ex) {
        // TF not ready, pass raw cloud
        pub_scan_->publish(*msg);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(swarm_mutex_);
        for (const auto& pose : latest_swarm_poses_.poses) {
            geometry_msgs::msg::PoseStamped p_in, p_out;
            p_in.header.frame_id = swarm_frame;
            p_in.pose = pose;

            tf2::doTransform(p_in, p_out, tf_map_to_lidar);
            
            // Ignore self (distance to lidar ~0) so we don't blind our own obstacle avoidance
            double dist_to_self_sq = (p_out.pose.position.x * p_out.pose.position.x) + 
                                     (p_out.pose.position.y * p_out.pose.position.y);
            if (dist_to_self_sq > 0.1) {
                local_teammates.push_back({p_out.pose.position.x, p_out.pose.position.y});
            }
        }
    }

    // --- B. PARALLEL POINT FILTERING ---
    pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(*msg, *raw_cloud);

    // Create a boolean mask for blazing fast thread-safe filtering
    std::vector<bool> keep_point(raw_cloud->points.size(), true);
    int num_teammates = local_teammates.size();

    #pragma omp parallel for
    for (size_t i = 0; i < raw_cloud->points.size(); ++i) {
        const auto& p = raw_cloud->points[i];
        
        // Check this point against all teammates
        for (int t = 0; t < num_teammates; ++t) {
            double dx = p.x - local_teammates[t].first;
            double dy = p.y - local_teammates[t].second;
            
            // No sqrt() needed! Huge CPU saver.
            if ((dx * dx + dy * dy) < filter_radius_sq_) {
                keep_point[i] = false;
                break; // Point is inside a teammate, drop it immediately
            }
        }
    }

    // --- C. REBUILD AND PUBLISH ---
    pcl::PointCloud<pcl::PointXYZ>::Ptr filtered_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    filtered_cloud->reserve(raw_cloud->points.size()); // Prevent reallocation

    for (size_t i = 0; i < raw_cloud->points.size(); ++i) {
        if (keep_point[i]) {
            filtered_cloud->points.push_back(raw_cloud->points[i]);
        }
    }

    // Convert back to ROS and publish
    sensor_msgs::msg::PointCloud2 output_msg;
    pcl::toROSMsg(*filtered_cloud, output_msg);
    output_msg.header = msg->header; // Keep original timestamp and frame
    pub_scan_->publish(output_msg);

    // static int log_throttle = 0;
    // if (log_throttle++ % 10 == 0) { // Print once per second
    //     int original = raw_cloud->points.size();
    //     int filtered = filtered_cloud->points.size();
    //     int erased = original - filtered;
        
    //     RCLCPP_INFO(this->get_logger(), 
    //         "STEALTH FIELD: [Raw: %d] -> [Filtered: %d] | Erased %d teammate points", 
    //         original, filtered, erased);
    // }
}

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SwarmLidarFilter>());
    rclcpp::shutdown();
    return 0;
}