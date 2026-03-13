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








// 03_05 WORKABLE BUT NOT FOLLOWERS DETECTION
// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/image.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <sensor_msgs/msg/joint_state.hpp>
// #include <nav_msgs/msg/odometry.hpp>
// #include <std_msgs/msg/float64.hpp>
// #include <std_msgs/msg/int8.hpp> // NEW
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
// #include <skyhunter_msgs/msg/detection_array.hpp>

// enum TrackingState { PATROL, SCANNING, COMBAT };

// class YoloDetectorNode : public rclcpp::Node {
// public:
//     YoloDetectorNode() : Node("yolo_detector_node") {
//         std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
//         engine_ = std::make_unique<YoloEngine>(pkg_dir + "/models/yolov8m.onnx", true);
//         tracker_ = std::make_unique<byte_track::BYTETracker>(30, 30);

//         auto qos = rclcpp::SensorDataQoS();
        
//         // Determine Topics based on Namespace
//         std::string ns = this->get_namespace();
//         bool is_follower = (ns.find("SH_") != std::string::npos);
//         if (is_follower && last_swarm_msg_.swarm_state != 3) return;

//         std::string img_topic, scan_topic, odom_topic, joint_topic;

//         // FIXED LOGIC: Handle the global namespace case correctly
//         if (ns == "/" || ns == "") {
//             img_topic = "/rgb_camera/image_raw";
//             scan_topic = "/scan/points";
//             odom_topic = "/odom";
//             joint_topic = "/joint_states";
//         } else {
//             // Remove leading slash if present to avoid //SH_02
//             if (ns[0] == '/') ns = ns.substr(1);
            
//             img_topic = "/" + ns + "/rgb_camera/image_raw";
//             scan_topic = "/" + ns + "/scan/points";
//             odom_topic = "/" + ns + "/odom_filtered"; // Followers use filtered odom
//             joint_topic = "/" + ns + "/joint_states";
//         }

//         sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(img_topic, qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));
//         sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(scan_topic, qos, std::bind(&YoloDetectorNode::scan_callback, this, std::placeholders::_1));
//         sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(odom_topic, qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; });
        
//         sub_joints_ = this->create_subscription<sensor_msgs::msg::JointState>(
//             joint_topic, qos, [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
//                 for(size_t i=0; i<msg->name.size(); i++) { if(msg->name[i] == "gimbal_pan_joint") current_pan_ = msg->position[i]; }
//             });

//         // NEW: Listen to Role & Swarm State
//         sub_role_ = this->create_subscription<std_msgs::msg::Int8>("local_role", 10, [this](const std_msgs::msg::Int8::SharedPtr msg) { current_role_ = msg->data; });
//         sub_swarm_state_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>("/leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) { last_swarm_msg_ = *msg; });


//         pub_leader_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
//         pub_detections_ = this->create_publisher<skyhunter_msgs::msg::DetectionArray>("swarm/detections", 10);
//         pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
//         pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
//         pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("perception/overlay", 10);

        
        
//         if (ns == "/") cv::namedWindow("TACTICAL_VIEW", cv::WINDOW_AUTOSIZE); // Only show window for Leader
//     }

// private:
//     void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
//         // LEADER ONLY: Run Radar
//         if (current_role_ != 2 && this->get_namespace() != "/") return; 

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
//         // OPTIMIZATION: If I am a Follower AND Swarm is NOT in Combat, Sleep.
//         std::string ns = this->get_namespace();
//         bool is_follower = (ns.find("SH_") != std::string::npos);
//         bool is_leader = !is_follower;

//         if (is_follower && last_swarm_msg_.swarm_state != 3) return;
        
//         if (is_follower && last_swarm_msg_.swarm_state == 3) {
//              RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
//                                   "Follower %s ACTIVE - COMBAT MODE", ns.c_str());
//         }

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

//         int best_idx = -1; double min_dist_m = 1e9;
//         std::vector<double> current_dists;
//         for (size_t i = 0; i < tracked_targets.size(); ++i) {
//             double d = (550.0 * 1.7) / tracked_targets[i]->getRect().height();
//             current_dists.push_back(d);
//             if (d < min_dist_m) { min_dist_m = d; best_idx = i; }
//         }

//         // --- PREPARE MESSAGES ---
//         skyhunter_msgs::msg::LeaderState state_msg;
//         state_msg.header.stamp = this->get_clock()->now();
        
//         skyhunter_msgs::msg::DetectionArray det_msg;
//         det_msg.header = msg->header;

//         // --- FIX IS HERE: Default to looking Straight ---
//         std_msgs::msg::Float64 pan_cmd, tilt_cmd;
//         pan_cmd.data = 0.0; tilt_cmd.data = 0.0; 

//         bool ai_sees_target = !tracked_targets.empty();

//         if (ai_sees_target) {
//             // --- DRAWING & PUBLISHING LOOP ---
//             for (size_t i = 0; i < tracked_targets.size(); ++i) {
//                 auto r = tracked_targets[i]->getRect();
//                 cv::Rect box(r.x(), r.y(), r.width(), r.height());
                
//                 std::string label = "ID:" + std::to_string(tracked_targets[i]->getTrackId()) + 
//                                     " " + std::to_string(current_dists[i]).substr(0,4) + "m";

//                 // Add to Detection Message Array
//                 skyhunter_msgs::msg::Detection det;
//                 det.class_id = tracked_targets[i]->getTrackId(); 
//                 det.label = "person";
//                 det.confidence = current_dists[i]; 
//                 det.x = box.x; det.y = box.y; det.w = box.width; det.h = box.height;
//                 det_msg.detections.push_back(det);

//                 if ((int)i == best_idx) {
//                     // PRIMARY TARGET (RED)
//                     cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 0, 255), 3);
//                     cv::drawMarker(cv_ptr->image, cv::Point(r.x()+r.width()/2, r.y()+r.height()/2), cv::Scalar(0, 0, 255), cv::MARKER_CROSS, 30, 2);
//                     cv::putText(cv_ptr->image, "LOCKED " + label, cv::Point(r.x(), r.y() - 10), 0, 0.6, cv::Scalar(0, 0, 255), 2);
                    
//                     // GIMBAL PID
//                     pan_cmd.data = (((r.x() + r.width()/2.0) - 320.0) / 320.0) * -0.8;
//                     tilt_cmd.data = (((r.y() + r.height()/2.0) - 240.0) / 240.0) * 0.5;

//                     // LEADER BROADCAST ONLY
//                     if (is_leader) {
//                         double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
//                         state_msg.target_pos.x = latest_odom_.pose.pose.position.x + (current_dists[i] * std::cos(r_yaw + current_pan_));
//                         state_msg.target_pos.y = latest_odom_.pose.pose.position.y + (current_dists[i] * std::sin(r_yaw + current_pan_));
//                         state_msg.target_locked = true; 
//                         state_msg.swarm_state = (current_dists[i] < 10.0) ? 3 : 0;
//                     }
//                 } else {
//                     // SITUATIONAL TARGETS (GREEN)
//                     cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1);
//                     cv::putText(cv_ptr->image, label, cv::Point(r.x(), r.y() - 5), 0, 0.4, cv::Scalar(0, 255, 0), 1);
//                 }
//             }
//         }
//         else if (is_leader && blob_detected_ && closest_blob_dist_ < 5.0) {
//             // Radar Logic (Leader Only) - If NO targets seen, but Radar sees something
//              pan_cmd.data = current_pan_ + closest_blob_angle_;
//         }
//         // IF NEITHER IS TRUE (No targets, no blobs): pan_cmd remains 0.0 (Look straight)

