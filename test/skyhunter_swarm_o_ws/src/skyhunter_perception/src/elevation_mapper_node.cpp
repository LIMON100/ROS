// // //WORKABLE 02-02
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
//         // --- Parameters ---
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

//         // Layers
//         grid_map_.add("elevation", NAN);
//         grid_map_.add("slope", NAN);
//         grid_map_.add("step_height", NAN);
//         grid_map_.add("roughness", NAN);
//         grid_map_.add("density_low", 0.0);  
//         grid_map_.add("density_high", 0.0); 
//         grid_map_.add("drop_risk", 0.0);
//         grid_map_.add("traversability", 0.0);

//         auto qos = rclcpp::SensorDataQoS();
//         cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//             cloud_topic, qos, 
//             std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));

//         map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("elevation_map", 1);
        
//         tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//         tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

//         RCLCPP_INFO(this->get_logger(), "Elevation Mapper Started. Filtering body points relative to robot.");
//     }

// private:
//     void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
//     {
//         std::string target_frame_id = msg->header.frame_id;
        
//         // Handle namespace prefixing if needed (Tin3bot specific)
//         size_t slash_pos = robot_base_frame_.find('/');
//         if (slash_pos != std::string::npos) {
//             std::string prefix = robot_base_frame_.substr(0, slash_pos + 1); 
//             if (target_frame_id.find(prefix) == std::string::npos) {
//                 target_frame_id = prefix + target_frame_id;
//             }
//         }

//         sensor_msgs::msg::PointCloud2 map_frame_cloud_msg;
//         geometry_msgs::msg::TransformStamped transform;

//         try {
//             transform = tf_buffer_->lookupTransform(
//                 map_frame_, target_frame_id,
//                 tf2::TimePointZero, std::chrono::milliseconds(100));
//             tf2::doTransform(*msg, map_frame_cloud_msg, transform);
//         } catch (tf2::TransformException &ex) {
//             RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "TF failure: %s", ex.what());
//             return;
//         }

//         // Get Robot Position from the transform (Translation from map -> sensor)
//         // Note: Ideally we want map -> base_link, but this is map -> sensor.
//         // For self-filtering, using the sensor position is actually safer as LIDAR is on the robot.
//         double robot_x = transform.transform.translation.x;
//         double robot_y = transform.transform.translation.y;
//         double yaw = tf2::getYaw(transform.transform.rotation);

//         // Move Grid Map Center to Robot
//         grid_map_.move(grid_map::Position(robot_x, robot_y));

//         pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
//         pcl::fromROSMsg(map_frame_cloud_msg, *cloud);

//         // Reset accumulation layers
//         grid_map_.clear("density_low");
//         grid_map_.clear("density_high");

//         for (const auto& point : cloud->points)
//         {
//             // --- FIXED SELF FILTER ---
//             // Calculate distance from point to robot center (in Odom frame)
//             double dx = point.x - robot_x;
//             double dy = point.y - robot_y;
//             double dist_sq = dx*dx + dy*dy;

//             // Filter out points within 0.9m radius of the sensor/robot center.
//             // Robot length ~1.3m (0.65m half). 0.9m clears the bumpers.
//             // if (dist_sq < (0.9 * 0.9)) {
//             //     continue; 
//             // }
//             // -------------------------
            
//             double rel_x = point.x - robot_x;
//             double rel_y = point.y - robot_y;
            
//             // Rotate to robot frame (approximate, using yaw)
//             // We need to filter based on the robot's orientation
//             double local_x = rel_x * cos(-yaw) - rel_y * sin(-yaw);
//             double local_y = rel_x * sin(-yaw) + rel_y * cos(-yaw);

//             // Robot Dimensions: Length ~1.3m, Width ~0.85m
//             // We add a safety margin (e.g., 0.2m)
//             // Exclusion Box: x: [-0.9, +0.9], y: [-0.6, +0.6]
//             if (std::abs(local_x) < 0.9 && std::abs(local_y) < 0.6) {
//                 continue; // Ignore points hitting the robot body
//             }


