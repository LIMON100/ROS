#ifndef SKYHUNTER_PERCEPTION__SWARM_LIDAR_FILTER_HPP_
#define SKYHUNTER_PERCEPTION__SWARM_LIDAR_FILTER_HPP_

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <vector>
#include <mutex>
#include <string>

class SwarmLidarFilter : public rclcpp::Node {
public:
    SwarmLidarFilter();
    virtual ~SwarmLidarFilter() = default;

private:
    // Callbacks
    void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg);
    void swarm_callback(const geometry_msgs::msg::PoseArray::SharedPtr msg);

    // ROS 2 Interfaces
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
    rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr sub_swarm_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_scan_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_ground_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_clearing_;

    // TF2 for Coordinate Transformations
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

    // State Variables
    geometry_msgs::msg::PoseArray latest_swarm_poses_;
    std::mutex swarm_mutex_;
    bool has_swarm_data_ = false;

    // Parameters
    double filter_radius_sq_;

    const double GRID_RES = 0.20;          // 20cm grid cells
    const double GRID_SIZE_M = 40.0;       // 40m x 40m
    const int GRID_CELLS = int(GRID_SIZE_M / GRID_RES); 
    const double SLOPE_THRESHOLD = 0.12;
};

#endif // SKYHUNTER_PERCEPTION__SWARM_LIDAR_FILTER_HPP_