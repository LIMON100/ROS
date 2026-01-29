
// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <geometry_msgs/msg/transform_stamped.hpp>

// // PCL Headers
// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <pcl/common/common.h>
// #include <pcl/filters/voxel_grid.h>

// // TF2 Headers
// #include <tf2/utils.h>
// #include <tf2_ros/buffer.h>
// #include <tf2_ros/transform_listener.h>
// #include <tf2_sensor_msgs/tf2_sensor_msgs.hpp>
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

// // Grid Map Headers
// #include <grid_map_ros/grid_map_ros.hpp>
// #include <grid_map_msgs/msg/grid_map.hpp>

// // Eigen
// #include <Eigen/Dense>

// using PointT = pcl::PointXYZ;

// class ElevationMapperNode : public rclcpp::Node
// {
// public:
//     ElevationMapperNode() : Node("elevation_mapper_node")
//     {
//         // --- Parameters based on Client Specs ---
//         this->declare_parameter<std::string>("base_frame", "base_link");
//         this->declare_parameter<double>("map.length", 20.0);
//         this->declare_parameter<double>("map.resolution", 0.05); // 5cm

//         // Robot Capabilities (Client Specs)
//         this->declare_parameter<double>("max_slope_deg", 30.0);
//         this->declare_parameter<double>("max_step_height", 0.235); // 235mm
//         this->declare_parameter<double>("max_gap_size", 0.220);    // 220mm

//         // --- Grid Map Initialization ---
//         map_frame_ = "odom"; 
//         robot_base_frame_ = this->get_parameter("base_frame").as_string();

//         grid_map_.setFrameId(map_frame_);
//         grid_map_.setGeometry(
//             grid_map::Length(this->get_parameter("map.length").as_double(), this->get_parameter("map.length").as_double()),
//             this->get_parameter("map.resolution").as_double()
//         );

//         // --- Define Layers ---
//         grid_map_.add("elevation", NAN);
//         grid_map_.add("slope", NAN);
//         grid_map_.add("step_height", NAN);
        
//         // Vegetation Analysis Layers
//         grid_map_.add("density_low", 0.0);  // 0.05m - 0.3m (Grass/Small Rocks)
//         grid_map_.add("density_high", 0.0); // 0.3m - 1.5m (Trunks/Walls)
        
//         // Final Costs
//         grid_map_.add("drop_risk", 0.0);
//         grid_map_.add("traversability", NAN);

//         // --- Subscribers and Publishers ---
//         auto qos = rclcpp::SensorDataQoS();
//         cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//             "/lidar/points", qos,
//             std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));

//         map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("/elevation_map", 1);

//         // TF Listener setup
//         tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//         tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//         RCLCPP_INFO(this->get_logger(), "Tactical Terrain Mapper (ROI Optimized) Started.");
//     }

// private:
//     void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
//     {
//         // 1. Transform Cloud to Map Frame (Odom)
//         sensor_msgs::msg::PointCloud2 map_frame_cloud_msg;
//         geometry_msgs::msg::TransformStamped transform;

//         try {
//             transform = tf_buffer_->lookupTransform(
//                 map_frame_, msg->header.frame_id,
//                 tf2::TimePointZero, std::chrono::milliseconds(100));
//             tf2::doTransform(*msg, map_frame_cloud_msg, transform);
//         } catch (tf2::TransformException &ex) {
//             RCLCPP_WARN(this->get_logger(), "TF failure: %s", ex.what());
//             return;
//         }

//         // Get Robot Position and Yaw for ROI Calculation
//         grid_map::Position robot_pos(transform.transform.translation.x, transform.transform.translation.y);
//         double yaw = tf2::getYaw(transform.transform.rotation);

//         // 2. Move Grid Map (Rolling Window)
//         grid_map_.move(robot_pos);

//         // 3. Process Point Cloud
//         pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
//         pcl::fromROSMsg(map_frame_cloud_msg, *cloud);

//         // Reset accumulation layers (Only need to clear dynamic layers)
//         grid_map_.clear("density_low");
//         grid_map_.clear("density_high");

