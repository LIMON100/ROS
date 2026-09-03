#pragma once
#include "Geofence.h"

#include "riposte/Types.h"

namespace riposte {

// Strategy for producing a velocity setpoint each control tick. TestPatternSource
// for bring-up, GuidanceSource for seeker-driven tracking. Returning false means
// "cannot produce a setpoint" (e.g. track stale) and is a disengage cause (G3).
class ISetpointSource {
public:
    ISetpointSource() = default;
    virtual ~ISetpointSource() = default;
    ISetpointSource(const ISetpointSource&) = delete;
    ISetpointSource& operator=(const ISetpointSource&) = delete;
    ISetpointSource(ISetpointSource&&) = delete;
    ISetpointSource& operator=(ISetpointSource&&) = delete;
    virtual bool compute(const TelemetrySnapshot& t, uint64_t now_ns,
                         VelocitySetpointNed& out) = 0;
    virtual const char* name() const = 0;
    // Called once per control session, at the PRESTREAM entry, before the first
    // compute(). Sources that latch per-control session state (home point, mission
    // phase, filter history) must clear it here — otherwise a second control session
    // continues on the previous one's state (e.g. returning to a home point
    // captured at a different location). The mission TARGET cue is NOT part of
    // that state: it arrives with the command that requests the engage.
    virtual void on_engage() {}
    // The control session's boundary, resolved to local NED, handed over right after
    // on_engage(). Sources that steer relative to it (the balloon patrol) take
    // it; the rest ignore it and are bounded by SM-3/SM-10 alone.
    virtual void set_fence(const Geofence& /*fence*/) {}
    virtual void set_mission_target(const MissionTarget& /*target*/) {}
    virtual void operator_hold(uint64_t /*now_ns*/) {}
    virtual void return_home(uint64_t /*now_ns*/) {}
    virtual bool requests_land() const { return false; }
    virtual bool requests_disarm(const TelemetrySnapshot& /*t*/) const { return false; }
};

} // namespace riposte
