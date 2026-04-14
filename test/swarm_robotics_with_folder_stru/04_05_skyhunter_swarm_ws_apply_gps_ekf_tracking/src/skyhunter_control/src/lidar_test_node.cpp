#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/kdtree/kdtree.h>
#include <omp.h> // Hardware Acceleration

class LidarProcessor : public rclcpp::Node {
public:
  LidarProcessor() : Node("lidar_processor") {
    auto qos = rclcpp::SensorDataQoS();
    subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
      "/robot_02/scan/points", qos, std::bind(&LidarProcessor::callback, this, std::placeholders::_1));
  }

private:
  void callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(*msg, *raw_cloud);
    if (raw_cloud->empty()) return;

    pcl::PointCloud<pcl::PointXYZ>::Ptr processed_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    
    float immediate_danger_dist = 10.0;
    
    // --- STEP 1: ULTRA-FAST PARALLEL FILTERING ---
    // Using OpenMP to use all CPU cores for the loop
    #pragma omp parallel
    {
        pcl::PointCloud<pcl::PointXYZ> thread_cloud;
        float thread_min_dist = 10.0;
        
        #pragma omp for nowait
        for (size_t i = 0; i < raw_cloud->size(); i += 5) { // Skip 80% of points for speed
            const auto& p = raw_cloud->points[i];

            // Filter Z (Keep boxes, ignore floor/ceiling)
            if (p.z < -0.45 || p.z > 0.4) continue;

            // --- THE FIX: SHRINK SELF-FILTER ---
            // Only ignore a tiny 30cm square around the sensor center
            float dist_sq = p.x*p.x + p.y*p.y;
            if (dist_sq < 0.04) continue; 

            float d = std::sqrt(dist_sq);
            if (d < thread_min_dist) thread_min_dist = d;
            
            thread_cloud.push_back(p);
        }

        #pragma omp critical
        {
            processed_cloud->insert(processed_cloud->end(), thread_cloud.begin(), thread_cloud.end());
            if (thread_min_dist < immediate_danger_dist) immediate_danger_dist = thread_min_dist;
        }
    }

    // --- STEP 2: FAST CLUSTERING ---
    std::vector<pcl::PointIndices> cluster_indices;
    if (!processed_cloud->empty()) {
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
        tree->setInputCloud(processed_cloud);
        pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
        ec.setClusterTolerance(0.4); 
        ec.setMinClusterSize(5);
        ec.setSearchMethod(tree);
        ec.setInputCloud(processed_cloud);
        ec.extract(cluster_indices);
    }

    // --- STEP 3: REAL-TIME DASHBOARD ---
    printf("\033[2J\033[1;1H"); // Clear screen
    printf("--- ROBOT 02: REAL-TIME RADAR ---\n");
    printf("Objects: %zu | Closest Point: %.2fm\n", cluster_indices.size(), immediate_danger_dist);
    
    if (immediate_danger_dist < 0.8) {
        printf("\033[1;31m[!!!] EMERGENCY STOP: COLLISION AT %.2fm\033[0m\n", immediate_danger_dist);
    }

    for (size_t i = 0; i < cluster_indices.size(); ++i) {
        float min_d = 10.0;
        float angle = 0.0;
        for (auto idx : cluster_indices[i].indices) {
            const auto& pt = processed_cloud->points[idx];
            float d = std::hypot(pt.x, pt.y);
            if (d < min_d) { 
                min_d = d; 
                angle = std::atan2(pt.y, pt.x) * 180.0 / M_PI; 
            }
        }
        printf("OBJ %zu: Dist %.2fm | Angle %+.1f deg\n", i+1, min_d, angle);
    }
  }
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscription_;
};

int main(int argc, char * argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LidarProcessor>());
  rclcpp::shutdown();
  return 0;
}



// #include <rclcpp/rclcpp.hpp>
// #include <rclcpp/executors/multi_threaded_executor.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl/point_types.h>
// #include <pcl/filters/voxel_grid.h>
// #include <pcl/filters/passthrough.h>
// #include <pcl/segmentation/extract_clusters.h>
// #include <pcl/search/kdtree.h>
// #include <pcl/common/centroid.h>
// #include <cmath>
// #include <string>
// #include <vector>
// #include <limits>
// #include <algorithm>
// #include <omp.h>  // For OpenMP parallelization

// class LidarProcessor : public rclcpp::Node
// {
// public:
//   LidarProcessor() : Node("lidar_processor")
//   {
//     subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//       "/robot_02/scan/points", 
//       10, 
//       std::bind(&LidarProcessor::callback, this, std::placeholders::_1)
//     );
//     RCLCPP_INFO(this->get_logger(), "Subscribed to /robot_02/scan/points");
//   }

// private:
//   void callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
//   {
//     // ── Convert ROS msg → PCL point cloud ────────────────────────────────
//     pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
//     pcl::fromROSMsg(*msg, *cloud);

//     if (cloud->empty()) {
//       RCLCPP_WARN(this->get_logger(), "Received empty point cloud");
//       return;
//     }