//         // --- EXECUTE COMMANDS ---
//         pub_pan_->publish(pan_cmd); 
//         pub_tilt_->publish(tilt_cmd);
//         pub_detections_->publish(det_msg);

//         if (is_leader) {
//             pub_leader_state_->publish(state_msg);
//             cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(255, 255, 0), cv::MARKER_CROSS, 20, 1);
//             cv::imshow("TACTICAL_VIEW", cv_ptr->image);
//             cv::waitKey(1);
//         }
        
//         // Overlay for RViz (Everyone)
//         pub_overlay_->publish(*(cv_ptr->toImageMsg()));
//     }

//     int current_role_ = 0; // 0=Follower, 2=Leader
//     skyhunter_msgs::msg::LeaderState last_swarm_msg_;
//     TrackingState current_state_ = PATROL; rclcpp::Time cue_start_time_;
//     double closest_blob_dist_ = 10.0, closest_blob_angle_ = 0.0; bool blob_detected_ = false;
//     nav_msgs::msg::Odometry latest_odom_; double current_pan_ = 0.0;
//     std::unique_ptr<YoloEngine> engine_; std::unique_ptr<byte_track::BYTETracker> tracker_;
//     rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
//     rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
//     rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
//     rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_joints_;
//     rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_role_;
//     rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_swarm_state_;
//     rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr pub_leader_state_;
    
//     rclcpp::Publisher<skyhunter_msgs::msg::DetectionArray>::SharedPtr pub_detections_; // <--- ADD THIS
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



// // 03_07 full workable multiple object like person,frisbee detection and pid tracking
// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/image.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <sensor_msgs/msg/joint_state.hpp>
// #include <nav_msgs/msg/odometry.hpp>
// #include <std_msgs/msg/float64.hpp>
// #include <std_msgs/msg/int8.hpp>
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
// #include <skyhunter_msgs/msg/detection_array.hpp>

// class YoloDetectorNode : public rclcpp::Node {
// public:
//     YoloDetectorNode() : Node("yolo_detector_node") {
//         std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
//         engine_ = std::make_unique<YoloEngine>(pkg_dir + "/models/yolov8m.onnx", true);
//         tracker_ = std::make_unique<byte_track::BYTETracker>(30, 30);

//         auto qos = rclcpp::SensorDataQoS();
        
//         // --- CRITICAL FIX: NATIVE NAMESPACE DISCOVERY ---
//         std::string ns = std::string(this->get_namespace());
//         if (ns.length() > 0 && ns[0] == '/') ns = ns.substr(1);
//         is_leader_ = (ns == ""); // Empty namespace means Global Leader

//         // Auto-Generate Topics based on real namespace
//         std::string img_topic = is_leader_ ? "/rgb_camera/image_raw" : "/" + ns + "/rgb_camera/image_raw";
//         std::string scan_topic = is_leader_ ? "/scan/points" : "/" + ns + "/scan/points";
//         std::string odom_topic = is_leader_ ? "/odom" : "/" + ns + "/odom_filtered"; 
//         std::string joint_topic = is_leader_ ? "/joint_states" : "/" + ns + "/joint_states";

//         sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(img_topic, qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));
        
//         if (is_leader_) {
//             sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(scan_topic, qos, std::bind(&YoloDetectorNode::scan_callback, this, std::placeholders::_1));
//             pub_leader_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/leader_state", 10);
//             cv::namedWindow("LEADER_TACTICAL_VIEW", cv::WINDOW_AUTOSIZE);
//             RCLCPP_INFO(this->get_logger(), "[SH_01] LEADER PREDATOR MODE ONLINE.");
//         } else {
//             RCLCPP_INFO(this->get_logger(), "[%s] FOLLOWER SNIPER MODE ONLINE.", ns.c_str());
//         }

//         sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(odom_topic, qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; });
//         sub_joints_ = this->create_subscription<sensor_msgs::msg::JointState>(
//             joint_topic, qos, [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
//                 for(size_t i=0; i<msg->name.size(); i++) { if(msg->name[i] == "gimbal_pan_joint") current_pan_ = msg->position[i]; }
//             });

//         // Listen to Global Swarm State
//         sub_swarm_state_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>("/leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) { last_swarm_msg_ = *msg; });

//         // Relative Publishers (ROS automatically prepends /SH_02/)
//         pub_detections_ = this->create_publisher<skyhunter_msgs::msg::DetectionArray>("swarm/detections", 10);
//         pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
//         pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
//         pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("perception/overlay", 10);
//     }

// private:
//     void scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
//         if (!is_leader_) return; // Safety check

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
//             objects.emplace_back(byte_track::Rect<float>(res.box.x, res.box.y, res.box.width, res.box.height), res.class_id, res.confidence);
            
//         }
//         auto tracked_targets = tracker_->update(objects, cv::Mat::eye(2, 3, CV_64F));

//         int best_idx = -1; double min_dist_m = 1e9;
//         std::vector<double> current_dists;
//         for (size_t i = 0; i < tracked_targets.size(); ++i) {
//             double d = (550.0 * 1.7) / tracked_targets[i]->getRect().height();
//             current_dists.push_back(d);
//             if (d < min_dist_m) { min_dist_m = d; best_idx = i; }
//         }

//         skyhunter_msgs::msg::DetectionArray det_msg;
//         det_msg.header = msg->header;

//         std_msgs::msg::Float64 pan_cmd, tilt_cmd;
//         pan_cmd.data = 0.0; tilt_cmd.data = 0.0; 

//         bool ai_sees_target = !tracked_targets.empty();

//         if (ai_sees_target) {
//             // ==========================================
//             // LOCAL LOCK (Works for both Leader & Follower)
//             // ==========================================
//             for (size_t i = 0; i < tracked_targets.size(); ++i) {
//                 auto r = tracked_targets[i]->getRect();
//                 cv::Rect box(r.x(), r.y(), r.width(), r.height());

//                 if ((int)i == best_idx) {
//                     cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 0, 255), 3);
//                     cv::drawMarker(cv_ptr->image, cv::Point(r.x()+r.width()/2, r.y()+r.height()/2), cv::Scalar(0, 0, 255), cv::MARKER_CROSS, 30, 2);
//                     cv::putText(cv_ptr->image, "LOCKED ID:" + std::to_string(tracked_targets[i]->getTrackId()), cv::Point(r.x(), r.y()-10), 0, 0.6, cv::Scalar(0,0,255), 2);
                    
//                     // LOCAL PID (Follower uses its own eyes to stay locked)
//                     pan_cmd.data = (((r.x() + r.width()/2.0) - 320.0) / 320.0) * -0.8;
//                     tilt_cmd.data = (((r.y() + r.height()/2.0) - 240.0) / 240.0) * 0.5;

//                     // LEADER ONLY: Broadcast Target coordinates
//                     if (is_leader_) {
//                         skyhunter_msgs::msg::LeaderState state_msg;
//                         state_msg.header.stamp = this->get_clock()->now();
//                         double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
//                         state_msg.target_pos.x = latest_odom_.pose.pose.position.x + (current_dists[i] * std::cos(r_yaw + current_pan_));
//                         state_msg.target_pos.y = latest_odom_.pose.pose.position.y + (current_dists[i] * std::sin(r_yaw + current_pan_));
//                         state_msg.target_locked = true; 
//                         state_msg.swarm_state = (current_dists[i] < 10.0) ? 3 : 0;
//                         pub_leader_state_->publish(state_msg);
//                     }
//                 } else {
//                     cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1);
//                 }
//             }
//         }
//         else if (is_leader_ && blob_detected_ && closest_blob_dist_ < 5.0) {
//             // LEADER RADAR AMBUSH FIX
//             pan_cmd.data = current_pan_ + closest_blob_angle_;
//         }
//         else if (!is_leader_ && last_swarm_msg_.swarm_state == 3) {
//             // ==========================================
//             // FOLLOWER CUEING (Map Triangulation)
//             // ==========================================
//             // YOLO is awake, but hasn't seen the person yet. Turn head to Leader's Map coordinates!
//             double tx = last_swarm_msg_.target_pos.x;
//             double ty = last_swarm_msg_.target_pos.y;
//             double my_x = latest_odom_.pose.pose.position.x;
//             double my_y = latest_odom_.pose.pose.position.y;
//             double my_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
            
