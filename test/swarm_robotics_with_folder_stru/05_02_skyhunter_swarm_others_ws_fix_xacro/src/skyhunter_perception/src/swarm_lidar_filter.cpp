#include "skyhunter_perception/swarm_lidar_filter.hpp"
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/common/transforms.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <tf2_eigen/tf2_eigen.hpp>
#include <omp.h>
#include <cmath>
#include <vector>
#include <algorithm>

// --- TACTICAL TERRAIN CONSTANTS ---
const double GRID_RES = 0.20;          // 20cm grid cells for high precision
const double GRID_SIZE_M = 40.0;       // 40m x 40m total grid area
const int GRID_CELLS = int(GRID_SIZE_M / GRID_RES); 
const double SLOPE_THRESHOLD = 0.25;   // Max vertical variance in a 20cm cell (~40 degrees)

struct GridCell {
    float min_z = 999.0f;
    float max_z = -999.0f;
    int point_count = 0;
};

SwarmLidarFilter::SwarmLidarFilter() : Node("swarm_lidar_filter") {
    this->declare_parameter<double>("stealth_radius", 1.2);
    double radius = this->get_parameter("stealth_radius").as_double();
    filter_radius_sq_ = radius * radius;

    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    auto qos = rclcpp::SensorDataQoS();
    
    sub_swarm_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
        "/swarm/poses", 10, std::bind(&SwarmLidarFilter::swarm_callback, this, std::placeholders::_1));

    sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
        "scan/points", qos, std::bind(&SwarmLidarFilter::scan_callback, this, std::placeholders::_1));

    // Publishers
    pub_scan_ = this->create_publisher<sensor_msgs::msg::PointCloud2>("scan/points_filtered", qos);
    pub_ground_ = this->create_publisher<sensor_msgs::msg::PointCloud2>("scan/ground_points", qos);

    RCLCPP_INFO(this->get_logger(), "Tactical Terrain Engine Online: Gravity Alignment & Cliff Guard Active.");
}

void SwarmLidarFilter::swarm_callback(const geometry_msgs::msg::PoseArray::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(swarm_mutex_);
    latest_swarm_poses_ = *msg;
    has_swarm_data_ = true;
}

