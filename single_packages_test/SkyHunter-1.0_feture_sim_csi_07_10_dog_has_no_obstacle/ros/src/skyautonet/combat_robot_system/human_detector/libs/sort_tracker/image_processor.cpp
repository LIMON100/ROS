/* for My modules */
#include "image_processor.hpp"
#include "tracker.h"
#include "bounding_box.h"
#include "common_helper.h"
#include "common_helper_cv.h"
#include "hungarian_algorithm.h"


/* for OpenCV */
#include <opencv2/imgproc.hpp>

/* for general */
#include <utility>
#include <fstream>
#include <sstream>

/*** Macro ***/
#define TAG "ImageProcessor"
#define NUM_MAX_RESULT 100

/*Global variable*/
Tracker s_tracker;

ImageProcessor::ImageProcessor(std::string label_text_path,
                               const processor_param_t& processor_param,
                               const camera_param_t& camera_param,
                               const engine_param_t& stage1_param,
                               const engine_param_t& stage2_param)
    : m_processor_param(processor_param)
{
    m_camera_transform = std::make_shared<CameraTransform>(camera_param.camera_matrix,
                                                           camera_param.distrotion_coefficients,
                                                           camera_param.perspective_matrix);

    m_stage1_engine = std::make_shared<Yolov5Detection>(stage1_param.model_path,
                                                        stage1_param.input_dims,
                                                        stage1_param.input_name,
                                                        stage1_param.output_names,
                                                        stage1_param.anchors,
                                                        stage1_param.box_thres,
                                                        stage1_param.nms_thres,
                                                        stage1_param.class_num,
                                                        stage1_param.sigmoid);

    m_stage2_engine = std::make_shared<Yolov5Detection>(stage2_param.model_path,
                                                        stage2_param.input_dims,
                                                        stage2_param.input_name,
                                                        stage2_param.output_names,
                                                        stage2_param.anchors,
                                                        stage2_param.box_thres,
                                                        stage2_param.nms_thres,
                                                        stage2_param.class_num,
                                                        stage2_param.sigmoid);

    ReadLabels(label_text_path);
    std::cout << "image_processor initialization" << std::endl;
}

bool ImageProcessor::ReadLabels(const std::string& filename) {
    std::ifstream ifs(filename);
    if (ifs.fail()) {
        std::cout << "Failed to read " << filename << std::endl;
        return false;
    }

    std::string str;
    while (getline(ifs, str)) {
        m_labels.push_back(str);
    }

    return true;
}

location_t ImageProcessor::CalcObjectLocalPoint(const bbox_t& bbox) {
    auto& tf = m_camera_transform;

    float bbox_cx = bbox.x + (bbox.width / 2.f);
    float bbox_cy = bbox.y + (bbox.height / 2.f);

    cv::Point2f image_center_left   = cv::Point2f(bbox.x, bbox_cy);
    cv::Point2f image_center_right  = cv::Point2f(bbox.x + bbox.width, bbox_cy);
    cv::Point2f image_center_top    = cv::Point2f(bbox_cx, bbox.y);
    cv::Point2f image_center_bottom = cv::Point2f(bbox_cx, bbox.y + bbox.height);

    cv::Point2f local_center_left   = tf->get_point_meter(image_center_left);
    cv::Point2f local_center_right  = tf->get_point_meter(image_center_right);
    cv::Point2f local_center_top    = tf->get_point_meter(image_center_top);
    cv::Point2f local_center_bottom = tf->get_point_meter(image_center_bottom);

    location_t local_point;
    local_point.x = local_center_bottom.x;
    local_point.y = local_center_bottom.y;
    local_point.width = std::fabs(local_center_left.y - local_center_right.y);
    local_point.height = std::fabs(local_center_top.x - local_center_bottom.x);

    return local_point;
}

