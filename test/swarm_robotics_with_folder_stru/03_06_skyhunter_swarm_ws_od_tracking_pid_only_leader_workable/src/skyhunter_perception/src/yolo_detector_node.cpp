
// // Detection+tracking+change gimbal works
// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/image.hpp>
// #include <std_msgs/msg/float64.hpp>
// #include <cv_bridge/cv_bridge.h>
// #include <skyhunter_msgs/msg/detection_array.hpp>
// #include "skyhunter_perception/yolo_engine.hpp"
// #include "ByteTrack/BYTETracker.h" 
// #include <ament_index_cpp/get_package_share_directory.hpp>

// class YoloDetectorNode : public rclcpp::Node {
// public:
//     YoloDetectorNode() : Node("yolo_detector_node") {
//         std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
//         std::string model_path = pkg_dir + "/models/yolov8m.onnx";
//         engine_ = std::make_unique<YoloEngine>(model_path, true);

//         // --- NEW: Initialize ByteTrack (30 FPS, 30 frame buffer) ---
//         tracker_ = std::make_unique<byte_track::BYTETracker>(30, 30);

//         auto qos = rclcpp::SensorDataQoS();
//         sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(
//             "/rgb_camera/image_raw", qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));

//         // --- NEW: Gimbal Command Publishers ---
//         pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("/gimbal/pan_cmd", 10);
//         pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("/gimbal/tilt_cmd", 10);

//         pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("/perception/sh01_overlay", 10);
//         cv::namedWindow("LOCK_ON_VIEW", cv::WINDOW_AUTOSIZE);
//     }

// private:
//     void image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
//         cv_bridge::CvImagePtr cv_ptr;
//         try {
//             cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
//         } catch (...) { return; }

//         auto yolo_results = engine_->run_inference(cv_ptr->image);

//         // 1. Convert to ByteTrack Objects
//         std::vector<byte_track::Object> objects;
//         for (const auto& res : yolo_results) {
//             if (engine_->class_names[res.class_id] == "person") {
//                 byte_track::Rect<float> rect(res.box.x, res.box.y, res.box.width, res.box.height);
//                 objects.emplace_back(rect, res.class_id, res.confidence);
//             }
//         }

//         // 2. Update Tracker
//         cv::Mat inv_affine = cv::Mat::eye(2, 3, CV_64F);
//         auto tracked_targets = tracker_->update(objects, inv_affine);

//         // --- TACTICAL SELECTION: FIND THE PHYSICALLY CLOSEST TARGET ---
//         int closest_target_idx = -1;
//         double min_meter_dist = 1e9;
//         std::vector<double> distances; // Keep track of distances for labels

//         for (size_t i = 0; i < tracked_targets.size(); ++i) {
//             auto r = tracked_targets[i]->getRect();
//             // Tactical Distance Math (Focal Length 550, Human height 1.7m)
//             double distance_m = (550.0 * 1.7) / r.height();
//             distances.push_back(distance_m);

//             if (distance_m < min_meter_dist) {
//                 min_meter_dist = distance_m;
//                 closest_target_idx = i;
//             }
//         }

//         // 3. DRAWING & PID CONTROL
//         for (size_t i = 0; i < tracked_targets.size(); ++i) {
//             auto r = tracked_targets[i]->getRect();
//             cv::Rect box(r.x(), r.y(), r.width(), r.height());
//             std::string id_str = "ID: " + std::to_string(tracked_targets[i]->getTrackId());
//             std::string dist_str = std::to_string(distances[i]).substr(0, 4) + "m";

//             if ((int)i == closest_target_idx) {
//                 // ==========================================
//                 // PRIMARY TARGET: RED BOX + LOCK-ON
//                 // ==========================================
//                 cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 0, 255), 3); // Thick Red
                
//                 // Draw Dynamic Crosshair on Target
//                 cv::drawMarker(cv_ptr->image, cv::Point(r.x() + r.width()/2, r.y() + r.height()/2), 
//                               cv::Scalar(0, 0, 255), cv::MARKER_CROSS, 40, 2);
                
