#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <std_msgs/msg/string.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h> // For downsampling
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/search/kdtree.h> // For clustering search
#include <pcl/common/centroid.h>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip> // For std::setprecision

// Define the point type we'll be using
using PointT = pcl::PointXYZ;

class LidarProcessorNode : public rclcpp::Node
{
public:
    LidarProcessorNode() : Node("lidar_processor_node")
    {
        // --- Parameters (equivalent to your Python self.ROI_... etc.) ---
        this->declare_parameter<double>("roi.x_min", 0.2);
        this->declare_parameter<double>("roi.x_max", 10.0);
        this->declare_parameter<double>("roi.y_min", -2.5);
        this->declare_parameter<double>("roi.y_max", 2.5);
        this->declare_parameter<double>("roi.z_min", -0.5);
        this->declare_parameter<double>("roi.z_max", 2.0);
        this->declare_parameter<double>("cluster.eps", 0.2);
        this->declare_parameter<int>("cluster.min_samples", 5);

        // --- Subscribers and Publishers ---
        // Define a "Best Effort" QoS profile to match bag files and Gazebo
        auto qos_profile = rclcpp::QoS(rclcpp::KeepLast(5)).best_effort();

        subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/lidar/points", qos_profile,
            std::bind(&LidarProcessorNode::lidar_callback, this, std::placeholders::_1));

        object_publisher_ = this->create_publisher<std_msgs::msg::String>("/lidar/closest_object", 10);

        RCLCPP_INFO(this->get_logger(), "C++ LIDAR Processor node started.");
    }

private:
    void lidar_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // 1. Convert ROS Msg to PCL PointCloud
        pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
        pcl::fromROSMsg(*msg, *cloud);

        // 2. Filter ROI (Region of Interest) using a manual loop for robustness
        pcl::PointCloud<PointT>::Ptr cloud_roi(new pcl::PointCloud<PointT>());
        cloud_roi->reserve(cloud->size()); // Reserve memory for efficiency

        // Get parameters once at the start of the callback
        double x_min = this->get_parameter("roi.x_min").as_double();
        double x_max = this->get_parameter("roi.x_max").as_double();
        double y_min = this->get_parameter("roi.y_min").as_double();
        double y_max = this->get_parameter("roi.y_max").as_double();
        double z_min = this->get_parameter("roi.z_min").as_double();
        double z_max = this->get_parameter("roi.z_max").as_double();

        for (const auto& point : cloud->points)
        {
            // Check if the point is valid and within the defined ROI box
            if (std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
                point.x > x_min && point.x < x_max &&
                point.y > y_min && point.y < y_max &&
                point.z > z_min && point.z < z_max)
            {
                cloud_roi->points.push_back(point);
            }
        }
        cloud_roi->width = cloud_roi->points.size();
        cloud_roi->height = 1;
        cloud_roi->is_dense = true;

        if (cloud_roi->points.empty())
        {
            std_msgs::msg::String out_msg;
            out_msg.data = "No obstacles detected in ROI.";
            object_publisher_->publish(out_msg);
            return;
        }

        // 2.5 Voxel Grid Downsampling (Crucial Step for Performance and Stability)
        pcl::PointCloud<PointT>::Ptr cloud_downsampled(new pcl::PointCloud<PointT>());
        pcl::VoxelGrid<PointT> voxel_filter;
        voxel_filter.setInputCloud(cloud_roi);
        voxel_filter.setLeafSize(0.05f, 0.05f, 0.05f); // 5cm leaf size
        voxel_filter.filter(*cloud_downsampled);

        if (cloud_downsampled->points.empty()) {
            std_msgs::msg::String out_msg;
            out_msg.data = "No points left after downsampling.";
            object_publisher_->publish(out_msg);
            return;
        }

        // 3. Perform Clustering (equivalent to DBSCAN)
        pcl::search::KdTree<PointT>::Ptr tree(new pcl::search::KdTree<PointT>);
        tree->setInputCloud(cloud_downsampled); // Use the cleaner, downsampled cloud

        std::vector<pcl::PointIndices> cluster_indices;
        pcl::EuclideanClusterExtraction<PointT> ec;
        ec.setClusterTolerance(this->get_parameter("cluster.eps").as_double());
        ec.setMinClusterSize(this->get_parameter("cluster.min_samples").as_int());
        ec.setMaxClusterSize(25000);
        ec.setSearchMethod(tree);
        ec.setInputCloud(cloud_downsampled); // Use the downsampled cloud here too
        ec.extract(cluster_indices);

        if (cluster_indices.empty())
        {
            std_msgs::msg::String out_msg;
            out_msg.data = "No clusters found (noise only).";
            object_publisher_->publish(out_msg);
            return;
        }

        // 4. Process and Report ALL Detected Clusters (Objects)
        std::stringstream ss;
        ss << "Multiple Objects Detected (" << cluster_indices.size() << " total):\n";
        ss << "--------------------\n";

        for (const auto& indices : cluster_indices)
        {
            pcl::PointCloud<PointT>::Ptr cloud_cluster(new pcl::PointCloud<PointT>);
            for (const auto& idx : indices.indices)
            {
                // Extract points for the current cluster from the downsampled cloud
                cloud_cluster->points.push_back((*cloud_downsampled)[idx]);
            }
            
            // Calculate the centroid (center) of the current cluster
            Eigen::Vector4f centroid;
            pcl::compute3DCentroid(*cloud_cluster, centroid);
            
            double cx = centroid[0];
            double cy = centroid[1];
            
            // Calculate distance and angle for this specific cluster
            double distance = std::sqrt(cx * cx + cy * cy); // 2D ground distance
            double angle_rad = std::atan2(cy, cx);
            double angle_deg = angle_rad * 180.0 / M_PI;

            std::string direction = "FRONT";
            if (angle_deg > 15.0) direction = "LEFT";
            else if (angle_deg < -15.0) direction = "RIGHT";

            ss << "  - Obj at " << std::fixed << std::setprecision(2) << distance << "m, "
            << std::fixed << std::setprecision(1) << angle_deg << " deg (" << direction << ")\n";
        }
        
        std_msgs::msg::String out_msg;
        out_msg.data = ss.str();
        object_publisher_->publish(out_msg);
    }

    // Declare member variables
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscription_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr object_publisher_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<LidarProcessorNode>());
    rclcpp::shutdown();
    return 0;
}