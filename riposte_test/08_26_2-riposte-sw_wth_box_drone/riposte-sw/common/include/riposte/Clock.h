#pragma once
#include <cstdint>
#include <ctime>

namespace riposte {

// All control-path timing uses CLOCK_MONOTONIC. Wall-clock (UTC, chrony-corrected
// from FC GPS time) is used only for log file naming / record stamps.
inline uint64_t mono_now_ns() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL) +
           static_cast<uint64_t>(ts.tv_nsec);
}

inline double ns_to_ms(uint64_t ns) {
    return static_cast<double>(ns) * 1e-6;
}
inline double ns_to_s(uint64_t ns) {
    return static_cast<double>(ns) * 1e-9;
}

// Age of a timestamp relative to a reference "now", clamped at 0. The control
// loop's now_ns is a scheduled deadline while telemetry/track samples are stamped
// at their actual arrival time, which can be a hair later than the deadline. A
// sample newer than now is, by definition, fresh — never let the unsigned
// subtraction underflow into a false staleness.
inline uint64_t age_ns(uint64_t now_ns, uint64_t stamp_ns) {
    return (now_ns > stamp_ns) ? (now_ns - stamp_ns) : 0;
}

} // namespace riposte
