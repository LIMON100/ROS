#ifndef IMAGE_PROCESSOR_HPP_
#define IMAGE_PROCESSOR_HPP_

/*** Include ***/
/* for general */
#include <iostream>
#include <vector>
#include <string>

/* for OpenCV */
#include <opencv2/core.hpp>

/* for My modules */
#include "camera_transform.hpp"
#include "object_detection.hpp"
#include "bounding_box.h"
#include "tracker.h"

typedef struct {
    int min_roi_pixel;
    float min_fod_size;
    float max_fod_size;
} processor_param_t;

typedef struct {
    cv::Mat camera_matrix;
    cv::Mat distrotion_coefficients;
    cv::Mat perspective_matrix;
} camera_param_t;

typedef struct {
    std::string model_path;

    std::vector<int> input_dims;
    std::string input_name;
    std::vector<std::string> output_names;
    std::vector<std::vector<int>> anchors;

    float box_thres;
    float nms_thres;

    uint8_t class_num;
    
    bool sigmoid;
} engine_param_t;

#define NUM_MAX_RESULT 100

typedef struct {
    float x      = 0.f;
    float y      = 0.f;
    float width  = 0.f;
    float height = 0.f;
} location_t;

typedef struct {
    std::string label;
    object_t roi;
    location_t local_point;
    int trackID = -1;
} fod_t;

typedef struct {
    fod_t stage1;
    fod_t stage2;
    cv::Rect roi;
} debug_fod_t;

class ImageProcessor {
public:
    typedef struct{
        int32_t object_num;
        struct{
            int32_t class_id;
            char label[32];
            double score;
            int32_t x;
            int32_t y;
            int32_t width;
            int32_t height;
            int32_t trackID;
        }object_list[NUM_MAX_RESULT];
    } Result;

    typedef struct{
        std::vector<BoundingBox> bbox_list;
    }ResultCrop;

    ImageProcessor(std::string label_text_path,
                   const processor_param_t& processor_param,
                   const camera_param_t& camera_param,
                   const engine_param_t& stage1_param,
                   const engine_param_t& stage2_param);
    ~ImageProcessor() {};

    cv::Rect fod_roi_rect(const bbox_t& bbox, const cv::Size image_size,
                          int min_pixel=64, int padding=0);

    cv::Mat fod_roi_padd(const cv::Mat &image, const bbox_t& bbox);

    std::vector<fod_t> stage1_process(const cv::Mat& image, cv::Rect roi, ImageProcessor::ResultCrop& result);
    std::vector<fod_t> stage2_process(const cv::Mat& image, const std::vector<fod_t>& stage1_objects);
    std::vector<fod_t> all_stage_process(const cv::Mat& image, cv::Rect roi, ImageProcessor::Result& result);
    
    /* tracking box matching detected box*/
    void getTrackID(std::vector<Track>& track_list, std::vector<fod_t>& detected_objects);
    std::vector<debug_fod_t> debug_getTrackID(const cv::Mat& image, std::vector<Track>& track_list, std::vector<fod_t>& detected_objects, cv::Mat& debug);
    
    std::pair<std::vector<fod_t>, bool> debug_process(const cv::Mat& image, cv::Rect roi, cv::Mat& debug, ImageProcessor::Result& result);

    bool isDist(const BoundingBox tb, const BoundingBox db);

private:
    bool ReadLabels(const std::string& filename);
    location_t CalcObjectLocalPoint(const bbox_t& bbox);
    std::vector<fod_t> FodObjectsNMS(const std::vector<object_t>& objects,
                                     const cv::Size image_size, int padding=0);
    cv::Size PutTextInfo(const std::string str, cv::Mat& image, cv::Point point, int min_w=150);
    
    std::vector<std::string> m_labels;

    processor_param_t m_processor_param;

    std::shared_ptr<CameraTransform> m_camera_transform;

    std::shared_ptr<Yolov5Detection> m_stage1_engine;
    std::shared_ptr<Yolov5Detection> m_stage2_engine;

    const cv::Scalar m_red    = cv::Scalar( 50,  50, 200);
    const cv::Scalar m_blue   = cv::Scalar(200,  50,  50);
    const cv::Scalar m_green  = cv::Scalar( 50, 200,  50);
    const cv::Scalar m_yellow = cv::Scalar( 50, 200, 200);
    const cv::Scalar m_gray   = cv::Scalar(200, 200, 200);
    const cv::Scalar m_black  = cv::Scalar( 50,  50,  50);
    const cv::Scalar m_white  = cv::Scalar(250, 250, 250);

};

#endif