std::vector<fod_t> ImageProcessor::FodObjectsNMS(const std::vector<object_t>& objects,
                                                 const cv::Size image_size, int padding)
{
    std::vector<fod_t> fods;
    std::vector<bool> valid(objects.size(), false);

    cv::Rect image_roi(padding, padding,
                       image_size.width - (padding * 2), image_size.height - (padding * 2));
    cv::Point padding_point(padding, padding);
    cv::Size padding_size(padding * 2, padding * 2);

    for (size_t i = 0; i < objects.size(); i++) {
        if (valid[i]) {
            continue;
        }

        valid[i] = true;

        object_t object1 = objects[i];
        cv::Rect first_fod(object1.bbox.x, object1.bbox.y, object1.bbox.width, object1.bbox.height);
        
        if (first_fod.area() > (first_fod & image_roi).area()) {
            continue;
        }

        cv::Rect first_roi = first_fod - padding_point;
        first_roi += padding_size;

        for (size_t j = i + 1; j < objects.size(); j++) {
            if (valid[j]) {
                continue;
            }
            
            object_t object2 = objects[j];
            cv::Rect second_fod(object2.bbox.x, object2.bbox.y, object2.bbox.width, object2.bbox.height);
        
            if (second_fod.area() > (second_fod & image_roi).area()) {
                valid[j] = true;
                continue;
            }
    
            cv::Rect second_roi = second_fod - padding_point;
            second_roi += padding_size;
    
            cv::Rect intersect_roi = first_roi & second_roi;
            if (intersect_roi.area() > 0) {
                valid[j] = true;
                first_roi |= second_roi;
                first_fod |= second_fod;

                if (object1.prob < object2.prob) {
                    object1 = object2;
                }
            }
        }

        bbox_t bbox = {first_fod.x, first_fod.y, first_fod.width, first_fod.height};
        location_t local_point = CalcObjectLocalPoint(bbox);

        int min_fod_area = std::pow(m_processor_param.min_fod_size, 2);
        int max_fod_area = std::pow(m_processor_param.max_fod_size, 2);
        int fod_area = (local_point.width * 100) * (local_point.height * 100);

        bool over_max = (fod_area > max_fod_area);
        bool under_min = (fod_area < min_fod_area);

        if (over_max || under_min) {
            continue;
        }

        fod_t fod;
        fod.label =  m_labels[object1.id];
        fod.roi.id = object1.id;
        fod.roi.prob = object1.prob;
        fod.roi.bbox = bbox;
        fod.local_point = local_point;

        fods.push_back(fod);
    }

    return fods;
}

cv::Size ImageProcessor::PutTextInfo(const std::string str, cv::Mat& image, cv::Point point, int min_w) {
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 0.5;
    int thickness = 2;
    int lineType = cv::LINE_4;
    int baseline = 0;

    cv::Size text_size = cv::getTextSize(str, fontFace, fontScale, thickness, &baseline);
    text_size.height += baseline;
    if (text_size.width < min_w) {
        text_size.width = min_w;
    }

    cv::Rect text_rect(point, text_size);
    cv::rectangle(image, text_rect, m_black, -1, lineType);

    point.y += (text_size.height - baseline);
    cv::putText(image, str, point, fontFace, fontScale, m_white, thickness);

    return text_size;
}

cv::Rect ImageProcessor::fod_roi_rect(const bbox_t& bbox, const cv::Size image_size,
                                      int min_pixel, int padding)
{
    int roi_size = std::max(bbox.width + (padding * 2), bbox.height + (padding * 2));
    roi_size = std::max(roi_size, min_pixel);
    
    int roi_cx = bbox.x + (bbox.width / 2);
    int roi_cy = bbox.y + (bbox.height / 2);

    int roi_x = std::max(0, roi_cx - (roi_size / 2));
    int roi_y = std::max(0, roi_cy - (roi_size / 2));
    int roi_w = roi_size;
    int roi_h = roi_size;

    int roi_x2 = roi_x + roi_w;
    if (roi_x2 > image_size.width) {
        roi_x -= std::abs(roi_x2 - image_size.width);
    }

    int roi_y2 = roi_y + roi_h;
    if (roi_y2 > image_size.height) {
        roi_y -= std::abs(roi_y2 - image_size.height);
    }

    return cv::Rect(roi_x, roi_y, roi_w, roi_h);
}

cv::Mat ImageProcessor::fod_roi_padd(const cv::Mat& img, const bbox_t& bbox)
{   
    int x = bbox.x, y = bbox.y, width = bbox.width, height = bbox.height;
    cv::Rect roi(x, y, width, height);
    cv::Mat background(img.size(), img.type(), cv::Scalar(128, 128, 128));
    cv::GaussianBlur(background, background, cv::Size(21, 21), 0);

    img(roi).copyTo(background(roi));
    
    return img;
}

