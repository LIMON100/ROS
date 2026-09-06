#pragma once
#include "FcuLink.h"
#include "Geofence.h"
#include "OperatorAuthorization.h"
#include "SafetyMonitor.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "riposte/SeqSlot.h"
#include "riposte/Types.h"
#include "sources/IAttitudeSource.h"
#include "sources/ISetpointSource.h"

namespace riposte {

// Top-level orchestrator + state machine. ALL state transitions happen inside
// tick(); external inputs (operator commands) only set request flags. This is
// the single control thread (G1) and the sole owner of ObcState (G2).
class OffboardController {
public:
    struct Config {
        std::string connection_url = "udpin://0.0.0.0:14540";
        SafetyMonitor::Limits limits;
        std::string operator_token;
        bool attitude_mode = false; // true = pitch/yaw attitude control path
        // SM-10 test boundary. The polygon (GPS) wins when present; otherwise a
        // square of fence_side_m centred on the engage point. Both are resolved
        // to local NED at engage, when telemetry carries both frames at once.
        std::vector<Geofence::GeoPoint> fence_polygon;
        float fence_side_m = 0.F; // 0 = no square fence
    };

    OffboardController();
    ~OffboardController();

    // Sole owner of the FSM and the FCU link; not copyable or movable.
    OffboardController(const OffboardController&) = delete;
    OffboardController& operator=(const OffboardController&) = delete;
    OffboardController(OffboardController&&) = delete;
    OffboardController& operator=(OffboardController&&) = delete;

    // In attitude mode `att_source` is used and `source` may be null; in velocity
    // mode the reverse. The mode is fixed by cfg.attitude_mode.
    bool init(const Config& cfg, ISetpointSource* source,
              IAttitudeSource* att_source = nullptr);

    // Called from the command channel. ENGAGE and TARGET require a valid
    // operator token (D-2). FSM transitions are only ever latched as requests
    // here and acted on in tick(), but the call also forwards cue/hold/
    // return-home straight into the setpoint source, so it MUST run on the
    // control thread (main.cpp polls the socket inside the control tick) —
    // calling it from another thread would race source_->compute().
    void on_command(const ObcCommand& cmd);

    // Advance the FSM one control period. now_ns must be CLOCK_MONOTONIC.
    void tick(uint64_t now_ns);

    // Best-effort commanded Hold for shutdown mid-control session. Call ONLY after
    // the control thread has exited (never concurrently with tick()): if PX4
    // was left in Offboard (start acked or confirmed) the setpoint stream is
    // gone, so stop_offboard + hold gives the FC a commanded mode instead of
    // tripping the offboard-loss failsafe.
    void shutdown_hold();

    // Read from the main thread's 1 Hz log line (relaxed atomic; see state_).
    ObcState state() const { return state_.load(std::memory_order_relaxed); }
    void publish_status(ShmSeqSlot<ObcStatus>& bus, uint64_t now_ns);
    // Publishes the latest GPS fix for the seeker's recording overlay (logging
    // only — never a flight input). Best-effort: no fix => fix_ok stays 0.
    void publish_gps(ShmSeqSlot<GpsSample>& bus, uint64_t now_ns);

private:
    void transition(ObcState to, uint64_t now_ns, const char* why);
    // Resolves cfg_'s fence description into local NED against an engage-time
    // telemetry sample. Returns an invalid fence when none is configured.
    Geofence build_fence(const TelemetrySnapshot& t) const;

    // Per-state tick handlers (G16.4 — keeps tick() within size limits).
    void tick_ready(const TelemetrySnapshot& t, uint64_t now_ns, bool have_telem,
                    bool req_eng, bool req_dis);
    void tick_prestream(const TelemetrySnapshot& t, uint64_t now_ns, bool req_dis);
    void tick_offboard_active(const TelemetrySnapshot& t, uint64_t now_ns,
                              uint64_t actual_period_ns, bool req_dis);
    void tick_auto_landing(const TelemetrySnapshot& t, uint64_t now_ns, bool have_telem,
                           bool req_dis);
    void tick_disengaging(const TelemetrySnapshot& t, uint64_t now_ns);

    // Setpoint production is split from sending so the active state can withhold
    // the send on a safety violation (velocity and attitude share this path).
    bool prepare_setpoint(const TelemetrySnapshot& t, uint64_t now_ns); // -> source_ok
    // Returns whether the FC accepted the send; tracks the consecutive-failure
    // streak that SB_CMD_LINK acts on (P1-03).
    bool send_setpoint(); // clamp already applied

    Config cfg_;
    ISetpointSource* source_ = nullptr;
    IAttitudeSource* att_source_ = nullptr;
    VelocitySetpointNed vel_buf_{};
    AttitudeSetpoint att_buf_{};
    FcuLink fcu_;
    SafetyMonitor safety_;
    std::unique_ptr<OperatorAuthorization> auth_;

    // Written only inside tick()/shutdown_hold() (control thread / post-join
    // main thread); read by the main thread's 1 Hz log line while the control
    // thread runs. Relaxed suffices: it is a lone status word, no other data
    // is published through it (G6).
    std::atomic<ObcState> state_{ObcState::IDLE};
    std::atomic<bool> req_engage_{false};
    std::atomic<bool> req_disengage_{false};
    // SM-2 reentry latch (G5): set when the pilot overrode the mode; READY
    // rejects ENGAGE while set. Cleared only by an explicit operator DISENGAGE
    // received in READY (acknowledgment that the pilot handed back control).
    bool reentry_blocked_ = false;
    // Violation mask that triggered the pending DISENGAGING (0 = operator or
    // other non-safety cause). SB_MODE_OVERRIDE selects the no-stop/hold path:
    // the pilot owns the vehicle, we must not command it back. Held until the
    // exit is CONFIRMED (P1-03), not cleared on the first attempt.
    uint32_t disengage_cause_ = 0;
    // DISENGAGING retry bookkeeping (P1-03): stop/hold are re-sent each tick
    // until each individually succeeds; reset when entering DISENGAGING.
    bool disengage_stop_ok_ = false;
    bool disengage_hold_ok_ = false;
    // Consecutive setpoint send failures while ACTIVE (SB_CMD_LINK, P1-03).
    int consec_send_fail_ = 0;

    uint64_t state_since_ns_ = 0;
    bool offboard_started_ = false; // PRESTREAM: start acked, awaiting telemetry
    bool landing_commanded_ = false;
    uint64_t offboard_started_ns_ = 0;
    uint64_t engage_start_ns_ = 0;
    uint64_t last_tick_ns_ = 0;   // scheduled tick time (deadline, O-1)
    uint64_t last_actual_ns_ = 0; // measured execution time (SM-5 jitter)
    uint32_t engage_count_ = 0;
    uint32_t last_violation_ = 0;
    // Highest TARGET sequence latched so far (TargetGate: a decreasing seq is
    // a stale/replayed cue and is rejected). on_command runs on the control
    // thread (see its contract above), so no atomicity needed.
    uint32_t last_target_seq_ = 0;
};

} // namespace riposte
