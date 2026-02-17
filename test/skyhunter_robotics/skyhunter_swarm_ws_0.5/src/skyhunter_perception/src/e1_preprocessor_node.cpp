#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/statistical_outlier_removal.h>

// Define the point type we'll be using
using PointT = pcl::PointXYZ;

// Renamed the class to better describe its function
class E1PreprocessorNode : public rclcpp::Node
{
public:
    E1PreprocessorNode() : Node("e1_preprocessor_node")
    {
        // --- Parameters for the filtering pipeline ---
        this->declare_parameter<double>("roi.x_min", 0.2);
        this->declare_parameter<double>("roi.x_max", 10.0);
        this->declare_parameter<double>("roi.y_min", -2.5);
        this->declare_parameter<double>("roi.y_max", 2.5);
        this->declare_parameter<double>("roi.z_min", -0.5);
        this->declare_parameter<double>("roi.z_max", 2.0);
        this->declare_parameter<double>("voxel_leaf_size", 0.05);
        this->declare_parameter<int>("sor.mean_k", 20);
        this->declare_parameter<double>("sor.std_dev_thresh", 1.0);

        // --- Subscribers and Publishers ---
        auto qos_profile = rclcpp::QoS(rclcpp::KeepLast(5)).best_effort();

        // Subscribe to the RAW LiDAR data
        subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/lidar/points", qos_profile,
            std::bind(&E1PreprocessorNode::cloud_callback, this, std::placeholders::_1));

        // Publish the new, FILTERED point cloud
        filtered_cloud_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>("/lidar/points_filtered", 10);

        RCLCPP_INFO(this->get_logger(), "E1 Preprocessor Node has started.");
    }

private:
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // 1. Convert ROS Msg to PCL PointCloud
        pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
        pcl::fromROSMsg(*msg, *cloud);

        // --- START OF FILTERING PIPELINE ---

        // 2. ROI (Region of Interest) Filtering using a manual loop
        pcl::PointCloud<PointT>::Ptr cloud_roi(new pcl::PointCloud<PointT>());
        cloud_roi->reserve(cloud->size());
        for (const auto& point : cloud->points)
        {
            if (std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
                point.x > this->get_parameter("roi.x_min").as_double() && point.x < this->get_parameter("roi.x_max").as_double() &&
                point.y > this->get_parameter("roi.y_min").as_double() && point.y < this->get_parameter("roi.y_max").as_double() &&
                point.z > this->get_parameter("roi.z_min").as_double() && point.z < this->get_parameter("roi.z_max").as_double())
            {
                cloud_roi->points.push_back(point);
            }
        }

        if (cloud_roi->points.empty()) return; // Nothing to process

        // 3. Voxel Grid Downsampling
        pcl::PointCloud<PointT>::Ptr cloud_downsampled(new pcl::PointCloud<PointT>());
        pcl::VoxelGrid<PointT> voxel_filter;
        voxel_filter.setInputCloud(cloud_roi);
        float leaf_size = this->get_parameter("voxel_leaf_size").as_double();
        voxel_filter.setLeafSize(leaf_size, leaf_size, leaf_size);
        voxel_filter.filter(*cloud_downsampled);

        if (cloud_downsampled->points.empty()) return;

        // 4. Statistical Outlier Removal (SOR)
        pcl::PointCloud<PointT>::Ptr cloud_denoised(new pcl::PointCloud<PointT>());
        pcl::StatisticalOutlierRemoval<PointT> sor_filter;
        sor_filter.setInputCloud(cloud_downsampled);
        sor_filter.setMeanK(this->get_parameter("sor.mean_k").as_int());
        sor_filter.setStddevMulThresh(this->get_parameter("sor.std_dev_thresh").as_double());
        sor_filter.filter(*cloud_denoised);
        
        if (cloud_denoised->points.empty()) return;

        // --- END OF FILTERING PIPELINE ---

        // 5. Convert final PCL cloud back to ROS message and publish
        sensor_msgs::msg::PointCloud2 output_msg;
        pcl::toROSMsg(*cloud_denoised, output_msg);
        output_msg.header = msg->header; // Preserve the original timestamp and frame_id
        filtered_cloud_pub_->publish(output_msg);
    }

    // Declare member variables
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscription_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr filtered_cloud_pub_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<E1PreprocessorNode>());
    rclcpp::shutdown();
    return 0;
}