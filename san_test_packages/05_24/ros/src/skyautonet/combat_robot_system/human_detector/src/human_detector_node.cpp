// SAN v1.5.1 PHASE 6 — HumanDetectorNode implementation.
//
// DCN-2026-003 D-003 (2026-05-13): camera -> NPU -> DetectionArray
// pipeline. See header for design rationale.

#include "human_detector/human_detector_node.hpp"

#include <chrono>

#include <cv_bridge/cv_bridge.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <combat_robot_msgs/msg/detection.hpp>

#include "human_detector/stub_backend.hpp"

namespace human_detector {

using Detection_msg      = combat_robot_msgs::msg::Detection;
using DetectionArray_msg = combat_robot_msgs::msg::DetectionArray;

// ─── ctors ─────────────────────────────────────────────────────────────

HumanDetectorNode::HumanDetectorNode()
    : HumanDetectorNode(rclcpp::NodeOptions())
{}

HumanDetectorNode::HumanDetectorNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("human_detector_node", options),
      last_infer_at_(0, 0, RCL_ROS_TIME)
{
    declareParameters();
    readParameters();
    selectBackend();
    wireInterfaces();
}

// ─── parameters ────────────────────────────────────────────────────────

void HumanDetectorNode::declareParameters() {
    // Backend selection (v1.3 PHASE 6)
    declare_parameter<std::string>("inference_backend",            "rk3588");
    declare_parameter<std::string>("model_path",                   "");
    declare_parameter<std::string>("rk3588_fallback_model_path",   "");

    // v1.5.1 pipeline (DCN-2026-003 D-003 + I-15 fix)
    //
    // image_mode default = "raw": subscribe to san_video_decoder's
    // output. Migration deployments can flip to "auto" to also
    // listen on the CompressedImage path if the decoder node is not
    // yet running.
    declare_parameter<std::string>("image_mode",       "raw");
    declare_parameter<std::string>("camera_topic",
                                    "/imx678_camera_node/image_compressed");
    declare_parameter<std::string>("decoded_topic",
                                    "/imx678_camera_node/image_decoded");
    declare_parameter<std::string>("detections_topic", "~/detections");
    declare_parameter<int>(        "max_inference_hz",   15);
    declare_parameter<bool>(       "drop_when_busy",     true);
}

void HumanDetectorNode::readParameters() {
    requested_backend_           = get_parameter("inference_backend").as_string();
    model_path_                  = get_parameter("model_path").as_string();
    rk3588_fallback_model_path_  =
        get_parameter("rk3588_fallback_model_path").as_string();
    if (rk3588_fallback_model_path_.empty()) {
        rk3588_fallback_model_path_ = model_path_;
    }

    image_mode_       = get_parameter("image_mode").as_string();
    camera_topic_     = get_parameter("camera_topic").as_string();
    decoded_topic_    = get_parameter("decoded_topic").as_string();
    detections_topic_ = get_parameter("detections_topic").as_string();
    max_inference_hz_ = get_parameter("max_inference_hz").as_int();
    drop_when_busy_   = get_parameter("drop_when_busy").as_bool();
    if (max_inference_hz_ < 1)   max_inference_hz_ = 1;
    if (max_inference_hz_ > 60)  max_inference_hz_ = 60;

    // Normalize image_mode_.
    if (image_mode_ != "raw" && image_mode_ != "compressed"
        && image_mode_ != "auto")
    {
        RCLCPP_WARN(get_logger(),
            "Unknown image_mode='%s'; defaulting to 'raw' (v1.5.1 production).",
            image_mode_.c_str());
        image_mode_ = "raw";
    }
}

// ─── backend selection (v1.3, unchanged) ───────────────────────────────

void HumanDetectorNode::selectBackend() {
    backend_ = createBackend(requested_backend_);
    if (backend_ == nullptr) {
        RCLCPP_WARN(get_logger(),
            "Unknown inference_backend='%s'; using stub",
            requested_backend_.c_str());
        backend_ = std::make_unique<StubBackend>();
        backend_->initialize(model_path_);
        return;
    }
    if (backend_->initialize(model_path_)) {
        RCLCPP_INFO(get_logger(),
            "AI inference backend: %s", backend_->getName().c_str());
        return;
    }
    // Fall back to RK3588 (spec'd baseline) before degrading to stub.
    if (backend_->getName() != "rk3588") {
        RCLCPP_WARN(get_logger(),
            "Backend %s init failed; falling back to rk3588",
            backend_->getName().c_str());
        backend_ = createBackend("rk3588");
        if (backend_ && backend_->initialize(rk3588_fallback_model_path_)) {
            RCLCPP_INFO(get_logger(),
                "AI inference backend: %s (fallback)",
                backend_->getName().c_str());
            return;
        }
    }
    RCLCPP_WARN(get_logger(),
        "rk3588 backend unavailable; using stub. "
        "AI features disabled until a valid backend is wired.");
    backend_ = std::make_unique<StubBackend>();
    backend_->initialize(model_path_);
}

// ─── ROS wiring (v1.5.1 NEW) ───────────────────────────────────────────

void HumanDetectorNode::wireInterfaces() {
    // ─── v1.5.1 (DCN-2026-003 D-003 / I-15 fix) image source selection ─
    //
    // image_mode_ governs which subscription(s) we create:
    //
    //   "raw"        — production. The san_video_decoder node
    //                  consumes the camera's H.265 CompressedImage,
    //                  decodes via MPP HW (or avdec_h265 on host),
    //                  and republishes sensor_msgs/Image on
    //                  decoded_topic_. We subscribe there directly.
    //                  cv::imdecode is NEVER invoked on H.265.
    //
    //   "compressed" — legacy / dev. Only useful when the upstream
    //                  camera publishes JPEG (which cv::imdecode
    //                  supports). The H.265 path is silently broken
    //                  here and should not be used.
    //
    //   "auto"       — migration window: subscribe to both. The
    //                  throttle (max_inference_hz_) prevents double-
    //                  firing, and image_sub_ takes precedence
    //                  because the decoded path arrives later but
    //                  with valid frames.
    if (image_mode_ == "raw" || image_mode_ == "auto") {
        image_sub_ = create_subscription<sensor_msgs::msg::Image>(
            decoded_topic_, rclcpp::SensorDataQoS().keep_last(2),
            std::bind(&HumanDetectorNode::onImage, this,
                      std::placeholders::_1));
    }
    if (image_mode_ == "compressed" || image_mode_ == "auto") {
        compressed_sub_ =
            create_subscription<sensor_msgs::msg::CompressedImage>(
                camera_topic_, rclcpp::SensorDataQoS().keep_last(2),
                std::bind(&HumanDetectorNode::onCompressedImage, this,
                          std::placeholders::_1));
    }

    // DetectionArray output — reliable, depth 10. san_perception
    // (Python) used `~/detections` on reliable QoS; mission_node
    // and threat_aggregator both subscribe with reliable, so keep
    // the same shape.
    det_pub_ = create_publisher<DetectionArray_msg>(
        detections_topic_, rclcpp::QoS(10).reliable());

    // 1 Hz health log so operators see the path is alive even when
    // there are no detections (e.g. patrolling empty corridor).
    health_timer_ = create_wall_timer(
        std::chrono::seconds(1),
        std::bind(&HumanDetectorNode::onHealthTick, this));

    RCLCPP_INFO(get_logger(),
        "HumanDetectorNode UP: backend=%s ready=%d "
        "image_mode=%s decoded=%s compressed=%s detections=%s max_hz=%d",
        backend_->getName().c_str(),
        static_cast<int>(backend_->isReady()),
        image_mode_.c_str(),
        decoded_topic_.c_str(),
        camera_topic_.c_str(),
        detections_topic_.c_str(),
        max_inference_hz_);
}

// ─── Camera callbacks ──────────────────────────────────────────────────

void HumanDetectorNode::onCompressedImage(
    sensor_msgs::msg::CompressedImage::SharedPtr msg)
{
    if (msg == nullptr) return;
    ++frames_in_;

    // Throttle to max_inference_hz_. Camera comes in at 30 fps but NPU
    // realistically delivers ~15 fps on RK3588 dual-core.
    const auto t = now();
    const double min_interval_s = 1.0 / static_cast<double>(max_inference_hz_);
    if (last_infer_at_.nanoseconds() != 0
        && (t - last_infer_at_).seconds() < min_interval_s)
    {
        ++frames_dropped_;
        return;
    }

    // If the previous inference is still running and drop_when_busy_
    // is set, skip; otherwise we'll queue behind the mutex.
    if (drop_when_busy_ && infer_busy_.load()) {
        ++frames_dropped_;
        return;
    }

    // Decode compressed (H.265 from imx678, jpeg from stub) into BGR.
    // cv::imdecode handles both via format=="hevc"/"h265"/"jpeg" via
    // the OpenCV format detector. We pass the raw bytes view.
    cv::Mat encoded(1, static_cast<int>(msg->data.size()), CV_8UC1,
                    const_cast<uint8_t*>(msg->data.data()));
    cv::Mat bgr = cv::imdecode(encoded, cv::IMREAD_COLOR);
    if (bgr.empty()) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
            "imdecode failed (format=%s, size=%zu) — skipping frame",
            msg->format.c_str(), msg->data.size());
        ++frames_dropped_;
        return;
    }

    last_infer_at_ = t;
    processFrame(bgr, msg->header);
}

