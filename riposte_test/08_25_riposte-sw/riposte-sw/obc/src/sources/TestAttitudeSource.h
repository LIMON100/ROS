#pragma once
#include "IAttitudeSource.h"

#include "riposte/Tunables.h"

namespace riposte {

// Bring-up attitude source: commands a fixed roll/pitch/yaw + thrust from config.
// Directly exercises the pitch/yaw control path on the bench without a target;
// the safety clamp still bounds whatever is asked for. Always "guiding" (true).
class TestAttitudeSource final : public IAttitudeSource {
public:
    struct Params {
        float roll_deg = 0.F;
        float pitch_deg = 0.F;
        float yaw_deg = 0.F;
        float thrust = tun::ATT_HOVER_THRUST;
    };

    explicit TestAttitudeSource(Params p) : p_(p) {}

    bool compute(const TelemetrySnapshot& /*t*/, uint64_t /*now_ns*/,
                 AttitudeSetpoint& out) override {
        out.roll_deg = p_.roll_deg;
        out.pitch_deg = p_.pitch_deg;
        out.yaw_deg = p_.yaw_deg;
        out.thrust = p_.thrust;
        return true;
    }
    const char* name() const override { return "TestAttitudeSource"; }

private:
    Params p_;
};

} // namespace riposte