//         // --- FAST POINT ITERATION ---
//         // Fills elevation and density layers. Fast because index lookup is O(1).
//         for (const auto& point : cloud->points)
//         {
//             grid_map::Index index;
//             grid_map::Position position(point.x, point.y);

//             if (!grid_map_.getIndex(position, index)) continue;

//             // Update Elevation (Min height strategy for ground)
//             float current_elev = grid_map_.at("elevation", index);
//             if (!std::isfinite(current_elev) || point.z < current_elev) {
//                 grid_map_.at("elevation", index) = point.z;
//             }

//             // --- VEGETATION LOGIC (Merged from Response 1) ---
//             // Calculate height relative to the ground at this cell
//             float h = point.z - grid_map_.at("elevation", index);
            
//             // Band A: 0.05m to 0.3m (Traversable Vegetation / Small Obstacles)
//             if (h > 0.05 && h < 0.3) {
//                 grid_map_.at("density_low", index) += 1.0;
//             }
//             // Band B: 0.3m to 1.5m (Hard Obstacles / Trunks)
//             else if (h >= 0.3 && h < 1.5) {
//                 grid_map_.at("density_high", index) += 1.0;
//             }
//         }

//         // =========================================================
//         // 4. ROI DEFINITION (CLIENT REQUIREMENT)
//         // =========================================================
//         // We define a polygon (Rectangle) representing the path ahead.
//         // Width: 1.2m. Length: 5.0m.
        
//         grid_map::Polygon roi_polygon;
//         roi_polygon.setFrameId(map_frame_);

//         double look_ahead = 5.0;
//         double look_behind = -1.0;
//         double half_width = 0.6; // Total 1.2m

//         double c = cos(yaw);
//         double s = sin(yaw);

//         auto add_corner = [&](double x, double y) {
//             double rx = x * c - y * s + robot_pos.x();
//             double ry = x * s + y * c + robot_pos.y();
//             roi_polygon.addVertex(grid_map::Position(rx, ry));
//         };

//         add_corner(look_ahead, half_width);   // Front-Left
//         add_corner(look_ahead, -half_width);  // Front-Right
//         add_corner(look_behind, -half_width); // Back-Right
//         add_corner(look_behind, half_width);  // Back-Left

//         // =========================================================
//         // 5. HEAVY PROCESSING (Calculated ONLY inside ROI)
//         // =========================================================
        
//         float max_safe_slope = this->get_parameter("max_slope_deg").as_double();
//         float max_step = this->get_parameter("max_step_height").as_double();
//         float max_gap = this->get_parameter("max_gap_size").as_double();

//         for (grid_map::PolygonIterator iterator(grid_map_, roi_polygon); !iterator.isPastEnd(); ++iterator)
//         {
//             if (!grid_map_.isValid(*iterator, "elevation")) continue;

//             float elevation = grid_map_.at("elevation", *iterator);

//             // --- Neighbor Analysis (Slope & Step & Roughness) ---
//             float max_height_diff = 0.0;
//             float max_step_down = 0.0; 
            
//             // Roughness Calculation: Standard Deviation of neighbors
//             float sum_z = 0.0;
//             float sum_sq_z = 0.0;
//             int count = 0;

//             // Check immediate neighbors (3x3 window)
//             grid_map::Index current_idx = *iterator;
//             grid_map::Index neighbor_idx;
            
//             for (int dx = -1; dx <= 1; dx++) {
//                 for (int dy = -1; dy <= 1; dy++) {
//                     // Include center cell for roughness calc
                    
//                     neighbor_idx(0) = current_idx(0) + dx;
//                     neighbor_idx(1) = current_idx(1) + dy;
                    
//                     if (grid_map_.isValid(neighbor_idx, "elevation")) {
//                         float n_elev = grid_map_.at("elevation", neighbor_idx);
                        
//                         // Roughness stats
//                         sum_z += n_elev;
//                         sum_sq_z += n_elev * n_elev;
//                         count++;

