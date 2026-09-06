#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "riposte/Clock.h"

// Minimal structured stderr logger. journald captures stderr for each systemd
// unit, so process identity and wall-clock come from the journal; we add the
// monotonic timestamp used by the control path so records can be correlated
// with TrackBus / blackbox entries.

namespace riposte::log {

enum class Level : std::uint8_t { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

inline Level& threshold() {
    static Level lv = Level::INFO;
    return lv;
}

// Best-effort logging: stderr write failures are deliberately ignored — there
// is no useful recovery path and the control loop must not depend on logging
// (G10.2). The void casts document that intent (cert-err33-c).
inline void vlog(Level lv, const char* tag, const char* fmt, va_list ap) {
    if (lv < threshold()) {
        return;
    }
    static std::mutex mtx;
    static const char* const LEVEL_NAMES[] = {"DBG", "INF", "WRN", "ERR"};
    std::lock_guard<std::mutex> const lock(mtx);
    (void)std::fprintf(stderr, "[%12.6f] %s %-10s ", ns_to_s(mono_now_ns()),
                       LEVEL_NAMES[static_cast<int>(lv)], tag);
    (void)std::vfprintf(stderr, fmt, ap);
    (void)std::fputc('\n', stderr);
}

inline void write(Level lv, const char* tag, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

inline void write(Level lv, const char* tag, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(lv, tag, fmt, ap);
    va_end(ap);
}

// Thread-safe errno formatting for log call sites (concurrency-mt-unsafe bans
// std::strerror). glibc + g++ define _GNU_SOURCE, so this is the GNU
// strerror_r that may return a pointer to a static immutable string instead
// of filling buf — always use the returned pointer, never buf directly.
inline const char* errno_str(int err, char* buf, std::size_t len) {
    return ::strerror_r(err, buf, len);
}

} // namespace riposte::log

// Structured leveled logging facade (G10.1). Variadic macros are the accepted
// deviation for printf-style logging — see .clang-tidy exclusion rationale.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define RLOG_DEBUG(tag, ...) \
    ::riposte::log::write(::riposte::log::Level::DEBUG, tag, __VA_ARGS__)
#define RLOG_INFO(tag, ...) \
    ::riposte::log::write(::riposte::log::Level::INFO, tag, __VA_ARGS__)
#define RLOG_WARN(tag, ...) \
    ::riposte::log::write(::riposte::log::Level::WARN, tag, __VA_ARGS__)
#define RLOG_ERROR(tag, ...) \
    ::riposte::log::write(::riposte::log::Level::ERROR, tag, __VA_ARGS__)
// NOLINTEND(cppcoreguidelines-macro-usage)
