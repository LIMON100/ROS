#include "HailoDetector.h"

#include "HailoNmsParse.h"
#include "ModelIo.h"
#include "Preproc.h"

#include <hailo/hailort.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <future>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "riposte/Log.h"
#include "riposte/Tunables.h"
#if defined(__unix__)
#include <sys/mman.h>
#endif

// This translation unit is the ONLY place HailoRT is referenced. It is compiled
// only when RIPOSTE_WITH_HAILO=ON (see CMakeLists.txt).
//
// The call sequence and the defensive patterns (page-aligned buffers, staging
// lifetime across a timed-out job, async->sync promise bridge, exception
// containment) follow the in-house lineage validated on Hailo-8 M.2 hardware:
// SkyHunter-1.0 human_detector -> the predecessor Riposte detector_hailo.cpp.
// What is deliberately NOT carried over from that lineage: the stretch-resize
// preprocessing (S-6 demands the letterbox — Preproc), OpenCV (S-9), and the
// 1-based class renumbering (class ids stay 0-based end to end).
//
// Everything decodable is decoded OUTSIDE this file and host-tested:
// parse_nms_by_class (HailoNmsParse.h, the S-7 primary path), decode_yolov8 +
// nms (ModelIo, the raw-head fallback), letterbox_undo (the common tail), and
// the NV12->RGB888 letterbox itself (Preproc). What remains here is only what
// no host test can stand in for: the device.

namespace riposte {

namespace {

// Page-aligned buffer (HailoRT DMA requirement), freed via shared_ptr deleter.
std::shared_ptr<uint8_t> page_aligned_alloc(std::size_t size) {
#if defined(__unix__)
    void* addr =
        mmap(nullptr, size, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == MAP_FAILED) {
        throw std::bad_alloc();
    }
    return std::shared_ptr<uint8_t>(reinterpret_cast<uint8_t*>(addr),
                                    [size](void* p) { munmap(p, size); });
#else
    throw std::runtime_error("page_aligned_alloc unsupported");
#endif
}

} // namespace

struct HailoDetector::Impl {
    HailoDetector::Params params;
    bool ready = false;

    // Which post-processing the loaded HEF calls for — decided once at init()
    // from the output stream's format order (S-7).
    enum class OutputMode : uint8_t { kNmsByClass, kRawHead };
    OutputMode mode = OutputMode::kNmsByClass;

    int num_anchors = 0;         // raw-head fallback only, from the HEF at init()
    int consec_faults = 0;       // consecutive detect() failures -> healthy() gate
    std::vector<Detection> work; // decode scratch, reused across frames

    // Runtime owner (S-10): every completion callback captures this shared_ptr,
    // so the HailoRT runtime outlives the last outstanding (timed-out) job no
    // matter when the detector itself is destroyed. Members destroy in reverse
    // declaration order — configured first, then infer_model, then vdevice —
    // which is HailoRT's required teardown order.
    struct DeviceCtx {
        std::unique_ptr<hailort::VDevice> vdevice;
        std::shared_ptr<hailort::InferModel> infer_model;
        hailort::ConfiguredInferModel configured;
    };
    std::shared_ptr<DeviceCtx> dev;

    // Jobs still in flight (incremented before run_async, decremented by the
    // completion callback). shared_ptr-owned so a late callback can decrement
    // it safely even after the detector object is gone.
    std::shared_ptr<std::atomic<int>> outstanding = std::make_shared<std::atomic<int>>(0);

    std::string in_name;
    std::string out_name;
    std::size_t in_frame_size = 0;
    std::size_t out_frame_size = 0;