void SwarmLidarFilter::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    if (!has_swarm_data_) { pub_scan_->publish(*msg); return; }

    // =========================================================================
    // GRAVITY ALIGNMENT 
    // =========================================================================
    // We transform the cloud to 'base_footprint' because that frame is always 
    // parallel to the ground map, even if the robot chassis is pitching up.
    std::string target_frame = "base_footprint"; 
    geometry_msgs::msg::TransformStamped tf_lidar_to_base;
    try {
        tf_lidar_to_base = tf_buffer_->lookupTransform(target_frame, msg->header.frame_id, tf2::TimePointZero);
    } catch (...) { 
        pub_scan_->publish(*msg); // Fallback if TF fails
        return; 
    }

    Eigen::Affine3d transform = tf2::transformToEigen(tf_lidar_to_base.transform);
    pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr aligned_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(*msg, *raw_cloud);
    pcl::transformPointCloud(*raw_cloud, *aligned_cloud, transform);

    // --- EXTRACT TEAMMATES  ---
    std::vector<std::pair<double, double>> local_teammates;
    {
        std::lock_guard<std::mutex> lock(swarm_mutex_);
        geometry_msgs::msg::TransformStamped tf_map_to_base;
        try { tf_map_to_base = tf_buffer_->lookupTransform(target_frame, "map", tf2::TimePointZero); } catch(...) { return; }
        
        for (const auto& pose : latest_swarm_poses_.poses) {
            geometry_msgs::msg::PoseStamped p_in, p_out;
            p_in.header.frame_id = "map"; p_in.pose = pose;
            tf2::doTransform(p_in, p_out, tf_map_to_base);
            double d2 = p_out.pose.position.x*p_out.pose.position.x + p_out.pose.position.y*p_out.pose.position.y;
            if (d2 > 0.5) local_teammates.push_back({p_out.pose.position.x, p_out.pose.position.y});
        }
    }

    // --- TERRAIN GRID MAPPING ---
    std::vector<GridCell> grid(GRID_CELLS * GRID_CELLS);
    
    for (const auto& p : aligned_cloud->points) {
        int gx = int((p.x + (GRID_SIZE_M / 2.0)) / GRID_RES);
        int gy = int((p.y + (GRID_SIZE_M / 2.0)) / GRID_RES);
        
        if (gx >= 0 && gx < GRID_CELLS && gy >= 0 && gy < GRID_CELLS) {
            int idx = gy * GRID_CELLS + gx;
            grid[idx].point_count++;
            if (p.z < grid[idx].min_z) grid[idx].min_z = p.z;
            if (p.z > grid[idx].max_z) grid[idx].max_z = p.z;
        }
    }

    // =========================================================================
    // THE VOID GUARD 
    // =========================================================================
    // Look 1.0m to 2.5m directly in front of the tracks. 
    bool cliff_detected = false;
    int check_start_x = int((1.0 + (GRID_SIZE_M / 2.0)) / GRID_RES);
    int check_end_x   = int((2.5 + (GRID_SIZE_M / 2.0)) / GRID_RES);
    int center_y      = int((0.0 + (GRID_SIZE_M / 2.0)) / GRID_RES);

    int empty_cells = 0;
    int total_cells = 0;

    for (int x = check_start_x; x < check_end_x; ++x) {
        for (int y = center_y - 3; y <= center_y + 3; ++y) { // ~1.2m width ahead
            int idx = y * GRID_CELLS + x;
            total_cells++;
            if (grid[idx].point_count < 2) { // Less than 2 points means a void
                empty_cells++;
            }
        }
    }

    // If more than 70% of the ground ahead is missing, we are at a cliff!
    if ((float)empty_cells / total_cells > 0.70) {
        cliff_detected = true;
    }

    // --- FILTERING LOOP (Parallel) ---
    pcl::PointCloud<pcl::PointXYZ>::Ptr obstacle_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr ground_cloud(new pcl::PointCloud<pcl::PointXYZ>);

    #pragma omp parallel for
    for (size_t i = 0; i < aligned_cloud->points.size(); ++i) {
        const auto& p = aligned_cloud->points[i];

        // Teammate Check (Stealth Field)
        bool is_teammate = false;
        for (int t = 0; t < (int)local_teammates.size(); ++t) {
            double dx = p.x - local_teammates[t].first;
            double dy = p.y - local_teammates[t].second;
            if ((dx * dx + dy * dy) < filter_radius_sq_) { is_teammate = true; break; }
        }
        if (is_teammate) continue; // Drop point

        // Terrain Classification
        int gx = int((p.x + (GRID_SIZE_M / 2.0)) / GRID_RES);
        int gy = int((p.y + (GRID_SIZE_M / 2.0)) / GRID_RES);
        
        if (gx >= 0 && gx < GRID_CELLS && gy >= 0 && gy < GRID_CELLS) {
            int idx = gy * GRID_CELLS + gx;
            float cell_height_diff = grid[idx].max_z - grid[idx].min_z;

            if (cell_height_diff < SLOPE_THRESHOLD) {
                #pragma omp critical
                ground_cloud->push_back(p);
            } else {
                // Keep only points that are actually sticking up from the ground
                if (p.z > grid[idx].min_z + 0.15) {
                    #pragma omp critical
                    obstacle_cloud->push_back(p);
                }
            }
        }
    }

    // =========================================================================
    //  VIRTUAL WALL INJECTION
    // =========================================================================
    if (cliff_detected) {
        // Build a fake wall 1.5m in front of the robot to force Nav2 to stop!
        for (double vy = -1.0; vy <= 1.0; vy += 0.1) {
            for (double vz = 0.0; vz <= 1.0; vz += 0.1) {
                obstacle_cloud->push_back(pcl::PointXYZ(1.5, vy, vz));
            }
        }
        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 500, "CLIFF DETECTED! Injecting Virtual Wall.");
    }

    // --- BUILD AND PUBLISH ---
    sensor_msgs::msg::PointCloud2 obs_msg, gnd_msg;
    pcl::toROSMsg(*obstacle_cloud, obs_msg);
    pcl::toROSMsg(*ground_cloud, gnd_msg);
    
    // CRITICAL: We changed the frame to base_footprint!
    obs_msg.header.stamp = msg->header.stamp;
    obs_msg.header.frame_id = target_frame; 
    gnd_msg.header = obs_msg.header;

    pub_scan_->publish(obs_msg);     // Nav2 looks at this
    pub_ground_->publish(gnd_msg);
}

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SwarmLidarFilter>());
    rclcpp::shutdown();
    return 0;
}