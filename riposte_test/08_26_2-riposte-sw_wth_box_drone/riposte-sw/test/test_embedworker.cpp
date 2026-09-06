// EmbedWorker — T1 embedding isolation (TRACKER-REQ TR-4, SEEKER-SDD S-12/§4.5).
//
// The failure this component exists for cannot be reproduced by reading code:
// rknn_run blocking forever inside the perception thread. So the tests drive a
// real thread with embedder doubles that are slow, that hang, and that fail,
// and assert the properties the 60 Hz loop depends on:
//   (a) a healthy embedder is byte-for-byte the old synchronous path
//   (b) a slow one bounds the perception thread at the deadline, not the call
//   (c) a late result is never mixed into a later frame's association
//   (d) sustained misses degrade to motion-only and STOP submitting
//   (e) an embedder hung forever still lets the process shut down
#include "AssocCost.h" // Embedding
#include "EmbedWorker.h"
#include "IDetector.h" // Frame, Detection
#include "IEmbedder.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "riposte/Clock.h"
#include "riposte/Tunables.h"

using namespace riposte;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
int checks = 0;

#define CHECK(c)                                                    \
    do {                                                            \
        ++checks;                                                   \
        if (!(c)) {                                                 \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            return 1;                                               \
        }                                                           \
    } while (0)

// An embedder whose latency the test controls. `delay_ms` is applied inside
// embed(), exactly where a real driver call blocks. `hang` parks it until the
// test releases it, standing in for a wedged NPU.
class ScriptedEmbedder final : public IEmbedder {
public:
    std::atomic<int> delay_ms{0};
    std::atomic<bool> hang{false};
    std::atomic<bool> release{false};
    std::atomic<bool> fail{false};
    std::atomic<bool> throws{false};
    std::atomic<int> calls{0};

    bool init() override { return true; }

    bool embed(const Frame& /*f*/, const std::vector<Detection>& dets,
               std::vector<Embedding>& out) override {
        ++calls;
        if (hang.load()) {
            while (!release.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } else if (delay_ms.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms.load()));
        }
        if (throws.load()) {
            throw std::runtime_error("simulated vendor fault");
        }
        out.clear();
        if (fail.load()) {
            return false;
        }
        for (std::size_t i = 0; i < dets.size(); ++i) {
            Embedding e;
            // Value identifies WHICH detection this embedding came from, so a
            // stale result cannot masquerade as a fresh one.
            e.v.assign(4, static_cast<float>(dets[i].cls));
            out.push_back(std::move(e));
        }
        return true;
    }

    bool healthy() const override { return true; }
    const char* name() const override { return "ScriptedEmbedder"; }
};

// A frame backed by real pixels: EmbedWorker copies them, so a null-data frame
// would not exercise the copy path.
struct TestFrame {
    std::vector<uint8_t> pixels;
    Frame f;

    TestFrame() : pixels(64 * 64 * 3 / 2, 7U) {
        f.width = 64;
        f.height = 64;
        f.stride = 64;
        f.data = pixels.data();
        f.mono_ns = 1'000;
    }
};

std::vector<Detection> dets_with_class(int cls) {
    Detection d{};
    d.cls = cls;
    d.cx = 0.5F;
    d.cy = 0.5F;
    d.w = 0.1F;
    d.h = 0.1F;
    d.score = 0.9F;
    return {d};
}

double ms_since(uint64_t t0) {
    return static_cast<double>(mono_now_ns() - t0) / 1e6;
}

// (a) A healthy embedder returns the same embeddings the synchronous call did.
int test_fast_embedder_delivers_in_time() {
    auto emb = std::make_unique<ScriptedEmbedder>();
    EmbedWorker w(std::move(emb), 200'000'000ULL); // 200 ms: never the limit here
    w.start();

    const TestFrame tf;
    std::vector<Embedding> out;
    CHECK(w.embed_by_deadline(tf.f, dets_with_class(3), out));
    CHECK(out.size() == 1);
    CHECK(out[0].v.size() == 4);
    CHECK(out[0].v[0] == 3.F); // came from THIS frame's detection
    CHECK(!w.degraded());
    CHECK(w.deadline_misses() == 0);
    return 0;
}

