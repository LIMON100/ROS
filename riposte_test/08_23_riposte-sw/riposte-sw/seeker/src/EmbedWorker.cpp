#include "EmbedWorker.h"

#include "AssocCost.h" // Embedding
#include "IDetector.h" // Frame, Detection
#include "IEmbedder.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "riposte/Clock.h"
#include "riposte/Log.h"
#include "riposte/Tunables.h"

namespace riposte {

namespace {
// NV12 payload for a frame: stride carries the luma row length, chroma is a
// half-height interleaved plane. Same formula the recorder uses.
std::size_t nv12_bytes(const Frame& f) {
    return static_cast<std::size_t>(f.stride) * static_cast<std::size_t>(f.height) * 3U /
           2U;
}
} // namespace

// Records a frame that produced no T1 result, whatever the cause, and reports
// whether that just crossed the degradation threshold. Caller holds the mutex.
bool EmbedWorker::note_no_result(State& st) {
    ++st.consec_misses;
    if (!st.degraded && st.consec_misses >= tun::EMBED_DEADLINE_FAULTS) {
        st.degraded = true;
        return true;
    }
    return false;
}

EmbedWorker::EmbedWorker(std::unique_ptr<IEmbedder> emb, uint64_t deadline_ns)
    : st_(std::make_shared<State>()),
      deadline_ns_(deadline_ns),
      name_((emb != nullptr) ? emb->name() : "none") {
    st_->emb = std::move(emb);
}

EmbedWorker::~EmbedWorker() {
    {
        const std::lock_guard<std::mutex> lock(st_->mtx);
        st_->stop = true;
    }
    st_->work_cv.notify_all();
    if (!th_.joinable()) {
        return;
    }
    // A worker parked in the condition variable exits at once; one blocked
    // inside a driver call never will. Wait a bounded time for the former, then
    // detach — st_ is shared with the thread, so the state it touches stays
    // alive after this object is gone (SDD §4.5).
    const uint64_t deadline = mono_now_ns() + tun::EMBED_DRAIN_NS;
    bool done = false;
    while (mono_now_ns() < deadline) {
        {
            const std::lock_guard<std::mutex> lock(st_->mtx);
            done = !st_->busy;
        }
        if (done) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (done) {
        th_.join();
    } else {
        RLOG_WARN("seeker", "EmbedWorker stuck in %s; detaching at shutdown", name_);
        th_.detach();
    }
}

void EmbedWorker::start() {
    if (st_->emb == nullptr || th_.joinable()) {
        return;
    }
    auto st = st_;
    th_ = std::thread([st]() { run(st); });
}

void EmbedWorker::run(const std::shared_ptr<State>& st) {
    while (true) {
        Frame frame;
        std::vector<Detection> dets;
        uint64_t gen = 0;
        {
            std::unique_lock<std::mutex> lock(st->mtx);
            st->work_cv.wait(lock, [&st]() { return st->stop || st->busy; });
            if (st->stop) {
                return;
            }
            frame = st->in_frame;
            dets = st->in_dets;
            gen = st->gen;
        }

        // The embedder runs UNLOCKED: this is the call that can block forever,
        // and holding the mutex across it would make the perception thread's
        // deadline wait meaningless.
        //
        // It also runs inside a catch-all (review CR-05). An exception escaping
        // a std::thread is std::terminate — the whole seeker dies — which would
        // turn "the optional T1 layer failed" into "perception stopped",
        // exactly inverting TR-3. A throwing embedder is a device fault like any
        // other: report failure and let the frame associate motion-only.
        std::vector<Embedding> embs;
        bool ok = false;
        try {
            ok = st->emb->embed(frame, dets, embs);
        } catch (const std::exception& e) {
            RLOG_WARN("seeker", "embedder threw (%s); frame runs motion-only", e.what());
            ok = false;
        } catch (...) {
            RLOG_WARN("seeker", "embedder threw; frame runs motion-only");
            ok = false;
        }
        if (!ok) {
            embs.clear();
        }

        {
            const std::lock_guard<std::mutex> lock(st->mtx);
            st->out_gen = gen;
            st->out_ok = ok;
            st->out_embs = std::move(embs);
            st->busy = false;
        }
        st->result_cv.notify_one();
    }
}

bool EmbedWorker::embed_by_deadline(const Frame& f, const std::vector<Detection>& dets,
                                    std::vector<Embedding>& out) {
    out.clear();
    if (st_->emb == nullptr || !th_.joinable()) {
        return false;
    }
    const std::size_t bytes = nv12_bytes(f);
    if (f.data == nullptr || bytes == 0U) {
        return false; // nothing to copy; the frame associates motion-only
    }

    uint64_t gen = 0;
    bool just_degraded = false;
    bool ok = false;
    {
        std::unique_lock<std::mutex> lock(st_->mtx);
        if (st_->degraded) {
            return false; // motion-only for good; nothing left to measure
        }
        if (st_->busy) {
            // The worker still owns the previous frame. Posting would either
            // queue work (latency the guidance loop pays for) or race the
            // buffer it is reading, so this frame runs motion-only.
            //
            // It must count toward degradation: a worker wedged in the driver
            // call leaves EVERY later frame right here, so if only true
            // deadline expiries counted, the exact failure this component
            // exists for would never trip the fault threshold. (Found by the
            // TR-4 hung-embedder test, which is why that test drives a thread
            // instead of asserting on the policy in isolation.)
            just_degraded = note_no_result(*st_);
        } else {
            // Copy the pixels: f.data points into the camera mmap buffer, which
            // is reused on the next grab while the worker may still be reading.
            st_->in_bytes.assign(f.data, f.data + bytes);
            st_->in_frame = f;
            st_->in_frame.data = st_->in_bytes.data();
            st_->in_dets = dets;
            gen = ++st_->gen;
            st_->busy = true;
            st_->work_cv.notify_one();

            const auto wait_for = std::chrono::nanoseconds(deadline_ns_);
            const bool got = st_->result_cv.wait_for(
                lock, wait_for, [this, gen]() { return st_->out_gen == gen; });
            if (got) {
                if (st_->out_ok) {
                    out = st_->out_embs;
                }
                st_->consec_misses = 0;
                // A device-level failure is not a deadline miss: it already
                // means motion-only here, and the embedder reported it itself.
                ok = st_->out_ok;
            } else {
                // The worker keeps running and will publish under `gen`, which
                // nothing waits for any more; the next frame sees busy above.
                ++st_->misses;
                just_degraded = note_no_result(*st_);
            }
        }
    }
    if (just_degraded) {
        RLOG_WARN("seeker",
                  "%s produced no embedding for %d consecutive frames; "
                  "T1 is now motion-only (TR-3)",
                  name_, tun::EMBED_DEADLINE_FAULTS);
    }
    return ok;
}

bool EmbedWorker::degraded() const {
    const std::lock_guard<std::mutex> lock(st_->mtx);
    return st_->degraded;
}

uint64_t EmbedWorker::deadline_misses() const {
    const std::lock_guard<std::mutex> lock(st_->mtx);
    return st_->misses;
}

} // namespace riposte
