// SAN v1.5.1 PHASE 6 - RK3588 NPU backend implementation.
//
// DCN-2026-003 D-003 (2026-05-13): full YOLOv5 inference path,
// ported from Airys V6.13.5 (src/board/rknn_detector_board.cpp).
// Airys is the field-validated reference; the YOLO decode + NMS
// here is a 1:1 translation with COCO label resolution and the
// Detection struct adapted to SAN (label + bbox float).
//
// Build profile:
//   HAVE_RKNN=1 (board)   -> librknnrt + real inference
//   HAVE_RKNN=0 (host)    -> initialize() returns false -> stub fallback
//                            (no behavioural difference vs v1.3 stub path)

#include "human_detector/rk3588_npu_backend.hpp"
#include "human_detector/postprocess_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#ifdef HAVE_RKNN
#include <opencv2/opencv.hpp>
#endif

namespace human_detector {

namespace {

double updateEma(double prev, double sample, double alpha) {
    if (prev <= 0.0) return sample;
    return alpha * sample + (1.0 - alpha) * prev;
}

}  // namespace

// ─── COCO label table ──────────────────────────────────────────────────

const std::string& cocoLabel(int class_id) {
    static const std::vector<std::string> kLabels = {
        "person", "bicycle", "car", "motorcycle", "airplane",
        "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird",
        "cat", "dog", "horse", "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack",
        "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat",
        "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
        "wine glass", "cup", "fork", "knife", "spoon",
        "bowl", "banana", "apple", "sandwich", "orange",
        "broccoli", "carrot", "hot dog", "pizza", "donut",
        "cake", "chair", "couch", "potted plant", "bed",
        "dining table", "toilet", "tv", "laptop", "mouse",
        "remote", "keyboard", "cell phone", "microwave", "oven",
        "toaster", "sink", "refrigerator", "book", "clock",
        "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
    };
    static const std::string kUnknown = "";
    if (class_id < 0 || class_id >= static_cast<int>(kLabels.size())) {
        return kUnknown;
    }
    return kLabels[class_id];
}

// ─── ctor/dtor ─────────────────────────────────────────────────────────

RK3588NPUBackend::RK3588NPUBackend()
    : ready_(false), latency_ema_ms_(0.0)
{}

RK3588NPUBackend::~RK3588NPUBackend() {
#ifdef HAVE_RKNN
    std::lock_guard<std::mutex> lock(infer_mutex_);
    if (ctx_ != 0) {
        rknn_destroy(ctx_);
        ctx_ = 0;
    }
#endif
}

#ifdef HAVE_RKNN

// ─── YOLO constants (ported from Airys V6.13.5) ────────────────────────

namespace {

constexpr int   YOLO_NUM_ANCHORS_PER_HEAD = 3;
constexpr int   YOLO_NUM_HEADS            = 3;

const int yolov5_anchors[YOLO_NUM_HEADS][YOLO_NUM_ANCHORS_PER_HEAD][2] = {
    {{10,13},   {16,30},   {33,23}},      // P3/8
    {{30,61},   {62,45},   {59,119}},     // P4/16
    {{116,90},  {156,198}, {373,326}},    // P5/32
};
const int yolov5_strides[YOLO_NUM_HEADS] = {8, 16, 32};

// [DCN-2026-006 EXT D-018] Numerically-safe sigmoid — implementation
// moved to header postprocess_helpers.hpp so the unit tests
// (test_postprocess_d018_d019.cpp) can exercise it directly. See
// the header for design rationale.
using human_detector::postprocess::fastSigmoid;
inline float fast_sigmoid(float x) { return fastSigmoid(x); }

float iou(const Detection& a, const Detection& b) {
    // bbox = {x1, y1, x2, y2}
    const float ax1 = a.bbox[0], ay1 = a.bbox[1];
    const float ax2 = a.bbox[2], ay2 = a.bbox[3];
    const float bx1 = b.bbox[0], by1 = b.bbox[1];
    const float bx2 = b.bbox[2], by2 = b.bbox[3];

    const float ix1 = std::max(ax1, bx1);
    const float iy1 = std::max(ay1, by1);
    const float ix2 = std::min(ax2, bx2);
    const float iy2 = std::min(ay2, by2);
    const float iw = std::max(0.0f, ix2 - ix1);
    const float ih = std::max(0.0f, iy2 - iy1);
    const float inter = iw * ih;
    const float area_a = std::max(0.0f, ax2 - ax1) * std::max(0.0f, ay2 - ay1);
    const float area_b = std::max(0.0f, bx2 - bx1) * std::max(0.0f, by2 - by1);
    const float uni   = area_a + area_b - inter;
    return (uni <= 0.0f) ? 0.0f : inter / uni;
}

// Airys V6.13.5 A2 fix — thread_local scratch buffers to avoid heap
// allocations on the hot path. uint8_t (not vector<bool>) for
// allocator-friendliness.
void apply_nms(std::vector<Detection>& dets, float thresh) {
    thread_local std::vector<uint8_t>  keep_scratch;
    thread_local std::vector<Detection> kept_scratch;

    keep_scratch.assign(dets.size(), 1);
    kept_scratch.clear();
    if (kept_scratch.capacity() < dets.size()) {
        kept_scratch.reserve(dets.size());
    }

    std::sort(dets.begin(), dets.end(),
              [](const Detection& a, const Detection& b) {
                  return a.confidence > b.confidence;
              });

    for (size_t i = 0; i < dets.size(); ++i) {
        if (!keep_scratch[i]) continue;
        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (!keep_scratch[j]) continue;
            if (dets[i].class_id != dets[j].class_id) continue;
            if (iou(dets[i], dets[j]) > thresh) keep_scratch[j] = 0;
        }
    }
    for (size_t i = 0; i < dets.size(); ++i) {
        if (keep_scratch[i]) kept_scratch.push_back(dets[i]);
    }
    dets.swap(kept_scratch);
}

}  // anonymous namespace

