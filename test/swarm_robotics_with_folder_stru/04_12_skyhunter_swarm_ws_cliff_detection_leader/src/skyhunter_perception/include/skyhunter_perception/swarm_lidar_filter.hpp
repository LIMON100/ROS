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

    // TF2 for Coordinate Transformations
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

    // State Variables
    geometry_msgs::msg::PoseArray latest_swarm_poses_;
    std::mutex swarm_mutex_;
    bool has_swarm_data_ = false;

    // Parameters
    double filter_radius_sq_;
};

#endif // SKYHUNTER_PERCEPTION__SWARM_LIDAR_FILTER_HPP_