std::vector<fod_t> ImageProcessor::stage1_process(const cv::Mat& image, cv::Rect roi, ImageProcessor::ResultCrop& result) {
    if (!m_stage1_engine) {
        std::cout << "Not stage1_engine initialized" << std::endl;
        return std::vector<fod_t>();
    }

    cv::Mat roi_img = image(roi);
    std::vector<object_t> stage1_objects = m_stage1_engine->infer(roi_img);

    for (auto& object : stage1_objects) {
        object.bbox.x += roi.x;
        object.bbox.y += roi.y;
    }

    std::vector<fod_t> fods = FodObjectsNMS(stage1_objects, image.size(), 10);

    std::vector<BoundingBox> bbox_list;
    bbox_list.reserve(fods.size());

    for(auto& fod:fods){
        bbox_list.push_back({fod.roi.id, fod.label, fod.roi.prob,
                             fod.roi.bbox.x, fod.roi.bbox.y,
                             fod.roi.bbox.width, fod.roi.bbox.height});
    }

    result.bbox_list = bbox_list;

    return fods;
}

std::vector<fod_t> ImageProcessor::stage2_process(const cv::Mat& image, const std::vector<fod_t>& stage1_objects) {
    if (!m_stage2_engine) {
        std::cout << "Not stage2_engine initialized" << std::endl;
        return std::vector<fod_t>();
    }

    std::vector<object_t> stage2_objects;
    for (const auto& stage1_object : stage1_objects) {
    
        cv::Rect roi = fod_roi_rect(stage1_object.roi.bbox, image.size(),
                                    m_processor_param.min_roi_pixel, 10);

        cv::Mat roi_img = image(roi);

        auto temp_objects = m_stage2_engine->infer(roi_img);

        for (auto& object : temp_objects) {
            // if(object.id >= 25) continue;
            // if((stage1_object.roi.prob > 0.45 && object.prob >= 0.65) || (stage1_object.roi.prob <= 0.45 && object.prob >= 0.75)){
            // if(object.prob >= 0.75){
                object.bbox.x += roi.x;
                object.bbox.y += roi.y;
                
                stage2_objects.push_back(object);
            // }
        }
    }

    std::vector<fod_t> fods = FodObjectsNMS(stage2_objects, image.size(), 10);

    return fods;
}

void ImageProcessor::getTrackID(std::vector<Track>& track_list, std::vector<fod_t>& detected_objects) {
    int T = (int)track_list.size();
    int D = (int)detected_objects.size();
    int N = std::max(T, D);

    std::vector<std::vector<float>> cost_matrix(N, std::vector<float>(N, 1.0f));

    for(int i = 0; i < T; i++){
        const auto& tb = track_list[i].GetLatestData().bbox;
        for(int j = 0; j < D; j++){
            BoundingBox det;
            det.x = detected_objects[j].roi.bbox.x;
            det.y = detected_objects[j].roi.bbox.y;   
            det.w = detected_objects[j].roi.bbox.width;
            det.h = detected_objects[j].roi.bbox.height;
            det.class_id = detected_objects[j].roi.id;
            det.label = detected_objects[j].label;
            det.score = detected_objects[j].roi.prob;
            float iou = BoundingBoxUtils::CalculateIoU(tb, det);       
            
            if(iou >= 0.3){
            }
            else if(iou < 0.1){
                iou = 0.0f;
            }
            else{
                if(tb.class_id != det.class_id){
                    iou = 0.0f;
                }
            }

            cost_matrix[i][j] = 1.0f - iou;     
        }
    }

    std::vector<int> det_for_track(N, -1);
    std::vector<int> track_for_det(N, -1);

    if(track_list.size() > 0 && detected_objects.size() > 0) {
        HungarianAlgorithm<float> solver(cost_matrix);
        solver.Solve(det_for_track, track_for_det);
    }

    for(int j = 0; j < D; j++){
        int ti = track_for_det[j];
        if(ti >= 0 && ti < T && cost_matrix[ti][j] < 1.0f){
            detected_objects[j].trackID = track_list[ti].GetId();
        }
        else{
            detected_objects[j].trackID = -1;
        }
    }
}