// ─── initialize() ──────────────────────────────────────────────────────

bool RK3588NPUBackend::initialize(const std::string& model_path) {
    std::lock_guard<std::mutex> lock(infer_mutex_);

    // 1. Read .rknn blob
    std::ifstream f(model_path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "[rk3588] cannot open model: %s\n",
                     model_path.c_str());
        return false;
    }
    f.seekg(0, std::ios::end);
    const size_t model_size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> model_data(model_size);
    f.read(reinterpret_cast<char*>(model_data.data()), model_size);
    f.close();

    // 2. rknn_init
    int rc = rknn_init(&ctx_, model_data.data(), model_size, 0, nullptr);
    if (rc != RKNN_SUCC) {
        std::fprintf(stderr, "[rk3588] rknn_init failed: %d\n", rc);
        ctx_ = 0;
        return false;
    }

    // 3. Query I/O num
    rknn_input_output_num io_num{};
    rc = rknn_query(ctx_, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (rc != RKNN_SUCC) {
        std::fprintf(stderr, "[rk3588] query IN_OUT_NUM failed: %d\n", rc);
        rknn_destroy(ctx_);
        ctx_ = 0;
        return false;
    }
    n_input_  = io_num.n_input;
    n_output_ = io_num.n_output;

    // 4. Input attrs
    input_attrs_.resize(n_input_);
    for (uint32_t i = 0; i < n_input_; ++i) {
        input_attrs_[i].index = i;
        rc = rknn_query(ctx_, RKNN_QUERY_INPUT_ATTR,
                        &input_attrs_[i], sizeof(rknn_tensor_attr));
        if (rc != RKNN_SUCC) {
            std::fprintf(stderr,
                "[rk3588] INPUT_ATTR[%u] failed: %d\n", i, rc);
            rknn_destroy(ctx_); ctx_ = 0;
            return false;
        }
    }
    if (n_input_ >= 1) {
        const auto& a = input_attrs_[0];
        model_is_nhwc_ = (a.fmt == RKNN_TENSOR_NHWC);
        if (model_is_nhwc_) {
            model_in_h_ = a.dims[1];
            model_in_w_ = a.dims[2];
        } else {
            model_in_h_ = a.dims[2];
            model_in_w_ = a.dims[3];
        }
        rgb_input_buf_.resize(
            static_cast<size_t>(model_in_w_) *
            static_cast<size_t>(model_in_h_) * 3);
    }

    // 5. Output attrs (3 heads for YOLOv5)
    output_attrs_.resize(n_output_);
    for (uint32_t i = 0; i < n_output_; ++i) {
        output_attrs_[i].index = i;
        rc = rknn_query(ctx_, RKNN_QUERY_OUTPUT_ATTR,
                        &output_attrs_[i], sizeof(rknn_tensor_attr));
        if (rc != RKNN_SUCC) {
            std::fprintf(stderr,
                "[rk3588] OUTPUT_ATTR[%u] failed: %d\n", i, rc);
            rknn_destroy(ctx_); ctx_ = 0;
            return false;
        }
    }

    ready_.store(true);
    std::fprintf(stderr,
        "[rk3588] initialized: model=%dx%d %s, n_input=%u n_output=%u\n",
        model_in_w_, model_in_h_,
        model_is_nhwc_ ? "NHWC" : "NCHW", n_input_, n_output_);
    return true;
}

// ─── infer() ───────────────────────────────────────────────────────────