//             double global_angle = std::atan2(ty - my_y, tx - my_x);
//             double p_angle = global_angle - my_yaw;
//             while (p_angle > M_PI) p_angle -= 2 * M_PI;
//             while (p_angle < -M_PI) p_angle += 2 * M_PI;
            
//             pan_cmd.data = p_angle;
//             cv::putText(cv_ptr->image, "CUEING TO LEADER COORDS", cv::Point(20, 60), 0, 0.7, cv::Scalar(0, 165, 255), 2);
//         }

//         pub_pan_->publish(pan_cmd); 
//         pub_tilt_->publish(tilt_cmd);

//         cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(255, 255, 0), cv::MARKER_CROSS, 20, 1);
//         pub_overlay_->publish(*(cv_ptr->toImageMsg()));

//         if (is_leader_) {
//             cv::imshow("LEADER_TACTICAL_VIEW", cv_ptr->image);
//             cv::waitKey(1);
//         }
//     }

//     bool is_leader_ = true;
//     double closest_blob_dist_ = 10.0, closest_blob_angle_ = 0.0; bool blob_detected_ = false;
//     nav_msgs::msg::Odometry latest_odom_; double current_pan_ = 0.0;
//     skyhunter_msgs::msg::LeaderState last_swarm_msg_;
    
//     std::unique_ptr<YoloEngine> engine_; std::unique_ptr<byte_track::BYTETracker> tracker_;
//     rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
//     rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_scan_;
//     rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
//     rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_joints_;
//     rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_swarm_state_;
    
//     rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr pub_leader_state_;
//     rclcpp::Publisher<skyhunter_msgs::msg::DetectionArray>::SharedPtr pub_detections_;
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









// #include "skyhunter_perception/yolo_detector_node.hpp"
// #include <cv_bridge/cv_bridge.h>
// #include <pcl_conversions/pcl_conversions.h>
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
// #include <ament_index_cpp/get_package_share_directory.hpp>
// #include <opencv2/highgui.hpp>
// #include <omp.h>

// // --- 1. RESTORE THE ENUM ---
// enum TrackingState { PATROL, SCANNING, COMBAT };

// YoloDetectorNode::YoloDetectorNode() : Node("yolo_detector_node") {
//     // 1. Initialize Engines
//     std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
//     std::string model_path = pkg_dir + "/models/yolov8m.onnx";
//     engine_ = std::make_unique<YoloEngine>(model_path, true);
//     tracker_ = std::make_unique<byte_track::BYTETracker>(30, 30);

//     auto qos = rclcpp::SensorDataQoS();
    
//     // 2. Determine Namespace Role
//     this->declare_parameter<std::string>("robot_ns", "");
//     std::string ns = this->get_parameter("robot_ns").as_string();
//     if (ns.length() > 0 && ns[0] == '/') ns = ns.substr(1);
//     is_leader_ = (ns == ""); 

//     // 3. Topic Selection
//     std::string img_topic = is_leader_ ? "/rgb_camera/image_raw" : "/" + ns + "/rgb_camera/image_raw";
//     std::string scan_topic = is_leader_ ? "/scan/points" : "/" + ns + "/scan/points";
//     std::string odom_topic = is_leader_ ? "/odom" : "/" + ns + "/odom_filtered"; 
//     std::string joint_topic = is_leader_ ? "/joint_states" : "/" + ns + "/joint_states";

//     // 4. Subscriptions
//     sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(img_topic, qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));
//     sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(odom_topic, qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; });
//     sub_joints_ = this->create_subscription<sensor_msgs::msg::JointState>(joint_topic, qos, [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
//             for(size_t i=0; i<msg->name.size(); i++) { if(msg->name[i] == "gimbal_pan_joint") current_pan_ = msg->position[i]; }
//         });
//     sub_swarm_state_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>("/leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) { last_swarm_msg_ = *msg; });

//     // 5. Leader Logic
//     if (is_leader_) {
//         sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(scan_topic, qos, std::bind(&YoloDetectorNode::scan_callback, this, std::placeholders::_1));
//         pub_leader_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("leader_state", 10);
//         cv::namedWindow("LEADER_TACTICAL_VIEW", cv::WINDOW_AUTOSIZE);
//         RCLCPP_INFO(this->get_logger(), "[SH_01] LEADER PREDATOR MODE ONLINE.");
//     } else {
//         RCLCPP_INFO(this->get_logger(), "[%s] FOLLOWER SNIPER MODE ONLINE.", ns.c_str());
//     }

//     // 6. Publishers
//     pub_detections_ = this->create_publisher<skyhunter_msgs::msg::DetectionArray>("swarm/detections", 10);
//     pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
//     pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
//     pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("perception/overlay", 10);
// }

// YoloDetectorNode::~YoloDetectorNode() {
//     if(is_leader_) cv::destroyWindow("LEADER_TACTICAL_VIEW");
// }

// void YoloDetectorNode::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
//     pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
//     pcl::fromROSMsg(*msg, *cloud);
//     double min_d = 8.0; double ang = 0.0; bool found = false;
//     #pragma omp parallel
//     {
//         double l_min = 8.0; double l_ang = 0.0; bool l_found = false;
//         #pragma omp for nowait
//         for (size_t i = 0; i < cloud->points.size(); i += 15) {
//             const auto& p = cloud->points[i];
//             if (p.z < -0.2 || p.z > 0.6) continue;
//             double d = std::hypot(p.x, p.y);
//             if (d > 0.8 && d < l_min) { l_min = d; l_ang = std::atan2(p.y, p.x); l_found = true; }
//         }
//         #pragma omp critical
//         { if (l_min < min_d) { min_d = l_min; ang = l_ang; found = l_found; } }
//     }
//     closest_blob_dist_ = min_d; closest_blob_angle_ = ang; blob_detected_ = found;
// }

// void YoloDetectorNode::image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
//     cv_bridge::CvImagePtr cv_ptr;
//     try { cv_ptr = cv_bridge::toCvCopy(msg, "bgr8"); } catch (...) { return; }

//     auto results = engine_->run_inference(cv_ptr->image);
//     std::vector<byte_track::Object> objects;
//     for (const auto& res : results) {
//          if (engine_->class_names[res.class_id] == "person") {
//              objects.emplace_back(byte_track::Rect<float>(res.box.x, res.box.y, res.box.width, res.box.height), res.class_id, res.confidence);
//          }
//          else if (res.class_id == 74) { 
//             objects.emplace_back(byte_track::Rect<float>(res.box.x, res.box.y, res.box.width, res.box.height), res.class_id, res.confidence);
//          }
//     }
//     auto tracked_targets = tracker_->update(objects, cv::Mat::eye(2, 3, CV_64F));

//     int best_idx = -1; double min_dist_m = 1e9;
//     std::vector<double> current_dists;
//     for (size_t i = 0; i < tracked_targets.size(); ++i) {
//         double d = (550.0 * 1.7) / tracked_targets[i]->getRect().height();
//         current_dists.push_back(d);
//         if (d < min_dist_m) { min_dist_m = d; best_idx = i; }
//     }