//             grid_map::Index index;
//             grid_map::Position position(point.x, point.y);

//             if (!grid_map_.getIndex(position, index)) continue;

//             float current_elev = grid_map_.at("elevation", index);
//             if (!std::isfinite(current_elev) || point.z < current_elev) {
//                 grid_map_.at("elevation", index) = point.z;
//             }

//             float h = point.z - grid_map_.at("elevation", index);
//             if (h > 0.1 && h < 0.3) {
//                 grid_map_.at("density_low", index) += 1.0;
//             }
//             else if (h >= 0.3 && h < 1.5) {
//                 grid_map_.at("density_high", index) += 1.0;
//             }
//         }

//         // --- ROI Iterator ---
//         grid_map::Polygon roi_polygon;
//         roi_polygon.setFrameId(map_frame_);

//         double look_ahead = 5.0;
//         double look_behind = -1.0;
//         double half_width = 1.0; 

//         double c = cos(yaw);
//         double s = sin(yaw);

//         auto add_corner = [&](double x, double y) {
//             double rx = x * c - y * s + robot_x;
//             double ry = x * s + y * c + robot_y;
//             roi_polygon.addVertex(grid_map::Position(rx, ry));
//         };

//         add_corner(look_ahead, half_width);   
//         add_corner(look_ahead, -half_width);  
//         add_corner(look_behind, -half_width); 
//         add_corner(look_behind, half_width);  

//         float max_safe_slope = this->get_parameter("max_slope_deg").as_double();
//         float max_step = this->get_parameter("max_step_height").as_double();
//         float max_gap = this->get_parameter("max_gap_size").as_double();


//         for (grid_map::PolygonIterator iterator(grid_map_, roi_polygon); !iterator.isPastEnd(); ++iterator)
//         {
//             if (!grid_map_.isValid(*iterator, "elevation")) continue;

//             float elevation = grid_map_.at("elevation", *iterator);
//             float max_height_diff = 0.0;
//             float max_step_down = 0.0; 
            
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

//             float roughness = 0.0;
//             if (count > 1) {
//                 float mean = sum_z / count;
//                 float variance = (sum_sq_z / count) - (mean * mean);
//                 roughness = std::sqrt(std::max(0.0f, variance));
//             }
//             grid_map_.at("roughness", *iterator) = roughness;

//             float slope_deg = (atan2(max_height_diff, grid_map_.getResolution()) * 180.0 / M_PI);
//             grid_map_.at("slope", *iterator) = slope_deg;
//             grid_map_.at("step_height", *iterator) = max_height_diff;

//             if (max_step_down > max_gap) {
//                 grid_map_.at("drop_risk", *iterator) = 1.0;
//             } else {
//                 grid_map_.at("drop_risk", *iterator) = 0.0;
//             }

//             float cost = 0.0;
//             if (grid_map_.at("density_high", *iterator) > 5) cost = 1.0; 
//             else if (grid_map_.at("drop_risk", *iterator) > 0.5) cost = 1.0; 
//             else if (slope_deg > max_safe_slope) cost = 1.0; 
//             else if (max_height_diff > max_step) cost = 1.0; 
//             else if (roughness > 0.1) cost = 0.8; 
//             else if (grid_map_.at("density_low", *iterator) > 10) cost = 0.6; 
//             else cost = (slope_deg / max_safe_slope) * 0.5;


//             grid_map_.at("traversability", *iterator) = std::clamp(cost, 0.0f, 1.0f);

//         }
        
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



// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <geometry_msgs/msg/transform_stamped.hpp>
// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <pcl/common/common.h>
// #include <pcl/filters/voxel_grid.h>
// #include <tf2/utils.h>
// #include <tf2_ros/buffer.h>
// #include <tf2_ros/transform_listener.h>
// #include <tf2_sensor_msgs/tf2_sensor_msgs.hpp>
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
// #include <grid_map_ros/grid_map_ros.hpp>
// #include <grid_map_msgs/msg/grid_map.hpp>
// #include <Eigen/Dense>

