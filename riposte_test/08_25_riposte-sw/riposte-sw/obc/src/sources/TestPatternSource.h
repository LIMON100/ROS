#pragma once
#include "ISetpointSource.h"

#include <cstdint>

namespace riposte {

// POC bring-up source: hover, constant-velocity, or circular velocity patterns.
// No dependency on the seeker; used to validate the Offboard lifecycle and the
// safety layer before wiring in guidance.
class TestPatternSource final : public ISetpointSource {
public:
    enum class Pattern : std::uint8_t { HOVER, CONSTANT, CIRCLE };

    explicit TestPatternSource(Pattern p) : pattern_(p) {}

    bool compute(const TelemetrySnapshot& t, uint64_t now_ns,
                 VelocitySetpointNed& out) override;
    const char* name() const override { return "TestPatternSource"; }

private:
    Pattern pattern_;
    uint64_t t0_ns_ = 0;
};

} // namespace riposte