//     skyhunter_msgs::msg::DetectionArray det_msg;
//     det_msg.header = msg->header;
//     std_msgs::msg::Float64 pan_cmd, tilt_cmd;
//     pan_cmd.data = 0.0; tilt_cmd.data = 0.0; 
    
//     // --- 2. STATE MACHINE LOGIC ---
//     // Default to PATROL (Look straight) unless something interesting happens
//     TrackingState current_state = PATROL; 

//     bool ai_sees_target = !tracked_targets.empty();

//     if (ai_sees_target) {
//         // --- 3. COMBAT STATE (Target Lock + Lead Point) ---
//         current_state = COMBAT;
        
//         for (size_t i = 0; i < tracked_targets.size(); ++i) {
//             auto r = tracked_targets[i]->getRect();
//             cv::Rect box(r.x(), r.y(), r.width(), r.height());
//             std::string label = "ID:" + std::to_string(tracked_targets[i]->getTrackId()) + " " + std::to_string(current_dists[i]).substr(0,4) + "m";
            
//             // Fill Detection Msg
//             skyhunter_msgs::msg::Detection det;
//             det.class_id = tracked_targets[i]->getTrackId(); det.label = "person"; det.confidence = current_dists[i];
//             det.x = box.x; det.y = box.y; det.w = box.width; det.h = box.height;
//             det_msg.detections.push_back(det);

//             if ((int)i == best_idx) {
//                 // ** LEAD POINT LOGIC **
//                 std::vector<float> vel = tracked_targets[i]->getVelocity();
//                 float vx = vel[0]; float vy = vel[1];
//                 float lead_time = 0.5f; 
//                 float pred_cx = (r.x() + r.width()/2.0f) + (vx * lead_time);
//                 float pred_cy = (r.y() + r.height()/2.0f) + (vy * lead_time);

//                 // DRAW VISUALS
//                 cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 0, 255), 3); 
//                 cv::circle(cv_ptr->image, cv::Point(pred_cx, pred_cy), 8, cv::Scalar(0, 255, 255), -1); // Yellow Dot
//                 cv::putText(cv_ptr->image, "LEAD", cv::Point(pred_cx+10, pred_cy), 0, 0.5, cv::Scalar(0, 255, 255), 1);
//                 cv::putText(cv_ptr->image, "LOCKED " + label, cv::Point(r.x(), r.y() - 10), 0, 0.6, cv::Scalar(0, 0, 255), 2);
                
//                 // AIM GIMBAL AT LEAD POINT (Not current point)
//                 pan_cmd.data = ((pred_cx - 320.0) / 320.0) * -0.8;
//                 tilt_cmd.data = ((pred_cy - 240.0) / 240.0) * 0.5;

//                 // LEADER BROADCAST
//                 if (is_leader_) {
//                     skyhunter_msgs::msg::LeaderState state_msg;
//                     state_msg.header.stamp = this->get_clock()->now();
//                     double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
                    
//                     // Project Lead Point to Map
//                     // Note: Calculating angle based on Lead Point Pixel, but distance on current box size
//                     double lead_angle_offset = std::atan2(320.0 - pred_cx, 550.0); // Simple pinhole model approx
//                     double total_angle = r_yaw + current_pan_ - lead_angle_offset;

//                     state_msg.target_pos.x = latest_odom_.pose.pose.position.x + (current_dists[i] * std::cos(total_angle));
//                     state_msg.target_pos.y = latest_odom_.pose.pose.position.y + (current_dists[i] * std::sin(total_angle));
//                     state_msg.target_locked = true; 
//                     state_msg.swarm_state = (current_dists[i] < 10.0) ? 3 : 0;
//                     pub_leader_state_->publish(state_msg);
//                 }
//             } else {
//                 // Secondary Targets
//                 cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1);
//                 cv::putText(cv_ptr->image, label, cv::Point(r.x(), r.y() - 5), 0, 0.4, cv::Scalar(0, 255, 0), 1);
//             }
//         }
//     }
//     // --- 4. SCANNING STATE (Radar Cueing) ---
//     else if (is_leader_ && blob_detected_ && closest_blob_dist_ < 5.0) {
//         // We see a blob but no person yet.
//         // Check timer logic here if you want the 2-second timeout back.
//         // For simplicity: Snap to it.
//         current_state = SCANNING;
//         pan_cmd.data = current_pan_ + closest_blob_angle_;
//         cv::putText(cv_ptr->image, "RADAR CUEING...", cv::Point(20, 60), 0, 0.7, cv::Scalar(255, 0, 255), 2);
//     }
//     // --- 5. FOLLOWER SLAVE MODE ---
//     else if (!is_leader_ && last_swarm_msg_.swarm_state == 3) {
//         current_state = COMBAT;
//         double tx = last_swarm_msg_.target_pos.x;
//         double ty = last_swarm_msg_.target_pos.y;
//         double my_x = latest_odom_.pose.pose.position.x;
//         double my_y = latest_odom_.pose.pose.position.y;
//         double my_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
        
//         double global_angle = std::atan2(ty - my_y, tx - my_x);
//         double p_angle = global_angle - my_yaw;
//         while (p_angle > M_PI) p_angle -= 2 * M_PI;
//         while (p_angle < -M_PI) p_angle += 2 * M_PI;
//         pan_cmd.data = p_angle;
//         cv::putText(cv_ptr->image, "SLAVED TO LEADER", cv::Point(20, 60), 0, 0.7, cv::Scalar(0, 165, 255), 2);
//     }
//     // ELSE: PATROL (Look Straight) -> pan_cmd is already 0.0

//     // Draw Status
//     std::string status_txt = (current_state == COMBAT) ? "COMBAT" : (current_state == SCANNING ? "SCANNING" : "PATROL");
//     cv::putText(cv_ptr->image, "MODE: " + status_txt, cv::Point(20, 30), 0, 0.6, cv::Scalar(255, 255, 0), 2);

//     pub_pan_->publish(pan_cmd); 
//     pub_tilt_->publish(tilt_cmd);
//     pub_detections_->publish(det_msg);

//     cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(255, 255, 0), cv::MARKER_CROSS, 20, 1);
//     pub_overlay_->publish(*(cv_ptr->toImageMsg()));

//     if (is_leader_) {
//         cv::imshow("LEADER_TACTICAL_VIEW", cv_ptr->image);
//         cv::waitKey(1);
//     }
// }



#include "skyhunter_perception/yolo_detector_node.hpp"
#include <cv_bridge/cv_bridge.h>
#include <pcl_conversions/pcl_conversions.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <opencv2/highgui.hpp>
#include <omp.h>
#include <pcl/filters/voxel_grid.h>

