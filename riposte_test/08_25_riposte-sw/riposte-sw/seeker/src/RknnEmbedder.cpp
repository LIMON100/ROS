#include "RknnEmbedder.h"

#include "AssocCost.h" // Embedding
#include "IDetector.h" // Frame, Detection
#include "Preproc.h"
#include "SearchScheduler.h" // crop_nv12

#include <rknn_api.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <ios>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "riposte/Log.h"
#include "riposte/Tunables.h"

// This translation unit is the ONLY place the RKNN runtime is referenced. It
// is compiled only when RIPOSTE_WITH_RKNN=ON (see CMakeLists.txt).
//
// Everything testable lives outside: the crop (crop_nv12, SearchScheduler
// tests), the stretch resize (Preproc, test_preproc), the normalization and
// all distance/matching maths (AssocCost, test_assoc). What remains here is
// the device: loading the .rknn, querying its I/O shape, and moving one crop
// through the NPU per detection.
//
// Failure containment (TR-3): a single bad crop yields ONE invalid embedding
// (that pairing degrades to motion-only); only a device-level fault fails the
// whole call, and consecutive faults drop healthy() — which the fusion layer
// treats as "run without T1", never as a tracking failure.

namespace riposte {

namespace {
// Returns a driver output buffer no matter how the scope is left, including by
// exception from the vector allocation that follows (review CR-05). The driver
// pool is small; a leak per detection would exhaust the NPU while every
// individual embedding still looked fine.
struct OutputRelease {
    rknn_context ctx;
    rknn_output* out;
    OutputRelease(const OutputRelease&) = delete;
    OutputRelease& operator=(const OutputRelease&) = delete;
    OutputRelease(OutputRelease&&) = delete;
    OutputRelease& operator=(OutputRelease&&) = delete;
    ~OutputRelease() { (void)rknn_outputs_release(ctx, 1, out); }
};
} // namespace

struct RknnEmbedder::Impl {
    RknnEmbedder::Params params;
    bool ready = false;
    int consec_faults = 0;
    rknn_context ctx = 0;
    int in_w = 0;
    int in_h = 0;
    std::size_t out_elems = 0;
    std::vector<uint8_t> model;   // .rknn bytes (must outlive rknn_init per API)
    std::vector<uint8_t> input;   // in_w*in_h*3 RGB888, reused
    std::vector<uint8_t> scratch; // NV12 crop scratch, reused
};

RknnEmbedder::RknnEmbedder(Params p) : impl_(std::make_unique<Impl>()) {
    impl_->params = std::move(p);
}

RknnEmbedder::~RknnEmbedder() {
    if (impl_ && impl_->ctx != 0) {
        rknn_destroy(impl_->ctx);
    }
}

bool RknnEmbedder::init_impl() {
    std::ifstream f(impl_->params.model_path, std::ios::binary | std::ios::ate);
    if (!f) {
        RLOG_ERROR("rknn", "cannot open model '%s'", impl_->params.model_path.c_str());
        return false;
    }
    // tellg() returns -1 on failure; casting that straight to size_t asks for a
    // ~16 EiB allocation (review CR-05). The ceiling is equally load-bearing:
    // rknn_init takes a uint32_t length, so a model larger than that would be
    // silently truncated into a "valid" shorter blob.
    const std::streamoff raw_size = f.tellg();
    constexpr std::streamoff MAX_MODEL_BYTES = 512LL * 1024 * 1024;
    if (raw_size <= 0 || raw_size > MAX_MODEL_BYTES) {
        RLOG_ERROR("rknn", "model '%s' has an unusable size",
                   impl_->params.model_path.c_str());
        return false;
    }
    const auto size = static_cast<std::size_t>(raw_size);
    impl_->model.resize(size);
    f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(impl_->model.data()),
                static_cast<std::streamsize>(size))) {
        return false;
    }

    if (rknn_init(&impl_->ctx, impl_->model.data(), static_cast<uint32_t>(size), 0,
                  nullptr) != RKNN_SUCC) {
        RLOG_ERROR("rknn", "rknn_init failed for '%s'", impl_->params.model_path.c_str());
        return false;
    }
    if (impl_->params.core >= 0 && impl_->params.core <= 2) {
        // Core pinning (TRACKER-REQ §5). Best effort: failure means the driver
        // schedules freely, which is a performance concern, not a fault.
        const rknn_core_mask masks[3] = {RKNN_NPU_CORE_0, RKNN_NPU_CORE_1,
                                         RKNN_NPU_CORE_2};
        if (rknn_set_core_mask(impl_->ctx, masks[impl_->params.core]) != RKNN_SUCC) {
            RLOG_WARN("rknn", "core pin %d refused; driver schedules freely",
                      impl_->params.core);
        }
    }

    // The model is the truth for the crop size, as the HEF is for the detector.
    rknn_input_output_num io{};
    if (rknn_query(impl_->ctx, RKNN_QUERY_IN_OUT_NUM, &io, sizeof(io)) != RKNN_SUCC ||
        io.n_input != 1 || io.n_output != 1) {
        RLOG_ERROR("rknn", "expected 1 input / 1 output");
        return false;
    }
    rknn_tensor_attr in_attr{};
    in_attr.index = 0;
    if (rknn_query(impl_->ctx, RKNN_QUERY_INPUT_ATTR, &in_attr, sizeof(in_attr)) !=
        RKNN_SUCC) {
        return false;
    }
    // NHWC uint8 expected: dims = [1, H, W, C]. Fail closed on anything else —
    // a silent mismatch would embed garbage that LOOKS like appearance.
    if (in_attr.n_dims != 4 || in_attr.dims[3] != 3 || in_attr.fmt != RKNN_TENSOR_NHWC) {
        RLOG_ERROR("rknn", "input is not NHWC RGB888");
        return false;
    }
    impl_->in_h = static_cast<int>(in_attr.dims[1]);
    impl_->in_w = static_cast<int>(in_attr.dims[2]);
    rknn_tensor_attr out_attr{};
    out_attr.index = 0;
    if (rknn_query(impl_->ctx, RKNN_QUERY_OUTPUT_ATTR, &out_attr, sizeof(out_attr)) !=
        RKNN_SUCC) {
        return false;
    }
    impl_->out_elems = out_attr.n_elems;
    // Model metadata is device-supplied input, so it gets ceilings like any
    // other (CR-05): without them a corrupt attr turns the input allocation
    // below into an overflowing product, and out_elems into an unbounded read
    // in embed().
    constexpr int MAX_SIDE = 4096;
    constexpr std::size_t MAX_EMBED_ELEMS = 65536;
    if (impl_->out_elems == 0 || impl_->out_elems > MAX_EMBED_ELEMS || impl_->in_w <= 0 ||
        impl_->in_h <= 0 || impl_->in_w > MAX_SIDE || impl_->in_h > MAX_SIDE) {
        RLOG_ERROR("rknn", "implausible tensor geometry %dx%d, %zu out elems",
                   impl_->in_w, impl_->in_h, impl_->out_elems);
        return false;
    }
    impl_->input.resize(static_cast<std::size_t>(impl_->in_w) *
                        static_cast<std::size_t>(impl_->in_h) * 3U);

    RLOG_INFO("rknn", "init model=%s input=%dx%d embed_dim=%zu core=%d",
              impl_->params.model_path.c_str(), impl_->in_w, impl_->in_h,
              impl_->out_elems, impl_->params.core);
    impl_->ready = true;
    return true;
}