std::vector<debug_fod_t> ImageProcessor::debug_getTrackID(const cv::Mat& image, std::vector<Track>& track_list, std::vector<fod_t>& detected_objects, cv::Mat& debug) {
    int T = (int)track_list.size();
    int D = (int)detected_objects.size();
    int N = std::max(T, D);
    std::vector<std::vector<float>> cost_matrix(N, std::vector<float>(N, 1.0f));

    for(int i = 0; i < T; i++){
        const auto& tb = track_list[i].GetLatestData().bbox;
        for(int j = 0; j < D; j++){
            BoundingBox det;
            det.x = detected_objects[j].roi.bbox.x;
            det.y = detected_objects[j].roi.bbox.y;   
            det.w = detected_objects[j].roi.bbox.width;
            det.h = detected_objects[j].roi.bbox.height;
            det.class_id = detected_objects[j].roi.id;
            det.label = detected_objects[j].label;
            det.score = detected_objects[j].roi.prob;
            float iou = BoundingBoxUtils::CalculateIoU(tb, det);       
            
            if(iou >= 0.3){
            }
            else if(iou < 0.1){
                iou = 0.0f;
            }
            else{
                if(tb.class_id != det.class_id){
                    iou = 0.0f;
                }
            }

            cost_matrix[i][j] = 1.0f - iou;     
        }
    }

    std::vector<int> det_for_track(N, -1);
    std::vector<int> track_for_det(N, -1);

    std::vector<debug_fod_t> fods;
    fods.reserve(detected_objects.size());
    int thickness = 2;
    int lineType = cv::LINE_4;

    if(track_list.size() > 0 && detected_objects.size() > 0) {
        HungarianAlgorithm<float> solver(cost_matrix);
        solver.Solve(det_for_track, track_for_det);
    }

    for(int j = 0; j < D; j++){
        int ti = track_for_det[j];
        if(ti >= 0 && ti < T && cost_matrix[ti][j] < 1.0f){
            detected_objects[j].trackID = track_list[ti].GetId();
        }
        else{
            detected_objects[j].trackID = -1;
        }

        cv::Rect bbox(detected_objects[j].roi.bbox.x, detected_objects[j].roi.bbox.y,
            detected_objects[j].roi.bbox.width, detected_objects[j].roi.bbox.height);
        cv::rectangle(debug, bbox, m_blue, thickness, lineType);

        cv::Rect stage1_roi = fod_roi_rect(detected_objects[j].roi.bbox, image.size(),
                                            m_processor_param.min_roi_pixel, 10);
        cv::rectangle(debug, stage1_roi, m_yellow, thickness, lineType);

        debug_fod_t fod;
        fod.stage1 = detected_objects[j];
        fod.roi = stage1_roi;

        fods.push_back(fod);
    }

    return fods;
}

std::vector<fod_t> ImageProcessor::all_stage_process(const cv::Mat& image, cv::Rect roi, ImageProcessor::Result& result) {
    ImageProcessor::ResultCrop det_result;
    std::vector<fod_t> stage1_objects = stage1_process(image, roi, det_result);
    // std::vector<fod_t> stage2_objects = stage2_process(image, stage1_objects);

    s_tracker.Update(det_result.bbox_list);
    auto& track_list = s_tracker.GetTrackList();

    getTrackID(track_list, stage1_objects);

    /* Return the results */
    int32_t bbox_num = 0;
    for (auto& track : track_list) {
        const auto& bbox = track.GetLatestData().bbox;
        result.object_list[bbox_num].class_id = bbox.class_id;
        snprintf(result.object_list[bbox_num].label, sizeof(result.object_list[bbox_num].label), "%s", bbox.label.c_str());
        result.object_list[bbox_num].score = bbox.score;
        result.object_list[bbox_num].x = bbox.x;
        result.object_list[bbox_num].y = bbox.y;
        result.object_list[bbox_num].width = bbox.w;
        result.object_list[bbox_num].height = bbox.h;
        result.object_list[bbox_num].trackID = track.GetId();
        bbox_num++;
        if (bbox_num >= NUM_MAX_RESULT) break;
    }
    result.object_num = bbox_num;

    return stage1_objects;
}