//                         // Slope/Step stats (skip center for this)
//                         if (dx == 0 && dy == 0) continue;
                        
//                         float diff = elevation - n_elev;
//                         if (std::abs(diff) > max_height_diff) max_height_diff = std::abs(diff);
//                         if (diff > max_step_down) max_step_down = diff; 
//                     }
//                 }
//             }

//             // Calculate Roughness (Standard Deviation)
//             float roughness = 0.0;
//             if (count > 1) {
//                 float mean = sum_z / count;
//                 float variance = (sum_sq_z / count) - (mean * mean);
//                 roughness = std::sqrt(std::max(0.0f, variance));
//             }
//             grid_map_.at("roughness", *iterator) = roughness;

//             // Slope Calculation
//             float slope_deg = (atan2(max_height_diff, grid_map_.getResolution()) * 180.0 / M_PI);
//             grid_map_.at("slope", *iterator) = slope_deg;
//             grid_map_.at("step_height", *iterator) = max_height_diff;

//             // Drop Risk
//             if (max_step_down > max_gap) {
//                 grid_map_.at("drop_risk", *iterator) = 1.0;
//             } else {
//                 grid_map_.at("drop_risk", *iterator) = 0.0;
//             }

//             // --- Traversability Classification ---
//             float cost = 0.0;

//             // 1. Check Hard Obstacles
//             if (grid_map_.at("density_high", *iterator) > 5) cost = 1.0; 
//             // 2. Check Pits
//             else if (grid_map_.at("drop_risk", *iterator) > 0.5) cost = 1.0;
//             // 3. Check Slope
//             else if (slope_deg > max_safe_slope) cost = 1.0;
//             // 4. Check Step Height
//             else if (max_height_diff > max_step) cost = 1.0;
//             // 5. Check Roughness (New Rule: Too bumpy to drive)
//             else if (roughness > 0.1) cost = 0.8; // High cost for very rough terrain
//             // 6. Check Vegetation
//             else if (grid_map_.at("density_low", *iterator) > 10) cost = 0.6; 
//             // 7. Base Cost
//             else cost = (slope_deg / max_safe_slope) + (roughness * 2.0); 

//             grid_map_.at("traversability", *iterator) = std::clamp(cost, 0.0f, 1.0f);
//         }
        
//         // 6. Publish
//         std::unique_ptr<grid_map_msgs::msg::GridMap> out_msg;
//         out_msg = grid_map::GridMapRosConverter::toMessage(grid_map_);
//         map_publisher_->publish(std::move(out_msg));
//     }

//     grid_map::GridMap grid_map_;
//     std::string map_frame_;
//     std::string robot_base_frame_;

//     rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_subscription_;
//     rclcpp::Publisher<grid_map_msgs::msg::GridMap>::SharedPtr map_publisher_;

//     std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
//     std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
// };

// int main(int argc, char *argv[])
// {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<ElevationMapperNode>());
//     rclcpp::shutdown();
//     return 0;
// }


#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>

// PCL Headers
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/common/common.h>
#include <pcl/filters/voxel_grid.h>

// TF2 Headers
#include <tf2/utils.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_sensor_msgs/tf2_sensor_msgs.hpp>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

// Grid Map Headers
#include <grid_map_ros/grid_map_ros.hpp>
#include <grid_map_msgs/msg/grid_map.hpp>

// Eigen
#include <Eigen/Dense>

using PointT = pcl::PointXYZ;