//                 // Label with Status
//                 cv::putText(cv_ptr->image, "LOCKED " + id_str, cv::Point(r.x(), r.y() - 25), 0, 0.7, cv::Scalar(0, 0, 255), 2);
//                 cv::putText(cv_ptr->image, dist_str, cv::Point(r.x(), r.y() - 5), 0, 0.6, cv::Scalar(0, 0, 255), 2);

//                 // GIMBAL CONTROL MATH
//                 double tx = r.x() + (r.width() / 2.0);
//                 double ty = r.y() + (r.height() / 2.0);
//                 double err_x = (tx - 320.0) / 320.0;
//                 double err_y = (ty - 240.0) / 240.0;

//                 std_msgs::msg::Float64 p_msg, t_msg;
//                 p_msg.data = err_x * -0.8; // Pan PID
//                 t_msg.data = err_y * 0.5;  // Tilt PID
//                 pub_pan_->publish(p_msg);
//                 pub_tilt_->publish(t_msg);

//             } else {
//                 // ==========================================
//                 // SECONDARY TARGETS: GREEN BOX
//                 // ==========================================
//                 cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1); // Thin Green
//                 cv::putText(cv_ptr->image, id_str + " [" + dist_str + "]", 
//                             cv::Point(r.x(), r.y() - 5), 0, 0.5, cv::Scalar(0, 255, 0), 1);
//             }
//         }

//         // 4. Draw FIXED RIFLE CENTER (Cyan)
//         cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(255, 255, 0), cv::MARKER_CROSS, 20, 1);

//         cv::imshow("TACTICAL_TRACKER", cv_ptr->image);
//         cv::waitKey(1);
//         pub_overlay_->publish(*(cv_ptr->toImageMsg()));
//     }

//     std::unique_ptr<YoloEngine> engine_;
//     std::unique_ptr<byte_track::BYTETracker> tracker_; // --- NEW TRACKER ---
//     rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
//     rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_pan_, pub_tilt_; // --- NEW COMMANDS ---
//     rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_overlay_;
// };

// int main(int argc, char** argv) {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<YoloDetectorNode>());
//     cv::destroyAllWindows();
//     rclcpp::shutdown();
//     return 0;
// }






// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/image.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <sensor_msgs/msg/joint_state.hpp>
// #include <nav_msgs/msg/odometry.hpp>
// #include <std_msgs/msg/float64.hpp>
// #include <cv_bridge/cv_bridge.h>
// #include <skyhunter_msgs/msg/leader_state.hpp>
// #include <skyhunter_msgs/msg/detection.hpp>
// #include "skyhunter_perception/yolo_engine.hpp"
// #include "ByteTrack/BYTETracker.h"
// #include <tf2/utils.h>
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <omp.h>
// #include <ament_index_cpp/get_package_share_directory.hpp>

// enum TrackingState { PATROL, SCANNING, COMBAT };

// class YoloDetectorNode : public rclcpp::Node {
// public:
//     YoloDetectorNode() : Node("yolo_detector_node") {
//         std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
//         engine_ = std::make_unique<YoloEngine>(pkg_dir + "/models/yolov8m.onnx", true);
//         tracker_ = std::make_unique<byte_track::BYTETracker>(30, 30);

//         auto qos = rclcpp::SensorDataQoS();
//         sub_image_ = this->create_subscription<sensor_msgs::msg::Image>("/rgb_camera/image_raw", qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));
//         sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>("/scan/points", qos, std::bind(&YoloDetectorNode::scan_callback, this, std::placeholders::_1));
//         sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>("/odom", qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; });
//         sub_joints_ = this->create_subscription<sensor_msgs::msg::JointState>("/joint_states", qos, [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
//                 for(size_t i=0; i<msg->name.size(); i++) { if(msg->name[i] == "gimbal_pan_joint") current_pan_ = msg->position[i]; }
//             });

//         pub_leader_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
//         pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("/gimbal/pan_cmd", 10);
//         pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("/gimbal/tilt_cmd", 10);
//         pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("/perception/sh01_overlay", 10);
        
//         cv::namedWindow("TACTICAL_VIEW", cv::WINDOW_AUTOSIZE);
//     }

