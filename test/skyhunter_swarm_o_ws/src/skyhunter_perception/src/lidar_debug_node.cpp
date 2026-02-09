// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <visualization_msgs/msg/marker_array.hpp>
// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <pcl/filters/extract_indices.h>
// #include <pcl/segmentation/sac_segmentation.h>
// #include <cmath>
// #include <pcl/filters/filter.h> 

// using PointT = pcl::PointXYZI;

// class LidarDebugNode : public rclcpp::Node
// {
// public:
//     LidarDebugNode() : Node("lidar_debug_node")
//     {
//         this->declare_parameter<std::string>("cloud_topic", "/robot_01/scan/points");
//         std::string topic = this->get_parameter("cloud_topic").as_string();

//         auto qos = rclcpp::SensorDataQoS();
//         sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//             topic, qos, std::bind(&LidarDebugNode::cloud_cb, this, std::placeholders::_1));

//         marker_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("obstacle_markers", 10);

//         RCLCPP_INFO(this->get_logger(), "LIDAR DEBUGGER STARTED with ground removal.");
//     }

// private:
//     void cloud_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
//     {
//         // 1. Convert to PCL
//         pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>);
//         pcl::fromROSMsg(*msg, *cloud);

//         // 2. CRITICAL FIX: Remove NaN/Inf points (The "Sky" rays)
//         std::vector<int> indices;
//         pcl::removeNaNFromPointCloud(*cloud, *cloud, indices);

//         // 3. Safety Check: Is cloud empty after removing NaNs?
//         if (cloud->empty()) {
//             RCLCPP_WARN(this->get_logger(), "Lidar sees NOTHING (Sky/Infinity). Ignoring.");
//             return; 
//         }

//         // 4. Process
//         process_points(cloud, msg->header);
//     }
    
//     void remove_ground_plane(pcl::PointCloud<PointT>::Ptr cloud, 
//                         pcl::PointCloud<PointT>::Ptr ground_removed)
//     {
//         if (cloud->size() < 10) { // Safety check
//             *ground_removed = *cloud;
//             return;
//         }
        
//         pcl::SACSegmentation<PointT> seg;
//         pcl::PointIndices::Ptr inliers(new pcl::PointIndices);
//         pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);
        
//         seg.setOptimizeCoefficients(true);
//         seg.setModelType(pcl::SACMODEL_PLANE);
//         seg.setMethodType(pcl::SAC_RANSAC);
//         seg.setMaxIterations(100);
//         seg.setDistanceThreshold(0.1); // 10cm tolerance for ground
        
//         seg.setInputCloud(cloud);
//         seg.segment(*inliers, *coefficients);
        
//         if (inliers->indices.size() == 0) {
//             // If RANSAC fails (e.g. robot tilted too much), assume NO ground found
//             // Just return the original cloud, but warn us
//             // RCLCPP_WARN(this->get_logger(), "Ground plane not found (Robot tilted?)");
//             *ground_removed = *cloud;
//             return;
//         }
        
//         pcl::ExtractIndices<PointT> extract;
//         extract.setInputCloud(cloud);
//         extract.setIndices(inliers);
//         extract.setNegative(true); // Remove the ground, keep obstacles
//         extract.filter(*ground_removed);
//     }
    
//     void process_points(pcl::PointCloud<PointT>::Ptr cloud, const std_msgs::msg::Header& header)
//     {
//         // Remove ground
//         pcl::PointCloud<PointT>::Ptr ground_removed(new pcl::PointCloud<PointT>);
//         remove_ground_plane(cloud, ground_removed);
        
//         visualization_msgs::msg::MarkerArray markers;
//         int marker_id = 0;
        
//         float min_dist_front = 99.0;
//         int front_obstacles = 0;
        
//         for (const auto& p : *ground_removed) {
//             if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
//                 continue;
//             }
            
//             // Filter vertical range
//             if (p.z < -0.1 || p.z > 2.0) continue;
            
//             float dist = sqrt(p.x*p.x + p.y*p.y);
//             float angle = atan2(p.y, p.x);
            
//             // Front sector
//             if (std::abs(angle) < 0.35 && dist < 10.0) {
//                 if (dist < min_dist_front) min_dist_front = dist;
//                 front_obstacles++;
                
//                 // Create marker
//                 if (marker_id < 200) {
//                     visualization_msgs::msg::Marker marker;
//                     marker.header = header;
//                     marker.ns = "front_obstacles";
//                     marker.id = marker_id++;
//                     marker.type = visualization_msgs::msg::Marker::SPHERE;
//                     marker.action = visualization_msgs::msg::Marker::ADD;
                    
//                     marker.pose.position.x = p.x;
//                     marker.pose.position.y = p.y;
//                     marker.pose.position.z = p.z;
                    
//                     marker.scale.x = marker.scale.y = marker.scale.z = 0.1;
//                     marker.color.a = 0.7;
                    
