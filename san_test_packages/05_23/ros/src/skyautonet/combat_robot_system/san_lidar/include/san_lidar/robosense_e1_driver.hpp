// SAN v1.5.2 PHASE 7 - Robosense E1 driver shim.
//
// Subscribes to the rslidar_sdk PointCloud2 topic, applies the mount
// transform (0.5 m above ground, 0.25 m forward of the body origin),
// runs ground/obstacle segmentation, and republishes the separated
// clouds plus a `slope_deg` float for the traversability layer.
//
// The upstream rslidar_sdk node is launched separately; this node is
// the SAN-side translator and only depends on sensor_msgs.
//
// DCN-2026-006 EXT (v1.5.2):
//   D-017 RANSAC fail alarm    : publish ThreatAlert (severity=
//                                 WARNING/CRITICAL based on the
//                                 consecutive-fail streak) and a
//                                 DiagnosticArray status whenever
//                                 RANSAC cannot fit a ground plane.

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <std_msgs/msg/float32.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>

#include "san_lidar/ground_segmenter.hpp"

#include <geometry_msgs/msg/pose_array.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <mutex>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

namespace san_lidar {

class RobosenseE1Driver : public rclcpp::Node {
public:
    RobosenseE1Driver();
    explicit RobosenseE1Driver(const rclcpp::NodeOptions& options);

    // Test hooks.
    bool hasGroundCloud() const { return last_ground_count_ > 0; }
    bool hasObstacleCloud() const { return last_obstacle_count_ > 0; }
    float lastSlopeDeg() const { return last_slope_deg_; }
    uint32_t consecutiveFailCount() const { return consecutive_fails_; }

private:
    // Mount params (SDD-SWARM §4.5).
    double mount_height_m_ = 0.50;
    double mount_forward_m_ = 0.25;

    // Topics.
    std::string input_topic_ = "/rslidar_points";
    std::string ground_topic_ = "/san/lidar/ground";
    std::string obstacle_topic_ = "/san/lidar/obstacles";

    GroundSegmenter segmenter_;
    std::size_t last_ground_count_ = 0;
    std::size_t last_obstacle_count_ = 0;
    float last_slope_deg_ = 0.0f;

    // [DCN-2026-006 EXT D-017] consecutive-fail streak. WARNING
    // emitted on first fail, escalated to CRITICAL after this many
    // consecutive fails (default 5 frames @ 10 Hz = 0.5 s blind).
    uint32_t consecutive_fails_ = 0;
    uint32_t critical_after_fails_ = 5;
    std::chrono::steady_clock::time_point last_threat_at_{};

    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr ground_pub_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr obstacle_pub_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr slope_pub_;

    // [DCN-2026-006 EXT D-017]
    rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diag_pub_;
    rclcpp::Publisher<combat_robot_msgs::msg::ThreatAlert>::SharedPtr threat_pub_;
    rclcpp::TimerBase::SharedPtr diag_timer_;

    // [Sanitizer-hardening] MutuallyExclusive callback group binding
    // sub_ and diag_timer_. The pointcloud callback writes
    // consecutive_fails_ / last_threat_at_ / last_*_count_; the 1 Hz
    // diagnostics timer reads them. Without serialization the timer
    // can read a torn time_point (16 bytes on most platforms) or an
    // inconsistent (level, message) pair.
    rclcpp::CallbackGroup::SharedPtr cb_group_;

    rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr swarm_sub_;
    geometry_msgs::msg::PoseArray latest_swarm_poses_;
    std::mutex swarm_mutex_;
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

    void declareParameters();
    void readParameters();
    void wireInterfaces();
    void onPointCloud(sensor_msgs::msg::PointCloud2::SharedPtr msg);

    // [DCN-2026-006 EXT D-017]
    void publishThreatAlert(GroundSegmenterFailReason reason);
    void publishDiagnostics();
};

}  // namespace san_lidar