YoloDetectorNode::YoloDetectorNode() : Node("yolo_detector_node") {
    std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
    std::string model_path = pkg_dir + "/models/yolov8m.onnx";
    engine_ = std::make_unique<YoloEngine>(model_path, true);
    tracker_ = std::make_unique<byte_track::BYTETracker>(30, 30);

    auto qos = rclcpp::SensorDataQoS();
    
    this->declare_parameter<std::string>("robot_ns", "");
    my_ns_ = this->get_parameter("robot_ns").as_string();
    if (my_ns_.length() > 0 && my_ns_[0] == '/') my_ns_ = my_ns_.substr(1);
    is_leader_ = (my_ns_ == ""); 

    std::string img_topic = is_leader_ ? "/rgb_camera/image_raw" : "/" + my_ns_ + "/rgb_camera/image_raw";
    std::string scan_topic = is_leader_ ? "/scan/points" : "/" + my_ns_ + "/scan/points";
    std::string odom_topic = is_leader_ ? "/odom" : "/" + my_ns_ + "/odom_filtered"; 
    std::string joint_topic = is_leader_ ? "/joint_states" : "/" + my_ns_ + "/joint_states";

    sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(img_topic, qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));
    sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>(odom_topic, qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) { latest_odom_ = *msg; });
    sub_joints_ = this->create_subscription<sensor_msgs::msg::JointState>(joint_topic, qos, [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
            for(size_t i=0; i<msg->name.size(); i++) { if(msg->name[i] == "gimbal_pan_joint") current_pan_ = msg->position[i]; }
        });
    sub_swarm_state_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>("/leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) { last_swarm_msg_ = *msg; });

    // --- VOTING SYSTEM PUBS/SUBS ---
    pub_confirm_ = this->create_publisher<std_msgs::msg::String>("/swarm/target_confirmations", 10);

    if (is_leader_) {
        sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(scan_topic, qos, std::bind(&YoloDetectorNode::scan_callback, this, std::placeholders::_1));
        pub_leader_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("leader_state", 10);
        
        sub_confirm_ = this->create_subscription<std_msgs::msg::String>(
            "/swarm/target_confirmations", 10, [this](const std_msgs::msg::String::SharedPtr msg) {
                RCLCPP_INFO(this->get_logger(), ">>> LEADER RECEIVED VOTE FROM: %s", msg->data.c_str());
                follower_confirmations_[msg->data] = this->get_clock()->now();
            });

        cv::namedWindow("LEADER_TACTICAL_VIEW", cv::WINDOW_AUTOSIZE);
        RCLCPP_INFO(this->get_logger(), "[SH_01] LEADER PREDATOR MODE ONLINE.");
    } else {
        RCLCPP_INFO(this->get_logger(), "[%s] FOLLOWER SNIPER MODE ONLINE.", my_ns_.c_str());
    }

    pub_detections_ = this->create_publisher<skyhunter_msgs::msg::DetectionArray>("swarm/detections", 10);
    pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
    pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
    pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("perception/overlay", 10);
}

YoloDetectorNode::~YoloDetectorNode() {
    if(is_leader_) cv::destroyWindow("LEADER_TACTICAL_VIEW");
}

// void YoloDetectorNode::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
//     pcl::PointCloud<pcl::PointXYZ>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//     pcl::fromROSMsg(*msg, *raw_cloud);
//     if (raw_cloud->empty()) return;

//     // 1. Voxel Grid Downsampling (Crucial for performance)
//     pcl::PointCloud<pcl::PointXYZ>::Ptr filtered_cloud(new pcl::PointCloud<pcl::PointXYZ>);
//     pcl::VoxelGrid<pcl::PointXYZ> vg;
//     vg.setInputCloud(raw_cloud);
//     vg.setLeafSize(0.2f, 0.2f, 0.2f); 
//     vg.filter(*filtered_cloud);

//     // 2. Angular Density Filtering (O(N) Complexity)
//     const int num_bins = 360;
//     std::vector<int> bin_counts(num_bins, 0);
//     std::vector<double> bin_min_dist(num_bins, 100.0);

//     for (const auto& p : filtered_cloud->points) {
//         if (p.z < -0.2 || p.z > 0.6) continue; // Ignore floor/sky
//         if (p.x < -0.5) continue;              // Ignore rear blind spot

//         double d = std::hypot(p.x, p.y);
//         if (d < 0.8 || d > 5.0) continue;      // Ignore too close or too far

//         double angle_deg = (std::atan2(p.y, p.x) * 180.0 / M_PI) + 180.0;
//         int idx = (int)angle_deg % num_bins;

//         bin_counts[idx]++;
//         if (d < bin_min_dist[idx]) {
//             bin_min_dist[idx] = d;
//         }
//     }

//     // 3. Find the "Best" Sparse Target
//     double best_dist = 100.0;
//     double best_angle = 0.0;
//     bool found = false;

//     for (int i = 0; i < num_bins; ++i) {
//         // Less than 6 points = Person. More than 6 = Wall.
//         if (bin_counts[i] > 0 && bin_counts[i] < 6) {
//             if (bin_min_dist[i] < best_dist) {
//                 best_dist = bin_min_dist[i];
//                 best_angle = (i - 180) * M_PI / 180.0;
//                 found = true;
//             }
//         }
//     }

//     closest_blob_dist_ = best_dist;
//     closest_blob_angle_ = best_angle;
//     blob_detected_ = found;
// }

void YoloDetectorNode::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    // --- 1. MEMORY OPTIMIZATION ---
    // Use static allocations to prevent memory fragmentation at 10+ Hz
    static pcl::PointCloud<pcl::PointXYZ> raw_cloud;
    static pcl::PointCloud<pcl::PointXYZ> filtered_cloud;
    
    raw_cloud.clear();
    filtered_cloud.clear();
    
    pcl::fromROSMsg(*msg, raw_cloud);
    if (raw_cloud.empty()) return;

    // --- 2. FAST DOWNSAMPLING ---
    // Voxel grid to drastically reduce points before the math loop
    pcl::VoxelGrid<pcl::PointXYZ> vg;
    vg.setInputCloud(raw_cloud.makeShared()); // Pass a temporary shared_ptr for API compliance
    vg.setLeafSize(0.2f, 0.2f, 0.2f); 
    vg.filter(filtered_cloud);

    // --- 3. HARDWARE-ACCELERATED ANGULAR FILTERING ---
    const int num_bins = 360;
    std::vector<int> bin_counts(num_bins, 0);
    std::vector<double> bin_min_dist_sq(num_bins, 10000.0); // Use Squared distance

    // Pre-calculate squared thresholds to avoid sqrt() in the heavy loop
    const double min_d_sq = 0.8 * 0.8; // 0.64
    const double max_d_sq = 5.0 * 5.0; // 25.0

    // Using OpenMP for loop vectorization
    #pragma omp parallel
    {
        // Thread-local variables to prevent race conditions
        std::vector<int> local_bin_counts(num_bins, 0);
        std::vector<double> local_bin_min_dist_sq(num_bins, 10000.0);

        #pragma omp for nowait
        for (size_t i = 0; i < filtered_cloud.points.size(); ++i) {
            const auto& p = filtered_cloud.points[i];
            
            // Fast Z & X filter (Ignore floor/sky, ignore rear)
            if (p.z < -0.2 || p.z > 0.6) continue; 
            if (p.x < -0.5) continue;              

            // FAST MATH: No sqrt()!
            double d_sq = (p.x * p.x) + (p.y * p.y);
            if (d_sq < min_d_sq || d_sq > max_d_sq) continue;      

            // Fast Angle Calculation
            double angle_deg = (std::atan2(p.y, p.x) * 180.0 / M_PI) + 180.0;
            int idx = (int)angle_deg % num_bins;

            local_bin_counts[idx]++;
            if (d_sq < local_bin_min_dist_sq[idx]) {
                local_bin_min_dist_sq[idx] = d_sq;
            }
        }

        // Merge thread-local results back into the global arrays
        #pragma omp critical
        {
            for (int i = 0; i < num_bins; ++i) {
                bin_counts[i] += local_bin_counts[i];
                if (local_bin_min_dist_sq[i] < bin_min_dist_sq[i]) {
                    bin_min_dist_sq[i] = local_bin_min_dist_sq[i];
                }
            }
        }
    }

    // --- 4. FIND THE PRIMARY THREAT ---
    double best_dist = 100.0;
    double best_angle = 0.0;
    bool found = false;

    for (int i = 0; i < num_bins; ++i) {
        // Less than 6 points = Person/Pole. More than 6 = Solid Wall.
        if (bin_counts[i] > 0 && bin_counts[i] < 6) {
            // ONLY do the square root here, on the very final candidates!
            double actual_dist = std::sqrt(bin_min_dist_sq[i]); 
            if (actual_dist < best_dist) {
                best_dist = actual_dist;
                best_angle = (i - 180) * M_PI / 180.0; // Convert back to Radians for Gimbal
                found = true;
            }
        }
    }

    // --- 5. UPDATE CLASS STATE ---
    closest_blob_dist_ = best_dist;
    closest_blob_angle_ = best_angle;
    blob_detected_ = found;
}

