#include "sources/TestPatternSource.h"

#include <cmath>
#include <cstdint>

#include "riposte/Clock.h"
#include "riposte/Types.h"

namespace riposte {

bool TestPatternSource::compute(const TelemetrySnapshot& t, uint64_t now_ns,
                                VelocitySetpointNed& out) {
    (void)t;
    if (t0_ns_ == 0) {
        t0_ns_ = now_ns;
    }
    const double dt = ns_to_s(now_ns - t0_ns_);

    out = VelocitySetpointNed{};
    switch (pattern_) {
        case Pattern::HOVER:
            break; // all zero
        case Pattern::CONSTANT:
            out.vn_mps = 2.0F; // 2 m/s north
            break;
        case Pattern::CIRCLE: {
            const double w = 0.3; // rad/s
            out.vn_mps = static_cast<float>(3.0 * std::cos(w * dt));
            out.ve_mps = static_cast<float>(3.0 * std::sin(w * dt));
            out.yaw_rad = std::atan2(out.ve_mps, out.vn_mps);
            break;
        }
    }
    return true; // pattern source can always produce a setpoint
}

} // namespace riposte
