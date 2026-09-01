#pragma once
#include "AssocCost.h" // Embedding
#include "IDetector.h" // Frame, Detection
#include "IEmbedder.h"

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace riposte {

// T1 appearance embedding, run OFF the perception thread (TRACKER-REQ TR-4,
// SEEKER-SDD S-12 / §4.5).
//
// RKNN's rknn_run can block INDEFINITELY when the driver misbehaves (K-2).
// Called inline, that stops the whole 60 Hz pipeline — capture, tracking,
// TrackBus publication and therefore the OBC's guidance input. So the call
// moves to a worker thread and the perception thread waits only up to a
// deadline for the answer.
//
// Why a deadline worker and not a fire-and-forget one: association needs
// embeddings INDEX-ALIGNED to the CURRENT frame's detections. A result that
// arrives a frame late describes detections that no longer exist, so it cannot
// be used and there is nothing to gain from consuming it. The useful property
// is purely the bound on how long the perception thread can be held up.
//
// Behaviour, in the order the perception loop meets it:
//   - submit is always non-blocking; if the worker is still busy with an
//     earlier frame nothing is posted and the frame runs motion-only
//   - a result that lands within the deadline is byte-for-byte what the old
//     synchronous call produced
//   - a result that misses the deadline is dropped by generation number, and
//     the frame runs motion-only (T0 association, TR-3)
//   - EMBED_DEADLINE_FAULTS consecutive misses degrade the embedder for good:
//     no further submissions. There is deliberately NO recovery test — nothing
//     would justify exposing the 60 Hz loop again, and running motion-only is
//     not a failure (TR-3), it is the T0 baseline.
//
// The frame is COPIED on submit: Frame.data points into the camera's mmap
// buffer and is only valid until the next grab.
class EmbedWorker {
public:
    EmbedWorker(std::unique_ptr<IEmbedder> emb, uint64_t deadline_ns);
    ~EmbedWorker();

    EmbedWorker(const EmbedWorker&) = delete;
    EmbedWorker& operator=(const EmbedWorker&) = delete;
    EmbedWorker(EmbedWorker&&) = delete;
    EmbedWorker& operator=(EmbedWorker&&) = delete;

    void start();

    // Perception thread entry point. Returns true with `out` index-aligned to
    // `dets`; false means this frame associates motion-only and `out` is empty.
    bool embed_by_deadline(const Frame& f, const std::vector<Detection>& dets,
                           std::vector<Embedding>& out);

    bool degraded() const;
    uint64_t deadline_misses() const;
    const char* name() const { return name_; }

private:
    // Shared with the worker thread through a shared_ptr so that a worker stuck
    // in a driver call keeps the state alive after the owner is destroyed and
    // the thread is detached (same ownership reasoning as the HailoDetector
    // destructor, SDD §4.4.1 S-10).
    struct State {
        std::mutex mtx;
        std::condition_variable work_cv;   // worker waits for a submission
        std::condition_variable result_cv; // perception waits for a result

        std::unique_ptr<IEmbedder> emb;
        bool stop = false;
        bool busy = false; // a submission is in flight (worker owns `in_*`)

        // Submission slot (depth 1). `gen` identifies it; a result carrying a
        // different generation is stale and dropped.
        uint64_t gen = 0;
        Frame in_frame;
        std::vector<uint8_t> in_bytes; // owns what in_frame.data points at
        std::vector<Detection> in_dets;

        // Result slot, valid only when out_gen == gen.
        uint64_t out_gen = 0;
        bool out_ok = false;
        std::vector<Embedding> out_embs;

        uint64_t misses = 0;
        int consec_misses = 0;
        bool degraded = false;
    };

    static void run(const std::shared_ptr<State>& st);
    // Counts a frame that yielded no embedding — deadline expiry or a worker
    // still busy are the same thing to the degradation policy. Returns true if
    // this call is the one that degraded the embedder. Caller holds st.mtx.
    static bool note_no_result(State& st);

    std::shared_ptr<State> st_;
    uint64_t deadline_ns_;
    std::thread th_;
    const char* name_ = "none";
};

} // namespace riposte
