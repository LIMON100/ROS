// // WORKABLE 3d data produces
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
//     // ElevationMapperNode() : Node("elevation_mapper_node")
//     // {
//     //     // --- Parameters ---
//     //     this->declare_parameter<std::string>("base_frame", "base_link");
//     //     this->declare_parameter<double>("map.length", 20.0);
//     //     this->declare_parameter<double>("map.resolution", 0.05); // 5cm

//     //     // Robot Capabilities
//     //     this->declare_parameter<double>("max_slope_deg", 30.0);
//     //     this->declare_parameter<double>("max_step_height", 0.235); // 235mm
//     //     this->declare_parameter<double>("max_gap_size", 0.220);    // 220mm

//     //     this->declare_parameter<std::string>("cloud_topic", "scan/points");
//     //     std::string cloud_topic = this->get_parameter("cloud_topic").as_string();

//     //     // --- Grid Map Initialization ---
//     //     // map_frame_ = "odom"; 
//     //     this->declare_parameter<std::string>("map_frame", "odom");
//     //     map_frame_ = this->get_parameter("map_frame").as_string();

//     //     robot_base_frame_ = this->get_parameter("base_frame").as_string();

//     //     grid_map_.setFrameId(map_frame_);
//     //     grid_map_.setGeometry(
//     //         grid_map::Length(this->get_parameter("map.length").as_double(), this->get_parameter("map.length").as_double()),
//     //         this->get_parameter("map.resolution").as_double()
//     //     );

//     //     // --- Define Layers (ADDED ROUGHNESS HERE) ---
//     //     grid_map_.add("elevation", NAN);
//     //     grid_map_.add("slope", NAN);
//     //     grid_map_.add("step_height", NAN);
//     //     grid_map_.add("roughness", NAN); // <--- THIS WAS MISSING
        
//     //     // Vegetation Analysis Layers
//     //     grid_map_.add("density_low", 0.0);  
//     //     grid_map_.add("density_high", 0.0); 
        
//     //     // Final Costs
//     //     grid_map_.add("drop_risk", 0.0);
//     //     grid_map_.add("traversability", NAN);

//     //     // --- Subscribers and Publishers ---
//     //     auto qos = rclcpp::SensorDataQoS();
//     //     cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//     //         "/lidar/points", qos,
//     //         std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));



//     //     // map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("/elevation_map", 1);
//     //     map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("elevation_map", 1);
        

//     //     // TF Listener setup
//     //     tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//     //     tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//     //     RCLCPP_INFO(this->get_logger(), "Tactical Terrain Mapper (ROI Optimized) Started.");
//     // }

//     ElevationMapperNode() : Node("elevation_mapper_node")
//     {
//         // --- Parameters ---
//         // Default to what tin3_bot uses for a single robot
//         this->declare_parameter<std::string>("base_frame", "base_footprint"); 
//         this->declare_parameter<std::string>("map_frame", "odom");
//         this->declare_parameter<std::string>("cloud_topic", "/scan/points"); 

//         this->declare_parameter<double>("map.length", 10.0);
//         this->declare_parameter<double>("map.resolution", 0.05);
//         this->declare_parameter<double>("max_slope_deg", 30.0);
//         this->declare_parameter<double>("max_step_height", 0.235);
//         this->declare_parameter<double>("max_gap_size", 0.220);

//         map_frame_ = this->get_parameter("map_frame").as_string();
//         robot_base_frame_ = this->get_parameter("base_frame").as_string();
//         std::string cloud_topic = this->get_parameter("cloud_topic").as_string();

//         grid_map_.setFrameId(map_frame_);
//         grid_map_.setGeometry(
//             grid_map::Length(this->get_parameter("map.length").as_double(), this->get_parameter("map.length").as_double()),
//             this->get_parameter("map.resolution").as_double()
//         );

//         // Layers...
//         grid_map_.add("elevation", NAN);
//         grid_map_.add("slope", NAN);
//         grid_map_.add("step_height", NAN);
//         grid_map_.add("roughness", NAN);
//         grid_map_.add("density_low", 0.0);  
//         grid_map_.add("density_high", 0.0); 
//         grid_map_.add("drop_risk", 0.0);
//         grid_map_.add("traversability", NAN);

