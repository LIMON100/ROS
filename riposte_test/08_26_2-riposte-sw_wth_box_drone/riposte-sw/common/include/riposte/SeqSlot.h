#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

// Single-writer / multi-reader "latest value" slot (seqlock), in two flavors:
//   ShmSeqSlot<T>   — POSIX shared memory, crosses process boundaries (TrackBus,
//                     StatusBus, HealthBus). Lock-free, torn-read safe, always
//                     yields the newest value (design decision A-2).
//   LocalSeqSlot<T> — same discipline inside one process (MAVSDK telemetry
//                     callbacks -> control thread, design principle G1).
//
// The payload is stored as an array of atomic 32-bit words and copied word by
// word with relaxed atomics (design decision C-5): a plain-memory payload would
// make the concurrent write/read a formal C++ data race even though the seq
// recheck discards torn copies — and the TSan suppression that race required
// could hide real races. With atomic words there is no data race, no
// suppression, and any TSan report against this header is a genuine defect.
// Memory order is the standard seqlock pattern: writer marks seq odd, release
// fence, word stores, then publishes with a release store of the even seq;
// reader acquires seq, copies words, acquire fence, rechecks seq.

namespace riposte {

namespace detail {

// Word-granular payload storage shared by both slot flavors (C-5). The last
// word is zero-padded on write so sizeof(T) not divisible by 4 stays defined.
template <typename T>
struct SeqPayload {
    static constexpr std::size_t WORDS = (sizeof(T) + 3U) / 4U;
    static_assert(std::atomic<uint32_t>::is_always_lock_free,
                  "seqlock payload requires lock-free u32 atomics");
    static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t),
                  "atomic u32 must be layout-identical to u32 (shm ABI)");

    std::atomic<uint32_t> words[WORDS];

    // Caller orders these with fences/seq; the word ops themselves are relaxed.
    void store_from(const T& v) {
        uint32_t buf[WORDS] = {}; // zero-pads the tail of the last word
        std::memcpy(buf, &v, sizeof(T));
        for (std::size_t i = 0; i < WORDS; ++i) {
            words[i].store(buf[i], std::memory_order_relaxed);
        }
    }
    void load_into(T& out) const {
        uint32_t buf[WORDS];
        for (std::size_t i = 0; i < WORDS; ++i) {
            buf[i] = words[i].load(std::memory_order_relaxed);
        }
        std::memcpy(&out, buf, sizeof(T));
    }
};

} // namespace detail

template <typename T>
class LocalSeqSlot {
    static_assert(std::is_trivially_copyable<T>::value, "payload must be POD");

public:
    void write(const T& v) {
        const uint32_t s = seq_.load(std::memory_order_relaxed);
        // OR (not +1): if a previous writer died mid-write and left seq odd,
        // the slot must stay "in progress" until THIS write completes — never
        // publish a torn payload by blindly bumping to even (C-5).
        const uint32_t begin = s | 1U;
        seq_.store(begin, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        payload_.store_from(v);
        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(begin + 1U, std::memory_order_release); // even: consistent
    }

    // false until the first COMPLETE write (or if the writer keeps
    // interrupting all retries).
    bool read(T& out) const {
        for (int i = 0; i < 8; ++i) {
            const uint32_t s1 = seq_.load(std::memory_order_acquire);
            if (s1 == 0 || ((s1 & 1U) != 0U)) {
                continue;
            }
            T copy{};
            payload_.load_into(copy);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq_.load(std::memory_order_relaxed) == s1) {
                out = copy;
                return true;
            }
        }
        return false;
    }

private:
    std::atomic<uint32_t> seq_{0};
    detail::SeqPayload<T> payload_{};
};

template <typename T>
class ShmSeqSlot {
    static_assert(std::is_trivially_copyable<T>::value, "payload must be POD");

    struct Shared {
        std::atomic<uint32_t> seq; // 0 = never written, odd = write in progress
        uint32_t size;             // sizeof(T), sanity check across processes
        detail::SeqPayload<T> payload;
    };
    static_assert(std::atomic<uint32_t>::is_always_lock_free,
                  "seqlock requires lock-free u32 atomics");

public:
    enum class Role : std::uint8_t { WRITER, READER };