void HumanDetectorNode::onImage(
    sensor_msgs::msg::Image::SharedPtr msg)
{
    if (msg == nullptr) return;
    ++frames_in_;

    const auto t = now();
    const double min_interval_s = 1.0 / static_cast<double>(max_inference_hz_);
    if (last_infer_at_.nanoseconds() != 0
        && (t - last_infer_at_).seconds() < min_interval_s)
    {
        ++frames_dropped_;
        return;
    }
    if (drop_when_busy_ && infer_busy_.load()) {
        ++frames_dropped_;
        return;
    }

    try {
        auto cv_ptr = cv_bridge::toCvShare(msg, "bgr8");
        if (!cv_ptr || cv_ptr->image.empty()) {
            ++frames_dropped_;
            return;
        }
        last_infer_at_ = t;
        processFrame(cv_ptr->image, msg->header);
    } catch (const cv_bridge::Exception& e) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
            "cv_bridge: %s", e.what());
        ++frames_dropped_;
    }
}

// ─── Core: infer + publish ─────────────────────────────────────────────

void HumanDetectorNode::processFrame(const cv::Mat& bgr,
                                      const std_msgs::msg::Header& src_header)
{
    if (!backend_ || !backend_->isReady() || bgr.empty()) {
        ++frames_dropped_;
        return;
    }

    infer_busy_.store(true);
    std::vector<Detection> backend_dets;
    {
        std::lock_guard<std::mutex> lock(infer_mutex_);
        const auto t0 = std::chrono::steady_clock::now();
        backend_dets = backend_->infer(bgr);
        (void)t0;  // latency tracked inside the backend (EMA)
    }
    infer_busy_.store(false);
    ++frames_inferred_;

    // Build DetectionArray. Format matches san_perception (Python)
    // so downstream consumers (mission_node, threat_aggregator,
    // san_operation_control) do not need topic schema changes.
    auto out = std::make_unique<DetectionArray_msg>();
    out->header.stamp    = now();
    out->header.frame_id = "perception";
    out->source_width    = static_cast<uint32_t>(bgr.cols);
    out->source_height   = static_cast<uint32_t>(bgr.rows);
    out->source_frame_id = src_header.frame_id;
    out->inference_time_ms = static_cast<uint32_t>(
        backend_->getInferenceLatencyMs() + 0.5);
    out->cycle_timestamp_ms = static_cast<uint64_t>(
        now().nanoseconds() / 1'000'000LL);

    out->detections.reserve(backend_dets.size());
    for (const auto& d : backend_dets) {
        Detection_msg det;
        det.class_id    = cocoToSanClassId(d.class_id, d.label);
        det.confidence  = d.confidence;
        // Clamp + cast — bbox is float in backend, uint32 on wire.
        const float x1 = std::max(0.0f, d.bbox[0]);
        const float y1 = std::max(0.0f, d.bbox[1]);
        const float x2 = std::max(0.0f, d.bbox[2]);
        const float y2 = std::max(0.0f, d.bbox[3]);
        det.bbox_x1 = static_cast<uint32_t>(x1);
        det.bbox_y1 = static_cast<uint32_t>(y1);
        det.bbox_x2 = static_cast<uint32_t>(x2);
        det.bbox_y2 = static_cast<uint32_t>(y2);
        // Sensor fusion fields left at defaults — depth / thermal
        // fusion happens in a downstream node (Turn 11-12.5).
        det.estimated_depth_m   = 0.0f;
        det.thermal_avg_temp_c  = std::nanf("");
        det.thermal_max_temp_c  = std::nanf("");
        det.has_thermal_signature = false;
        out->detections.push_back(std::move(det));
    }

    if (det_pub_) {
        det_pub_->publish(std::move(out));
        ++publishes_;
    }
}