//         auto qos = rclcpp::SensorDataQoS();
//         cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//             cloud_topic, qos, // <--- NOW USING THE PARAMETER
//             std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));

//         map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("elevation_map", 1);
        
//         tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//         tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//         RCLCPP_INFO(this->get_logger(), "Elevation Mapper Started.");
//         RCLCPP_INFO(this->get_logger(), "Listening on: %s", cloud_topic.c_str());
//         RCLCPP_INFO(this->get_logger(), "Base Frame: %s", robot_base_frame_.c_str());
//     }

// private:
//     void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
//     {

//         std::string target_frame_id = msg->header.frame_id;
        
//         // Find the slash in "robot2/base_link"
//         size_t slash_pos = robot_base_frame_.find('/');
//         if (slash_pos != std::string::npos) {
//             std::string prefix = robot_base_frame_.substr(0, slash_pos + 1); // e.g., "robot2/"
            
//             // If message is just "lidar_link", rename it to "robot2/lidar_link"
//             if (target_frame_id.find(prefix) == std::string::npos) {
//                 target_frame_id = prefix + target_frame_id;
//             }
//         }

//         // 1. Transform Cloud to Map Frame (Odom)
//         sensor_msgs::msg::PointCloud2 map_frame_cloud_msg;
//         geometry_msgs::msg::TransformStamped transform;

//         try {
//             // USE THE CORRECTED target_frame_id HERE
//             transform = tf_buffer_->lookupTransform(
//                 map_frame_, target_frame_id, // <--- CHANGED FROM msg->header.frame_id
//                 tf2::TimePointZero, std::chrono::milliseconds(100));
//             tf2::doTransform(*msg, map_frame_cloud_msg, transform);
//         } catch (tf2::TransformException &ex) {
//             // Reduce log spam
//             RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "TF failure: %s", ex.what());
//             return;
//         }

//         // try {
//         //     transform = tf_buffer_->lookupTransform(
//         //         map_frame_, msg->header.frame_id,
//         //         tf2::TimePointZero, std::chrono::milliseconds(100));
//         //     tf2::doTransform(*msg, map_frame_cloud_msg, transform);
//         // } catch (tf2::TransformException &ex) {
//         //     RCLCPP_WARN(this->get_logger(), "TF failure: %s", ex.what());
//         //     return;
//         // }

//         // Get Robot Position and Yaw for ROI Calculation
//         grid_map::Position robot_pos(transform.transform.translation.x, transform.transform.translation.y);
//         double yaw = tf2::getYaw(transform.transform.rotation);

//         // 2. Move Grid Map (Rolling Window)
//         grid_map_.move(robot_pos);

//         // 3. Process Point Cloud
//         pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
//         pcl::fromROSMsg(map_frame_cloud_msg, *cloud);

//         // Reset accumulation layers
//         grid_map_.clear("density_low");
//         grid_map_.clear("density_high");
//         // Don't clear elevation everywhere, only move handles that. 
//         // Ideally we might decay old data, but for now standard rolling is fine.

//         // --- FAST POINT ITERATION ---
//         // for (const auto& point : cloud->points)
//         // {
            
//         //     grid_map::Index index;
//         //     grid_map::Position position(point.x, point.y);

//         //     if (!grid_map_.getIndex(position, index)) continue;

//         //     // Update Elevation (Min height strategy for ground)
//         //     float current_elev = grid_map_.at("elevation", index);
//         //     if (!std::isfinite(current_elev) || point.z < current_elev) {
//         //         grid_map_.at("elevation", index) = point.z;
//         //     }

//         //     // Vegetation Density Calc
//         //     float h = point.z - grid_map_.at("elevation", index);
//         //     if (h > 0.05 && h < 0.3) {
//         //         grid_map_.at("density_low", index) += 1.0;
//         //     }
//         //     else if (h >= 0.3 && h < 1.5) {
//         //         grid_map_.at("density_high", index) += 1.0;
//         //     }
//         // }