    ShmSeqSlot() = default;
    ~ShmSeqSlot() { close(); }
    // Owns an mmap mapping; copy/move would double-unmap (G5.2 RAII).
    ShmSeqSlot(const ShmSeqSlot&) = delete;
    ShmSeqSlot& operator=(const ShmSeqSlot&) = delete;
    ShmSeqSlot(ShmSeqSlot&&) = delete;
    ShmSeqSlot& operator=(ShmSeqSlot&&) = delete;

    // WRITER creates/initializes the segment; readers attach (may fail until
    // the writer exists — call again later, see ensure_open()).
    bool open(const char* name, Role role) {
        if (shared_) {
            return true;
        }
        name_ = name;
        role_ = role;

        const int oflag = (role == Role::WRITER) ? (O_CREAT | O_RDWR) : O_RDWR;
        const int fd = ::shm_open(name, oflag, 0644);
        if (fd < 0) {
            return false;
        }

        bool ok = true;
        if (role == Role::WRITER) {
            ok = (::ftruncate(fd, sizeof(Shared)) == 0);
        }

        // Reject undersized segments (peer caught between shm_open and
        // ftruncate, or a leftover zero-length segment): mmap would succeed
        // but the first access would SIGBUS. Readers retry via ensure_open().
        if (ok) {
            struct stat st {};
            ok = (::fstat(fd, &st) == 0) &&
                 (st.st_size >= static_cast<off_t>(sizeof(Shared)));
        }

        void* mem = nullptr;
        if (ok) {
            mem = ::mmap(nullptr, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                         0);
            ok = (mem != MAP_FAILED);
        }
        ::close(fd);
        if (!ok) {
            return false;
        }

        shared_ = static_cast<Shared*>(mem);
        if (role == Role::WRITER) {
            // Do NOT reset seq: a fresh segment is zero-filled by ftruncate
            // (seq starts at 0 naturally), and resetting on re-attach would
            // invalidate the last value under live readers. If the previous
            // writer died mid-write (seq odd), the payload may be torn — leave
            // seq odd so readers keep skipping the slot until this writer's
            // first COMPLETE write republishes it (C-5); write() handles the
            // odd start via `seq | 1`. Publishing "no sample" degrades
            // freshness (SM-7's fail-safe path); publishing a torn sample
            // would feed garbage downstream.
            shared_->size = sizeof(T);
        } else if (shared_->size != 0 && shared_->size != sizeof(T)) {
            close(); // ABI mismatch between processes
            return false;
        }
        return true;
    }

    bool ensure_open(const char* name, Role role) {
        return shared_ ? true : open(name, role);
    }

    bool is_open() const { return shared_ != nullptr; }

    void close() {
        if (shared_) {
            ::munmap(shared_, sizeof(Shared));
            shared_ = nullptr;
        }
    }

    void write(const T& v) {
        if (shared_ == nullptr) {
            return;
        }
        const uint32_t s = shared_->seq.load(std::memory_order_relaxed);
        const uint32_t begin = s | 1U; // see LocalSeqSlot::write / C-5
        shared_->seq.store(begin, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        shared_->payload.store_from(v);
        std::atomic_thread_fence(std::memory_order_release);
        shared_->seq.store(begin + 1U, std::memory_order_release);
    }

    bool read(T& out) const {
        if (shared_ == nullptr) {
            return false;
        }
        for (int i = 0; i < 8; ++i) {
            const uint32_t s1 = shared_->seq.load(std::memory_order_acquire);
            if (s1 == 0 || ((s1 & 1U) != 0U)) {
                continue;
            }
            T copy{};
            shared_->payload.load_into(copy);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (shared_->seq.load(std::memory_order_relaxed) == s1) {
                out = copy;
                return true;
            }
        }
        return false;
    }

    static void unlink(const char* name) { ::shm_unlink(name); }

private:
    Shared* shared_ = nullptr;
    std::string name_;
    Role role_ = Role::READER;
};

} // namespace riposte