// using PointT = pcl::PointXYZ;

// class ElevationMapperNode : public rclcpp::Node
// {
// public:
//     ElevationMapperNode() : Node("elevation_mapper_node")
//     {
//         // Parameters
//         this->declare_parameter<std::string>("base_frame", "base_footprint"); 
//         this->declare_parameter<std::string>("map_frame", "odom");
//         this->declare_parameter<std::string>("cloud_topic", "/scan/points"); 
//         this->declare_parameter<double>("map.length", 12.0);
//         this->declare_parameter<double>("map.resolution", 0.05);
//         this->declare_parameter<double>("max_slope_deg", 35.0);
//         this->declare_parameter<double>("max_step_height", 0.20); // 20cm step limit
//         this->declare_parameter<double>("max_gap_size", 0.25);    // 25cm ditch limit

//         map_frame_ = this->get_parameter("map_frame").as_string();
//         robot_base_frame_ = this->get_parameter("base_frame").as_string();
//         std::string cloud_topic = this->get_parameter("cloud_topic").as_string();

//         // Grid Map Setup
//         grid_map_.setFrameId(map_frame_);
//         grid_map_.setGeometry(
//             grid_map::Length(this->get_parameter("map.length").as_double(), this->get_parameter("map.length").as_double()),
//             this->get_parameter("map.resolution").as_double()
//         );

//         // FULL LAYERS RESTORED
//         grid_map_.add("elevation", NAN);
//         grid_map_.add("slope", NAN);
//         grid_map_.add("step_height", NAN);
//         grid_map_.add("roughness", NAN);
//         grid_map_.add("density_high", 0.0); 
//         grid_map_.add("density_low", 0.0);
//         grid_map_.add("drop_risk", 0.0);
//         grid_map_.add("traversability", 0.0); 

//         auto qos = rclcpp::SensorDataQoS();
//         cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
//             cloud_topic, qos, std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));

//         map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("elevation_map", 1);
        
//         tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
//         tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
        
//         startup_time_ = this->get_clock()->now();

//         RCLCPP_INFO(this->get_logger(), "Full Feature Elevation Mapper Started.");
//     }

// private:
//     void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
//     {
//         // 1. WARMUP (Avoid Spawn Glitches)
//         if ((this->get_clock()->now() - startup_time_).seconds() < 2.0) return;

//         std::string target_frame_id = msg->header.frame_id;
//         // Namespace fix
//         size_t slash_pos = robot_base_frame_.find('/');
//         if (slash_pos != std::string::npos) {
//             std::string prefix = robot_base_frame_.substr(0, slash_pos + 1); 
//             if (target_frame_id.find(prefix) == std::string::npos) {
//                 target_frame_id = prefix + target_frame_id;
//             }
//         }

//         // 2. TRANSFORM TO SENSOR FRAME FIRST (For Filtering)
//         // Actually, PCL handles this. We just need the raw cloud to check distance from origin.
//         pcl::PointCloud<PointT>::Ptr raw_cloud(new pcl::PointCloud<PointT>());
//         pcl::fromROSMsg(*msg, *raw_cloud);
        
//         pcl::PointCloud<PointT>::Ptr clean_cloud(new pcl::PointCloud<PointT>());
//         clean_cloud->reserve(raw_cloud->size());

//         // --- FILTERING (In Sensor Frame) ---
//         for (const auto& p : raw_cloud->points) {
//             // Ignore Self (1.2m radius from sensor)
//             if ((p.x*p.x + p.y*p.y) < (1.2 * 1.2)) continue;
//             // Ignore Ceiling
//             if (p.z > 2.0) continue;
//             // Ignore Floor Noise (Below tracks)
//             if (p.z < -0.3) continue;

//             clean_cloud->push_back(p);
//         }

//         // 3. TRANSFORM TO MAP FRAME
//         geometry_msgs::msg::TransformStamped tf;
//         try {
//             tf = tf_buffer_->lookupTransform(map_frame_, target_frame_id, tf2::TimePointZero);
//         } catch (...) { return; }