//         for (const auto& point : cloud->points)
//         {
//             // --- SELF FILTER ---
//             // Robot is approx 1.3m long (x) and 0.85m wide (y).
//             // We ignore points inside a box slightly larger than the robot.
//             // X: -0.7 to +0.7
//             // Y: -0.5 to +0.5
//             if (point.x > -0.7 && point.x < 0.7 && point.y > -0.5 && point.y < 0.5) {
//                 continue; // Skip points hitting the robot itself
//             }
            
//             if ( (point.x*point.x + point.y*point.y) < (0.7*0.7) ) {
//                 continue;
//             }
//             // --- GROUND PLANE FILTER (Crucial for Flat Ground) ---
//             // If the point is near z=0 (ground), it's traversable, not an obstacle.
//             // But we must still record it for "elevation".
//             // However, high density usually implies "vertical" structure.
//             // We only count density if point is significantly ABOVE the cell's ground.

//             grid_map::Index index;
//             grid_map::Position position(point.x, point.y);
//             if (!grid_map_.getIndex(position, index)) continue;

//             float current_elev = grid_map_.at("elevation", index);
//             if (!std::isfinite(current_elev) || point.z < current_elev) {
//                 grid_map_.at("elevation", index) = point.z;
//             }

//             // Density Check Correction:
//             // Calculate height relative to the LOWEST point in that cell (ground)
//             float h = point.z - grid_map_.at("elevation", index);
            
//             // Ignore small noise (< 10cm)
//             if (h > 0.1 && h < 0.3) {
//                 grid_map_.at("density_low", index) += 1.0;
//             }
//             else if (h >= 0.3 && h < 1.5) {
//                 grid_map_.at("density_high", index) += 1.0;
//             }
//         }

//         // =========================================================
//         // 4. ROI DEFINITION
//         // =========================================================
//         grid_map::Polygon roi_polygon;
//         roi_polygon.setFrameId(map_frame_);

//         double look_ahead = 5.0;
//         double look_behind = -1.0;
//         double half_width = 0.8; 

//         double c = cos(yaw);
//         double s = sin(yaw);

//         auto add_corner = [&](double x, double y) {
//             double rx = x * c - y * s + robot_pos.x();
//             double ry = x * s + y * c + robot_pos.y();
//             roi_polygon.addVertex(grid_map::Position(rx, ry));
//         };

//         add_corner(look_ahead, half_width);   
//         add_corner(look_ahead, -half_width);  
//         add_corner(look_behind, -half_width); 
//         add_corner(look_behind, half_width);  

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

//             // --- Neighbor Analysis ---
//             float max_height_diff = 0.0;
//             float max_step_down = 0.0; 
            
//             // Roughness Stats
//             float sum_z = 0.0;
//             float sum_sq_z = 0.0;
//             int count = 0;

//             grid_map::Index current_idx = *iterator;
//             grid_map::Index neighbor_idx;
            
//             for (int dx = -1; dx <= 1; dx++) {
//                 for (int dy = -1; dy <= 1; dy++) {
//                     neighbor_idx(0) = current_idx(0) + dx;
//                     neighbor_idx(1) = current_idx(1) + dy;
                    
//                     if (grid_map_.isValid(neighbor_idx, "elevation")) {
//                         float n_elev = grid_map_.at("elevation", neighbor_idx);
                        
//                         // Roughness stats
//                         sum_z += n_elev;
//                         sum_sq_z += n_elev * n_elev;
//                         count++;

//                         if (dx == 0 && dy == 0) continue;
                        
//                         float diff = elevation - n_elev;
//                         if (std::abs(diff) > max_height_diff) max_height_diff = std::abs(diff);
//                         if (diff > max_step_down) max_step_down = diff; 
//                     }
//                 }
//             }

//             // Calculate Roughness
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

//             // Drop Risk (Ditch detection)
//             if (max_step_down > max_gap) {
//                 grid_map_.at("drop_risk", *iterator) = 1.0;
//             } else {
//                 grid_map_.at("drop_risk", *iterator) = 0.0;
//             }

//             // --- Traversability Classification ---
//             float cost = 0.0;