class ElevationMapperNode : public rclcpp::Node
{
public:
    ElevationMapperNode() : Node("elevation_mapper_node")
    {
        // --- Parameters ---
        this->declare_parameter<std::string>("base_frame", "base_link");
        this->declare_parameter<double>("map.length", 20.0);
        this->declare_parameter<double>("map.resolution", 0.05); // 5cm

        // Robot Capabilities
        this->declare_parameter<double>("max_slope_deg", 30.0);
        this->declare_parameter<double>("max_step_height", 0.235); // 235mm
        this->declare_parameter<double>("max_gap_size", 0.220);    // 220mm

        // --- Grid Map Initialization ---
        // map_frame_ = "odom"; 
        this->declare_parameter<std::string>("map_frame", "odom");
        map_frame_ = this->get_parameter("map_frame").as_string();

        robot_base_frame_ = this->get_parameter("base_frame").as_string();

        grid_map_.setFrameId(map_frame_);
        grid_map_.setGeometry(
            grid_map::Length(this->get_parameter("map.length").as_double(), this->get_parameter("map.length").as_double()),
            this->get_parameter("map.resolution").as_double()
        );

        // --- Define Layers (ADDED ROUGHNESS HERE) ---
        grid_map_.add("elevation", NAN);
        grid_map_.add("slope", NAN);
        grid_map_.add("step_height", NAN);
        grid_map_.add("roughness", NAN); // <--- THIS WAS MISSING
        
        // Vegetation Analysis Layers
        grid_map_.add("density_low", 0.0);  
        grid_map_.add("density_high", 0.0); 
        
        // Final Costs
        grid_map_.add("drop_risk", 0.0);
        grid_map_.add("traversability", NAN);

        // --- Subscribers and Publishers ---
        auto qos = rclcpp::SensorDataQoS();
        cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/lidar/points", qos,
            std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));

        map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("/elevation_map", 1);
        

        // TF Listener setup
        tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        RCLCPP_INFO(this->get_logger(), "Tactical Terrain Mapper (ROI Optimized) Started.");
    }