//     // ── Downsample ────────────────────────────────────────────────────────
//     pcl::VoxelGrid<pcl::PointXYZ> vg;
//     vg.setInputCloud(cloud);
//     vg.setLeafSize(0.05f, 0.05f, 0.05f);
//     vg.filter(*cloud);

//     // ── Remove points too close / too far ─────────────────────────────────
//     pcl::PassThrough<pcl::PointXYZ> pass;
//     pass.setInputCloud(cloud);
//     pass.setFilterFieldName("x");
//     pass.setFilterLimits(0.2, 10.0);  // Min 0.2m to ignore self, max 10m
//     pass.filter(*cloud);

//     if (cloud->empty()) {
//       RCLCPP_INFO(this->get_logger(), "No points after filtering");
//       return;
//     }

//     // ── Clustering ────────────────────────────────────────────────────────
//     pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
//     tree->setInputCloud(cloud);

//     std::vector<pcl::PointIndices> cluster_indices;
//     pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
//     ec.setClusterTolerance(0.18);
//     ec.setMinClusterSize(30);
//     ec.setMaxClusterSize(8000);
//     ec.setSearchMethod(tree);
//     ec.setInputCloud(cloud);
//     ec.extract(cluster_indices);

//     if (cluster_indices.empty()) {
//       RCLCPP_INFO(this->get_logger(), "No objects detected this scan");
//       return;
//     }

//     // ── Deduplication & struct ────────────────────────────────────────────
//     struct Detection {
//       float dist;
//       float angle_deg;
//       std::string direction;
//       int points;
//       int id;
//     };
//     std::vector<Detection> detections;
//     const float dist_similar_thresh  = 0.12;
//     const float angle_similar_thresh = 6.0;
//     const float too_close_thresh     = 0.5;  // Warn if < this (m)

//     // ── Process clusters in parallel ──────────────────────────────────────
//     int num_clusters = static_cast<int>(cluster_indices.size());
//     #pragma omp parallel for schedule(dynamic)
//     for (int i = 0; i < num_clusters; ++i) {
//       const auto& indices = cluster_indices[i];
//       pcl::PointCloud<pcl::PointXYZ>::Ptr cluster(new pcl::PointCloud<pcl::PointXYZ>);
//       for (const auto& idx : indices.indices) {
//         cluster->push_back((*cloud)[idx]);
//       }

//       if (cluster->empty()) continue;

//       Eigen::Vector4f centroid;
//       pcl::compute3DCentroid(*cluster, centroid);
//       float cx = centroid[0];
//       float cy = centroid[1];

//       float dist = std::sqrt(cx*cx + cy*cy);
//       if (dist < 0.25f) continue;  // Ignore very close (self/noise)

//       float angle_rad  = std::atan2(cy, cx);
//       float angle_deg  = angle_rad * 180.0f / M_PI;

//       std::string direction;
//       if      (angle_deg >= -45.0f  && angle_deg <= 45.0f)   direction = "front";
//       else if (angle_deg >  45.0f   && angle_deg <= 135.0f)  direction = "left";
//       else if (angle_deg < -45.0f   && angle_deg >= -135.0f) direction = "right";
//       else                                                   direction = "rear";

//       // Dedup check (not parallel-safe, but simple: collect first)
//       #pragma omp critical
//       {
//         bool is_duplicate = false;
//         for (const auto& prev : detections) {
//           if (std::abs(dist - prev.dist) < dist_similar_thresh &&
//               std::abs(angle_deg - prev.angle_deg) < angle_similar_thresh) {
//             is_duplicate = true;
//             break;
//           }
//         }
//         if (!is_duplicate) {
//           detections.push_back({dist, angle_deg, direction, static_cast<int>(cluster->size()), i + 1});
//         }
//       }
//     }

//     if (detections.empty()) {
//       RCLCPP_INFO(this->get_logger(), "All clusters were duplicates or too close");
//       return;
//     }

//     // Sort by distance (nearest first)
//     std::sort(detections.begin(), detections.end(), [](const Detection& a, const Detection& b) {
//       return a.dist < b.dist;
//     });

//     // ── Log with red warning for too close ────────────────────────────────
//     for (const auto& det : detections) {
//       if (det.dist < too_close_thresh) {
//         RCLCPP_WARN_STREAM(this->get_logger(), "\033[1;31mWARNING: Object too close!\033[0m "
//           << "[cluster " << det.id << " | " << det.points << " pts] Object → dist: " << det.dist
//           << " m | angle: " << det.angle_deg << "° | dir: " << det.direction);
//       } else {
//         RCLCPP_INFO(this->get_logger(), "[cluster %d | %d pts] Object → dist: %.2f m | angle: %6.1f° | dir: %s",
//           det.id, det.points, det.dist, det.angle_deg, det.direction.c_str());
//       }
//     }
//   }

//   rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscription_;
// };

// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   auto node = std::make_shared<LidarProcessor>();
//   auto executor = std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
//   executor->add_node(node);
//   executor->spin();
//   rclcpp::shutdown();
//   return 0;
// }