//             if (grid_map_.at("density_high", *iterator) > 5) cost = 1.0; // Wall/Tree
//             else if (grid_map_.at("drop_risk", *iterator) > 0.5) cost = 1.0; // Pit
//             else if (slope_deg > max_safe_slope) cost = 1.0; // Steep Slope
//             else if (max_height_diff > max_step) cost = 1.0; // High Step
//             else if (roughness > 0.1) cost = 0.8; // Rough Terrain
//             else if (grid_map_.at("density_low", *iterator) > 10) cost = 0.6; // Grass
//             else cost = (slope_deg / max_safe_slope) * 0.5; // Base traversal cost

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
        this->declare_parameter<std::string>("base_frame", "base_footprint"); 
        this->declare_parameter<std::string>("map_frame", "odom");
        this->declare_parameter<std::string>("cloud_topic", "/scan/points"); 

        this->declare_parameter<double>("map.length", 10.0);
        this->declare_parameter<double>("map.resolution", 0.05);
        this->declare_parameter<double>("max_slope_deg", 30.0);
        this->declare_parameter<double>("max_step_height", 0.235);
        this->declare_parameter<double>("max_gap_size", 0.220);

        map_frame_ = this->get_parameter("map_frame").as_string();
        robot_base_frame_ = this->get_parameter("base_frame").as_string();
        std::string cloud_topic = this->get_parameter("cloud_topic").as_string();

        grid_map_.setFrameId(map_frame_);
        grid_map_.setGeometry(
            grid_map::Length(this->get_parameter("map.length").as_double(), this->get_parameter("map.length").as_double()),
            this->get_parameter("map.resolution").as_double()
        );

        // Layers
        grid_map_.add("elevation", NAN);
        grid_map_.add("slope", NAN);
        grid_map_.add("step_height", NAN);
        grid_map_.add("roughness", NAN);
        grid_map_.add("density_low", 0.0);  
        grid_map_.add("density_high", 0.0); 
        grid_map_.add("drop_risk", 0.0);
        grid_map_.add("traversability", 0.0);

        auto qos = rclcpp::SensorDataQoS();
        cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            cloud_topic, qos, 
            std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));

        map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("elevation_map", 1);
        
        tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        RCLCPP_INFO(this->get_logger(), "Elevation Mapper Started. Filtering body points relative to robot.");
    }

