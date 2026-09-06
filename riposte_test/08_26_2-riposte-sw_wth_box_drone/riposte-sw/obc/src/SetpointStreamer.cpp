#include "SetpointStreamer.h"

#include <pthread.h>
#include <sched.h>
#include <time.h> // POSIX clock_gettime/clock_nanosleep/TIMER_ABSTIME

#include <atomic>
#include <cerrno>
#include <cstdint>

#include "riposte/Log.h"

namespace riposte {

void SetpointStreamer::apply_realtime(const Params& p) {
    if (p.cpu_affinity >= 0) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(p.cpu_affinity, &set);
        if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
            char eb[64];
            RLOG_WARN("stream", "cpu affinity %d failed: %s", p.cpu_affinity,
                      log::errno_str(errno, eb, sizeof(eb)));
        } else {
            RLOG_INFO("stream", "pinned to CPU %d", p.cpu_affinity);
        }
    }
    if (p.rt_priority > 0) {
        // sched_param comes from <sched.h>; the checker misattributes it to a
        // glibc-internal bits/ header.
        // NOLINTNEXTLINE(misc-include-cleaner)
        sched_param sp{};
        sp.sched_priority = p.rt_priority;
        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
            char eb[64];
            RLOG_WARN("stream", "SCHED_FIFO prio %d failed: %s (need CAP_SYS_NICE)",
                      p.rt_priority, log::errno_str(errno, eb, sizeof(eb)));
        } else {
            RLOG_INFO("stream", "SCHED_FIFO prio %d", p.rt_priority);
        }
    }
}

void SetpointStreamer::run(const Params& p, const TickFn& tick,
                           const std::atomic<bool>& run) {
    apply_realtime(p);

    // Absolute-deadline scheduling: next += period each iteration so drift in one
    // tick does not push all subsequent ticks (no accumulation).
    timespec next{};
    // CLOCK_MONOTONIC is provided by <time.h>; the checker misattributes it to a
    // glibc-internal bits/ header.
    // NOLINTNEXTLINE(misc-include-cleaner)
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (run.load()) {
        const uint64_t now_ns = (static_cast<uint64_t>(next.tv_sec) * 1000000000ULL) +
                                static_cast<uint64_t>(next.tv_nsec);
        tick(now_ns);

        next.tv_nsec += static_cast<long>(p.period_ns);
        while (next.tv_nsec >= 1'000'000'000L) {
            next.tv_nsec -= 1'000'000'000L;
            ++next.tv_sec;
        }

        // If we already blew past the deadline (overrun), resync to now so we do
        // not spin issuing back-to-back ticks; the jitter monitor (SM-5) will see
        // the gap.
        timespec cur{};
        clock_gettime(CLOCK_MONOTONIC, &cur);
        if (cur.tv_sec > next.tv_sec ||
            (cur.tv_sec == next.tv_sec && cur.tv_nsec > next.tv_nsec)) {
            next = cur;
            continue;
        }
        // NOLINTNEXTLINE(misc-include-cleaner) — TIMER_ABSTIME is from <time.h>
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
    }
}

} // namespace riposte
