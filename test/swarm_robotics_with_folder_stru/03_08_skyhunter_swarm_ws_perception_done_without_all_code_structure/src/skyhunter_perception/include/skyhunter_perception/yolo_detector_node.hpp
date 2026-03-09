// #ifndef SKYHUNTER_PERCEPTION__YOLO_DETECTOR_NODE_HPP_
// #define SKYHUNTER_PERCEPTION__YOLO_DETECTOR_NODE_HPP_

// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/image.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <sensor_msgs/msg/joint_state.hpp>
// #include <nav_msgs/msg/odometry.hpp>
// #include <std_msgs/msg/float64.hpp>
// #include <std_msgs/msg/int8.hpp>
// #include <skyhunter_msgs/msg/leader_state.hpp>
// #include <skyhunter_msgs/msg/detection_array.hpp>

// // Internal Libraries
// #include "skyhunter_perception/yolo_engine.hpp"
// #include "ByteTrack/BYTETracker.h"

// // Math & PCL
// #include <tf2/utils.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <map> 


// struct TargetHistory {
//     cv::Point center;
//     rclcpp::Time timestamp;
// };

// class YoloDetectorNode : public rclcpp::Node {
// public:
//     YoloDetectorNode();
//     virtual ~YoloDetectorNode();

// private:
//     // Callbacks
//     void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg);
//     void image_callback(const sensor_msgs::msg::Image::SharedPtr msg);

//     // State
//     bool is_leader_ = true;
//     double closest_blob_dist_ = 10.0;
//     double closest_blob_angle_ = 0.0;
//     bool blob_detected_ = false;
//     nav_msgs::msg::Odometry latest_odom_;
//     double current_pan_ = 0.0;
//     skyhunter_msgs::msg::LeaderState last_swarm_msg_;

//     // Engines
//     std::unique_ptr<YoloEngine> engine_;
//     std::unique_ptr<byte_track::BYTETracker> tracker_;
//     std::map<int, TargetHistory> track_history_;

//     // Subscribers
//     rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
//     rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
//     rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
//     rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_joints_;
//     rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_swarm_state_;

//     // Publishers
//     rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr pub_leader_state_;
//     rclcpp::Publisher<skyhunter_msgs::msg::DetectionArray>::SharedPtr pub_detections_;
//     rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_pan_;
//     rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_tilt_;
//     rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_overlay_;
// };

// #endif // SKYHUNTER_PERCEPTION__YOLO_DETECTOR_NODE_HPP_



#ifndef SKYHUNTER_PERCEPTION__YOLO_DETECTOR_NODE_HPP_
#define SKYHUNTER_PERCEPTION__YOLO_DETECTOR_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/int8.hpp>
#include <std_msgs/msg/string.hpp> // NEW: For voting
#include <skyhunter_msgs/msg/leader_state.hpp>
#include <skyhunter_msgs/msg/detection_array.hpp>

// Internal Libraries
#include "skyhunter_perception/yolo_engine.hpp"
#include "ByteTrack/BYTETracker.h"

// Math & PCL
#include <tf2/utils.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <map> // NEW: For counting votes

// --- 1. RESTORE THE ENUM ---
enum TrackingState { PATROL, SCANNING, COMBAT, ENGAGED }; // Added ENGAGED

class YoloDetectorNode : public rclcpp::Node {
public:
    YoloDetectorNode();
    virtual ~YoloDetectorNode();

private:
    // Callbacks
    void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg);
    void image_callback(const sensor_msgs::msg::Image::SharedPtr msg);

    // State
    bool is_leader_ = true;
    std::string my_ns_; 
    double closest_blob_dist_ = 10.0;
    double closest_blob_angle_ = 0.0;
    bool blob_detected_ = false;
    nav_msgs::msg::Odometry latest_odom_;
    double current_pan_ = 0.0;
    skyhunter_msgs::msg::LeaderState last_swarm_msg_;

    // NEW: Vote tracking
    std::map<std::string, rclcpp::Time> follower_confirmations_;

    // Engines
    std::unique_ptr<YoloEngine> engine_;
    std::unique_ptr<byte_track::BYTETracker> tracker_;

    // Subscribers
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_joints_;
    rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_swarm_state_;
    
    // NEW: Vote Subscriber
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_confirm_; 

    // Publishers
    rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr pub_leader_state_;
    rclcpp::Publisher<skyhunter_msgs::msg::DetectionArray>::SharedPtr pub_detections_;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_pan_;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_tilt_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_overlay_;
    
    // NEW: Vote Publisher
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_confirm_; 
};

#endif // SKYHUNTER_PERCEPTION__YOLO_DETECTOR_NODE_HPP_