//         sensor_msgs::msg::PointCloud2 clean_msg, map_cloud_msg;
//         pcl::toROSMsg(*clean_cloud, clean_msg);
//         clean_msg.header = msg->header; // Restore original header
//         tf2::doTransform(clean_msg, map_cloud_msg, tf);
        
//         pcl::PointCloud<PointT>::Ptr map_cloud(new pcl::PointCloud<PointT>());
//         pcl::fromROSMsg(map_cloud_msg, *map_cloud);

//         // 4. MOVE MAP
//         double robot_x = tf.transform.translation.x;
//         double robot_y = tf.transform.translation.y;
//         double yaw = tf2::getYaw(tf.transform.rotation);
//         grid_map_.move(grid_map::Position(robot_x, robot_y));

//         // Reset Dynamic Layers
//         grid_map_.clear("density_high");
//         grid_map_.clear("density_low");

//         // 5. UPDATE ELEVATION
//         for (const auto& p : map_cloud->points) {
//             grid_map::Index idx;
//             if (!grid_map_.getIndex(grid_map::Position(p.x, p.y), idx)) continue;

//             // Min-Height Logic (Find Ground)
//             float curr = grid_map_.at("elevation", idx);
//             if (!std::isfinite(curr) || p.z < curr) {
//                 grid_map_.at("elevation", idx) = p.z;
//             }

//             // Density Check (Obstacles)
//             float h = p.z - grid_map_.at("elevation", idx);
//             if (h > 0.3) grid_map_.at("density_high", idx) += 1.0;
//             else if (h > 0.05) grid_map_.at("density_low", idx) += 1.0;
//         }

//         // 6. PROCESS FEATURES (ROI Only)
//         grid_map::Polygon roi;
//         roi.setFrameId(map_frame_);
//         double c=cos(yaw), s=sin(yaw);
//         auto add = [&](double x, double y){ roi.addVertex(grid_map::Position(x*c-y*s+robot_x, x*s+y*c+robot_y)); };
//         add(6.0, 2.0); add(6.0, -2.0); add(-2.0, -2.0); add(-2.0, 2.0);

//         float max_slope = this->get_parameter("max_slope_deg").as_double();
//         float max_step = this->get_parameter("max_step_height").as_double();

//         for (grid_map::PolygonIterator it(grid_map_, roi); !it.isPastEnd(); ++it) {
//             if (!grid_map_.isValid(*it, "elevation")) continue;

//             float elevation = grid_map_.at("elevation", *it);
//             float max_diff = 0.0, max_down = 0.0;
//             float sum_z=0, sum_sq=0; int n=0;

//             // Neighbor Check (Roughness & Slope)
//             grid_map::Index idx = *it;
//             for (int dx=-1; dx<=1; ++dx) {
//                 for (int dy=-1; dy<=1; ++dy) {
//                     if (dx==0 && dy==0) continue;
//                     grid_map::Index ni(idx(0)+dx, idx(1)+dy);
//                     if (grid_map_.isValid(ni, "elevation")) {
//                         float ne = grid_map_.at("elevation", ni);
//                         float diff = elevation - ne;
//                         if (std::abs(diff) > max_diff) max_diff = std::abs(diff);
//                         if (diff > max_down) max_down = diff;
//                         sum_z += ne; sum_sq += ne*ne; n++;
//                     }
//                 }
//             }

//             // Metrics
//             float roughness = 0.0;
//             if (n>0) roughness = std::sqrt((sum_sq/n) - (sum_z/n)*(sum_z/n));
//             float slope = atan2(max_diff, 0.05) * 180.0 / M_PI;
            
//             grid_map_.at("roughness", *it) = roughness;
//             grid_map_.at("slope", *it) = slope;
//             grid_map_.at("step_height", *it) = max_diff;
//             grid_map_.at("drop_risk", *it) = (max_down > 0.25) ? 1.0 : 0.0;

