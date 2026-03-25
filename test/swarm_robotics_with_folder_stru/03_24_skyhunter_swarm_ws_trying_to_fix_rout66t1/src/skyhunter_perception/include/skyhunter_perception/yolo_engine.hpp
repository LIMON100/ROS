#ifndef SKYHUNTER_PERCEPTION__YOLO_ENGINE_HPP_
#define SKYHUNTER_PERCEPTION__YOLO_ENGINE_HPP_

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

struct YOLOBox {
    cv::Rect box;
    float confidence;
    int class_id;
};

class YoloEngine {
public:
    YoloEngine(const std::string& model_path, bool use_gpu = true);
    std::vector<YOLOBox> run_inference(cv::Mat& frame);
    std::vector<std::string> class_names;

private:
    void preprocess(cv::Mat& frame, float* blob);
    
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "YoloEngine"};
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;

    std::vector<std::string> input_names_str_;
    std::vector<std::string> output_names_str_;
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;

    const int input_w_ = 640;
    const int input_h_ = 640;
};

#endif