// private:
//     void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
//         pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
//         pcl::fromROSMsg(*msg, *cloud);
//         double min_d = 8.0; double ang = 0.0; bool found = false;
//         #pragma omp parallel
//         {
//             double l_min = 8.0; double l_ang = 0.0; bool l_found = false;
//             #pragma omp for nowait
//             for (size_t i = 0; i < cloud->points.size(); i += 15) {
//                 const auto& p = cloud->points[i];
//                 if (p.z < -0.2 || p.z > 0.6) continue;
//                 double d = std::hypot(p.x, p.y);
//                 if (d > 0.8 && d < l_min) { l_min = d; l_ang = std::atan2(p.y, p.x); l_found = true; }
//             }
//             #pragma omp critical
//             { if (l_min < min_d) { min_d = l_min; ang = l_ang; found = l_found; } }
//         }
//         closest_blob_dist_ = min_d; closest_blob_angle_ = ang; blob_detected_ = found;
//     }

//     void image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
//         cv_bridge::CvImagePtr cv_ptr;
//         try { cv_ptr = cv_bridge::toCvCopy(msg, "bgr8"); } catch (...) { return; }

//         auto results = engine_->run_inference(cv_ptr->image);
//         std::vector<byte_track::Object> objects;
//         for (const auto& res : results) {
//             if (engine_->class_names[res.class_id] == "person") {
//                 objects.emplace_back(byte_track::Rect<float>(res.box.x, res.box.y, res.box.width, res.box.height), res.class_id, res.confidence);
//             }
//         }
//         auto tracked_targets = tracker_->update(objects, cv::Mat::eye(2, 3, CV_64F));

//         // 1. TACTICAL ANALYSIS: Find closest person
//         int best_idx = -1; double min_dist_m = 1e9;
//         std::vector<double> current_dists;
//         for (size_t i = 0; i < tracked_targets.size(); ++i) {
//             double d = (550.0 * 1.7) / tracked_targets[i]->getRect().height();
//             current_dists.push_back(d);
//             if (d < min_dist_m) { min_dist_m = d; best_idx = i; }
//         }

//         skyhunter_msgs::msg::LeaderState state_msg;
//         state_msg.header.stamp = this->get_clock()->now();
//         std_msgs::msg::Float64 pan_cmd, tilt_cmd;
//         pan_cmd.data = 0.0; tilt_cmd.data = 0.0; // Default: Reset to straight

//         // 2. DRAWING LOOP (Ensures Multiple Detection visualization)
//         for (size_t i = 0; i < tracked_targets.size(); ++i) {
//             auto r = tracked_targets[i]->getRect();
//             cv::Rect box(r.x(), r.y(), r.width(), r.height());
//             std::string label = "ID:" + std::to_string(tracked_targets[i]->getTrackId()) + " " + std::to_string(current_dists[i]).substr(0,4) + "m";

//             if ((int)i == best_idx) {
//                 // PRIMARY TARGET (RED)
//                 current_state_ = COMBAT;
//                 cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 0, 255), 3);
//                 cv::drawMarker(cv_ptr->image, cv::Point(r.x()+r.width()/2, r.y()+r.height()/2), cv::Scalar(0, 0, 255), cv::MARKER_CROSS, 30, 2);
//                 cv::putText(cv_ptr->image, "LOCKED " + label, cv::Point(r.x(), r.y() - 10), 0, 0.6, cv::Scalar(0, 0, 255), 2);

//                 // GIMBAL PID
//                 pan_cmd.data = (((r.x() + r.width()/2.0) - 320.0) / 320.0) * -0.8;
//                 tilt_cmd.data = (((r.y() + r.height()/2.0) - 240.0) / 240.0) * 0.5;

//                 // DATA LINK
//                 double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
//                 state_msg.target_pos.x = latest_odom_.pose.pose.position.x + (current_dists[i] * std::cos(r_yaw + current_pan_));
//                 state_msg.target_pos.y = latest_odom_.pose.pose.position.y + (current_dists[i] * std::sin(r_yaw + current_pan_));
//                 state_msg.target_locked = true; state_msg.swarm_state = 3;
//             } else {
//                 // SITUATIONAL TARGETS (GREEN)
//                 cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1);
//                 cv::putText(cv_ptr->image, label, cv::Point(r.x(), r.y() - 5), 0, 0.4, cv::Scalar(0, 255, 0), 1);
//             }
//         }

