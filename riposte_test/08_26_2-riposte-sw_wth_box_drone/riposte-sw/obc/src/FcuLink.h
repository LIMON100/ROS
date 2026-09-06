#pragma once
#include <memory>
#include <string>

#include "riposte/SeqSlot.h"
#include "riposte/Types.h"

namespace riposte {

// Boundary around MAVSDK. Everything MAVSDK-specific lives in FcuLink.cpp, built
// only when RIPOSTE_WITH_MAVSDK=ON. In SIL builds a stub implementation models a
// simple vehicle so the FSM and safety layer run end-to-end on a dev PC.
//
// Telemetry callbacks (MAVSDK threads) publish into an internal LocalSeqSlot; the
// control thread reads an atomic snapshot (design principle G1).
class FcuLink {
public:
    FcuLink();
    ~FcuLink();

    // Owns the MAVSDK session / SIL vehicle model (PIMPL); copying or moving
    // would split ownership of the live link (G5.2 RAII).
    FcuLink(const FcuLink&) = delete;
    FcuLink& operator=(const FcuLink&) = delete;
    FcuLink(FcuLink&&) = delete;
    FcuLink& operator=(FcuLink&&) = delete;

    // connection_url e.g. "udpin://0.0.0.0:14540" (via mavlink-router local endpoint).
    bool connect(const std::string& connection_url);
    bool discovered() const;

    // Latest telemetry snapshot (torn-read safe). Returns false before first fix.
    bool snapshot(TelemetrySnapshot& out) const;

    // Offboard lifecycle. sendSetpoint must be called >=2 Hz before start and
    // continuously after (PX4 requirement). A session streams ONE setpoint type;
    // velocity and attitude are alternative control modes, not mixed.
    bool send_velocity_ned(const VelocitySetpointNed& sp);
    bool send_attitude(const AttitudeSetpoint& sp); // pitch/yaw attitude control
    bool start_offboard();
    bool stop_offboard();

    // DISENGAGE / failsafe path.
    bool hold();
    bool land();
    bool disarm();

    // SIL only: advance the internal vehicle model by dt (no-op with MAVSDK).
    void sil_step(double dt_s);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace riposte