static cv::Scalar GetColorForId(int32_t id)
{
    static constexpr int32_t kMaxNum = 100;
    static std::vector<cv::Scalar> color_list;
    if (color_list.empty()) {
        std::srand(123);
        for (int32_t i = 0; i < kMaxNum; i++) {
            color_list.push_back(CommonHelper::CreateCvColor(std::rand() % 255, std::rand() % 255, std::rand() % 255));
        }
    }
    return color_list[id % kMaxNum];
}

float ComputeIoU(const cv::Rect& a, const cv::Rect& b){
    float interArea = (a & b).area();
    float unionArea = a.area() + b.area() - interArea;
    return unionArea > 0 ? interArea / unionArea : 0.0f;
}

bool ImageProcessor::isDist(const BoundingBox tb, const BoundingBox db){
    constexpr float MAX_DIST = 5.0f;

    float tb_cx = tb.x + tb.w * 0.5f;
    float tb_cy = tb.y + tb.h * 0.5f;

    float db_cx = db.x + db.w * 0.5f;
    float db_cy = db.y + db.h * 0.5f;

    float dx = tb_cx - db_cx;
    float dy = tb_cy - db_cy;

    float dist = std::hypot(dx, dy);

    return dist <= MAX_DIST;
}

std::pair<std::vector<fod_t>, bool> ImageProcessor::debug_process(const cv::Mat& image, cv::Rect roi, cv::Mat& debug, ImageProcessor::Result& result) {
    ImageProcessor::ResultCrop det_result;

    std::vector<fod_t> stage1_objects = stage1_process(image, roi, det_result);

    s_tracker.Update(det_result.bbox_list);
    
    auto& track_list = s_tracker.GetTrackList();

    std::vector<debug_fod_t> fods = debug_getTrackID(image, track_list, stage1_objects, debug);

    int thickness = 2;
    int lineType = cv::LINE_4;

    /* Return the results */
    int32_t bbox_num = 0;
    for (auto& track : track_list) {
        const auto& bbox = track.GetLatestData().bbox;
        result.object_list[bbox_num].class_id = bbox.class_id;
        snprintf(result.object_list[bbox_num].label, sizeof(result.object_list[bbox_num].label), "%s", bbox.label.c_str());
        result.object_list[bbox_num].score = bbox.score;
        result.object_list[bbox_num].x = bbox.x;
        result.object_list[bbox_num].y = bbox.y;
        result.object_list[bbox_num].width = bbox.w;
        result.object_list[bbox_num].height = bbox.h;
        result.object_list[bbox_num].trackID = track.GetId();
        bbox_num++;
        if (bbox_num >= NUM_MAX_RESULT) break;
    }
    result.object_num = bbox_num;

    int min_roi = 255;
    int start_roi = 10;
    cv::Point start_point(start_roi, start_roi);
    cv::Size roi_size(min_roi, min_roi);
    
    for (const auto& fod: fods) {
        cv::Mat roi_image = debug(fod.roi).clone();
        cv::resize(roi_image, roi_image, roi_size, 0, 0, cv::INTER_LINEAR);
        
        cv::Rect put_roi(start_point, roi_size);
        roi_image.copyTo(debug(put_roi));
        
        fod_t fod_s1 = fod.stage1;
        
        fod_s1.roi.prob = std::round(fod_s1.roi.prob * 100.0) / 100.0;

        std::stringstream ss;
        ss << "s1 l: " << fod_s1.label.substr(0, 15) << " p: " << fod_s1.roi.prob << "\n";
        ss << "trackID: " << fod_s1.trackID;

        std::istringstream iss(ss.str());
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        
        cv::Point text_point(start_point);
        for (const auto &txt : lines) {
            text_point.y += PutTextInfo(txt, debug, text_point, min_roi).height;
        }

        start_point.x += roi_size.width;
        if ((start_point.x + roi_size.width) > debug.cols) {
            start_point.x = start_roi;
            start_point.y += roi_size.height;
        }

        if ((start_point.y + roi_size.height) > roi.y) {
            break;
        }
    }

    cv::rectangle(debug, roi, m_yellow, thickness, lineType);
    bool success = !stage1_objects.empty();
    //  || !stage2_objects.empty();

    return std::make_pair(stage1_objects, success);
}