void YoloDetectorNode::image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
    
    // --- 1. TACTICAL SLEEP MODE ---
    // if (!is_leader_ && last_swarm_msg_.swarm_state < 3) {
    //     return; // EXIT IMMEDIATELY. 0% GPU USAGE.
    // }

    cv_bridge::CvImagePtr cv_ptr;
    try { cv_ptr = cv_bridge::toCvCopy(msg, "bgr8"); } catch (...) { return; }

    // --- 2. ACTIVE INFERENCE ---
    auto results = engine_->run_inference(cv_ptr->image);
    std::vector<byte_track::Object> objects;
    
    for (const auto& res : results) {
        bool is_person = (engine_->class_names[res.class_id] == "person");
        bool is_frisbee = (res.class_id == 74);

        if (is_person || is_frisbee) {
            byte_track::Rect<float> rect(res.box.x, res.box.y, res.box.width, res.box.height);
            objects.emplace_back(rect, res.class_id, res.confidence);
        }
    }
    
    auto tracked_targets = tracker_->update(objects, cv::Mat::eye(2, 3, CV_64F));

    int best_idx = -1; 
    double min_dist_m = 1e9;
    std::vector<double> current_dists;
    
    for (size_t i = 0; i < tracked_targets.size(); ++i) {
        double height_factor = (tracked_targets[i]->getScore() > 0.5) ? 1.7 : 0.3; 
        double d = (550.0 * height_factor) / tracked_targets[i]->getRect().height();
        current_dists.push_back(d);
        if (d < min_dist_m) { min_dist_m = d; best_idx = i; }
    }
    
    skyhunter_msgs::msg::DetectionArray det_msg;
    det_msg.header = msg->header;
    std_msgs::msg::Float64 pan_cmd, tilt_cmd;
    pan_cmd.data = 0.0; tilt_cmd.data = 0.0; 
    
    TrackingState current_state = PATROL; 
    bool ai_sees_target = !tracked_targets.empty();

    // --- LEADER VOTE COUNTING ---
    int active_votes = 0;
    if (is_leader_) {
        auto now = this->get_clock()->now();
        for (const auto& [f_ns, time] : follower_confirmations_) {
            if ((now - time).seconds() < 1.5) active_votes++;
        }
    }

    // STATIC HYSTERESIS VARIABLE ---
    static int blind_frames = 0;

    if (ai_sees_target) {
        blind_frames = 0; // Reset counter because AI sees the target

        if (!is_leader_ && (last_swarm_msg_.swarm_state == 3 || last_swarm_msg_.swarm_state == 4)) {
            std_msgs::msg::String vote; vote.data = my_ns_;
            pub_confirm_->publish(vote);
            cv::putText(cv_ptr->image, "VOTING: TARGET CONFIRMED", cv::Point(20, 90), 0, 0.7, cv::Scalar(0, 255, 0), 2);
        }

        for (size_t i = 0; i < tracked_targets.size(); ++i) {
            auto r = tracked_targets[i]->getRect();
            cv::Rect box(r.x(), r.y(), r.width(), r.height());
            std::string label = "ID:" + std::to_string(tracked_targets[i]->getTrackId()) + " " + std::to_string(current_dists[i]).substr(0,4) + "m";
            
            skyhunter_msgs::msg::Detection det;
            det.class_id = tracked_targets[i]->getTrackId(); 
            det.label = "person";
            det.confidence = current_dists[i];
            det.x = box.x; det.y = box.y; det.w = box.width; det.h = box.height;
            det_msg.detections.push_back(det);

            if ((int)i == best_idx) {
                // ** LEAD POINT LOGIC (RCWS FIRE SOLUTION) **
                std::vector<float> vel = tracked_targets[i]->getVelocity();
                float vx = vel[0]; float vy = vel[1];
                float lead_time = 0.5f; 
                float pred_cx = (r.x() + r.width()/2.0f) + (vx * lead_time);
                float pred_cy = (r.y() + r.height()/2.0f) + (vy * lead_time);

                // --- TACTICAL COLOR & TEXT LOGIC ---
                cv::Scalar target_color;
                std::string target_text;

                if (is_leader_) {
                    if (active_votes >= 1) {
                        target_color = cv::Scalar(0, 0, 255); // Red
                        target_text = "TARGET CONFIRMED " + label;
                        current_state = ENGAGED;
                    } else {
                        target_color = cv::Scalar(0, 255, 255); // Yellow
                        target_text = "WAITING FOR VOTES " + label;
                        current_state = COMBAT;
                    }
                } else {
                    target_color = cv::Scalar(0, 165, 255); // Orange
                    target_text = "SLAVE LOCK " + label;
                }

                // DRAW VISUALS DYNAMICALLY
                cv::rectangle(cv_ptr->image, box, target_color, 3); 
                cv::circle(cv_ptr->image, cv::Point(pred_cx, pred_cy), 8, target_color, -1); 
                cv::putText(cv_ptr->image, "LEAD", cv::Point(pred_cx+10, pred_cy), 0, 0.5, target_color, 1);
                cv::putText(cv_ptr->image, target_text, cv::Point(r.x(), r.y() - 10), 0, 0.6, target_color, 2);
                
                // --- THE FULL PID CONTROLLER ---
                double err_x = (pred_cx - 320.0) / 320.0;
                double err_y = (pred_cy - 240.0) / 240.0;

                static double integral_x = 0.0;
                static double integral_y = 0.0;
                double p_gain_pan = -1.2; double i_gain_pan = -0.15; 
                double p_gain_tilt = 0.8; double i_gain_tilt = 0.1;  

                if (std::abs(pred_cx - 320.0) < 5.0 && std::abs(pred_cy - 240.0) < 5.0) {
                    pan_cmd.data = 0.0; tilt_cmd.data = 0.0;
                    integral_x = 0.0; integral_y = 0.0;
                } else {
                    integral_x += err_x; integral_y += err_y;
                    integral_x = std::max(-2.0, std::min(2.0, integral_x));
                    integral_y = std::max(-2.0, std::min(2.0, integral_y));

                    double p_out_x = std::copysign(std::pow(std::abs(err_x), 0.7), err_x) * p_gain_pan;
                    double p_out_y = std::copysign(std::pow(std::abs(err_y), 0.7), err_y) * p_gain_tilt;

                    pan_cmd.data = p_out_x + (integral_x * i_gain_pan);
                    tilt_cmd.data = p_out_y + (integral_y * i_gain_tilt);
                }

                // --- LEADER STATE BROADCAST ---
                if (is_leader_) {
                    skyhunter_msgs::msg::LeaderState state_msg;
                    state_msg.header.stamp = this->get_clock()->now();
                    double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
                    double lead_angle_offset = std::atan2(320.0 - pred_cx, 550.0); 
                    double total_angle = r_yaw + current_pan_ - lead_angle_offset;

                    state_msg.target_pos.x = latest_odom_.pose.pose.position.x + (current_dists[i] * std::cos(total_angle));
                    state_msg.target_pos.y = latest_odom_.pose.pose.position.y + (current_dists[i] * std::sin(total_angle));
                    state_msg.target_locked = true; 

                    if (current_dists[i] < 10.0) {
                        if (active_votes >= 1) {
                            state_msg.swarm_state = 4; // STATE 4: ENGAGED!
                            cv::putText(cv_ptr->image, "DOUBLE LOCK (" + std::to_string(active_votes) + " VOTES)", cv::Point(20, 60), 0, 0.8, cv::Scalar(0, 0, 255), 2);
                        } else {
                            state_msg.swarm_state = 3; // STATE 3: CUEING
                            cv::putText(cv_ptr->image, "WAITING FOR SWARM CONFIRMATION...", cv::Point(20, 60), 0, 0.7, cv::Scalar(0, 255, 255), 2);
                        }
                    } else {
                        state_msg.swarm_state = 0;
                    }
                    pub_leader_state_->publish(state_msg);
                }
            } else {
                // Secondary Targets (GREEN)
                cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1);
                cv::putText(cv_ptr->image, label, cv::Point(r.x(), r.y() - 5), 0, 0.4, cv::Scalar(0, 255, 0), 1);
            }
        }
    }
    else {
        // ==========================================
        // NO AI TARGETS SEEN - BLIND SPOT LOGIC
        // ==========================================
        blind_frames++;

        // 4. SCANNING STATE (Radar Cueing)
        // Wait ~15 frames (0.5 sec) to confirm camera is truly blind
        if (is_leader_ && blob_detected_ && closest_blob_dist_ < 5.0 && blind_frames > 15) {
            current_state = SCANNING;
            
            // Use absolute angle from LiDAR, do not add current_pan
            pan_cmd.data = closest_blob_angle_;
            tilt_cmd.data = 0.0; // Look straight ahead while checking
            
            cv::putText(cv_ptr->image, "RADAR CUEING...", cv::Point(20, 60), 0, 0.7, cv::Scalar(255, 0, 255), 2);
            cv::putText(cv_ptr->image, "ANG: " + std::to_string(closest_blob_angle_*57.3).substr(0,4) + " deg", cv::Point(20, 90), 0, 0.7, cv::Scalar(255, 0, 255), 2);
        }
        // 5. FOLLOWER SLAVE MODE
        else if (!is_leader_ && (last_swarm_msg_.swarm_state == 3 || last_swarm_msg_.swarm_state == 4)) {
            current_state = COMBAT;
            double tx = last_swarm_msg_.target_pos.x;
            double ty = last_swarm_msg_.target_pos.y;
            double my_x = latest_odom_.pose.pose.position.x;
            double my_y = latest_odom_.pose.pose.position.y;
            double my_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
            
            double global_angle = std::atan2(ty - my_y, tx - my_x);
            double p_angle = global_angle - my_yaw;
            while (p_angle > M_PI) p_angle -= 2 * M_PI;
            while (p_angle < -M_PI) p_angle += 2 * M_PI;
            pan_cmd.data = p_angle;
            cv::putText(cv_ptr->image, "SEARCHING LEADER COORDS", cv::Point(20, 60), 0, 0.7, cv::Scalar(0, 165, 255), 2);
        }
        else {
            // PATROL: Look Straight
            pan_cmd.data = 0.0;
            tilt_cmd.data = 0.0;
        }
    }

    // Draw Status
    std::string status_txt = "PATROL";
    if (current_state == SCANNING) status_txt = "SCANNING";
    if (current_state == COMBAT) status_txt = "COMBAT (CUE)";
    if (current_state == ENGAGED) status_txt = "ENGAGED (FIRE NET READY)";
    cv::putText(cv_ptr->image, "MODE: " + status_txt, cv::Point(20, 30), 0, 0.6, cv::Scalar(255, 255, 0), 2);

    pub_pan_->publish(pan_cmd); 
    pub_tilt_->publish(tilt_cmd);
    pub_detections_->publish(det_msg);

    cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(180, 180, 0), cv::MARKER_CROSS, 25, 2);
    pub_overlay_->publish(*(cv_ptr->toImageMsg()));

    // if (is_leader_) {
    //     cv::imshow("LEADER_TACTICAL_VIEW", cv_ptr->image);
    //     cv::waitKey(1);
    // }
}