    bool fault() {
        ++consec_faults;
        return false;
    }
};

HailoDetector::HailoDetector(Params p) : impl_(std::make_unique<Impl>()) {
    impl_->params = std::move(p);
}

HailoDetector::~HailoDetector() {
    // Bounded drain (S-10): give timed-out jobs a chance to complete for an
    // orderly shutdown. Proceeding with jobs still in flight is SAFE — their
    // callbacks own shared_ptr guards on the runtime and every buffer they
    // touch — the wait only avoids doing device teardown work concurrently
    // with a completing job when a short sleep would have separated them.
    if (!impl_ || impl_->outstanding->load() == 0) {
        return;
    }
    constexpr auto kDrain = std::chrono::nanoseconds(2ULL * tun::INFER_TIMEOUT_NS);
    const auto t0 = std::chrono::steady_clock::now();
    while (impl_->outstanding->load() > 0 &&
           (std::chrono::steady_clock::now() - t0) < kDrain) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const int left = impl_->outstanding->load();
    if (left > 0) {
        RLOG_WARN("hailo",
                  "%d job(s) still in flight at destruction — runtime kept "
                  "alive by their callbacks (S-10)",
                  left);
    }
}

bool HailoDetector::init() {
    try {
        impl_->dev = std::make_shared<Impl::DeviceCtx>();
        auto vd = hailort::VDevice::create();
        if (!vd) {
            RLOG_ERROR("hailo", "VDevice::create failed: %d",
                       static_cast<int>(vd.status()));
            return false;
        }
        impl_->dev->vdevice = vd.release();

        auto im = impl_->dev->vdevice->create_infer_model(impl_->params.hef_path);
        if (!im) {
            RLOG_ERROR("hailo", "create_infer_model('%s') failed: %d",
                       impl_->params.hef_path.c_str(), static_cast<int>(im.status()));
            return false;
        }
        impl_->dev->infer_model = im.release();

        // ---- Input: demand exactly the contract Preproc produces -----------
        // (square RGB888/uint8). The config's model_size was only the pre-load
        // expectation; the HEF is the truth. A silent mismatch would mis-scale
        // every box, so anything unexpected fails init instead (fail closed).
        const auto in_names = impl_->dev->infer_model->get_input_names();
        if (in_names.size() != 1) {
            RLOG_ERROR("hailo", "expected 1 input, hef has %zu", in_names.size());
            return false;
        }
        impl_->in_name = in_names.front();
        auto in_stream = impl_->dev->infer_model->input(impl_->in_name);
        if (!in_stream) {
            return false;
        }
        const auto in_shape = in_stream->shape();
        if (in_shape.width != in_shape.height || in_shape.features != 3) {
            RLOG_ERROR("hailo", "input %ux%ux%u is not square RGB", in_shape.width,
                       in_shape.height, in_shape.features);
            return false;
        }
        impl_->params.model_size = static_cast<int>(in_shape.width);
        impl_->in_frame_size = in_stream->get_frame_size();
        const std::size_t expect = static_cast<std::size_t>(impl_->params.model_size) *
                                   static_cast<std::size_t>(impl_->params.model_size) *
                                   3U;
        if (impl_->in_frame_size != expect) {
            RLOG_ERROR("hailo", "input frame %zu B != RGB888 %zu B (uint8 expected)",
                       impl_->in_frame_size, expect);
            return false;
        }

        // ---- Output: S-7 mode split on the format order --------------------
        const auto out_names = impl_->dev->infer_model->get_output_names();
        if (out_names.size() != 1) {
            RLOG_ERROR("hailo", "expected 1 output, hef has %zu", out_names.size());
            return false;
        }
        impl_->out_name = out_names.front();
        auto out_stream = impl_->dev->infer_model->output(impl_->out_name);
        if (!out_stream) {
            return false;
        }
        // Covers the HAILO_NMS order family; a newer HailoRT may report
        // *_BY_CLASS as a distinct order — bring-up checklist §4.4.5 item 2.
        const auto out_order = out_stream->format().order;
        const bool nms_out = (out_order == HAILO_FORMAT_ORDER_HAILO_NMS ||
                            out_order == HAILO_FORMAT_ORDER_HAILO_NMS_BY_CLASS);
        if (nms_out) {
            impl_->mode = Impl::OutputMode::kNmsByClass;
            // Class count is the HEF's, not the config's expectation.
            auto nms_shape = out_stream->get_nms_shape();
            if (nms_shape) {
                impl_->params.num_classes =
                    static_cast<int>(nms_shape->number_of_classes);
            }
            // S-8: device threshold is only a floor; the operating threshold is
            // applied per frame on the host. If this HailoRT predates the
            // setter, compile the HEF with SCORE_THR_DEVICE instead (§4.4.5).
            out_stream->set_nms_score_threshold(tun::SCORE_THR_DEVICE);
        } else {
            impl_->mode = Impl::OutputMode::kRawHead;
            // Dequantized on the host side of the vstream, so decode_yolov8
            // reads plain float32.
            out_stream->set_format_type(HAILO_FORMAT_TYPE_FLOAT32);
        }

        // The HEF's class count may have replaced the config expectation just
        // above, so the configured target_class must be re-judged against the
        // count ACTUALLY LOADED. Startup validation checked it against the
        // config's model_classes only — deploying a single-class HEF with
        // target_class=1 would otherwise filter every parsed record forever
        // while detect() keeps returning true and healthy() stays true: a
        // permanently empty detector reporting clean, the exact silent
        // HEF/config mismatch this fail-closed init exists to refuse.
        if (impl_->params.target_class < 0 ||
            impl_->params.target_class >= impl_->params.num_classes) {
            RLOG_ERROR("hailo",
                       "target_class %d out of range for the loaded HEF "
                       "(%d classes) — config/HEF mismatch, failing init",
                       impl_->params.target_class, impl_->params.num_classes);
            return false;
        }

        auto cm = impl_->dev->infer_model->configure();
        if (!cm) {
            RLOG_ERROR("hailo", "configure failed: %d", static_cast<int>(cm.status()));
            return false;
        }
        impl_->dev->configured = cm.release();
        // Bindings are created PER JOB in detect() (S-10): a shared bindings
        // object would be mutated by the next call while a timed-out job is
        // still using it. Validate creation once here so a broken HEF fails
        // init, not the first frame.
        auto bd = impl_->dev->configured.create_bindings();
        if (!bd) {
            RLOG_ERROR("hailo", "create_bindings failed: %d",
                       static_cast<int>(bd.status()));
            return false;
        }

        // Frame size read AFTER the format decisions above so it reflects them.
        impl_->out_frame_size = out_stream->get_frame_size();
        if (impl_->mode == Impl::OutputMode::kRawHead) {
            // [4+nc, anchors] float32: derive the anchor count from the actual
            // tensor size. Non-divisible means the layout assumption is wrong
            // (per-scale branches?) — fail closed, decode would read garbage.
            const std::size_t elems = impl_->out_frame_size / sizeof(float);
            const std::size_t rows =
                static_cast<std::size_t>(4 + impl_->params.num_classes);
            if (rows == 0 || elems == 0 || (elems % rows) != 0) {
                RLOG_ERROR("hailo", "raw head %zu floats not divisible by %zu rows",
                           elems, rows);
                return false;
            }
            impl_->num_anchors = static_cast<int>(elems / rows);
        }

        RLOG_INFO("hailo", "init hef=%s input=%dx%d classes=%d mode=%s",
                  impl_->params.hef_path.c_str(), impl_->params.model_size,
                  impl_->params.model_size, impl_->params.num_classes,
                  nms_out ? "nms-by-class" : "raw-head");
        impl_->ready = true;
        return true;
    } catch (const std::exception& e) {
        // HailoRT exceptions stop at this boundary: an init throw is a failed
        // detector, not a dead seeker process.
        RLOG_ERROR("hailo", "init exception: %s", e.what());
        return false;
    } catch (...) {
        // The vendor runtime is not bound to std::exception; a non-std throw
        // escaping here would terminate the process (the sibling RKNN
        // boundaries pair both handlers for the same reason).
        RLOG_ERROR("hailo", "init exception (non-std type)");
        return false;
    }
}

bool HailoDetector::detect(const Frame& f, std::vector<Detection>& out) {
    out.clear();
    if (!impl_->ready) {
        return false;
    }
    try {
        // Fit the frame (a whole image or one search tile) into the square
        // network input WITHOUT stretching (S-6): a stretch would distort every
        // bbox's width/height ratio, which is what the estimator turns into
        // range. model_size here is the HEF's actual edge from init().
        const Letterbox lb = letterbox_fit(f.width, f.height, impl_->params.model_size);
        if (!lb.valid()) {
            return impl_->fault();
        }
        // Per-JOB resources (S-10): input buffer, output buffer and bindings
        // all belong to this job and travel in its completion callback's
        // captures. A shared input staging once let the next detect() overwrite
        // memory a timed-out job's DMA was still reading (mixed-frame
        // inference), and a shared bindings object was mutated under an
        // in-flight job. Per-frame mmap/memset cost is a bring-up measurement
        // item (§4.4.5); a buffer pool is DEFERRED — correctness first.
        auto in_buf = page_aligned_alloc(impl_->in_frame_size);
        std::memset(in_buf.get(), tun::LETTERBOX_PAD_VALUE, impl_->in_frame_size);
        bool staged = false;
#ifdef RIPOSTE_WITH_RGA
        staged = letterbox_nv12_rgb888_rga(f, lb, in_buf.get(), impl_->in_frame_size);
#endif
        if (!staged) {
            staged = letterbox_nv12_rgb888(f, lb, in_buf.get(), impl_->in_frame_size);
        }
        if (!staged) {
            return impl_->fault();
        }

        auto bd = impl_->dev->configured.create_bindings();
        if (!bd) {
            return impl_->fault();
        }
        auto bindings =
            std::make_shared<hailort::ConfiguredInferModel::Bindings>(bd.release());
        if (bindings->input(impl_->in_name)
                ->set_buffer(hailort::MemoryView(in_buf.get(), impl_->in_frame_size)) !=
            HAILO_SUCCESS) {
            return impl_->fault();
        }
        auto out_buf = page_aligned_alloc(impl_->out_frame_size);
        if (bindings->output(impl_->out_name)
                ->set_buffer(hailort::MemoryView(out_buf.get(), impl_->out_frame_size)) !=
            HAILO_SUCCESS) {
            return impl_->fault();
        }

        constexpr auto kWait =
            std::chrono::milliseconds(tun::INFER_TIMEOUT_NS / 1'000'000ULL);
        if (impl_->dev->configured.wait_for_async_ready(kWait) != HAILO_SUCCESS) {
            return impl_->fault();
        }
        // Async->sync bridge: IDetector::detect() is a synchronous contract.
        // shared_ptr promise so a late callback after a timeout sets a value
        // nobody reads instead of touching a destroyed promise. The callback
        // additionally holds the job's buffers, its bindings, the DeviceCtx
        // (runtime owner) and the outstanding counter — everything a late
        // completion touches stays alive past detector destruction (S-10).
        auto prom = std::make_shared<std::promise<hailo_status>>();
        auto fut = prom->get_future();
        auto dev = impl_->dev;
        auto outstanding = impl_->outstanding;
        outstanding->fetch_add(1);
        auto job = dev->configured.run_async(
            *bindings, [prom, in_buf, out_buf, bindings, dev,
                        outstanding](const hailort::AsyncInferCompletionInfo& info) {
                prom->set_value(info.status);
                outstanding->fetch_sub(1);
            });
        if (!job) {
            outstanding->fetch_sub(1);
            return impl_->fault();
        }
        auto j = job.release();
        j.detach();
        if (fut.wait_for(kWait) != std::future_status::ready) {
            RLOG_WARN("hailo", "infer timeout (%d consecutive faults)",
                      impl_->consec_faults + 1);
            return impl_->fault(); // a fault, NOT an empty detection result
        }
        if (fut.get() != HAILO_SUCCESS) {
            return impl_->fault();
        }

        // --- Post-processing: host-tested code from here on -----------------
        if (impl_->mode == Impl::OutputMode::kNmsByClass) {
            // Device already suppressed duplicates; parse defends against the
            // buffer itself (untrusted device data).
            parse_nms_by_class(out_buf.get(), impl_->out_frame_size,
                               impl_->params.num_classes, tun::NMS_MAX_DETS, impl_->work);
        } else {
            decode_yolov8(reinterpret_cast<const float*>(out_buf.get()),
                          impl_->out_frame_size / sizeof(float),
                          impl_->params.num_classes, impl_->num_anchors,
                          impl_->params.model_size, impl_->params.score_threshold,
                          impl_->work);
            // Suppress duplicates in MODEL space, where the input is square and
            // IoU is measured consistently on both axes.
            nms(impl_->work, impl_->params.nms_iou);
        }
        for (auto& d : impl_->work) {
            if (d.cls != impl_->params.target_class) {
                continue;
            }
            // S-8 host operating threshold, inverted so a NaN score (compares
            // false both ways) is rejected rather than passed through.
            if (!(d.score >= impl_->params.score_threshold)) {
                continue;
            }
            // Back to frame-normalized coordinates; drops anything the network
            // found in the letterbox padding.
            if (letterbox_undo(lb, d)) {
                out.push_back(d);
            }
        }
        impl_->consec_faults = 0;
        return true;
    } catch (const std::exception& e) {
        // Containment boundary: an exception escaping into the pipeline loop
        // would kill the seeker process — a supervisor restart, not the local
        // fault this is supposed to be (SM-7 handles local faults).
        RLOG_ERROR("hailo", "detect exception: %s", e.what());
        return impl_->fault();
    } catch (...) {
        // Non-std throws from the vendor runtime get the SAME local-fault
        // treatment: the perception loop has no handler, so anything escaping
        // here is std::terminate — full perception loss instead of a fault
        // count and an eventual healthy() degradation.
        RLOG_ERROR("hailo", "detect exception (non-std type)");
        return impl_->fault();
    }
}

bool HailoDetector::healthy() const {
    return impl_ && impl_->ready && impl_->consec_faults < tun::HAILO_MAX_CONSEC_FAULTS;
}

} // namespace riposte
