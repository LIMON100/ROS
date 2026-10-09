#ifndef HUMAN_DETECTOR__HUMAN_DETECTOR_HPP_
#define HUMAN_DETECTOR__HUMAN_DETECTOR_HPP_

/*** Include ***/
/* for general */
#include <string>
#include <vector>

/* for OpenCV */
#include <opencv2/opencv.hpp>

/* for ROS2 */
#include <rclcpp/rclcpp.hpp>
#include <image_transport/image_transport.hpp>

#if __has_include(<cv_bridge/cv_bridge.hpp>)
#include <cv_bridge/cv_bridge.hpp>
#else
#include <cv_bridge/cv_bridge.h>
#endif

/* for Combat Robot system msgs */
#include "combat_robot_msgs/msg/detected_object.hpp"
#include "combat_robot_msgs/msg/detected_objects.hpp"
#include "combat_robot_msgs/msg/target_point.hpp"

/* for Tracker */
#include "tracker.h"
#include "bounding_box.h"
#include "common_helper.h"
#include "common_helper_cv.h"
#include "hungarian_algorithm.h"

/* for My modules */
#include "detection_processor.hpp"

namespace human_detector {

using DetectedObjectMsg = combat_robot_msgs::msg::DetectedObject;
using DetectedObjectsMsg = combat_robot_msgs::msg::DetectedObjects;
using TargetPointMsg = combat_robot_msgs::msg::TargetPoint;

// yolo11s class id
const uint8_t BACKGROUND = 0;
const uint8_t HEAD = 1;
const uint8_t ARMS = 2;
const uint8_t TORSO = 3;
const uint8_t LEGS = 4;

struct FrameData {
    // Core Data
    int frame_id;
    cv::Mat org_frame;
    cv::Mat resized_for_infer;
    cv::Mat affine_matrix;
};

struct TargetInfo {
    bool is_found = false;
    float norm_x = -1.0f;
    float norm_y = -1.0f;
    float norm_h = 0.0f;
    int32_t track_id = -1;
    uint8_t class_id = 0;
    hailo::util::bbox_t box = {0, 0, 0, 0};
};

class HumanDetectorComponent : public rclcpp::Node {
public:
    HumanDetectorComponent(const rclcpp::NodeOptions& options);
	~HumanDetectorComponent() {};

private:
    void InitRosParam();
    void InitRosCommon();
    void InitCommonClass();

    void MakeDir(const std::string& dir);

    TargetInfo GetTargetPoint(const std::vector<hailo::util::object_t>& human_objects, const cv::Size& image_size, const std::vector<int>& track_ids);
    int selectTargetByPriority(const std::vector<hailo::util::object_t>& human_objects, const cv::Size& image_size, const std::vector<int>& track_ids);

    void getTrackID(std::vector<Track>& track_list, std::vector<hailo::util::object_t>& detected_objects, std::vector<int>& track_ids);

    bool isPredictedOnlyThisFrame(Track& tr);
    void drawDashRect(cv::Mat& image, const cv::Rect& r, const cv::Scalar& col, int thickness=2, int dash=8);
    void drawDetectionsAndPredictions(cv::Mat& image, const std::vector<hailo::util::object_t>& human_objects, const std::vector<int>& track_ids, std::vector<Track>& track_list);

    void CallbackImage(const sensor_msgs::msg::Image::ConstSharedPtr& msg);


    rclcpp::Publisher<DetectedObjectsMsg>::SharedPtr m_pub_human_objects;
    rclcpp::Publisher<TargetPointMsg>::SharedPtr m_pub_target_point;

    image_transport::Publisher m_pub_human_image;
    image_transport::Subscriber m_sub_camera_image;

    bool m_is_compressed;
    
    std::string m_label_text_path;
    hailo_param_t m_hailo_param;
    std::shared_ptr<DetectionProcessor> m_detector;

    bool m_debug_mode;
    bool m_save_image;
    int m_save_count = 0;
    int m_class_num = 1;
    float m_human_thresh;
    float m_drone_thresh;
    std::string m_save_dir;

    // for fps
    std::chrono::steady_clock::time_point m_last_fps_time;
    int m_frame_count = 0;

    // Tracker member variable
    Tracker m_tracker;
};

} // namespace human_detector

#endif // HUMAN_DETECTOR__HUMAN_DETECTOR_HPP_