// void YoloDetectorNode::image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
//     cv_bridge::CvImagePtr cv_ptr;
//     try { cv_ptr = cv_bridge::toCvCopy(msg, "bgr8"); } catch (...) { return; }

//     // --- 2. ACTIVE INFERENCE ---
//     auto results = engine_->run_inference(cv_ptr->image);
//     std::vector<byte_track::Object> objects;
    
//     for (const auto& res : results) {
//         // Keep both "person" and ID 74 (Frisbee)
//         bool is_person = (engine_->class_names[res.class_id] == "person");
//         bool is_frisbee = (res.class_id == 74);

//         if (is_person || is_frisbee) {
//             byte_track::Rect<float> rect(res.box.x, res.box.y, res.box.width, res.box.height);
//             objects.emplace_back(rect, res.class_id, res.confidence);
//         }
//     }
    
//     auto tracked_targets = tracker_->update(objects, cv::Mat::eye(2, 3, CV_64F));

//     int best_idx = -1; 
//     double min_dist_m = 1e9;
//     std::vector<double> current_dists;
    
//     for (size_t i = 0; i < tracked_targets.size(); ++i) {
//         // Calculate distance (assuming person height for person, scale accordingly for frisbee)
//         double height_factor = (tracked_targets[i]->getScore() > 0.5) ? 1.7 : 0.3; // 1.7m for person, 0.3 for frisbee
//         double d = (550.0 * height_factor) / tracked_targets[i]->getRect().height();
//         current_dists.push_back(d);
//         if (d < min_dist_m) { min_dist_m = d; best_idx = i; }
//     }
    
//     skyhunter_msgs::msg::DetectionArray det_msg;
//     det_msg.header = msg->header;
//     std_msgs::msg::Float64 pan_cmd, tilt_cmd;
//     pan_cmd.data = 0.0; tilt_cmd.data = 0.0; 
    
//     TrackingState current_state = PATROL; 
//     bool ai_sees_target = !tracked_targets.empty();

//     // --- LEADER VOTE COUNTING ---
//     int active_votes = 0;
//     if (is_leader_) {
//         auto now = this->get_clock()->now();
//         for (const auto& [f_ns, time] : follower_confirmations_) {
//             if ((now - time).seconds() < 2.0) active_votes++; // Increased window for stability
//         }
//     }
    
//     static int blind_frames = 0;
//     if (ai_sees_target) {
//         blind_frames = 0;
//         if (!is_leader_ && (last_swarm_msg_.swarm_state == 3 || last_swarm_msg_.swarm_state == 4)) {
//             std_msgs::msg::String vote; vote.data = my_ns_;
//             pub_confirm_->publish(vote);
//         }

//         for (size_t i = 0; i < tracked_targets.size(); ++i) {
//             auto r = tracked_targets[i]->getRect();
//             cv::Rect box(r.x(), r.y(), r.width(), r.height());
//             std::string label = "ID:" + std::to_string(tracked_targets[i]->getTrackId());
            
//             skyhunter_msgs::msg::Detection det;
//             det.class_id = tracked_targets[i]->getTrackId(); 
//             det.label = "person";
//             det.confidence = current_dists[i];
//             det.x = box.x; det.y = box.y; det.w = box.width; det.h = box.height;
//             det_msg.detections.push_back(det);

//             if ((int)i == best_idx) {
//                 // ** LEAD POINT LOGIC (RCWS FIRE SOLUTION) **
//                 std::vector<float> vel = tracked_targets[i]->getVelocity();
//                 float vx = vel[0]; float vy = vel[1];
//                 float lead_time = 0.5f; // Predict 0.5 seconds into the future
                
