#include "RecordWorker.h"

#include "IDetector.h" // IDetector, Frame, Detection
#include "Tracker.h"   // Tracker::Track
#include "VideoRecorder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "riposte/Log.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"

namespace riposte {

namespace {

// Highest-scoring target-class detection centre (normalized); false if none.
bool best_detection(const std::vector<Detection>& dets, int target_cls, float& cx,
                    float& cy) {
    const Detection* best = nullptr;
    for (const auto& d : dets) {
        if (d.cls == target_cls && (best == nullptr || d.score > best->score)) {
            best = &d;
        }
    }
    if (best == nullptr) {
        return false;
    }
    cx = best->cx;
    cy = best->cy;
    return true;
}
} // namespace

RecordWorker::RecordWorker(std::unique_ptr<VideoRecorder> rec,
                           std::unique_ptr<IDetector> narrow_det, int target_cls)
    : rec_(std::move(rec)), narrow_det_(std::move(narrow_det)), target_cls_(target_cls) {}

RecordWorker::~RecordWorker() {
    stop();
}

void RecordWorker::start() {
    run_ = true;
    thread_ = std::thread([this] { run(); });
}

namespace {
// NV12 payload for a frame; stride carries the luma row length.
std::size_t nv12_bytes(const Frame& f) {
    return static_cast<std::size_t>(f.stride) * static_cast<std::size_t>(f.height) * 3U /
           2U;
}
} // namespace

void RecordWorker::post(const Frame& eo, const Frame& narrow,
                        const Tracker::Track& primary, bool narrow_valid, float narrow_cx,
                        float narrow_cy) {
    // Copy both frames: the camera mmap buffers are reused on the next grab.
    const std::size_t n = nv12_bytes(eo);
    const bool have_narrow = narrow.data != nullptr && nv12_bytes(narrow) != 0U;
    {
        const std::lock_guard<std::mutex> lock(mtx_);
        bytes_.assign(eo.data, eo.data + n);
        frame_ = eo;
        frame_.data = bytes_.data();
        if (have_narrow) {
            nar_bytes_.assign(narrow.data, narrow.data + nv12_bytes(narrow));
            nar_frame_ = narrow;
            nar_frame_.data = nar_bytes_.data();
        } else {
            nar_frame_ = Frame{}; // no pair this tick
        }
        overlay_ = OverlayMeta{};
        if (primary.valid) {
            overlay_.wide_valid = true;
            overlay_.wide_cx = primary.cx;
            overlay_.wide_cy = primary.cy;
        }
        // A detection the perception loop already made on the narrow frame
        // (S-13). When present the worker runs no detector of its own.
        if (narrow_valid) {
            overlay_.nar_valid = true;
            overlay_.nar_cx = narrow_cx;
            overlay_.nar_cy = narrow_cy;
        }
        have_pending_ = true; // latest-wins: overwrite any unprocessed frame
    }
    cv_.notify_one();
}

void RecordWorker::stop() {
    if (!thread_.joinable()) {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_);
        run_ = false;
    }
    cv_.notify_one();
    thread_.join();
    if (rec_) {
        rec_->close(); // finalize the recording from the owning thread
        // The dual path's failure signature is a SILENTLY empty recording
        // (every mismatched pair dropped with no log) — report the counters
        // the single-channel path already reports, dimension mismatches
        // called out separately.
        RLOG_INFO("recorder",
                  "dual recorder: %llu frames written, %llu dropped "
                  "(%llu dimension mismatches)",
                  static_cast<unsigned long long>(rec_->frames_written()),
                  static_cast<unsigned long long>(rec_->frames_dropped()),
                  static_cast<unsigned long long>(rec_->dim_mismatch()));
    }
}

void RecordWorker::run() {
    while (true) {
        Frame eo{};
        Frame narrow{};
        OverlayMeta meta{};
        std::vector<uint8_t> bytes;
        std::vector<uint8_t> nar_bytes;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return have_pending_ || !run_; });
            if (!run_ && !have_pending_) {
                return;
            }
            // Take the pending pair; move the bytes out so the poster can refill.
            bytes = std::move(bytes_);
            eo = frame_;
            eo.data = bytes.data();
            nar_bytes = std::move(nar_bytes_);
            narrow = nar_frame_;
            narrow.data = nar_bytes.empty() ? nullptr : nar_bytes.data();
            meta = overlay_;
            have_pending_ = false;
        }

        // Both frames come from the perception loop (S-13): a V4L2 device
        // cannot be opened twice, so the camera belongs to whoever infers on it.
        // Without a pair there is nothing to composite this tick.
        if (narrow.data == nullptr) {
            continue;
        }
        // Recording-only configuration: nothing else looked at this frame, so
        // the overlay box is inferred here. With the P4 pipeline on, the
        // perception loop already posted its detection and this is skipped —
        // one NPU call per frame instead of two.
        if (narrow_det_ != nullptr && !meta.nar_valid) {
            std::vector<Detection> dets;
            float cx = 0.F;
            float cy = 0.F;
            if (narrow_det_->detect(narrow, dets) &&
                best_detection(dets, target_cls_, cx, cy)) {
                meta.nar_valid = true;
                meta.nar_cx = cx;
                meta.nar_cy = cy;
            }
        }
        gps_bus_.ensure_open(tun::SHM_GPS, ShmSeqSlot<GpsSample>::Role::READER);
        GpsSample g{};
        if (gps_bus_.read(g) && g.fix_ok != 0U) {
            meta.gps_ok = true;
            meta.lat_deg = g.lat_deg;
            meta.lon_deg = g.lon_deg;
            meta.alt_m = g.alt_m;
        }
        rec_->write_dual(eo, narrow, meta, eo.mono_ns);
    }
}

} // namespace riposte