//             // Traversability
//             float cost = 0.0;
//             if (grid_map_.at("density_high", *it) > 3) cost = 1.0; // Solid Obstacle
//             else if (grid_map_.at("drop_risk", *it) > 0.5) cost = 1.0; // Cliff
//             else if (slope > max_slope) cost = 1.0; // Too Steep
//             else if (max_diff > max_step) cost = 1.0; // Step
//             else if (roughness > 0.1) cost = 0.6; // Bumpy
            
//             grid_map_.at("traversability", *it) = cost;
//         }

//         std::unique_ptr<grid_map_msgs::msg::GridMap> out = grid_map::GridMapRosConverter::toMessage(grid_map_);
//         map_publisher_->publish(std::move(out));
//     }

//     rclcpp::Time startup_time_;
//     grid_map::GridMap grid_map_;
//     std::string map_frame_, robot_base_frame_;
//     rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_subscription_;
//     rclcpp::Publisher<grid_map_msgs::msg::GridMap>::SharedPtr map_publisher_;
//     std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
//     std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
// };

// int main(int argc, char *argv[]) {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<ElevationMapperNode>());
//     rclcpp::shutdown();
//     return 0;
// }






#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <visualization_msgs/msg/marker_array.hpp> 

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/common/common.h>
#include <pcl/filters/voxel_grid.h>

#include <tf2/utils.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_sensor_msgs/tf2_sensor_msgs.hpp>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include <grid_map_ros/grid_map_ros.hpp>
#include <grid_map_msgs/msg/grid_map.hpp>
#include <Eigen/Dense>
#include <omp.h>

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

        this->declare_parameter<double>("map.length", 12.0); 
        this->declare_parameter<double>("map.resolution", 0.05);

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
        grid_map_.add("density_high", 0.0); 
        grid_map_.add("drop_risk", 0.0);
        grid_map_.add("traversability", 0.0); 

        auto qos = rclcpp::SensorDataQoS();
        cloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            cloud_topic, qos, 
            std::bind(&ElevationMapperNode::cloud_callback, this, std::placeholders::_1));

        map_publisher_ = this->create_publisher<grid_map_msgs::msg::GridMap>("elevation_map", 1);
        
        // DEBUG: Publishes Markers where obstacles are detected
        debug_obstacle_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>("debug_obstacles", 1);

        tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        RCLCPP_INFO(this->get_logger(), "Elevation Mapper: ROBUST MODE (Box Filter + Debugging)");
    }