//                 // Calculate future position
//                 float pred_cx = (r.x() + r.width()/2.0f) + (vx * lead_time);
//                 float pred_cy = (r.y() + r.height()/2.0f) + (vy * lead_time);

//                 // --- TACTICAL COLOR & TEXT LOGIC ---
//                 cv::Scalar target_color; std::string target_text;

//                 if (is_leader_) {
//                     if (active_votes >= 1) {
//                         target_color = cv::Scalar(0, 0, 255); // Red (Engaged)
//                         target_text = "ENGAGED: DOUBLE LOCK";
//                         current_state = ENGAGED;
//                     } else {
//                         target_color = cv::Scalar(0, 255, 255); // Yellow (Wait)
//                         target_text = "COMBAT: WAITING FOR VOTES";
//                         current_state = COMBAT;
//                     }
//                 } else {
//                     target_color = cv::Scalar(0, 165, 255); // Orange (Slave)
//                     target_text = "SLAVE LOCK";
//                 }

//                 // 1. Draw the current target (Red/Yellow box)
//                 cv::rectangle(cv_ptr->image, box, target_color, 3); 
//                 cv::putText(cv_ptr->image, target_text, cv::Point(r.x(), r.y() - 10), 0, 0.6, target_color, 2);
                
//                 // 2. Draw the LEAD POINT (The actual Fire Solution)
//                 cv::circle(cv_ptr->image, cv::Point(pred_cx, pred_cy), 8, cv::Scalar(0, 255, 255), -1); // Solid Yellow Dot
//                 cv::putText(cv_ptr->image, "LEAD", cv::Point(pred_cx + 10, pred_cy), 0, 0.5, cv::Scalar(0, 255, 255), 2);
                
//                 // 3. AIM GIMBAL AT THE LEAD POINT (Not the box center)
//                 // pan_cmd.data = ((pred_cx - 320.0) / 320.0) * -0.8;
//                 // tilt_cmd.data = ((pred_cy - 240.0) / 240.0) * 0.5;

//                 // --- THE FULL PID CONTROLLER  ---
//                 double err_x = (pred_cx - 320.0) / 320.0;
//                 double err_y = (pred_cy - 240.0) / 240.0;

//                 // 1. Static Integral Accumulators
//                 static double integral_x = 0.0;
//                 static double integral_y = 0.0;

//                 // 2. PID Gains
//                 double p_gain_pan = -1.2; 
//                 double i_gain_pan = -0.15; // Pushes pan to exact zero

//                 double p_gain_tilt = 0.8; 
//                 double i_gain_tilt = 0.1;  // Pushes tilt to exact zero

//                 // 3. Precision Deadband & Integral Windup Protection
//                 if (std::abs(pred_cx - 320.0) < 5.0 && std::abs(pred_cy - 240.0) < 5.0) {
//                     pan_cmd.data = 0.0;
//                     tilt_cmd.data = 0.0;
//                     integral_x = 0.0; // Reset when perfectly locked
//                     integral_y = 0.0;
//                 } else {
//                     // Accumulate Error
//                     integral_x += err_x;
//                     integral_y += err_y;

//                     // Anti-Windup (Prevent it from spinning out of control)
//                     integral_x = std::max(-2.0, std::min(2.0, integral_x));
//                     integral_y = std::max(-2.0, std::min(2.0, integral_y));

//                     // Non-Linear Proportional + Integral
//                     double p_out_x = std::copysign(std::pow(std::abs(err_x), 0.7), err_x) * p_gain_pan;
//                     double p_out_y = std::copysign(std::pow(std::abs(err_y), 0.7), err_y) * p_gain_tilt;

//                     pan_cmd.data = p_out_x + (integral_x * i_gain_pan);
//                     tilt_cmd.data = p_out_y + (integral_y * i_gain_tilt);
//                 }


//                 if (is_leader_) {
//                     skyhunter_msgs::msg::LeaderState state_msg;
//                     state_msg.header.stamp = this->get_clock()->now();
//                     double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
                    
//                     // Project the LEAD POINT to the Map (Not the current box)
//                     double lead_angle_offset = std::atan2(320.0 - pred_cx, 550.0); 
//                     double total_angle = r_yaw + current_pan_ - lead_angle_offset;

//                     state_msg.target_pos.x = latest_odom_.pose.pose.position.x + (current_dists[i] * std::cos(total_angle));
//                     state_msg.target_pos.y = latest_odom_.pose.pose.position.y + (current_dists[i] * std::sin(total_angle));
//                     state_msg.target_locked = true; 

//                     if (current_dists[i] < 10.0) {
//                         state_msg.swarm_state = (active_votes >= 1) ? 4 : 3;
//                         cv::putText(cv_ptr->image, "TARGET: " + std::to_string(state_msg.target_pos.x).substr(0,4) + ", " + std::to_string(state_msg.target_pos.y).substr(0,4), cv::Point(20, 60), 0, 0.8, cv::Scalar(255, 255, 255), 2);
//                     } else {
//                         state_msg.swarm_state = 0;
//                     }
//                     pub_leader_state_->publish(state_msg);
//                 }
//             } else {
//                 cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1);
//             }
//         }
//     }
//     else if (is_leader_ && blob_detected_ && closest_blob_dist_ < 5.0) {
//         current_state = SCANNING;
//         pan_cmd.data = current_pan_ + closest_blob_angle_;
//     }
//     else if (!is_leader_ && (last_swarm_msg_.swarm_state == 3 || last_swarm_msg_.swarm_state == 4)) {
//         // -- FOR ISSUE 2: FOLLOWER POINTING WRONG WAY ---
//         current_state = COMBAT;
//         double tx = last_swarm_msg_.target_pos.x;
//         double ty = last_swarm_msg_.target_pos.y;
//         double my_x = latest_odom_.pose.pose.position.x;
//         double my_y = latest_odom_.pose.pose.position.y;
        
//         // Get Global Orientation from Odometry
//         double my_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
        
//         double global_angle = std::atan2(ty - my_y, tx - my_x);
//         double p_angle = global_angle - my_yaw;
        
//         // Normalize to -180 to 180 degrees
//         while (p_angle > M_PI) p_angle -= 2 * M_PI;
//         while (p_angle < -M_PI) p_angle += 2 * M_PI;
        
//         pan_cmd.data = p_angle;
        
//         cv::putText(cv_ptr->image, "SEARCHING LEADER TARGET COORDS", cv::Point(20, 60), 0, 0.7, cv::Scalar(0, 165, 255), 2);
//         cv::putText(cv_ptr->image, "CUE ANGLE: " + std::to_string(p_angle*57.3).substr(0,4) + " deg", cv::Point(20, 90), 0, 0.7, cv::Scalar(0, 165, 255), 2);
//     }

//     // Draw Status
//     std::string status_txt = "PATROL";
//     if (current_state == SCANNING) status_txt = "SCANNING";
//     if (current_state == COMBAT) status_txt = "COMBAT";
//     if (current_state == ENGAGED) status_txt = "ENGAGED (FIRE NET READY)";
//     cv::putText(cv_ptr->image, "MODE: " + status_txt, cv::Point(20, 30), 0, 0.6, cv::Scalar(255, 255, 0), 2);

//     pub_pan_->publish(pan_cmd); 
//     pub_tilt_->publish(tilt_cmd);
//     pub_detections_->publish(det_msg);

//     cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(180, 180, 0), cv::MARKER_CROSS, 25, 2);
//     // cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(255, 255, 0), cv::MARKER_CROSS, 20, 1);
//     pub_overlay_->publish(*(cv_ptr->toImageMsg()));

// }