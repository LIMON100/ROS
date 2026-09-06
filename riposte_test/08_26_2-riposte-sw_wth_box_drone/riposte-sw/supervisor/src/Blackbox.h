#pragma once
// Size-bounded JSONL blackbox with logrotate-style numbered retention.
//
// When the active file reaches max_bytes it is rotated: base -> base.1 -> ... ->
// base.N, and base.(N+1) is discarded, so total on-disk usage is bounded to
// roughly (keep + 1) * max_bytes. Bounding matters on the target's small flash —
// an unbounded append would eventually fill the disk and take down other
// services. Every filesystem op is best-effort (design decision A-4): a failure
// degrades logging, never the supervisor.
//
// Header-only so the rotation logic is unit-testable off-target (G11.2).

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

namespace riposte {

// RAII ownership of the stream (G5.2): closes on every exit path. The std::fclose
// function-pointer deleter (vs a wrapper functor) lets the static analyzer see
// the close and not false-positive a leak.
using FilePtr = std::unique_ptr<std::FILE, int (*)(std::FILE*)>;

class Blackbox {
public:
    Blackbox(std::string path, long max_bytes, int keep)
        : path_(std::move(path)), max_bytes_(max_bytes), keep_(keep) {
        open_append();
    }
    bool ok() const { return f_ != nullptr; }

    // Appends one record. On a write/flush failure (ENOSPC, a partial write)
    // the stream is CLOSED so ok() flips to false (P2-02): the old code
    // ignored the return and kept reporting success while nothing reached the
    // disk. Returns whether the whole line was durably written.
    bool write_line(const char* buf, int len) {
        if (f_ == nullptr || len <= 0) {
            return false;
        }
        const std::size_t want = static_cast<std::size_t>(len);
        const std::size_t wrote = std::fwrite(buf, 1, want, f_.get());
        if (wrote != want || std::fflush(f_.get()) != 0) {
            // Evidence is not actually being recorded — degrade honestly.
            f_.reset(); // ok() -> false; the supervisor logs and stops writing
            return false;
        }
        bytes_ += len;
        if (max_bytes_ > 0 && bytes_ >= max_bytes_) {
            rotate();
        }
        return true;
    }

private:
    // Current on-disk size of path (0 if absent). Local scope so the analyzer
    // sees the stream open and close together (no leak false-positive).
    static long file_bytes(const std::string& path) {
        const FilePtr f(std::fopen(path.c_str(), "rb"), &std::fclose);
        if (f == nullptr || std::fseek(f.get(), 0, SEEK_END) != 0) {
            return 0;
        }
        const long n = std::ftell(f.get());
        return (n > 0) ? n : 0;
    }

    void open_append() {
        // Build a local owner first (the analyzer models a unique_ptr with a
        // known deleter), then move it into the member — this avoids the
        // reset(fopen) shape that the analyzer misreads as a leak.
        FilePtr fresh(std::fopen(path_.c_str(), "a"), &std::fclose);
        bytes_ = (fresh != nullptr) ? file_bytes(path_) : 0;
        f_ = std::move(fresh);
    }

    std::string suffixed(int n) const { return path_ + "." + std::to_string(n); }

    void rotate() {
        f_.reset(); // close before renaming
        if (keep_ < 1) {
            (void)std::remove(path_.c_str()); // no archives kept: truncate
            open_append();
            return;
        }
        (void)std::remove(suffixed(keep_).c_str()); // drop the oldest archive
        for (int i = keep_ - 1; i >= 1; --i) {
            (void)std::rename(suffixed(i).c_str(), suffixed(i + 1).c_str());
        }
        (void)std::rename(path_.c_str(), suffixed(1).c_str());
        open_append(); // fresh, empty base file
    }

    std::string path_;
    long max_bytes_;
    int keep_;
    FilePtr f_{nullptr, &std::fclose}; // deleter bound at construction
    long bytes_ = 0;
};

} // namespace riposte
