#pragma once
#include "riposte/Types.h"

namespace riposte {

// Strategy for producing an attitude setpoint (pitch/yaw + thrust) each control
// tick, the attitude-mode analogue of ISetpointSource. Returning false means
// "cannot produce a guided setpoint" (e.g. track stale) and is a disengage cause
// (G3); implementations MUST still leave `out` at a safe hold (level, hover
// thrust) so the single tick sent before disengage cannot upset the vehicle.
class IAttitudeSource {
public:
    IAttitudeSource() = default;
    virtual ~IAttitudeSource() = default;
    IAttitudeSource(const IAttitudeSource&) = delete;
    IAttitudeSource& operator=(const IAttitudeSource&) = delete;
    IAttitudeSource(IAttitudeSource&&) = delete;
    IAttitudeSource& operator=(IAttitudeSource&&) = delete;
    virtual bool compute(const TelemetrySnapshot& t, uint64_t now_ns,
                         AttitudeSetpoint& out) = 0;
    virtual const char* name() const = 0;
    // Called once per control session, at the PRESTREAM entry, before the first
    // compute() — the same session boundary ISetpointSource::on_engage() marks.
    // Sources that cache per-session state (the tracking source's last-valid
    // track and detection clock) must clear it here, or a second session starts
    // steering on the PREVIOUS session's cached track for up to the coast
    // window before the freshness gate catches up.
    virtual void on_engage() {}
};

} // namespace riposte
