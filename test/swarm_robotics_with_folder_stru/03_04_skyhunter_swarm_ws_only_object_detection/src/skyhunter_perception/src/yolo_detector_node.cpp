// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/image.hpp>
// #include <cv_bridge/cv_bridge.h>
// #include <skyhunter_msgs/msg/detection_array.hpp>
// #include <skyhunter_msgs/msg/detection.hpp>
// #include "skyhunter_perception/yolo_engine.hpp"
// #include <ament_index_cpp/get_package_share_directory.hpp>

// class YoloDetectorNode : public rclcpp::Node {
// public:
//     YoloDetectorNode() : Node("yolo_detector_node") {
//         std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
//         std::string model_path = pkg_dir + "/models/yolov8m.onnx";

//         engine_ = std::make_unique<YoloEngine>(model_path, true);

//         // --- FIX 1: USE SENSOR DATA QoS (This matches Gazebo) ---
//         auto qos = rclcpp::SensorDataQoS();

//         sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(
//             "/rgb_camera/image_raw", qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));

//         pub_detections_ = this->create_publisher<skyhunter_msgs::msg::DetectionArray>("/swarm/detections", 10);
//         pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("/perception/sh01_overlay", 10);

//         RCLCPP_INFO(this->get_logger(), "YOLO NODE: Waiting for Gazebo frames on /rgb_camera/image_raw...");
//     }

// private:
//     void image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
//         cv_bridge::CvImagePtr cv_ptr;
//         try {
//             cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
//         } catch (...) { return; }

//         auto results = engine_->run_inference(cv_ptr->image);

//         skyhunter_msgs::msg::DetectionArray out_msg;
//         out_msg.header = msg->header;

//         for (const auto& res : results) {
//             std::string label = engine_->class_names[res.class_id];
            
//             // --- TACTICAL FILTERS ---
//             // 1. ONLY "person"
//             // 2. Confidence MUST be > 0.65 (High reliability)
//             if (label == "person" && res.confidence > 0.65) {
                
//                 double distance = (550.0 * 1.7) / res.box.height;

//                 skyhunter_msgs::msg::Detection det;
//                 det.label = label;
//                 det.confidence = res.confidence;
//                 det.x = res.box.x; det.y = res.box.y;
//                 det.w = res.box.width; det.h = res.box.height;
//                 out_msg.detections.push_back(det);

//                 // --- DRAW BOUNDING BOX (For RViz) ---
//                 cv::rectangle(cv_ptr->image, res.box, cv::Scalar(0, 0, 255), 3); // Red Box
//                 std::string info = "TARGET: PERSON [" + std::to_string(distance).substr(0,4) + "m]";
//                 cv::putText(cv_ptr->image, info, cv::Point(res.box.x, res.box.y - 10), 
//                             cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
                
//                 RCLCPP_INFO(this->get_logger(), "ALERT: %s detected at %.2fm", label.c_str(), distance);
//             }
//         }

//         pub_detections_->publish(out_msg);
        
//         // --- ALWAYS PUBLISH THE VIEWING FEED ---
//         auto overlay_msg = cv_ptr->toImageMsg();
//         overlay_msg->header = msg->header;
//         pub_overlay_->publish(*overlay_msg);
//     }

//     std::unique_ptr<YoloEngine> engine_;
//     rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
//     rclcpp::Publisher<skyhunter_msgs::msg::DetectionArray>::SharedPtr pub_detections_;
//     rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_overlay_;
// };

// int main(int argc, char** argv) {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<YoloDetectorNode>());
//     rclcpp::shutdown();
//     return 0;
// }

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <skyhunter_msgs/msg/detection_array.hpp>
#include <skyhunter_msgs/msg/detection.hpp>
#include "skyhunter_perception/yolo_engine.hpp"
#include <ament_index_cpp/get_package_share_directory.hpp>

class YoloDetectorNode : public rclcpp::Node {
public:
    YoloDetectorNode() : Node("yolo_detector_node") {
        std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
        std::string model_path = pkg_dir + "/models/yolov8m.onnx";

        engine_ = std::make_unique<YoloEngine>(model_path, true);

        // --- CRITICAL FIX 1: TOPIC NAMES ---
        // Based on your 'ros2 topic list', the leader topic is exactly /rgb_camera/image_raw
        std::string image_topic = "/rgb_camera/image_raw";

        // --- CRITICAL FIX 2: QoS SETTINGS ---
        // Gazebo publishes as 'SensorData'. Reliable subscribers will stay silent forever.
        auto qos = rclcpp::SensorDataQoS();

        sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(
            image_topic, qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));

        pub_detections_ = this->create_publisher<skyhunter_msgs::msg::DetectionArray>("/swarm/detections", 10);
        pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("/perception/sh01_overlay", 10);

        RCLCPP_INFO(this->get_logger(), "Listening to: %s with SensorData QoS", image_topic.c_str());
        
        // Create a named window for immediate feedback (Like your Python demo)
        cv::namedWindow("TACTICAL_VIEW", cv::WINDOW_AUTOSIZE);
    }

private:
    void image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
        cv_bridge::CvImagePtr cv_ptr;
        try {
            // Gazebo images are usually RGB8
            cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
        } catch (cv_bridge::Exception& e) {
            RCLCPP_ERROR(this->get_logger(), "CV Bridge Error: %s", e.what());
            return;
        }

        // Run Inference
        auto results = engine_->run_inference(cv_ptr->image);

        skyhunter_msgs::msg::DetectionArray out_msg;
        out_msg.header = msg->header;

        for (const auto& res : results) {
            // Only process 'person' class for the tactical mission
            if (engine_->class_names[res.class_id] == "person") {
                
                double distance = (550.0 * 1.7) / res.box.height;

                skyhunter_msgs::msg::Detection det;
                det.label = "PERSON";
                det.confidence = res.confidence;
                out_msg.detections.push_back(det);

                // DRAW BOXES (Red for targets)
                cv::rectangle(cv_ptr->image, res.box, cv::Scalar(0, 0, 255), 3);
                std::string txt = "PERSON " + std::to_string(distance).substr(0,4) + "m";
                cv::putText(cv_ptr->image, txt, cv::Point(res.box.x, res.box.y - 10), 
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
            }
        }

        // --- VISUAL FEEDBACK (Like Python Demo) ---
        cv::imshow("TACTICAL_VIEW", cv_ptr->image);
        cv::waitKey(1); // Crucial for OpenCV window refresh

        // Publish ROS message
        pub_detections_->publish(out_msg);
        
        // Publish Overlay to ROS
        auto overlay_msg = cv_ptr->toImageMsg();
        overlay_msg->header = msg->header;
        pub_overlay_->publish(*overlay_msg);
    }

    std::unique_ptr<YoloEngine> engine_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
    rclcpp::Publisher<skyhunter_msgs::msg::DetectionArray>::SharedPtr pub_detections_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_overlay_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<YoloDetectorNode>());
    cv::destroyAllWindows();
    rclcpp::shutdown();
    return 0;
}