//                     if (dist < 1.5) {
//                         marker.color.r = 1.0; marker.color.g = 0.0; marker.color.b = 0.0;
//                     } else if (dist < 3.0) {
//                         marker.color.r = 1.0; marker.color.g = 1.0; marker.color.b = 0.0;
//                     } else {
//                         marker.color.r = 0.0; marker.color.g = 1.0; marker.color.b = 0.0;
//                     }
                    
//                     markers.markers.push_back(marker);
//                 }
//             }
//         }
        
//         // Print summary
//         printf("\n================ LIDAR SUMMARY ================\n");
//         printf("Total points: %zu\n", cloud->size());
//         printf("After ground removal: %zu\n", ground_removed->size());
//         printf("Front obstacles: %d\n", front_obstacles);
//         printf("Closest obstacle: %.2fm\n", min_dist_front);
        
//         if (min_dist_front < 1.5) {
//             printf("⚠️  WARNING: Obstacle at %.2fm!\n", min_dist_front);
//         }
//         printf("===============================================\n\n");
        
//         marker_pub_->publish(markers);
//     }

//     rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
//     rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr marker_pub_;
// };

// int main(int argc, char **argv)
// {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<LidarDebugNode>());
//     rclcpp::shutdown();
//     return 0;
// }


#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <cmath>
#include <chrono>

using PointT = pcl::PointXYZ;
using namespace std::chrono_literals;

class LidarDebugNode : public rclcpp::Node
{
public:
    LidarDebugNode() : Node("lidar_debug_node")
    {
        // Parameters
        this->declare_parameter<std::string>("cloud_topic", "/robot_01/scan/points");
        std::string topic = this->get_parameter("cloud_topic").as_string();

        // Statistics tracking
        this->declare_parameter<int>("stats_window", 10);
        stats_window_ = this->get_parameter("stats_window").as_int();

        // Subscribe to Lidar
        auto qos = rclcpp::SensorDataQoS();
        sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            topic, qos, std::bind(&LidarDebugNode::cloud_cb, this, std::placeholders::_1));

        // Publish Visual Markers for RViz
        marker_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("obstacle_markers", 10);

        // Statistics timer
        stats_timer_ = this->create_wall_timer(1s, std::bind(&LidarDebugNode::print_stats, this));

        RCLCPP_INFO(this->get_logger(), "LIDAR DEBUGGER STARTED. Listening to: %s", topic.c_str());
    }

