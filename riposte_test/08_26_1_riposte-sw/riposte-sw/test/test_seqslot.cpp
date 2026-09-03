// SeqSlot: single-writer/multi-reader latest-value consistency.
#include <fcntl.h>
#include <signal.h> // kill, SIGKILL
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#include "riposte/SeqSlot.h"

using namespace riposte;

namespace {

struct Payload {
    uint64_t a;
    uint64_t b;
}; // invariant: b == a * 2

// Test-run counter, incremented by CHECK across all test functions.
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

// Local slot basic read-after-write.
int test_local_basic() {
    LocalSeqSlot<Payload> slot;
    Payload out{};
    CHECK(!slot.read(out)); // nothing written yet
    slot.write({7, 14});
    CHECK(slot.read(out));
    CHECK(out.a == 7);
    CHECK(out.b == 14);
    return 0;
}

// Concurrent torn-read stress: reader must never observe b != a*2.
int test_local_torn_read_stress() {
    LocalSeqSlot<Payload> slot;
    slot.write({1, 2});
    std::atomic<bool> run{true};
    std::atomic<uint64_t> reads{0};
    std::atomic<uint64_t> torn{0};

    std::thread writer([&] {
        for (uint64_t i = 0; run.load(); ++i) {
            slot.write({i, i * 2});
        }
    });
    std::thread reader([&] {
        Payload p{};
        while (run.load()) {
            if (slot.read(p)) {
                reads.fetch_add(1);
                if (p.b != p.a * 2) {
                    torn.fetch_add(1);
                }
            }
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    run.store(false);
    writer.join();
    reader.join();

    std::printf("seqslot: %llu reads, %llu torn\n", (unsigned long long)reads.load(),
                (unsigned long long)torn.load());
    CHECK(reads.load() > 0);
    CHECK(torn.load() == 0);
    return 0;
}

// Shared-memory slot round trip (writer + reader in one process).
int test_shm_round_trip() {
    const char* nm = "/riposte_test_slot";
    ShmSeqSlot<Payload>::unlink(nm);
    ShmSeqSlot<Payload> w;
    ShmSeqSlot<Payload> r;
    CHECK(w.open(nm, ShmSeqSlot<Payload>::Role::WRITER));
    CHECK(r.open(nm, ShmSeqSlot<Payload>::Role::READER));
    Payload out{};
    CHECK(!r.read(out)); // writer opened but not yet written
    w.write({100, 200});
    CHECK(r.read(out));
    CHECK(out.a == 100);
    CHECK(out.b == 200);
    w.close();
    r.close();
    ShmSeqSlot<Payload>::unlink(nm);
    return 0;
}

// A zero-length segment (peer caught between shm_open and ftruncate, or a
// leftover) must fail open() cleanly instead of SIGBUS-ing on first access.
int test_shm_zero_length_segment_rejected() {
    const char* nm = "/riposte_test_slot_zero";
    ShmSeqSlot<Payload>::unlink(nm);
    const int fd = ::shm_open(nm, O_CREAT | O_RDWR, 0644);
    CHECK(fd >= 0); // created, deliberately NOT ftruncate'd
    ::close(fd);
    ShmSeqSlot<Payload> r;
    CHECK(!r.open(nm, ShmSeqSlot<Payload>::Role::READER));
    ShmSeqSlot<Payload>::unlink(nm);
    return 0;
}

// Writer re-attach must not reset seq: the last value stays readable.
int test_shm_writer_reopen_preserves_seq() {
    const char* nm = "/riposte_test_slot_reopen";
    ShmSeqSlot<Payload>::unlink(nm);
    {
        ShmSeqSlot<Payload> w;
        CHECK(w.open(nm, ShmSeqSlot<Payload>::Role::WRITER));
        w.write({3, 6});
        w.write({4, 8});
    } // writer gone, segment persists
    ShmSeqSlot<Payload> w2;
    ShmSeqSlot<Payload> r;
    CHECK(w2.open(nm, ShmSeqSlot<Payload>::Role::WRITER));
    CHECK(r.open(nm, ShmSeqSlot<Payload>::Role::READER));
    Payload out{};
    CHECK(r.read(out)); // seq preserved: last value still readable
    CHECK(out.a == 4);
    CHECK(out.b == 8);
    w2.close();
    r.close();
    ShmSeqSlot<Payload>::unlink(nm);
    return 0;
}

// A writer that died mid-copy leaves seq odd and a possibly-torn payload. The
// slot must publish NOTHING until the next writer's first COMPLETE write (C-5):
// a temporary no-sample degrades freshness (SM-7's safe path); a "repaired"
// torn sample would feed garbage downstream as a valid value.
// G16.6 deviation: scripted corruption/recovery scenario; splitting hides the timeline.
// NOLINTNEXTLINE(readability-function-size)
int test_shm_writer_death_mid_write_withholds_torn_payload() {
    const char* nm = "/riposte_test_slot_death";
    ShmSeqSlot<Payload>::unlink(nm);
    {
        ShmSeqSlot<Payload> w;
        CHECK(w.open(nm, ShmSeqSlot<Payload>::Role::WRITER));
        w.write({4, 8});
    }
    {
        // Simulate a death mid-copy: seq odd AND the payload half-overwritten.
        // Raw shm layout: [seq u32][size u32][payload words...].
        const int fd = ::shm_open(nm, O_RDWR, 0644);
        CHECK(fd >= 0);
        void* mem = ::mmap(nullptr, 4U * sizeof(uint32_t), PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, 0);
        ::close(fd);
        CHECK(mem != MAP_FAILED);
        auto* words = static_cast<std::atomic<uint32_t>*>(mem);
        words[2].store(0xDEADBEEFU); // first payload word: torn garbage
        words[0].store(3);           // seq odd: write was in progress
        ::munmap(mem, 4U * sizeof(uint32_t));
    }
    ShmSeqSlot<Payload> w2;
    ShmSeqSlot<Payload> r;
    CHECK(w2.open(nm, ShmSeqSlot<Payload>::Role::WRITER));
    CHECK(r.open(nm, ShmSeqSlot<Payload>::Role::READER));
    Payload out{};
    CHECK(!r.read(out)); // torn payload is withheld, not "repaired" into view
    w2.write({5, 10});
    CHECK(r.read(out)); // first complete write republishes the slot
    CHECK(out.a == 5);
    CHECK(out.b == 10);
    w2.close();
    r.close();
    ShmSeqSlot<Payload>::unlink(nm);
    return 0;
}

// Cross-PROCESS stress: a forked child writes continuously while the parent
// reads; the invariant must hold across the address-space boundary, and a
// SIGKILL of the writer at an arbitrary point must never yield a torn read
// afterwards (only a consistent value or no sample).
int test_shm_fork_process_stress_and_kill() {
    const char* nm = "/riposte_test_slot_fork";
    ShmSeqSlot<Payload>::unlink(nm);
    const pid_t pid = ::fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        // Child: writer loop until killed. No CHECK/stdio here — the child
        // reports only through the slot itself.
        ShmSeqSlot<Payload> w;
        if (!w.open(nm, ShmSeqSlot<Payload>::Role::WRITER)) {
            ::_exit(1);
        }
        for (uint64_t i = 1;; ++i) {
            w.write({i, i * 2});
        }
    }
    ShmSeqSlot<Payload> r;
    for (int i = 0; i < 2000 && !r.is_open(); ++i) {
        r.ensure_open(nm, ShmSeqSlot<Payload>::Role::READER);
        ::usleep(1000);
    }
    CHECK(r.is_open());

    uint64_t reads = 0;
    uint64_t torn = 0;
    Payload p{};
    for (uint64_t iter = 0; reads < 20000 && iter < 100'000'000ULL; ++iter) {
        if (r.read(p)) {
            ++reads;
            if (p.b != p.a * 2) {
                ++torn;
            }
        }
    }
    ::kill(pid, SIGKILL); // die at an arbitrary point, possibly mid-write
    (void)::waitpid(pid, nullptr, 0);
    // Post-mortem reads: consistent value or no sample — never torn.
    for (int i = 0; i < 1000; ++i) {
        if (r.read(p) && p.b != p.a * 2) {
            ++torn;
        }
    }
    std::printf("seqslot fork: %llu reads, %llu torn\n", (unsigned long long)reads,
                (unsigned long long)torn);
    CHECK(reads >= 20000);
    CHECK(torn == 0);
    r.close();
    ShmSeqSlot<Payload>::unlink(nm);
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_local_basic();
    rc = rc != 0 ? rc : test_local_torn_read_stress();
    rc = rc != 0 ? rc : test_shm_round_trip();
    rc = rc != 0 ? rc : test_shm_zero_length_segment_rejected();
    rc = rc != 0 ? rc : test_shm_writer_reopen_preserves_seq();
    rc = rc != 0 ? rc : test_shm_writer_death_mid_write_withholds_torn_payload();
    rc = rc != 0 ? rc : test_shm_fork_process_stress_and_kill();
    if (rc != 0) {
        return rc;
    }

    std::printf("test_seqslot: %d checks passed\n", checks);
    return 0;
}