private:
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        std::string target_frame_id = msg->header.frame_id;
        
        // Handle namespace prefixing if needed (Tin3bot specific)
        size_t slash_pos = robot_base_frame_.find('/');
        if (slash_pos != std::string::npos) {
            std::string prefix = robot_base_frame_.substr(0, slash_pos + 1); 
            if (target_frame_id.find(prefix) == std::string::npos) {
                target_frame_id = prefix + target_frame_id;
            }
        }

        sensor_msgs::msg::PointCloud2 map_frame_cloud_msg;
        geometry_msgs::msg::TransformStamped transform;

        try {
            transform = tf_buffer_->lookupTransform(
                map_frame_, target_frame_id,
                tf2::TimePointZero, std::chrono::milliseconds(100));
            tf2::doTransform(*msg, map_frame_cloud_msg, transform);
        } catch (tf2::TransformException &ex) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "TF failure: %s", ex.what());
            return;
        }

        // Get Robot Position from the transform (Translation from map -> sensor)
        // Note: Ideally we want map -> base_link, but this is map -> sensor.
        // For self-filtering, using the sensor position is actually safer as LIDAR is on the robot.
        double robot_x = transform.transform.translation.x;
        double robot_y = transform.transform.translation.y;
        double yaw = tf2::getYaw(transform.transform.rotation);

        // Move Grid Map Center to Robot
        grid_map_.move(grid_map::Position(robot_x, robot_y));

        pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
        pcl::fromROSMsg(map_frame_cloud_msg, *cloud);

        // Reset accumulation layers
        grid_map_.clear("density_low");
        grid_map_.clear("density_high");

        for (const auto& point : cloud->points)
        {
            // --- FIXED SELF FILTER ---
            // Calculate distance from point to robot center (in Odom frame)
            double dx = point.x - robot_x;
            double dy = point.y - robot_y;
            double dist_sq = dx*dx + dy*dy;

            // Filter out points within 0.9m radius of the sensor/robot center.
            // Robot length ~1.3m (0.65m half). 0.9m clears the bumpers.
            // if (dist_sq < (0.9 * 0.9)) {
            //     continue; 
            // }
            // -------------------------
            
            double rel_x = point.x - robot_x;
            double rel_y = point.y - robot_y;
            
            // Rotate to robot frame (approximate, using yaw)
            // We need to filter based on the robot's orientation
            double local_x = rel_x * cos(-yaw) - rel_y * sin(-yaw);
            double local_y = rel_x * sin(-yaw) + rel_y * cos(-yaw);

            // Robot Dimensions: Length ~1.3m, Width ~0.85m
            // We add a safety margin (e.g., 0.2m)
            // Exclusion Box: x: [-0.9, +0.9], y: [-0.6, +0.6]
            if (std::abs(local_x) < 0.9 && std::abs(local_y) < 0.6) {
                continue; // Ignore points hitting the robot body
            }


            grid_map::Index index;
            grid_map::Position position(point.x, point.y);

            if (!grid_map_.getIndex(position, index)) continue;

            float current_elev = grid_map_.at("elevation", index);
            if (!std::isfinite(current_elev) || point.z < current_elev) {
                grid_map_.at("elevation", index) = point.z;
            }

            float h = point.z - grid_map_.at("elevation", index);
            if (h > 0.1 && h < 0.3) {
                grid_map_.at("density_low", index) += 1.0;
            }
            else if (h >= 0.3 && h < 1.5) {
                grid_map_.at("density_high", index) += 1.0;
            }
        }

        // --- ROI Iterator ---
        grid_map::Polygon roi_polygon;
        roi_polygon.setFrameId(map_frame_);

        double look_ahead = 5.0;
        double look_behind = -1.0;
        double half_width = 1.0; 

        double c = cos(yaw);
        double s = sin(yaw);

        auto add_corner = [&](double x, double y) {
            double rx = x * c - y * s + robot_x;
            double ry = x * s + y * c + robot_y;
            roi_polygon.addVertex(grid_map::Position(rx, ry));
        };

        add_corner(look_ahead, half_width);   
        add_corner(look_ahead, -half_width);  
        add_corner(look_behind, -half_width); 
        add_corner(look_behind, half_width);  

        float max_safe_slope = this->get_parameter("max_slope_deg").as_double();
        float max_step = this->get_parameter("max_step_height").as_double();
        float max_gap = this->get_parameter("max_gap_size").as_double();


        for (grid_map::PolygonIterator iterator(grid_map_, roi_polygon); !iterator.isPastEnd(); ++iterator)
        {
            if (!grid_map_.isValid(*iterator, "elevation")) continue;

            float elevation = grid_map_.at("elevation", *iterator);
            float max_height_diff = 0.0;
            float max_step_down = 0.0; 
            
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

            float roughness = 0.0;
            if (count > 1) {
                float mean = sum_z / count;
                float variance = (sum_sq_z / count) - (mean * mean);
                roughness = std::sqrt(std::max(0.0f, variance));
            }
            grid_map_.at("roughness", *iterator) = roughness;

            float slope_deg = (atan2(max_height_diff, grid_map_.getResolution()) * 180.0 / M_PI);
            grid_map_.at("slope", *iterator) = slope_deg;
            grid_map_.at("step_height", *iterator) = max_height_diff;

            if (max_step_down > max_gap) {
                grid_map_.at("drop_risk", *iterator) = 1.0;
            } else {
                grid_map_.at("drop_risk", *iterator) = 0.0;
            }

            float cost = 0.0;
            if (grid_map_.at("density_high", *iterator) > 5) cost = 1.0; 
            else if (grid_map_.at("drop_risk", *iterator) > 0.5) cost = 1.0; 
            else if (slope_deg > max_safe_slope) cost = 1.0; 
            else if (max_height_diff > max_step) cost = 1.0; 
            else if (roughness > 0.1) cost = 0.8; 
            else if (grid_map_.at("density_low", *iterator) > 10) cost = 0.6; 
            else cost = (slope_deg / max_safe_slope) * 0.5;


            grid_map_.at("traversability", *iterator) = std::clamp(cost, 0.0f, 1.0f);

        }
        
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