private:
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // 1. Transform Cloud to Map Frame (Odom)
        sensor_msgs::msg::PointCloud2 map_frame_cloud_msg;
        geometry_msgs::msg::TransformStamped transform;

        try {
            transform = tf_buffer_->lookupTransform(
                map_frame_, msg->header.frame_id,
                tf2::TimePointZero, std::chrono::milliseconds(100));
            tf2::doTransform(*msg, map_frame_cloud_msg, transform);
        } catch (tf2::TransformException &ex) {
            RCLCPP_WARN(this->get_logger(), "TF failure: %s", ex.what());
            return;
        }

        // Get Robot Position and Yaw for ROI Calculation
        grid_map::Position robot_pos(transform.transform.translation.x, transform.transform.translation.y);
        double yaw = tf2::getYaw(transform.transform.rotation);

        // 2. Move Grid Map (Rolling Window)
        grid_map_.move(robot_pos);

        // 3. Process Point Cloud
        pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
        pcl::fromROSMsg(map_frame_cloud_msg, *cloud);

        // Reset accumulation layers
        grid_map_.clear("density_low");
        grid_map_.clear("density_high");
        // Don't clear elevation everywhere, only move handles that. 
        // Ideally we might decay old data, but for now standard rolling is fine.

        // --- FAST POINT ITERATION ---
        for (const auto& point : cloud->points)
        {
            grid_map::Index index;
            grid_map::Position position(point.x, point.y);

            if (!grid_map_.getIndex(position, index)) continue;

            // Update Elevation (Min height strategy for ground)
            float current_elev = grid_map_.at("elevation", index);
            if (!std::isfinite(current_elev) || point.z < current_elev) {
                grid_map_.at("elevation", index) = point.z;
            }

            // Vegetation Density Calc
            float h = point.z - grid_map_.at("elevation", index);
            if (h > 0.05 && h < 0.3) {
                grid_map_.at("density_low", index) += 1.0;
            }
            else if (h >= 0.3 && h < 1.5) {
                grid_map_.at("density_high", index) += 1.0;
            }
        }

        // =========================================================
        // 4. ROI DEFINITION
        // =========================================================
        grid_map::Polygon roi_polygon;
        roi_polygon.setFrameId(map_frame_);

        double look_ahead = 5.0;
        double look_behind = -1.0;
        double half_width = 0.8; 

        double c = cos(yaw);
        double s = sin(yaw);

        auto add_corner = [&](double x, double y) {
            double rx = x * c - y * s + robot_pos.x();
            double ry = x * s + y * c + robot_pos.y();
            roi_polygon.addVertex(grid_map::Position(rx, ry));
        };

        add_corner(look_ahead, half_width);   
        add_corner(look_ahead, -half_width);  
        add_corner(look_behind, -half_width); 
        add_corner(look_behind, half_width);  

        // =========================================================
        // 5. HEAVY PROCESSING (Calculated ONLY inside ROI)
        // =========================================================
        
        float max_safe_slope = this->get_parameter("max_slope_deg").as_double();
        float max_step = this->get_parameter("max_step_height").as_double();
        float max_gap = this->get_parameter("max_gap_size").as_double();

        for (grid_map::PolygonIterator iterator(grid_map_, roi_polygon); !iterator.isPastEnd(); ++iterator)
        {
            if (!grid_map_.isValid(*iterator, "elevation")) continue;

            float elevation = grid_map_.at("elevation", *iterator);

            // --- Neighbor Analysis ---
            float max_height_diff = 0.0;
            float max_step_down = 0.0; 
            
            // Roughness Stats
            float sum_z = 0.0;
            float sum_sq_z = 0.0;
            int count = 0;

            grid_map::Index current_idx = *iterator;
            grid_map::Index neighbor_idx;
            
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    neighbor_idx(0) = current_idx(0) + dx;
                    neighbor_idx(1) = current_idx(1) + dy;
                    
                    if (grid_map_.isValid(neighbor_idx, "elevation")) {
                        float n_elev = grid_map_.at("elevation", neighbor_idx);
                        
                        // Roughness stats
                        sum_z += n_elev;
                        sum_sq_z += n_elev * n_elev;
                        count++;

                        if (dx == 0 && dy == 0) continue;
                        
                        float diff = elevation - n_elev;
                        if (std::abs(diff) > max_height_diff) max_height_diff = std::abs(diff);
                        if (diff > max_step_down) max_step_down = diff; 
                    }
                }
            }

            // Calculate Roughness
            float roughness = 0.0;
            if (count > 1) {
                float mean = sum_z / count;
                float variance = (sum_sq_z / count) - (mean * mean);
                roughness = std::sqrt(std::max(0.0f, variance));
            }
            grid_map_.at("roughness", *iterator) = roughness;

            // Slope Calculation
            float slope_deg = (atan2(max_height_diff, grid_map_.getResolution()) * 180.0 / M_PI);
            grid_map_.at("slope", *iterator) = slope_deg;
            grid_map_.at("step_height", *iterator) = max_height_diff;

            // Drop Risk (Ditch detection)
            if (max_step_down > max_gap) {
                grid_map_.at("drop_risk", *iterator) = 1.0;
            } else {
                grid_map_.at("drop_risk", *iterator) = 0.0;
            }

            // --- Traversability Classification ---
            float cost = 0.0;

            if (grid_map_.at("density_high", *iterator) > 5) cost = 1.0; // Wall/Tree
            else if (grid_map_.at("drop_risk", *iterator) > 0.5) cost = 1.0; // Pit
            else if (slope_deg > max_safe_slope) cost = 1.0; // Steep Slope
            else if (max_height_diff > max_step) cost = 1.0; // High Step
            else if (roughness > 0.1) cost = 0.8; // Rough Terrain
            else if (grid_map_.at("density_low", *iterator) > 10) cost = 0.6; // Grass
            else cost = (slope_deg / max_safe_slope) * 0.5; // Base traversal cost

            grid_map_.at("traversability", *iterator) = std::clamp(cost, 0.0f, 1.0f);
        }
        
        // 6. Publish
        std::unique_ptr<grid_map_msgs::msg::GridMap> out_msg;
        out_msg = grid_map::GridMapRosConverter::toMessage(grid_map_);
        map_publisher_->publish(std::move(out_msg));
    }

    grid_map::GridMap grid_map_;
    std::string map_frame_;
    std::string robot_base_frame_;

    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_subscription_;
    rclcpp::Publisher<grid_map_msgs::msg::GridMap>::SharedPtr map_publisher_;

    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ElevationMapperNode>());
    rclcpp::shutdown();
    return 0;
}