// (b) The perception thread is bounded by the deadline, not by the call. This
// is the whole point of the component: a 300 ms driver stall must cost the
// 60 Hz loop its deadline, not 300 ms.
int test_slow_embedder_bounded_by_deadline() {
    auto emb = std::make_unique<ScriptedEmbedder>();
    emb->delay_ms.store(300);
    EmbedWorker w(std::move(emb), 10'000'000ULL); // 10 ms
    w.start();

    const TestFrame tf;
    std::vector<Embedding> out;
    const uint64_t t0 = mono_now_ns();
    const bool ok = w.embed_by_deadline(tf.f, dets_with_class(1), out);
    const double waited_ms = ms_since(t0);

    CHECK(!ok);               // motion-only for this frame
    CHECK(out.empty());       // and nothing half-formed handed to association
    CHECK(waited_ms < 100.0); // bounded by the deadline, not the 300 ms call
    CHECK(w.deadline_misses() == 1);
    return 0;
}

// (c) The result of a missed frame must never be mixed into a later frame.
// Detections differ per frame, so a stale embedding would describe boxes that
// no longer exist — silently corrupting association instead of degrading it.
int test_late_result_is_discarded() {
    auto owned = std::make_unique<ScriptedEmbedder>();
    auto* raw = owned.get();
    raw->delay_ms.store(60);
    EmbedWorker w(std::move(owned), 5'000'000ULL); // 5 ms
    w.start();

    const TestFrame tf;
    std::vector<Embedding> out;
    CHECK(!w.embed_by_deadline(tf.f, dets_with_class(1), out)); // misses

    // While the worker is still busy with frame 1, frame 2 must not be posted
    // (no queue growth) and must not receive frame 1's answer.
    CHECK(!w.embed_by_deadline(tf.f, dets_with_class(2), out));
    CHECK(out.empty());

    // Once the worker drains, a fresh frame gets ITS OWN embedding.
    raw->delay_ms.store(0);
    bool fresh = false;
    for (int i = 0; i < 200 && !fresh; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        fresh = w.embed_by_deadline(tf.f, dets_with_class(9), out);
    }
    CHECK(fresh);
    CHECK(out.size() == 1);
    CHECK(out[0].v[0] == 9.F); // frame 3's class, not frame 1's
    return 0;
}