private:
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // 1. TF Lookup
        std::string target_frame_id = msg->header.frame_id;
        size_t slash_pos = robot_base_frame_.find('/');
        if (slash_pos != std::string::npos) {
            std::string prefix = robot_base_frame_.substr(0, slash_pos + 1); 
            if (target_frame_id.find(prefix) == std::string::npos) {
                target_frame_id = prefix + target_frame_id;
            }
        }

        geometry_msgs::msg::TransformStamped transform;
        try {
             // Use latest available transform
             transform = tf_buffer_->lookupTransform(
                map_frame_, target_frame_id,
                tf2::TimePointZero);
        } catch (tf2::TransformException &ex) { return; }

        double sensor_x = transform.transform.translation.x;
        double sensor_y = transform.transform.translation.y;
        double yaw = tf2::getYaw(transform.transform.rotation);

        // 2. Convert to PCL
        pcl::PointCloud<PointT>::Ptr raw_cloud(new pcl::PointCloud<PointT>());
        pcl::fromROSMsg(*msg, *raw_cloud);

        // 3. Voxel Downsampling (Hardware Acceleration Step A)
        pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
        pcl::VoxelGrid<PointT> sor;
        sor.setInputCloud(raw_cloud);
        sor.setLeafSize(0.1f, 0.1f, 0.1f); // 10cm grid
        sor.filter(*cloud);

        grid_map_.move(grid_map::Position(sensor_x, sensor_y));
        grid_map_.clear("density_high");

        // 4. Parallel Transform & Filter (Hardware Acceleration Step B)
        size_t n_points = cloud->points.size();
        
        // Temporary buffers for thread-safe writing
        std::vector<PointT> valid_points;
        valid_points.reserve(n_points);

        // Pre-calc rotation
        double cy = cos(yaw);
        double sy = sin(yaw);
        double tx = sensor_x;
        double ty = sensor_y;
        double tz = transform.transform.translation.z;

        // We use a thread-private buffer strategy to avoid locking
        #pragma omp parallel
        {
            std::vector<PointT> thread_buffer;
            
            #pragma omp for nowait
            for (size_t i = 0; i < n_points; ++i) {
                const auto& p = cloud->points[i];
                
                // Manual Transform
                double map_x = (p.x * cy - p.y * sy) + tx;
                double map_y = (p.x * sy + p.y * cy) + ty;
                double map_z = p.z + tz;

                // --- ROBUST FILTERS ---
                // Z-Filter
                if (map_z < -0.2 || map_z > 2.0) continue; 

                // Box Self-Filter (Local Coords check)
                // Filter out robot body BEHIND the sensor (x < 0.1)
                // Assuming standard robot size: length ~1.4, width ~0.9
                if (p.x > -1.4 && p.x < 0.1 && std::abs(p.y) < 0.5) continue;

                // If valid, store it
                PointT p_out;
                p_out.x = map_x; p_out.y = map_y; p_out.z = map_z;
                thread_buffer.push_back(p_out);
            }

            // Merge thread buffers
            #pragma omp critical
            valid_points.insert(valid_points.end(), thread_buffer.begin(), thread_buffer.end());
        }

        // 5. Serial Map Update (GridMap is not thread-safe for writing)
        visualization_msgs::msg::MarkerArray markers;
        int id = 0;

        for (const auto& p : valid_points)
        {
            grid_map::Index index;
            if (!grid_map_.getIndex(grid_map::Position(p.x, p.y), index)) continue;

            float current_elev = grid_map_.at("elevation", index);
            if (!std::isfinite(current_elev) || p.z > current_elev) {
                grid_map_.at("elevation", index) = p.z;
            }

            if (p.z > 0.25) {
                grid_map_.at("density_high", index) += 1.0;

                // Debug Visualization (Downsampled)
                if (id < 50 && (id % 5 == 0)) {
                    visualization_msgs::msg::Marker m;
                    m.header.frame_id = map_frame_;
                    m.header.stamp = this->get_clock()->now();
                    m.ns = "obs"; m.id = id++; m.type = 1; 
                    m.pose.position.x = p.x; m.pose.position.y = p.y; m.pose.position.z = p.z;
                    m.scale.x = 0.1; m.scale.y = 0.1; m.scale.z = 0.1;
                    m.color.a = 1.0; m.color.r = 1.0; 
                    markers.markers.push_back(m);
                }
            }
        }
        debug_obstacle_pub_->publish(markers);

        // 6. Traversability (Parallelizable)
        grid_map::Polygon roi_polygon;
        roi_polygon.setFrameId(map_frame_);
        double look_ahead = 5.0; double half_width = 1.2; 
        auto add_corner = [&](double x, double y) {
            double rx = x * cy - y * sy + sensor_x; double ry = x * sy + y * cy + sensor_y;
            roi_polygon.addVertex(grid_map::Position(rx, ry));
        };
        add_corner(look_ahead, half_width); add_corner(look_ahead, -half_width);  
        add_corner(-1.0, -half_width); add_corner(-1.0, half_width);  

        for (grid_map::PolygonIterator iterator(grid_map_, roi_polygon); !iterator.isPastEnd(); ++iterator) {
            if (!grid_map_.isValid(*iterator, "elevation")) continue;
            
            float d_high = grid_map_.at("density_high", *iterator);
            if (std::isnan(d_high)) d_high = 0.0;

            if (d_high > 1.0) grid_map_.at("traversability", *iterator) = 1.0; 
            else grid_map_.at("traversability", *iterator) = 0.0; 
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
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr debug_obstacle_pub_;

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