combat_robot_msgs::msg::DetectionArray
HumanDetectorNode::detectOnFrameForTest(const cv::Mat& bgr)
{
    // Side-effect: publish too, so integration tests can watch the
    // topic. Counter increments mirror the production path so health
    // logs stay coherent.
    std_msgs::msg::Header h;
    h.stamp = now();
    h.frame_id = "test";
    processFrame(bgr, h);

    // Mirror what we published: re-run the conversion deterministically.
    DetectionArray_msg snapshot;
    snapshot.header.stamp = now();
    snapshot.header.frame_id = "perception";
    snapshot.source_width  = static_cast<uint32_t>(bgr.cols);
    snapshot.source_height = static_cast<uint32_t>(bgr.rows);
    snapshot.inference_time_ms = static_cast<uint32_t>(
        (backend_ ? backend_->getInferenceLatencyMs() : 0.0) + 0.5);
    return snapshot;
}

// ─── Class id remap (COCO -> SAN) ──────────────────────────────────────

uint8_t HumanDetectorNode::cocoToSanClassId(int coco_id,
                                             const std::string& label)
{
    // COCO 80-class:
    //   0 = person, 1 = bicycle, 2 = car, 3 = motorcycle, 5 = bus,
    //   7 = truck, 14 = bird, 15 = cat, 16 = dog, 17 = horse, ...
    // SAN combat_robot_msgs/Detection:
    //   CLASS_UNKNOWN=0, PERSON=1, VEHICLE=2, DRONE=3, WEAPON=4, ANIMAL=5
    using D = Detection_msg;

    // Prefer label match — RK3588 Airys port emits the COCO label
    // string. Drone is COCO-untrained (Phase 1 detection only emits
    // it if a custom drone-trained model is loaded; the label string
    // is the explicit cue).
    if (!label.empty()) {
        if (label == "person")                  return D::CLASS_PERSON;
        if (label == "car"        || label == "truck"     ||
            label == "bus"        || label == "motorcycle"||
            label == "bicycle")                 return D::CLASS_VEHICLE;
        if (label == "drone"      || label == "airplane") return D::CLASS_DRONE;
        if (label == "knife"      || label == "rifle"   ||
            label == "gun")                     return D::CLASS_WEAPON;
        if (label == "bird"       || label == "cat"     ||
            label == "dog"        || label == "horse"   ||
            label == "sheep"      || label == "cow"     ||
            label == "elephant"   || label == "bear"    ||
            label == "zebra"      || label == "giraffe") return D::CLASS_ANIMAL;
    }

    // Fallback: COCO id ranges.
    switch (coco_id) {
        case 0:   return D::CLASS_PERSON;
        case 1: case 2: case 3: case 5: case 7:
                  return D::CLASS_VEHICLE;
        case 4:   return D::CLASS_DRONE;    // COCO id 4 = airplane
        case 14: case 15: case 16: case 17: case 18:
        case 19: case 20: case 21: case 22: case 23:
                  return D::CLASS_ANIMAL;
        default:  return D::CLASS_UNKNOWN;
    }
}

// ─── Health log ────────────────────────────────────────────────────────

void HumanDetectorNode::onHealthTick() {
    const uint64_t in_   = frames_in_.load();
    const uint64_t drop_ = frames_dropped_.load();
    const uint64_t inf_  = frames_inferred_.load();
    const uint64_t pub_  = publishes_.load();
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
        "human_detector: in=%lu drop=%lu inferred=%lu pub=%lu lat_ms=%.1f "
        "backend=%s ready=%d",
        in_, drop_, inf_, pub_,
        backend_ ? backend_->getInferenceLatencyMs() : 0.0,
        backend_ ? backend_->getName().c_str() : "none",
        backend_ ? static_cast<int>(backend_->isReady()) : 0);
}

}  // namespace human_detector