bool RknnEmbedder::embed_impl(const Frame& f, const std::vector<Detection>& dets,
                              std::vector<Embedding>& out) {
    out.clear();
    if (!impl_->ready) {
        return false;
    }
    out.resize(dets.size()); // all invalid until individually filled
    bool device_fault = false;

    // Per-frame cap (P1-10, TR-4 partial): each embedding is a SYNCHRONOUS
    // NPU round-trip on the pipeline thread, so a crowded frame could issue
    // dozens of them. Embed only the top-K detections by score — the tracker
    // holds at most that many tracks anyway — and leave the rest invalid
    // (motion-only, TR-3). The blocking call itself moves to a deadline
    // worker with the TR-4 InferThread split at bring-up.
    std::vector<std::size_t> order(dets.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    const auto cap = static_cast<std::size_t>(tun::REID_EMBED_MAX_PER_FRAME);
    if (order.size() > cap) {
        std::nth_element(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(cap),
                         order.end(), [&dets](std::size_t a, std::size_t b) {
                             return dets[a].score > dets[b].score;
                         });
        order.resize(cap);
    }

    for (const std::size_t i : order) {
        const Detection& d = dets[i];
        // Detection centre/size -> top-left ROI in the detection's own frame.
        Roi roi;
        roi.x = d.cx - (d.w * 0.5F);
        roi.y = d.cy - (d.h * 0.5F);
        roi.w = d.w;
        roi.h = d.h;
        Frame crop{};
        Roi used;
        if (!crop_nv12(f, roi, impl_->scratch, crop, used)) {
            continue; // degenerate box: this one stays invalid (motion-only)
        }
        if (!resize_nv12_rgb888(crop, impl_->input.data(), impl_->input.size(),
                                impl_->in_w, impl_->in_h)) {
            continue;
        }

        rknn_input in{};
        in.index = 0;
        in.type = RKNN_TENSOR_UINT8;
        in.fmt = RKNN_TENSOR_NHWC;
        in.size = static_cast<uint32_t>(impl_->input.size());
        in.buf = impl_->input.data();
        if (rknn_inputs_set(impl_->ctx, 1, &in) != RKNN_SUCC ||
            rknn_run(impl_->ctx, nullptr) != RKNN_SUCC) {
            device_fault = true;
            break;
        }
        rknn_output o{};
        o.index = 0;
        o.want_float = 1; // runtime dequantizes; AssocCost sees plain float
        if (rknn_outputs_get(impl_->ctx, 1, &o, nullptr) != RKNN_SUCC) {
            device_fault = true;
            break;
        }
        // Release is owed from here on, including down the throwing paths below
        // (assign allocates). Leaking a driver buffer per detection would starve
        // the NPU long before anyone noticed a wrong embedding (CR-05).
        const OutputRelease release_guard{impl_->ctx, &o};
        // Trust the returned SIZE, not the metadata: a short buffer read as
        // out_elems floats is an out-of-bounds read that yields plausible
        // garbage — appearance evidence invented from adjacent memory.
        const std::size_t want_bytes = impl_->out_elems * sizeof(float);
        if (o.buf == nullptr || static_cast<std::size_t>(o.size) < want_bytes) {
            device_fault = true;
            break;
        }
        Embedding& e = out[i];
        const auto* fp = reinterpret_cast<const float*>(o.buf);
        e.v.assign(fp, fp + impl_->out_elems);
        if (!l2_normalize(e.v)) {
            e.v.clear(); // NaN/zero output: not appearance evidence
        }
    }

    if (device_fault) {
        ++impl_->consec_faults;
        out.clear();
        return false; // frame proceeds motion-only (TR-3)
    }
    impl_->consec_faults = 0;
    return true;
}

bool RknnEmbedder::healthy() const {
    return impl_ && impl_->ready && impl_->consec_faults < tun::HAILO_MAX_CONSEC_FAULTS;
}

// Vendor calls and the allocations around them are wrapped here (CR-05): an
// exception escaping into EmbedWorker's thread would be std::terminate, turning
// "the optional T1 layer failed" into "the seeker died" — the exact inversion of
// TR-3, which says a T1 fault degrades association to motion-only.
bool RknnEmbedder::init() {
    try {
        return init_impl();
    } catch (const std::exception& e) {
        RLOG_ERROR("rknn", "init threw (%s); T1 unavailable, motion-only", e.what());
    } catch (...) {
        RLOG_ERROR("rknn", "init threw; T1 unavailable, motion-only");
    }
    return false;
}

bool RknnEmbedder::embed(const Frame& f, const std::vector<Detection>& dets,
                         std::vector<Embedding>& out) {
    try {
        return embed_impl(f, dets, out);
    } catch (const std::exception& e) {
        RLOG_WARN("rknn", "embed threw (%s); frame runs motion-only", e.what());
    } catch (...) {
        RLOG_WARN("rknn", "embed threw; frame runs motion-only");
    }
    out.clear();
    return false;
}

} // namespace riposte
