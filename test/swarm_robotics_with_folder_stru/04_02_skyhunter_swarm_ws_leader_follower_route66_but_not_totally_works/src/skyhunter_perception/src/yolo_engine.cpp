#include "skyhunter_perception/yolo_engine.hpp"
#include <iostream>
#include <opencv2/dnn/dnn.hpp>

YoloEngine::YoloEngine(const std::string& model_path, bool use_gpu) {
    if (use_gpu) {
        try {
            OrtCUDAProviderOptions cuda_options;
            cuda_options.device_id = 0;
            session_options_.AppendExecutionProvider_CUDA(cuda_options);
            std::cout << "[INFO] Perception Engine: Initializing on RTX 5070 Ti (CUDA 12)." << std::endl;
        } catch (...) {
            std::cout << "[WARN] CUDA failed. Falling back to CPU Provider." << std::endl;
        }
    }
    session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), session_options_);

    Ort::AllocatorWithDefaultOptions allocator;
    auto in_name = session_->GetInputNameAllocated(0, allocator);
    input_names_str_.push_back(in_name.get());
    input_names_.push_back(input_names_str_.back().c_str());

    auto out_name = session_->GetOutputNameAllocated(0, allocator);
    output_names_str_.push_back(out_name.get());
    output_names_.push_back(output_names_str_.back().c_str());

    class_names = {"person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};
}

// void YoloEngine::preprocess(cv::Mat& frame, float* blob) {
//     cv::Mat resized;
//     cv::resize(frame, resized, cv::Size(input_w_, input_h_));
//     cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
    
//     cv::Mat float_img;
//     resized.convertTo(float_img, CV_32FC3, 1.0 / 255.0);

//     // Optimized NCHW format conversion
//     std::vector<cv::Mat> channels(3);
//     for (int i = 0; i < 3; ++i) {
//         channels[i] = cv::Mat(input_h_, input_w_, CV_32FC1, blob + i * input_w_ * input_h_);
//     }
//     cv::split(float_img, channels);
// }


void YoloEngine::preprocess(cv::Mat& frame, float* blob) {
    cv::Mat blob_img;
    cv::dnn::blobFromImage(frame, blob_img, 1.0 / 255.0, cv::Size(input_w_, input_h_), cv::Scalar(0,0,0), true, false, CV_32F);
    std::memcpy(blob, blob_img.ptr<float>(), 3 * input_w_ * input_h_ * sizeof(float));
}

std::vector<YOLOBox> YoloEngine::run_inference(cv::Mat& frame) {
    std::vector<float> input_tensor_values(3 * input_w_ * input_h_);
    preprocess(frame, input_tensor_values.data());

    std::vector<int64_t> input_dims = {1, 3, input_h_, input_w_};
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor_values.data(), input_tensor_values.size(), input_dims.data(), input_dims.size());

    auto output_tensors = session_->Run(Ort::RunOptions{nullptr}, input_names_.data(), &input_tensor, 1, output_names_.data(), 1);

    float* data = output_tensors[0].GetTensorMutableData<float>();
    auto shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
    
    cv::Mat output_mat(shape[1], shape[2], CV_32F, data);
    output_mat = output_mat.t(); // Transpose to [8400, 84]

    std::vector<int> class_ids; std::vector<float> confidences; std::vector<cv::Rect> boxes;
    float x_factor = (float)frame.cols / 640.0; float y_factor = (float)frame.rows / 640.0;

    for (int i = 0; i < output_mat.rows; ++i) {
        float* row_ptr = output_mat.ptr<float>(i);
        cv::Mat scores(1, 80, CV_32F, row_ptr + 4);
        cv::Point class_id; double max_score;
        cv::minMaxLoc(scores, 0, &max_score, 0, &class_id);

        if (max_score > 0.45) {
            float w = row_ptr[2] * x_factor; float h = row_ptr[3] * y_factor;
            int left = int((row_ptr[0] * x_factor) - 0.5 * w);
            int top = int((row_ptr[1] * y_factor) - 0.5 * h);
            boxes.push_back(cv::Rect(left, top, (int)w, (int)h));
            confidences.push_back(max_score);
            class_ids.push_back(class_id.x);
        }
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, 0.45, 0.5, indices);

    std::vector<YOLOBox> results;
    for (int idx : indices) results.push_back({boxes[idx], confidences[idx], class_ids[idx]});
    return results;
}