std::vector<Detection> RK3588NPUBackend::infer(const cv::Mat& frame) {
    if (!ready_.load() || frame.empty() || ctx_ == 0) return {};
    std::lock_guard<std::mutex> lock(infer_mutex_);

    const auto t0 = std::chrono::steady_clock::now();

    // 1. Resize BGR -> letterbox to model input. Airys uses RGA for
    //    zero-copy NV12->RGB; here we use OpenCV (CPU) because the
    //    HumanDetectorNode receives decoded BGR from cv::imdecode.
    //    On RK3588 with rga_processor available, swap this for
    //    rga->convert (Airys path2-m, ~3-5ms saving).
    cv::Mat resized;
    if (frame.cols != model_in_w_ || frame.rows != model_in_h_) {
        cv::resize(frame, resized,
                   cv::Size(model_in_w_, model_in_h_), 0, 0,
                   cv::INTER_LINEAR);
    } else {
        resized = frame;
    }

    // 2. BGR -> RGB (RKNN YOLO models expect RGB888 NHWC UINT8).
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    // 3. Bind input tensor.
    const size_t rgb_size =
        static_cast<size_t>(model_in_w_) *
        static_cast<size_t>(model_in_h_) * 3;
    if (rgb_input_buf_.size() < rgb_size) {
        rgb_input_buf_.resize(rgb_size);
    }
    if (rgb.isContinuous()) {
        std::memcpy(rgb_input_buf_.data(), rgb.data, rgb_size);
    } else {
        // Pack row by row (rare; OpenCV resize usually returns continuous).
        for (int y = 0; y < rgb.rows; ++y) {
            std::memcpy(rgb_input_buf_.data() + y * rgb.cols * 3,
                        rgb.ptr<uint8_t>(y),
                        static_cast<size_t>(rgb.cols) * 3);
        }
    }

    rknn_input inputs[1] = {};
    inputs[0].index = 0;
    inputs[0].type  = RKNN_TENSOR_UINT8;
    inputs[0].fmt   = model_is_nhwc_ ? RKNN_TENSOR_NHWC : RKNN_TENSOR_NCHW;
    inputs[0].buf   = rgb_input_buf_.data();
    inputs[0].size  = static_cast<uint32_t>(rgb_size);
    inputs[0].pass_through = 0;

    int rc = rknn_inputs_set(ctx_, 1, inputs);
    if (rc != RKNN_SUCC) {
        std::fprintf(stderr, "[rk3588] inputs_set failed: %d\n", rc);
        return {};
    }

    // 4. Run inference.
    rc = rknn_run(ctx_, nullptr);
    if (rc != RKNN_SUCC) {
        std::fprintf(stderr, "[rk3588] rknn_run failed: %d\n", rc);
        return {};
    }

    // 5. Get outputs (want_float=1; INT8 quant handled inside librknnrt).
    std::vector<rknn_output> outputs(n_output_);
    for (uint32_t i = 0; i < n_output_; ++i) {
        outputs[i].index       = i;
        outputs[i].is_prealloc = 0;
        outputs[i].want_float  = 1;
    }
    rc = rknn_outputs_get(ctx_, n_output_, outputs.data(), nullptr);
    if (rc != RKNN_SUCC) {
        std::fprintf(stderr, "[rk3588] outputs_get failed: %d\n", rc);
        return {};
    }

    // 6. YOLOv5 decode (Airys-faithful).
    std::vector<Detection> raw;
    raw.reserve(64);
    const int model_w = model_in_w_;
    const int model_h = model_in_h_;
    const int max_heads = std::min<int>(n_output_, YOLO_NUM_HEADS);

    for (int h = 0; h < max_heads; ++h) {
        const auto& out_attr = output_attrs_[h];
        const float* data = static_cast<float*>(outputs[h].buf);
        if (!data) continue;

        const int stride = yolov5_strides[h];
        const int grid_h = model_h / stride;
        const int grid_w = model_w / stride;
        const int chan = (out_attr.fmt == RKNN_TENSOR_NHWC)
                            ? out_attr.dims[3] : out_attr.dims[1];

        // [DCN-2026-006 EXT D-019] Validate the head's channel layout
        // against our anchor table. Three failure modes are guarded
        // (see postprocess_helpers.hpp for the catalog and rationale).
        // We log once per (head, run) when an error fires, then skip
        // the head so the decode loop never reads out-of-bounds and
        // never produces phantom detections.
        using human_detector::postprocess::validateYoloHead;
        using human_detector::postprocess::kOk;
        using human_detector::postprocess::kErrChanNotDivisible;
        using human_detector::postprocess::kErrNonPositiveClasses;
        using human_detector::postprocess::kErrTooManyClasses;
        const auto v = validateYoloHead(chan, YOLO_NUM_ANCHORS_PER_HEAD);
        if (v != kOk) {
            const char* reason =
                v == kErrChanNotDivisible   ? "chan not divisible by num_anchors" :
                v == kErrNonPositiveClasses ? "n_classes <= 0"                    :
                v == kErrTooManyClasses     ? "n_classes > 80 (wrong model?)"     :
                                              "unknown";
            std::fprintf(stderr,
                "[human_detector][D-019] head %d: chan=%d "
                "num_anchors=%d → %s; skipping head\n",
                h, chan, YOLO_NUM_ANCHORS_PER_HEAD, reason);
            continue;
        }
        const int n_per_anchor = chan / YOLO_NUM_ANCHORS_PER_HEAD;
        const int n_classes = n_per_anchor - 5;

        const int sa = grid_h * grid_w;
        for (int a = 0; a < YOLO_NUM_ANCHORS_PER_HEAD; ++a) {
            for (int gy = 0; gy < grid_h; ++gy) {
                for (int gx = 0; gx < grid_w; ++gx) {
                    const int base = (a * n_per_anchor) * sa + gy * grid_w + gx;

                    const float tx  = data[base + 0 * sa];
                    const float ty  = data[base + 1 * sa];
                    const float tw  = data[base + 2 * sa];
                    const float th  = data[base + 3 * sa];
                    const float obj = fast_sigmoid(data[base + 4 * sa]);
                    if (obj < conf_threshold_) continue;

                    int best_cls = 0;
                    float best_score = 0.0f;
                    for (int c = 0; c < n_classes; ++c) {
                        const float s = fast_sigmoid(data[base + (5 + c) * sa]);
                        if (s > best_score) {
                            best_score = s;
                            best_cls   = c;
                        }
                    }
                    const float conf = obj * best_score;
                    if (conf < conf_threshold_) continue;

                    const float cx = (fast_sigmoid(tx) * 2.0f - 0.5f + gx) * stride;
                    const float cy = (fast_sigmoid(ty) * 2.0f - 0.5f + gy) * stride;
                    const float bw = std::pow(fast_sigmoid(tw) * 2.0f, 2)
                                     * yolov5_anchors[h][a][0];
                    const float bh = std::pow(fast_sigmoid(th) * 2.0f, 2)
                                     * yolov5_anchors[h][a][1];

                    Detection d{};
                    d.class_id   = best_cls;
                    d.confidence = conf;
                    d.label      = cocoLabel(best_cls);
                    // bbox = [x1, y1, x2, y2] in MODEL input pixels.
                    // HumanDetectorNode is responsible for scaling back
                    // to source resolution if needed; for v1.5.1 we
                    // emit in source-frame coords directly when the
                    // caller passed a model-sized frame, otherwise we
                    // rescale below.
                    float x1 = cx - bw * 0.5f;
                    float y1 = cy - bh * 0.5f;
                    float x2 = cx + bw * 0.5f;
                    float y2 = cy + bh * 0.5f;
                    if (x1 < 0)        x1 = 0;
                    if (y1 < 0)        y1 = 0;
                    if (x2 > model_w)  x2 = static_cast<float>(model_w);
                    if (y2 > model_h)  y2 = static_cast<float>(model_h);

                    // Rescale bbox to ORIGINAL frame coordinates if
                    // we resized. The HumanDetectorNode wants source-
                    // image bbox per IDS §5.21.
                    if (frame.cols != model_w || frame.rows != model_h) {
                        const float sx =
                            static_cast<float>(frame.cols) / model_w;
                        const float sy =
                            static_cast<float>(frame.rows) / model_h;
                        x1 *= sx; y1 *= sy; x2 *= sx; y2 *= sy;
                    }
                    d.bbox = {x1, y1, x2, y2};
                    raw.push_back(d);
                }
            }
        }
    }

    rknn_outputs_release(ctx_, n_output_, outputs.data());

    // 7. NMS (IoU 0.45 by default).
    apply_nms(raw, nms_iou_threshold_);

    // 8. Latency EMA.
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    latency_ema_ms_.store(updateEma(latency_ema_ms_.load(), ms, kLatencyEmaAlpha));

    return raw;
}

#else  // HAVE_RKNN

// Host build: RKNN runtime absent -> initialize fails, factory falls
// back to stub. infer() never gets called because ready_ stays false.

bool RK3588NPUBackend::initialize(const std::string& /*model_path*/) {
    return false;
}

std::vector<Detection> RK3588NPUBackend::infer(const cv::Mat&) {
    return {};
}

#endif  // HAVE_RKNN

}  // namespace human_detector