private:
    void cloud_cb(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        pcl::PointCloud<PointT> cloud;
        pcl::fromROSMsg(*msg, cloud);

        // ====== CRITICAL DEBUG INFO ======
        // Print frame and point count
        static size_t last_count = 0;
        size_t current_count = cloud.size();
        
        if (std::abs((int)current_count - (int)last_count) > 1000) {
            RCLCPP_WARN(this->get_logger(), "Point cloud size changed drastically: %zu -> %zu (Frame: %s)", 
                       last_count, current_count, msg->header.frame_id.c_str());
        }
        last_count = current_count;

        // Track statistics
        point_counts_.push_back(current_count);
        if (point_counts_.size() > stats_window_) {
            point_counts_.erase(point_counts_.begin());
        }

        // ====== FILTERING DEBUG ======
        int total_points = 0;
        int filtered_below = 0;
        int filtered_above = 0;
        int filtered_self = 0;
        int kept_points = 0;

        // Variables to track closest obstacles
        float min_dist_front = 99.0;
        float min_dist_left = 99.0;
        float min_dist_right = 99.0;

        // Bounding Box trackers
        float f_min_x = 99, f_max_x = -99, f_min_y = 99, f_max_y = -99;
        float min_z = 99, max_z = -99;  // Track Z range

        // Loop through every single point
        for (const auto& p : cloud.points)
        {
            total_points++;
            
            // Track Z range
            if (p.z < min_z) min_z = p.z;
            if (p.z > max_z) max_z = p.z;

            // 1. FILTER: Underground noise
            if (p.z < -0.2) {
                filtered_below++;
                continue;
            }
            
            // 2. FILTER: Ceiling
            if (p.z > 0.5) {
                filtered_above++;
                continue;
            }

            // 3. FILTER: Self (Robot Body)
            float dist_sq = p.x*p.x + p.y*p.y;
            if (dist_sq < (1.2 * 1.2)) {
                filtered_self++;
                continue;
            }

            kept_points++;

            // Calculate Distance and Angle
            float dist = sqrt(dist_sq);
            float angle = atan2(p.y, p.x); // Radians

            // 4. CLASSIFY SECTORS
            // Front: -20 to +20 degrees
            if (std::abs(angle) < 0.35) {
                if (dist < min_dist_front) min_dist_front = dist;
                
                // Track width of the object in front
                if (dist < 3.0) {
                    if (p.x < f_min_x) f_min_x = p.x;
                    if (p.x > f_max_x) f_max_x = p.x;
                    if (p.y < f_min_y) f_min_y = p.y;
                    if (p.y > f_max_y) f_max_y = f_max_y;
                }
            }
            // Left: +20 to +60 degrees
            else if (angle > 0.35 && angle < 1.05) {
                if (dist < min_dist_left) min_dist_left = dist;
            }
            // Right: -20 to -60 degrees
            else if (angle < -0.35 && angle > -1.05) {
                if (dist < min_dist_right) min_dist_right = dist;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // ====== DEBUG OUTPUT ======
        static int counter = 0;
        if (counter++ % 10 == 0) {  // Print every 10th message
            printf("\n========================================\n");
            printf("Frame: %s\n", msg->header.frame_id.c_str());
            printf("Time: %.3f sec\n", duration.count() / 1000.0);
            printf("Total Points: %d\n", total_points);
            printf("Z-Range: %.2f to %.2f\n", min_z, max_z);
            printf("Filtered: below=%d, above=%d, self=%d\n", 
                   filtered_below, filtered_above, filtered_self);
            printf("Kept Points: %d (%.1f%%)\n", 
                   kept_points, (kept_points * 100.0) / std::max(1, total_points));
            printf("------------------------------------------------\n");
            printf("FRONT: %.2fm  |  LEFT: %.2fm  |  RIGHT: %.2fm\n", 
                (min_dist_front > 10 ? 99 : min_dist_front),
                (min_dist_left > 10 ? 99 : min_dist_left),
                (min_dist_right > 10 ? 99 : min_dist_right));

            if (min_dist_front < 2.0) {
                float width = f_max_y - f_min_y;
                float depth = f_max_x - f_min_x;
                printf(">> OBJECT AHEAD! Width: %.2fm, Depth: %.2fm\n", width, depth);
            }
        }

        // ====== VISUALIZE IN RVIZ ======
        visualization_msgs::msg::MarkerArray markers;
        
        // Marker 1: The Bounding Box of the Front Object
        if (min_dist_front < 3.0 && f_min_x != 99) {
            visualization_msgs::msg::Marker box;
            box.header = msg->header;
            box.ns = "debug_box"; 
            box.id = 0; 
            box.type = 1; // Cube
            box.action = 0;
            
            box.pose.position.x = (f_min_x + f_max_x) / 2.0;
            box.pose.position.y = (f_min_y + f_max_y) / 2.0;
            box.pose.position.z = 0.0;
            
            box.scale.x = (f_max_x - f_min_x);
            box.scale.y = (f_max_y - f_min_y);
            box.scale.z = 0.5;

            box.color.a = 0.5; 
            box.color.r = 1.0; 
            box.color.g = 0.0; 
            box.color.b = 0.0; // Red
            markers.markers.push_back(box);
        }

        // Marker 2: Visualize some sample points (for debugging)
        int sample_count = std::min(50, kept_points);
        int step = std::max(1, kept_points / sample_count);
        
        int visualized = 0;
        for (const auto& p : cloud.points) {
            if (visualized >= sample_count) break;
            
            // Skip filtered points
            if (p.z < -0.2 || p.z > 0.5) continue;
            float dist_sq = p.x*p.x + p.y*p.y;
            if (dist_sq < (1.2 * 1.2)) continue;
            
            visualization_msgs::msg::Marker point;
            point.header = msg->header;
            point.ns = "sample_points";
            point.id = visualized;
            point.type = 2; // Sphere
            point.action = 0;
            
            point.pose.position.x = p.x;
            point.pose.position.y = p.y;
            point.pose.position.z = p.z;
            
            point.scale.x = 0.05;
            point.scale.y = 0.05;
            point.scale.z = 0.05;
            
            // Color by height
            point.color.a = 0.8;
            if (p.z > 0.3) {
                point.color.r = 1.0; point.color.g = 1.0; point.color.b = 0.0; // Yellow
            } else if (p.z < 0) {
                point.color.r = 0.0; point.color.g = 1.0; point.color.b = 1.0; // Cyan
            } else {
                point.color.r = 0.0; point.color.g = 1.0; point.color.b = 0.0; // Green
            }
            
            markers.markers.push_back(point);
            visualized++;
        }

        marker_pub_->publish(markers);
    }

    void print_stats()
    {
        if (point_counts_.empty()) return;
        
        size_t min = *std::min_element(point_counts_.begin(), point_counts_.end());
        size_t max = *std::max_element(point_counts_.begin(), point_counts_.end());
        size_t avg = std::accumulate(point_counts_.begin(), point_counts_.end(), 0) / point_counts_.size();
        
        RCLCPP_INFO(this->get_logger(), "Point Cloud Stats (last %d frames): Min=%zu, Max=%zu, Avg=%zu",
                   stats_window_, min, max, avg);
        
        if ((max - min) > 1000) {
            RCLCPP_WARN(this->get_logger(), "LARGE VARIATION DETECTED! This could indicate frame issues.");
        }
    }

    // Member variables
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr marker_pub_;
    rclcpp::TimerBase::SharedPtr stats_timer_;
    
    std::vector<size_t> point_counts_;
    int stats_window_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<LidarDebugNode>());
    rclcpp::shutdown();
    return 0;
}