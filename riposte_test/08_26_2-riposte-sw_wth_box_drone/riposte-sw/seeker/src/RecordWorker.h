#pragma once
#include "IDetector.h"     // IDetector, Frame, Detection
#include "Tracker.h"       // Tracker::Track
#include "VideoRecorder.h" // VideoRecorder, OverlayMeta

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "riposte/SeqSlot.h" // GpsBus reader
#include "riposte/Types.h"

namespace riposte {

// Dual-channel recording, run OFF the perception thread (P2-06). The narrow-EO
// overlay detection is a synchronous NPU call that could block the EO capture
// cadence; this worker owns the recorder, the narrow camera + detector and the
// GPS reader, and the perception thread only POSTS the latest EO frame + its
// primary track into a depth-1 latest-wins slot. If the worker falls behind,
// the pending frame is overwritten (a dropped recording frame, never a stalled
// loop) — the same latest-wins policy the whole seeker runs on.
//
// The posted EO frame is COPIED (its data is a pointer into the camera mmap
// buffer, valid only until the next grab): a ~1.3 MB memcpy on the perception
// thread is far cheaper than the detect() it replaces there.
//
// Single-channel recording does NOT use this worker — that path is a single
// non-blocking pipe write the perception loop can do inline.
class RecordWorker {
public:
    // `narrow_det` is optional: with the P4 perception pipeline on, the seeker
    // already inferred on the narrow frame and posts the result, so the worker
    // runs no detector at all. It is only used in the recording-only
    // configuration (record_dual without dual_eo), where nothing else looks at
    // the narrow channel.
    RecordWorker(std::unique_ptr<VideoRecorder> rec,
                 std::unique_ptr<IDetector> narrow_det, int target_cls);
    ~RecordWorker();

    RecordWorker(const RecordWorker&) = delete;
    RecordWorker& operator=(const RecordWorker&) = delete;
    RecordWorker(RecordWorker&&) = delete;
    RecordWorker& operator=(RecordWorker&&) = delete;

    void start();
    // Copy the wide frame, the paired narrow frame (may be empty) and the
    // overlay hints into the depth-1 slot, then wake the worker. Non-blocking.
    //
    // The narrow frame arrives from the perception loop rather than being
    // grabbed here (S-13): a V4L2 device cannot be opened twice, so once the
    // perception pipeline needs the narrow channel it must own the camera.
    // `narrow_valid`/`narrow_cx`/`narrow_cy` carry a detection the perception
    // loop already made on that frame; when they are absent and a detector was
    // supplied, the worker infers for the overlay itself.
    void post(const Frame& eo, const Frame& narrow, const Tracker::Track& primary,
              bool narrow_valid, float narrow_cx, float narrow_cy);
    void stop(); // signal + join; finalizes the recording

private:
    void run();

    std::unique_ptr<VideoRecorder> rec_;
    std::unique_ptr<IDetector> narrow_det_;
    int target_cls_;
    ShmSeqSlot<GpsSample> gps_bus_;

    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool run_ = false;
    bool have_pending_ = false;
    // Depth-1 pending frame (latest-wins). Owns its NV12 bytes; `frame_` points
    // into `bytes_`.
    std::vector<uint8_t> bytes_;
    Frame frame_{};
    // The paired narrow frame, owned the same way. `nar_frame_.data` is null
    // when the narrow channel produced nothing this tick.
    std::vector<uint8_t> nar_bytes_;
    Frame nar_frame_{};
    OverlayMeta overlay_{};
};

} // namespace riposte