//         // 3. BLIND SPOT / RADAR LOGIC
//         if (tracked_targets.empty()) {
//             if (blob_detected_ && closest_blob_dist_ < 5.0) {
//                 if (current_state_ != SCANNING) { cue_start_time_ = this->get_clock()->now(); current_state_ = SCANNING; }
                
//                 if ((this->get_clock()->now() - cue_start_time_).seconds() < 2.0) {
//                     pan_cmd.data = current_pan_ + closest_blob_angle_;
//                     cv::putText(cv_ptr->image, "RADAR CUEING...", cv::Point(20, 60), 0, 0.7, cv::Scalar(255, 0, 255), 2);
//                 } else { pan_cmd.data = 0.0; current_state_ = PATROL; }
//             } else { current_state_ = PATROL; pan_cmd.data = 0.0; }
//         }

//         // 4. EXECUTION
//         pub_pan_->publish(pan_cmd); pub_tilt_->publish(tilt_cmd);
//         cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(255, 255, 0), cv::MARKER_CROSS, 20, 1);
//         cv::imshow("TACTICAL_VIEW", cv_ptr->image);
//         cv::waitKey(1);
//         pub_leader_state_->publish(state_msg);
//         pub_overlay_->publish(*(cv_ptr->toImageMsg()));
//     }

//     TrackingState current_state_ = PATROL; rclcpp::Time cue_start_time_;
//     double closest_blob_dist_ = 10.0, closest_blob_angle_ = 0.0; bool blob_detected_ = false;
//     nav_msgs::msg::Odometry latest_odom_; double current_pan_ = 0.0;
//     std::unique_ptr<YoloEngine> engine_; std::unique_ptr<byte_track::BYTETracker> tracker_;
//     rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
//     rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
//     rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
//     rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_joints_;
//     rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr pub_leader_state_;
//     rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_pan_, pub_tilt_;
//     rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_overlay_;
// };

// int main(int argc, char** argv) {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<YoloDetectorNode>());
//     cv::destroyAllWindows();
//     rclcpp::shutdown();
//     return 0;
// }









#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/int8.hpp> // NEW
#include <cv_bridge/cv_bridge.h>
#include <skyhunter_msgs/msg/leader_state.hpp>
#include <skyhunter_msgs/msg/detection.hpp>
#include "skyhunter_perception/yolo_engine.hpp"
#include "ByteTrack/BYTETracker.h"
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <omp.h>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <skyhunter_msgs/msg/detection_array.hpp>

enum TrackingState { PATROL, SCANNING, COMBAT };

class YoloDetectorNode : public rclcpp::Node {
public:
    YoloDetectorNode() : Node("yolo_detector_node") {
        std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
        engine_ = std::make_unique<YoloEngine>(pkg_dir + "/models/yolov8m.onnx", true);
        tracker_ = std::make_unique<byte_track::BYTETracker>(30, 30);

        auto qos = rclcpp::SensorDataQoS();
        
        // Determine Topics based on Namespace
        std::string ns = this->get_namespace();
        bool is_follower = (ns.find("SH_") != std::string::npos);
        if (is_follower && last_swarm_msg_.swarm_state != 3) return;

        std::string img_topic, scan_topic, odom_topic, joint_topic;

        // FIXED LOGIC: Handle the global namespace case correctly
        if (ns == "/" || ns == "") {
            img_topic = "/rgb_camera/image_raw";
            scan_topic = "/scan/points";
            odom_topic = "/odom";
            joint_topic = "/joint_states";
        } else {
            // Remove leading slash if present to avoid //SH_02
            if (ns[0] == '/') ns = ns.substr(1);
            
            img_topic = "/" + ns + "/rgb_camera/image_raw";
            scan_topic = "/" + ns + "/scan/points";
            odom_topic = "/" + ns + "/odom_filtered"; // Followers use filtered odom
            joint_topic = "/" + ns + "/joint_states";
        }

        sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(img_topic, qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));
        sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(scan_topic, qos, std::bind(&YoloDetectorNode::scan_callback, this, std::placeholders::_1));
        sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(odom_topic, qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; });
        
        sub_joints_ = this->create_subscription<sensor_msgs::msg::JointState>(
            joint_topic, qos, [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
                for(size_t i=0; i<msg->name.size(); i++) { if(msg->name[i] == "gimbal_pan_joint") current_pan_ = msg->position[i]; }
            });

        // NEW: Listen to Role & Swarm State
        sub_role_ = this->create_subscription<std_msgs::msg::Int8>("local_role", 10, [this](const std_msgs::msg::Int8::SharedPtr msg) { current_role_ = msg->data; });
        sub_swarm_state_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>("/leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) { last_swarm_msg_ = *msg; });

        // pub_leader_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
        // pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
        // pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
        // pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("perception/overlay", 10);

        pub_leader_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
        
        // --- THIS IS THE LINE YOU WERE LOOKING FOR (Relative Name) ---
        pub_detections_ = this->create_publisher<skyhunter_msgs::msg::DetectionArray>("swarm/detections", 10);
        // -----------------------------------------------------------

        pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
        pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
        pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("perception/overlay", 10);

        
        
        if (ns == "/") cv::namedWindow("TACTICAL_VIEW", cv::WINDOW_AUTOSIZE); // Only show window for Leader
    }