// (d) Sustained misses degrade the embedder permanently AND stop submitting —
// a degraded worker that kept posting would keep paying the deadline every
// frame for an answer that never arrives.
int test_sustained_misses_degrade_and_stop_submitting() {
    auto owned = std::make_unique<ScriptedEmbedder>();
    auto* raw = owned.get();
    raw->hang.store(true);
    EmbedWorker w(std::move(owned), 1'000'000ULL); // 1 ms
    w.start();

    const TestFrame tf;
    std::vector<Embedding> out;
    for (int i = 0; i < tun::EMBED_DEADLINE_FAULTS + 5; ++i) {
        CHECK(!w.embed_by_deadline(tf.f, dets_with_class(1), out));
    }
    CHECK(w.degraded());

    // Only the FIRST frame ever reached the embedder: after that the worker was
    // busy, and past the fault threshold submission stops outright.
    CHECK(raw->calls.load() == 1);

    // Degraded is permanent by design (no recovery test): even a now-healthy
    // device stays motion-only.
    raw->release.store(true);
    raw->hang.store(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(!w.embed_by_deadline(tf.f, dets_with_class(1), out));
    CHECK(w.degraded());
    return 0;
}

// (e) Shutdown with a worker wedged inside the driver call must still complete.
// A join() here would hang the process forever; the destructor detaches after a
// bounded drain and the shared state keeps the thread's memory alive.
int test_destructor_survives_hung_embedder() {
    auto owned = std::make_unique<ScriptedEmbedder>();
    auto* raw = owned.get();
    raw->hang.store(true);
    const TestFrame tf;
    std::vector<Embedding> out;

    const uint64_t t0 = mono_now_ns();
    {
        EmbedWorker w(std::move(owned), 1'000'000ULL);
        w.start();
        CHECK(!w.embed_by_deadline(tf.f, dets_with_class(1), out));
    } // destructor runs here with the worker still inside embed()
    const double teardown_ms = ms_since(t0);

    CHECK(teardown_ms < 2000.0); // bounded drain, then detach — never a hang
    // Release the detached thread so the test process exits cleanly. The
    // shared state outlives the EmbedWorker precisely so this is safe.
    raw->release.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return 0;
}

// A device-level embed() failure is not a deadline miss: it already means
// motion-only for that frame, and treating it as a miss would degrade a
// perfectly responsive device.
int test_device_failure_is_not_a_deadline_miss() {
    auto owned = std::make_unique<ScriptedEmbedder>();
    auto* raw = owned.get();
    raw->fail.store(true);
    EmbedWorker w(std::move(owned), 200'000'000ULL);
    w.start();

    const TestFrame tf;
    std::vector<Embedding> out;
    CHECK(!w.embed_by_deadline(tf.f, dets_with_class(1), out));
    CHECK(out.empty());
    CHECK(w.deadline_misses() == 0);
    CHECK(!w.degraded());
    return 0;
}

// A worker with no embedder (built without RKNN) is a no-op, not a crash.
int test_null_embedder_is_motion_only() {
    EmbedWorker w(nullptr, 10'000'000ULL);
    w.start(); // must not spawn a thread

    const TestFrame tf;
    std::vector<Embedding> out;
    CHECK(!w.embed_by_deadline(tf.f, dets_with_class(1), out));
    CHECK(out.empty());
    return 0;
}

// (f) A throwing embedder must not take the process with it. An exception that
// escapes a std::thread is std::terminate, which would turn "the optional T1
// layer failed" into "the seeker died" — the exact inversion of TR-3 (review
// CR-05). The worker must absorb it and keep serving later frames.
int test_throwing_embedder_does_not_kill_the_process() {
    auto owned = std::make_unique<ScriptedEmbedder>();
    auto* raw = owned.get();
    raw->throws.store(true);
    EmbedWorker w(std::move(owned), 200'000'000ULL);
    w.start();

    const TestFrame tf;
    std::vector<Embedding> out;
    CHECK(!w.embed_by_deadline(tf.f, dets_with_class(1), out));
    CHECK(out.empty());
    CHECK(!w.degraded()); // a throw is a device fault, not a deadline miss

    // The worker survived: a healthy frame afterwards still gets embedded.
    raw->throws.store(false);
    CHECK(w.embed_by_deadline(tf.f, dets_with_class(4), out));
    CHECK(out.size() == 1);
    CHECK(out[0].v[0] == 4.F);
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_fast_embedder_delivers_in_time();
    rc = rc != 0 ? rc : test_slow_embedder_bounded_by_deadline();
    rc = rc != 0 ? rc : test_late_result_is_discarded();
    rc = rc != 0 ? rc : test_sustained_misses_degrade_and_stop_submitting();
    rc = rc != 0 ? rc : test_destructor_survives_hung_embedder();
    rc = rc != 0 ? rc : test_device_failure_is_not_a_deadline_miss();
    rc = rc != 0 ? rc : test_null_embedder_is_motion_only();
    rc = rc != 0 ? rc : test_throwing_embedder_does_not_kill_the_process();
    if (rc == 0) {
        std::printf("test_embedworker: OK (%d checks)\n", checks);
    }
    return rc;
}
