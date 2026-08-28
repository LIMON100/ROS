#pragma once
#include <atomic>
#include <cstdint>
#include <functional>

namespace riposte {

// Fixed-rate driver for the single control thread. Sleeps to an absolute
// CLOCK_MONOTONIC deadline (not relative) so period error does not accumulate,
// and applies SCHED_FIFO + CPU affinity when permitted so the 20 Hz loop is
// isolated from best-effort work on the RK3588 (design principle: jitter budget).
class SetpointStreamer {
public:
    // tick receives the scheduled tick time in ns (CLOCK_MONOTONIC).
    using TickFn = std::function<void(uint64_t now_ns)>;

    struct Params {
        uint64_t period_ns = 50'000'000; // 20 Hz
        int rt_priority = 80;            // SCHED_FIFO; 0 disables
        int cpu_affinity = -1;           // pin to this core; -1 disables
    };

    static void run(const Params& p, const TickFn& tick, const std::atomic<bool>& run);

private:
    static void apply_realtime(const Params& p);
};

} // namespace riposte