private:
    void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
        // LEADER ONLY: Run Radar
        if (current_role_ != 2 && this->get_namespace() != "/") return; 

        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
        pcl::fromROSMsg(*msg, *cloud);
        double min_d = 8.0; double ang = 0.0; bool found = false;
        #pragma omp parallel
        {
            double l_min = 8.0; double l_ang = 0.0; bool l_found = false;
            #pragma omp for nowait
            for (size_t i = 0; i < cloud->points.size(); i += 15) {
                const auto& p = cloud->points[i];
                if (p.z < -0.2 || p.z > 0.6) continue;
                double d = std::hypot(p.x, p.y);
                if (d > 0.8 && d < l_min) { l_min = d; l_ang = std::atan2(p.y, p.x); l_found = true; }
            }
            #pragma omp critical
            { if (l_min < min_d) { min_d = l_min; ang = l_ang; found = l_found; } }
        }
        closest_blob_dist_ = min_d; closest_blob_angle_ = ang; blob_detected_ = found;
    }

    void image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
        // OPTIMIZATION: If I am a Follower AND Swarm is NOT in Combat, Sleep.
        std::string ns = this->get_namespace();
        bool is_follower = (ns.find("SH_") != std::string::npos);
        bool is_leader = !is_follower;

        if (is_follower && last_swarm_msg_.swarm_state != 3) return;
        
        if (is_follower && last_swarm_msg_.swarm_state == 3) {
             RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                                  "Follower %s ACTIVE - COMBAT MODE", ns.c_str());
        }

        cv_bridge::CvImagePtr cv_ptr;
        try { cv_ptr = cv_bridge::toCvCopy(msg, "bgr8"); } catch (...) { return; }

        auto results = engine_->run_inference(cv_ptr->image);
        std::vector<byte_track::Object> objects;
        for (const auto& res : results) {
            if (engine_->class_names[res.class_id] == "person") {
                objects.emplace_back(byte_track::Rect<float>(res.box.x, res.box.y, res.box.width, res.box.height), res.class_id, res.confidence);
            }
        }
        auto tracked_targets = tracker_->update(objects, cv::Mat::eye(2, 3, CV_64F));

        int best_idx = -1; double min_dist_m = 1e9;
        std::vector<double> current_dists;
        for (size_t i = 0; i < tracked_targets.size(); ++i) {
            double d = (550.0 * 1.7) / tracked_targets[i]->getRect().height();
            current_dists.push_back(d);
            if (d < min_dist_m) { min_dist_m = d; best_idx = i; }
        }

        // --- PREPARE MESSAGES ---
        skyhunter_msgs::msg::LeaderState state_msg;
        state_msg.header.stamp = this->get_clock()->now();
        
        skyhunter_msgs::msg::DetectionArray det_msg;
        det_msg.header = msg->header;

        std_msgs::msg::Float64 pan_cmd, tilt_cmd;
        pan_cmd.data = 0.0; tilt_cmd.data = 0.0; 

        // --- DRAWING & PUBLISHING LOOP ---
        for (size_t i = 0; i < tracked_targets.size(); ++i) {
            auto r = tracked_targets[i]->getRect();
            cv::Rect box(r.x(), r.y(), r.width(), r.height());
            
            // Format the string exactly like you wanted
            std::string label = "ID:" + std::to_string(tracked_targets[i]->getTrackId()) + 
                                " " + std::to_string(current_dists[i]).substr(0,4) + "m";

            // Add to Detection Message Array
            skyhunter_msgs::msg::Detection det;
            det.class_id = tracked_targets[i]->getTrackId(); // Use track ID instead of class 0
            det.label = "person";
            det.confidence = current_dists[i]; // Store distance in confidence for now so followers know how far it is
            det.x = box.x; det.y = box.y; det.w = box.width; det.h = box.height;
            det_msg.detections.push_back(det);

            if ((int)i == best_idx) {
                // PRIMARY TARGET (RED)
                cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 0, 255), 3);
                cv::drawMarker(cv_ptr->image, cv::Point(r.x()+r.width()/2, r.y()+r.height()/2), cv::Scalar(0, 0, 255), cv::MARKER_CROSS, 30, 2);
                cv::putText(cv_ptr->image, "LOCKED " + label, cv::Point(r.x(), r.y() - 10), 0, 0.6, cv::Scalar(0, 0, 255), 2);
                
                // GIMBAL PID
                pan_cmd.data = (((r.x() + r.width()/2.0) - 320.0) / 320.0) * -0.8;
                tilt_cmd.data = (((r.y() + r.height()/2.0) - 240.0) / 240.0) * 0.5;
                pub_pan_->publish(pan_cmd); 
                pub_tilt_->publish(tilt_cmd);

                // LEADER BROADCAST ONLY
                if (is_leader) {
                    double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
                    state_msg.target_pos.x = latest_odom_.pose.pose.position.x + (current_dists[i] * std::cos(r_yaw + current_pan_));
                    state_msg.target_pos.y = latest_odom_.pose.pose.position.y + (current_dists[i] * std::sin(r_yaw + current_pan_));
                    state_msg.target_locked = true; 
                    state_msg.swarm_state = (current_dists[i] < 10.0) ? 3 : 0;
                }
            } else {
                // SITUATIONAL TARGETS (GREEN)
                cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1);
                cv::putText(cv_ptr->image, label, cv::Point(r.x(), r.y() - 5), 0, 0.4, cv::Scalar(0, 255, 0), 1);
            }
        }

        // --- PUBLISH DETECTION ARRAY (Everyone) ---
        pub_detections_->publish(det_msg);

        // Radar Logic (Leader Only)
        if (is_leader && tracked_targets.empty() && blob_detected_ && closest_blob_dist_ < 5.0) {
             std_msgs::msg::Float64 p; p.data = current_pan_ + closest_blob_angle_;
             pub_pan_->publish(p);
        }

        // Leader UI
        if (is_leader) {
            pub_leader_state_->publish(state_msg);
            cv::imshow("TACTICAL_VIEW", cv_ptr->image);
            cv::waitKey(1);
        }
        
        // Overlay for RViz (Everyone)
        pub_overlay_->publish(*(cv_ptr->toImageMsg()));
    }

    int current_role_ = 0; // 0=Follower, 2=Leader
    skyhunter_msgs::msg::LeaderState last_swarm_msg_;
    TrackingState current_state_ = PATROL; rclcpp::Time cue_start_time_;
    double closest_blob_dist_ = 10.0, closest_blob_angle_ = 0.0; bool blob_detected_ = false;
    nav_msgs::msg::Odometry latest_odom_; double current_pan_ = 0.0;
    std::unique_ptr<YoloEngine> engine_; std::unique_ptr<byte_track::BYTETracker> tracker_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_joints_;
    rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_role_;
    rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_swarm_state_;
    rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr pub_leader_state_;
    
    rclcpp::Publisher<skyhunter_msgs::msg::DetectionArray>::SharedPtr pub_detections_; // <--- ADD THIS
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_pan_, pub_tilt_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_overlay_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<YoloDetectorNode>());
    cv::destroyAllWindows();
    rclcpp::shutdown();
    return 0;
}