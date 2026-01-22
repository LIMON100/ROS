#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/common/common.h>
#include <pcl/search/kdtree.h>
#include <pcl/features/normal_3d.h> 

// Grid Map Headers
#include <grid_map_ros/grid_map_ros.hpp>
#include <grid_map_msgs/msg/grid_map.hpp>

// Eigen for PCA
#include <Eigen/Dense>

using PointT = pcl::PointXYZ;

class ElevationMapperNode : public rclcpp::Node
{
public:
    ElevationMapperNode() : Node("elevation_mapper_node")
    {
        // --- Parameters ---
        this->declare_parameter<std::string>("base_frame", "base_link");
        this->declare_parameter<double>("map.length", 10.0);
        this->declare_parameter<double>("map.resolution", 0.05);
        this->declare_parameter<double>("pca.neighbor_radius", 0.25);
        this->declare_parameter<int>("pca.min_neighbors", 10);

        // --- Grid Map Initialization ---
        map_frame_ = this->get_parameter("base_frame").as_string();
        grid_map_.setFrameId(map_frame_);
        grid_map_.setGeometry(
            grid_map::Length(this->get_parameter("map.length").as_double(), this->get_parameter("map.length").as_double()),
            this->get_parameter("map.resolution").as_double()
        );
        
        // Add all the layers we will use
        grid_map_.add("elevation", NAN);
        grid_map_.add("slope_deg", NAN);
        grid_map_.add("roughness", NAN);

        // --- Subscribers and Publishers ---
        auto qos_profile = rclcpp::QoS(rclcpp::KeepLast(5)).best_effort();
        cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/lidar/points_filtered", qos_profile,
            std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));
        map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("/elevation_map", 10);

        RCLCPP_INFO(this->get_logger(), "Elevation Mapper Node with Terrain Classification has started.");
    }

private:
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // 1. Convert ROS Msg to PCL PointCloud
        pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
        pcl::fromROSMsg(*msg, *cloud);
        if (cloud->empty()) return;

        // 2. Clear old map data
        grid_map_.clearAll();

        // 3. Populate the elevation layer
        for (const auto& point : cloud->points)
        {
            grid_map::Position position(point.x, point.y);
            grid_map::Index index;
            if (grid_map_.getIndex(position, index))
            {
                auto& cell_elevation = grid_map_.at("elevation", index);
                if (!std::isfinite(cell_elevation) || point.z > cell_elevation)
                {
                    cell_elevation = point.z;
                }
            }
        }

        // --- START OF NEW TERRAIN CLASSIFICATION LOGIC ---

        // 4. Create a KdTree for efficient neighborhood searches
        pcl::search::KdTree<PointT>::Ptr tree(new pcl::search::KdTree<PointT>());
        tree->setInputCloud(cloud);

        // 5. Iterate through the grid map to calculate slope and roughness
        for (grid_map::GridMapIterator iterator(grid_map_); !iterator.isPastEnd(); ++iterator)
        {
            // Get the 2D position of the current grid cell's center
            grid_map::Position cell_position;
            grid_map_.getPosition(*iterator, cell_position);
            
            PointT search_point;
            search_point.x = cell_position.x();
            search_point.y = cell_position.y();
            search_point.z = grid_map_.at("elevation", *iterator);

            if (!std::isfinite(search_point.z)) continue; // Skip cells with no elevation data

            // Find all neighbors within a radius
            std::vector<int> point_indices;
            std::vector<float> point_distances;
            double radius = this->get_parameter("pca.neighbor_radius").as_double();
            int min_neighbors = this->get_parameter("pca.min_neighbors").as_int();

            if (tree->radiusSearch(search_point, radius, point_indices, point_distances) >= min_neighbors)
            {
                // We have enough neighbors to perform PCA
                
                // Compute the centroid and covariance matrix of the neighborhood
                Eigen::Vector4f pca_centroid;
                Eigen::Matrix3f covariance_matrix;
                pcl::computeMeanAndCovarianceMatrix(*cloud, point_indices, covariance_matrix, pca_centroid);

                // Compute eigenvalues and eigenvectors
                Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigen_solver(covariance_matrix);
                Eigen::Vector3f eigenvalues = eigen_solver.eigenvalues(); // ascending order
                Eigen::Matrix3f eigenvectors = eigen_solver.eigenvectors();

                // The normal vector is the eigenvector associated with the smallest eigenvalue
                Eigen::Vector3f normal = eigenvectors.col(0);
                if (normal.z() < 0) normal = -normal; // Ensure normal points "up"

                // Calculate Slope
                float slope_rad = std::acos(normal.dot(Eigen::Vector3f::UnitZ()));
                float slope_deg = slope_rad * 180.0 / M_PI;

                // Calculate Roughness
                float lambda3 = eigenvalues(0); // Smallest eigenvalue
                float total_variance = eigenvalues.sum();
                float roughness = (total_variance > 0) ? (lambda3 / total_variance) : 0.0f;

                // Store the results in the grid map
                grid_map_.at("slope_deg", *iterator) = slope_deg;
                grid_map_.at("roughness", *iterator) = roughness;
            }
        }
        // --- END OF NEW TERRAIN CLASSIFICATION LOGIC ---

        // 6. Publish the enriched Grid Map
        auto out_msg = grid_map::GridMapRosConverter::toMessage(grid_map_);
        out_msg->header.stamp = this->get_clock()->now();
        out_msg->header.frame_id = map_frame_;
        map_publisher_->publish(std::move(out_msg));
    }

    // Declare member variables
    grid_map::GridMap grid_map_;
    std::string map_frame_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_subscription_;
    rclcpp::Publisher<grid_map_msgs::msg::GridMap>::SharedPtr map_publisher_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ElevationMapperNode>());
    rclcpp::shutdown();
    return 0;
}