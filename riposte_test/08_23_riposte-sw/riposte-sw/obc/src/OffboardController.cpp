#include "OffboardController.h"

#include "Geofence.h"
#include "OperatorAuthorization.h"
#include "SafetyMonitor.h"
#include "TargetGate.h"

#include <atomic>
#include <cstdint>
#include <memory>

#include "riposte/Clock.h"
#include "riposte/Log.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"
#include "sources/IAttitudeSource.h"
#include "sources/ISetpointSource.h"

namespace riposte {

OffboardController::OffboardController() = default;
OffboardController::~OffboardController() = default;

bool OffboardController::init(const Config& cfg, ISetpointSource* source,
                              IAttitudeSource* att_source) {
    cfg_ = cfg;
    source_ = source;
    att_source_ = att_source;
    safety_.configure(cfg.limits);
    auth_ = std::make_unique<OperatorAuthorization>(cfg.operator_token);
    if (!fcu_.connect(cfg.connection_url)) {
        RLOG_ERROR("obc", "FCU connect failed");
        return false;
    }
    state_ = ObcState::IDLE;
    state_since_ns_ = mono_now_ns();
    return true;
}

void OffboardController::on_command(const ObcCommand& cmd) {
    switch (cmd.type) {
        case ObcCommandType::ENGAGE:
            if (auth_ && auth_->authorize(cmd)) {
                req_engage_.store(true);
                RLOG_INFO("obc", "engage authorized");
            } else {
                RLOG_WARN("obc", "engage DENIED (bad/absent operator token)");
            }
            break;
        case ObcCommandType::DISENGAGE:
            req_disengage_.store(true);
            RLOG_INFO("obc", "disengage requested");
            break;
        case ObcCommandType::TARGET:
            if (auth_ && auth_->authorize(cmd)) {
                // A mission cue needs a velocity source to consume it; the
                // attitude path has none. Latching engage anyway would start a
                // control session whose cue evaporated on arrival — reject the
                // command whole instead, so the operator learns immediately.
                if (cfg_.attitude_mode || source_ == nullptr) {
                    RLOG_WARN("obc",
                              "target REJECTED: attitude mode has no mission-cue "
                              "consumer (no engage latched)");
                    break;
                }
                // Authentication proves the sender; the gate proves the numbers
                // (finite, in-range, non-decreasing seq) before anything is
                // latched — a NaN coordinate must never reach the guidance path.
                const char* why = target_reject_reason(cmd, last_target_seq_);
                if (why != nullptr) {
                    RLOG_WARN("obc", "target REJECTED: %s", why);
                    break;
                }
                last_target_seq_ = cmd.target_seq;
                MissionTarget mt{};
                mt.mono_ns = mono_now_ns();
                mt.seq = cmd.target_seq;
                mt.pos_ned_m[0] = cmd.target_pos_ned_m[0];
                mt.pos_ned_m[1] = cmd.target_pos_ned_m[1];
                mt.pos_ned_m[2] = cmd.target_pos_ned_m[2];
                mt.vel_ned_mps[0] = cmd.target_vel_ned_mps[0];
                mt.vel_ned_mps[1] = cmd.target_vel_ned_mps[1];
                mt.vel_ned_mps[2] = cmd.target_vel_ned_mps[2];
                mt.heading_rad = cmd.target_heading_rad;
                mt.valid = 1;
                source_->set_mission_target(mt);
                req_engage_.store(true);
                RLOG_INFO("obc", "target accepted; engage requested");
            } else {
                RLOG_WARN("obc", "target DENIED (bad/absent operator token)");
            }
            break;
        // HOLD / RETURN_HOME are velocity-source behaviours. In attitude mode
        // there is no source to execute them, and the previous handlers logged
        // "requested" while doing NOTHING — an operator command silently
        // ignored mid-session. The attitude path maps both onto the disengage
        // request instead: DISENGAGING stops the offboard stream and commands
        // PX4 Hold, which is the safe executable meaning of "stop what you are
        // doing and hold here". (RETURN_HOME additionally warns that the
        // vehicle will hold, not fly home — FcuLink has no RTL action.)
        case ObcCommandType::OPERATOR_HOLD:
            if (cfg_.attitude_mode || source_ == nullptr) {
                req_disengage_.store(true);
                RLOG_INFO("obc",
                          "operator hold in attitude mode -> disengage (PX4 Hold)");
                break;
            }
            source_->operator_hold(mono_now_ns());
            RLOG_INFO("obc", "operator hold requested");
            break;
        case ObcCommandType::RETURN_HOME:
            if (cfg_.attitude_mode || source_ == nullptr) {
                req_disengage_.store(true);
                RLOG_WARN("obc",
                          "return-home unsupported in attitude mode -> disengage "
                          "(PX4 Hold; vehicle will HOLD, not fly home)");
                break;
            }
            source_->return_home(mono_now_ns());
            RLOG_INFO("obc", "operator return-home requested");
            break;
    }
}

// Produces and clamps the next setpoint (velocity or attitude per mode) into the
// internal buffer. Returns whether the source could actually guide (source_ok);
// the buffer is left at a safe, clamped value even when it could not.
bool OffboardController::prepare_setpoint(const TelemetrySnapshot& t, uint64_t now_ns) {
    if (cfg_.attitude_mode) {
        att_buf_ = AttitudeSetpoint{};
        const bool ok =
            (att_source_ != nullptr) && att_source_->compute(t, now_ns, att_buf_);
        safety_.clamp_attitude(att_buf_); // G4 final line (attitude)
        return ok;
    }
    vel_buf_ = VelocitySetpointNed{};
    const bool ok = (source_ != nullptr) && source_->compute(t, now_ns, vel_buf_);
    safety_.clamp(vel_buf_); // G4 final line (velocity)
    return ok;
}

bool OffboardController::send_setpoint() {
    const bool sent = cfg_.attitude_mode ? fcu_.send_attitude(att_buf_)
                                         : fcu_.send_velocity_ned(vel_buf_);
    if (sent) {
        consec_send_fail_ = 0;
    } else {
        ++consec_send_fail_;
    }
    return sent;
}

void OffboardController::transition(ObcState to, uint64_t now_ns, const char* why) {
    RLOG_INFO("obc", "%s -> %s (%s)", to_string(state_.load(std::memory_order_relaxed)),
              to_string(to), why);
    if (to == ObcState::DISENGAGING) {
        // Fresh exit attempt: stop/hold each retried until they individually
        // succeed, and the cause survives until the exit is confirmed (P1-03).
        disengage_stop_ok_ = false;
        disengage_hold_ok_ = false;
    }
    state_.store(to, std::memory_order_relaxed);
    state_since_ns_ = now_ns;
}

void OffboardController::tick(uint64_t now_ns) {
    const uint64_t last_period = (last_tick_ns_ != 0U) ? (now_ns - last_tick_ns_) : 0;
    last_tick_ns_ = now_ns;

    // SM-5 must see the ACTUAL execution cadence, not the scheduled one:
    // now_ns is the deadline stamp (O-1), and after a stall the streamer fires
    // the pending tick still stamped at its deadline — scheduled periods then
    // read exactly one period and a clean tick precedes every overrun, so
    // consecutive-violation jitter detection can never trip. Found by SM-5
    // fault injection in PX4 SITL.
    const uint64_t actual_ns = mono_now_ns();
    const uint64_t actual_period =
        (last_actual_ns_ != 0U) ? (actual_ns - last_actual_ns_) : 0;
    last_actual_ns_ = actual_ns;

    // SIL vehicle integration is a no-op with real MAVSDK.
    fcu_.sil_step((last_period != 0U) ? ns_to_s(last_period)
                                      : ns_to_s(tun::CONTROL_PERIOD_NS));

    TelemetrySnapshot t{};
    const bool have_telem = fcu_.snapshot(t);

    const bool req_dis = req_disengage_.exchange(false);
    const bool req_eng = req_engage_.exchange(false);

    switch (state_.load(std::memory_order_relaxed)) {
        case ObcState::IDLE:
            transition(ObcState::CONNECTING, now_ns, "startup");
            break;

        case ObcState::CONNECTING:
            if (fcu_.discovered() && have_telem && (t.connected != 0U) &&
                (t.position_ok != 0U)) {
                transition(ObcState::READY, now_ns, "fcu discovered + telemetry valid");
            } else if (now_ns - state_since_ns_ > tun::CONNECT_TIMEOUT_NS) {
                transition(ObcState::FAULT, now_ns, "connect timeout");
            }
            break;

        case ObcState::READY:
            tick_ready(t, now_ns, have_telem, req_eng, req_dis);
            break;

        case ObcState::PRESTREAM:
            tick_prestream(t, now_ns, req_dis);
            break;

        case ObcState::OFFBOARD_ACTIVE:
            // Jitter (SM-5) is judged on the measured cadence, not the stamp.
            tick_offboard_active(t, now_ns, actual_period, req_dis);
            break;

        case ObcState::AUTO_LANDING:
            tick_auto_landing(t, now_ns, have_telem, req_dis);
            break;

        case ObcState::DISENGAGING:
            tick_disengaging(t, now_ns);
            break;

        case ObcState::FAULT:
            // D-1: do not command the FC from FAULT. Streaming has stopped, so
            // PX4's offboard-loss failsafe (COM_OF_LOSS_T) takes the aircraft.
            // Manual restart only (POC policy).
            break;
    }
}

Geofence OffboardController::build_fence(const TelemetrySnapshot& t) const {
    Geofence f;
    if (!cfg_.fence_polygon.empty()) {
        // A fix that is absent OR stale cannot place the GPS vertices in the
        // local frame (P0-04: gps_ok alone latches true forever — the stream
        // stamp is what proves the lat/lon belong to this instant).
        //
        // Since CR-04 the engage gate refuses to leave READY in exactly this
        // case, so this branch is a belt-and-braces guard rather than the
        // policy: it must never invent a boundary, and if it is ever reached
        // the ERROR says the gate was bypassed.
        const uint64_t now = mono_now_ns();
        if (t.gps_ok == 0U ||
            age_ns(now, t.gpos_mono_ns) > cfg_.limits.telem_flag_stale_ns) {
            RLOG_ERROR("obc",
                       "fence polygon configured but no fresh GPS fix — SM-10 DISABLED");
            return f;
        }
        (void)f.set_polygon_gps(cfg_.fence_polygon, t.lat_deg, t.lon_deg, t.pos_ned_m[0],
                                t.pos_ned_m[1]);
        RLOG_INFO("obc", "SM-10 fence: %zu-vertex polygon projected at engage",
                  f.vertex_count());
        return f;
    }
    if (cfg_.fence_side_m > 0.F) {
        f.set_square(cfg_.fence_side_m, t.pos_ned_m[0], t.pos_ned_m[1]);
        RLOG_INFO("obc", "SM-10 fence: %.0f m square about the engage point",
                  static_cast<double>(cfg_.fence_side_m));
    }
    return f;
}

void OffboardController::tick_ready(const TelemetrySnapshot& t, uint64_t now_ns,
                                    bool have_telem, bool req_eng, bool req_dis) {
    // An explicit DISENGAGE in READY is the operator's acknowledgment of a
    // pilot override: it is the ONLY thing that clears the reentry latch (G5).
    if (req_dis && reentry_blocked_) {
        reentry_blocked_ = false;
        RLOG_INFO("obc", "reentry block cleared by operator disengage");
    }
    if (!req_eng) {
        return;
    }
    if (reentry_blocked_) {
        RLOG_WARN("obc",
                  "engage rejected: reentry blocked after pilot override "
                  "(send DISENGAGE to acknowledge)");
        return;
    }
    // Engage gate: telemetry must be FRESH under the same bound SM-1 uses — a
    // frozen snapshot still reading armed=1 must not pass. Field-level
    // (P0-04): the flags this gate reads must each come from a live stream —
    // engage is the one moment every safety input must be verifiably current.
    const bool telem_fresh =
        have_telem && age_ns(now_ns, t.mono_ns) <= cfg_.limits.telem_stale_ns;
    // The attitude stream is part of the gate: guidance rotates track vectors
    // FRD->NED through it every tick, so a session must not start while the
    // rotation basis is already stale (SM-1 field-level would only catch it
    // after the first bad commands went out).
    const bool flags_fresh =
        have_telem &&
        age_ns(now_ns, t.armed_mono_ns) <= cfg_.limits.telem_flag_stale_ns &&
        age_ns(now_ns, t.mode_mono_ns) <= cfg_.limits.telem_flag_stale_ns &&
        age_ns(now_ns, t.health_mono_ns) <= cfg_.limits.telem_flag_stale_ns &&
        age_ns(now_ns, t.att_mono_ns) <= cfg_.limits.telem_flag_stale_ns;
    // SM-9 preflight half: refuse to start an control session on a battery that is
    // low, unreadable or stale (fractions 0..1; FcuLink normalized at the
    // boundary).
    const bool battery_ok = SafetyMonitor::engage_battery_ok(cfg_.limits, t, now_ns);
    // SM-10 (CR-04): a CONFIGURED polygon is an operator decision that this
    // flight happens inside that boundary. Projecting it needs a fresh GPS fix,
    // and the old policy started the session with SM-10 silently disabled when
    // one was missing — removing an explicitly configured hard limit because a
    // stream faulted. Refuse instead and stay in READY; the fix usually returns
    // in seconds, and the operator can drop the polygon from the profile if they
    // genuinely want to fly without it. Profiles with no polygon are unaffected.
    const bool can_project_fence = fence_projectable(
        !cfg_.fence_polygon.empty(), t, now_ns, cfg_.limits.telem_flag_stale_ns);
    if (telem_fresh && flags_fresh && (t.armed != 0U) && (t.position_ok != 0U) &&
        battery_ok && can_project_fence) {
        offboard_started_ = false;
        landing_commanded_ = false;
        // Clear the previous control session's source state (home point, mission
        // phase, guidance filter history) before the first setpoint of this one.
        // The attitude source caches track state across ticks the same way, so
        // it gets the same session boundary — without it a second session
        // starts steering on the previous session's cached track.
        if (source_ != nullptr) {
            source_->on_engage();
        }
        if (att_source_ != nullptr) {
            att_source_->on_engage();
        }
        // Resolve the SM-10 boundary here, where telemetry has just been checked
        // fresh and position-valid and still carries GPS and local NED for the
        // same instant. Both the monitor (hard limit) and the source (steering)
        // get the same polygon, so they can never disagree about where it is.
        const Geofence fence = build_fence(t);
        safety_.set_fence(fence);
        if (source_ != nullptr) {
            source_->set_fence(fence);
        }
        transition(ObcState::PRESTREAM, now_ns, "engage");
    } else if (!telem_fresh) {
        RLOG_WARN("obc", "engage ignored: telemetry stale");
    } else if (!flags_fresh) {
        RLOG_WARN("obc",
                  "engage ignored: mode/armed/health/attitude stream stale (P0-04)");
    } else if (!can_project_fence) {
        RLOG_WARN("obc",
                  "engage ignored: fence polygon configured but no fresh GPS fix "
                  "— refusing to start without SM-10 (CR-04)");
    } else if (!battery_ok) {
        RLOG_WARN("obc", "engage ignored: battery %s (SM-9 gate %.0f%%)",
                  (t.battery_ok != 0U) ? "low" : "unknown",
                  static_cast<double>(cfg_.limits.bat_engage_min_frac) * 100.0);
    } else {
        RLOG_WARN("obc", "engage ignored: not armed / no position");
    }
}

void OffboardController::tick_prestream(const TelemetrySnapshot& t, uint64_t now_ns,
                                        bool req_dis) {
    // Pre-stream setpoints (>=1s) BEFORE requesting Offboard, as PX4 requires
    // an existing setpoint stream of the SAME type to accept the mode.
    (void)prepare_setpoint(t, now_ns); // seed (velocity or attitude); often hover
    send_setpoint();
    // Once start_offboard() is acked, PX4 is in Offboard even though we are not
    // in OFFBOARD_ACTIVE yet: every exit from here must then go through
    // DISENGAGING (stop_offboard + hold), never straight to READY, or the FC is
    // left in Offboard with no stream (offboard-loss failsafe).
    if (req_dis) {
        if (offboard_started_) {
            transition(ObcState::DISENGAGING, now_ns,
                       "disengage during prestream (offboard started)");
        } else {
            transition(ObcState::READY, now_ns, "disengage during prestream");
        }
        return;
    }
    // Safety runs in PRESTREAM too (state != OFFBOARD_ACTIVE keeps the
    // active-only checks off, leaving SM-1 staleness + SM-6 disarm): a
    // telemetry freeze or a disarm during the pre-stream window must abort the
    // engage, not ride through into the offboard start.
    const SafetyVerdict v = safety_.evaluate(t, ObcState::PRESTREAM, now_ns,
                                             /*last_period_ns=*/0,
                                             /*engage_start_ns=*/0, /*source_ok=*/true);
    last_violation_ = v.violation_mask;
    if (!v.ok) {
        RLOG_WARN("obc", "prestream safety violation mask=0x%02x", v.violation_mask);
        if (offboard_started_) {
            disengage_cause_ = v.violation_mask;
            transition(ObcState::DISENGAGING, now_ns, "prestream safety violation");
        } else {
            transition(ObcState::READY, now_ns, "prestream safety violation");
        }
        return;
    }
    if (!offboard_started_) {
        if (now_ns - state_since_ns_ >= tun::PRESTREAM_NS) {
            if (fcu_.start_offboard()) {
                offboard_started_ = true;
                offboard_started_ns_ = now_ns;
            } else {
                transition(ObcState::FAULT, now_ns, "offboard start failed");
            }
        }
        return;
    }
    // Started (FC acked). Enter OFFBOARD_ACTIVE only once telemetry confirms
    // the mode: the flight-mode stream lags the ack by up to a heartbeat, and
    // transitioning early false-fires SM-2 on the very next tick (found in
    // PX4 SITL integration).
    if (t.in_offboard != 0U) {
        safety_.capture_home(t);
        engage_start_ns_ = now_ns;
        ++engage_count_;
        transition(ObcState::OFFBOARD_ACTIVE, now_ns, "offboard confirmed");
        return;
    }
    if (now_ns - offboard_started_ns_ <= tun::OFFBOARD_CONFIRM_NS) {
        return; // still inside the confirmation window: heartbeat lag, wait
    }
    // The window expired without a confirming sample. WHY decides the exit
    // path (P1-02): a FRESH mode sample reporting another mode means the pilot
    // holds the vehicle — take the SM-2 no-command path (latch reentry, send
    // nothing) rather than fighting for the mode with stop/hold. No fresh mode
    // information at all is a link problem, which the commanded exit handles.
    // Judging this only at the TIMEOUT (never earlier) is deliberate: a
    // post-ack heartbeat can still carry the pre-ack mode, and treating that
    // lag as a takeover aborted every legitimate engage (PX4 SITL, SM-3).
    if (mode_reports_non_offboard(t, now_ns, cfg_.limits.telem_flag_stale_ns)) {
        reentry_blocked_ = true; // G5
        disengage_cause_ = SB_MODE_OVERRIDE;
        last_violation_ = SB_MODE_OVERRIDE;
        transition(ObcState::DISENGAGING, now_ns,
                   "offboard never confirmed; pilot holds another mode");
        return;
    }
    transition(ObcState::DISENGAGING, now_ns, "offboard mode not confirmed by telemetry");
}

void OffboardController::tick_offboard_active(const TelemetrySnapshot& t, uint64_t now_ns,
                                              uint64_t actual_period_ns, bool req_dis) {
    // Produce+clamp the setpoint (also gives source_ok); withhold the send until
    // safety has passed so a violation tick commands nothing.
    const bool source_ok = prepare_setpoint(t, now_ns);

    const SafetyVerdict v =
        safety_.evaluate(t, ObcState::OFFBOARD_ACTIVE, now_ns, actual_period_ns,
                         engage_start_ns_, source_ok);
    last_violation_ = v.violation_mask;

    // Latch BEFORE the operator-disengage early return below: a disengage
    // request landing on the same tick as the pilot's mode takeover must not
    // swallow the reentry block (G5).
    if ((v.violation_mask & SB_MODE_OVERRIDE) != 0U) {
        reentry_blocked_ = true; // G5
    }

    if (req_dis) {
        disengage_cause_ = v.violation_mask; // SM-2 may coincide; keep the cause
        transition(ObcState::DISENGAGING, now_ns, "operator disengage");
        return;
    }
    if (!v.ok) {
        RLOG_WARN("obc", "safety violation mask=0x%02x", v.violation_mask);
        disengage_cause_ = v.violation_mask;
        transition(ObcState::DISENGAGING, now_ns, "safety violation");
        return;
    }
    if (!cfg_.attitude_mode && source_ != nullptr && source_->requests_land()) {
        if (fcu_.land()) {
            landing_commanded_ = true;
            offboard_started_ = false;
            engage_start_ns_ = 0;
            transition(ObcState::AUTO_LANDING, now_ns, "mission requested PX4 land");
            return;
        }
        RLOG_WARN("obc", "mission land command failed; continuing controlled descent");
    }
    (void)send_setpoint(); // clamp already applied in prepare_setpoint (G4)
    // The FC refusing the stream, repeatedly, means the command link cannot be
    // trusted (P1-03): staying "active" desynchronizes the OBC state from the
    // PX4 offboard-loss failsafe that is about to fire anyway. End it.
    if (consec_send_fail_ >= tun::SETPOINT_FAIL_MAX_CONSEC) {
        RLOG_WARN("obc", "%d consecutive setpoint send failures — disengaging",
                  consec_send_fail_);
        disengage_cause_ = SB_CMD_LINK;
        last_violation_ = SB_CMD_LINK;
        transition(ObcState::DISENGAGING, now_ns, "setpoint send failures");
    }
}

void OffboardController::tick_disengaging(const TelemetrySnapshot& t, uint64_t now_ns) {
    engage_start_ns_ = 0;
    landing_commanded_ = false;
    // SM-2 (pilot took the mode, or ownership unverifiable): the pilot owns
    // the vehicle; commanding stop/hold would fight that takeover. Streaming
    // has already stopped — get out of the way immediately.
    if ((disengage_cause_ & SB_MODE_OVERRIDE) != 0U) {
        RLOG_INFO("obc", "pilot mode override respected — no stop/hold sent");
        disengage_cause_ = 0;
        // After a pilot override we do NOT return to a state that can
        // re-engage on the next ENGAGE alone: READY keeps reentry_blocked_
        // latched until an explicit operator DISENGAGE acknowledges it.
        transition(ObcState::READY, now_ns,
                   reentry_blocked_ ? "disengaged (reentry blocked)" : "disengaged");
        return;
    }
    // Commanded exit (P1-03): stop/hold results were once ignored and READY
    // was reported unconditionally — with the FC possibly still in Offboard on
    // a dead stream. Retry each command until it individually succeeds, and
    // leave only when a fresh mode sample CONFIRMS the FC is out of Offboard
    // (our stop, or the PX4 offboard-loss failsafe, either way verified).
    if (!disengage_stop_ok_) {
        disengage_stop_ok_ = fcu_.stop_offboard();
    }
    if (!disengage_hold_ok_) {
        disengage_hold_ok_ = fcu_.hold();
    }
    if (mode_reports_non_offboard(t, now_ns, cfg_.limits.telem_flag_stale_ns)) {
        disengage_cause_ = 0;
        transition(ObcState::READY, now_ns,
                   reentry_blocked_ ? "disengaged (reentry blocked)" : "disengaged");
        return;
    }
    if (now_ns - state_since_ns_ > tun::DISENGAGE_CONFIRM_NS) {
        // Neither our commands nor the failsafe produced a confirming mode
        // sample inside the window: the mode stream is effectively dead. The
        // setpoint stream is already stopped, so the PX4 offboard-loss
        // failsafe owns the vehicle (D-1) — FAULT, manual restart.
        disengage_cause_ = 0;
        transition(ObcState::FAULT, now_ns, "offboard exit unconfirmed");
    }
}

void OffboardController::tick_auto_landing(const TelemetrySnapshot& t, uint64_t now_ns,
                                           bool have_telem, bool req_dis) {
    if (req_dis) {
        fcu_.hold();
        transition(ObcState::READY, now_ns, "operator disengage during auto landing");
        return;
    }
    // Disarm is decided from rel_alt / landed, so it must never run on a frozen
    // snapshot: a telemetry freeze at 0.3 m would keep "landed" true no matter
    // where the vehicle actually is. Same bound SM-1 uses.
    const bool telem_fresh =
        have_telem && age_ns(now_ns, t.mono_ns) <= cfg_.limits.telem_stale_ns;
    if (!telem_fresh) {
        last_violation_ = SB_TELEM_STALE;
        RLOG_WARN("obc", "auto landing: telemetry stale, withholding disarm");
        return;
    }
    // Every field below is proved fresh by its OWN stream stamp inside
    // disarm_authorized() — t.mono_ns above covers only position/velocity, and
    // rel_alt_m / landed / armed each arrive on their own callbacks (CR-01).
    const bool source_requests = (source_ != nullptr) && source_->requests_disarm(t);
    if (disarm_authorized(t, now_ns, cfg_.limits.telem_flag_stale_ns, source_requests,
                          tun::DISARM_MAX_ALT_M)) {
        (void)fcu_.disarm();
    }
    // Leave AUTO_LANDING only once the vehicle is ACTUALLY disarmed — a disarm
    // that was commanded but refused must keep the landing watch running rather
    // than report completion. Operator DISENGAGE (handled above) is the way out
    // if the FC never disarms.
    if (landing_complete(t, now_ns, cfg_.limits.telem_flag_stale_ns)) {
        landing_commanded_ = false;
        transition(ObcState::READY, now_ns, "auto landing complete");
    }
}

// See the header contract: runs on the main thread strictly AFTER the control
// thread has joined, so reading offboard_started_ / touching fcu_ is race-free.
void OffboardController::shutdown_hold() {
    const ObcState s = state_.load(std::memory_order_relaxed);
    // A shutdown can land mid-DISENGAGING too (P1-03): if the commanded exit
    // was not completed — and it is not a pilot-override case, which must
    // never be fought — the stop/hold attempt must not be skipped just
    // because the state already says "disengaging".
    const bool exiting_uncommanded = (s == ObcState::DISENGAGING) &&
                                     ((disengage_cause_ & SB_MODE_OVERRIDE) == 0U) &&
                                     (!disengage_stop_ok_ || !disengage_hold_ok_);
    const bool engaged = (s == ObcState::OFFBOARD_ACTIVE) ||
                         ((s == ObcState::PRESTREAM) && offboard_started_) ||
                         exiting_uncommanded;
    if (!engaged) {
        return;
    }
    RLOG_WARN("obc", "shutdown during control session (%s) — commanding stop+hold",
              to_string(s));
    fcu_.stop_offboard();
    fcu_.hold();
}

void OffboardController::publish_status(ShmSeqSlot<ObcStatus>& bus, uint64_t now_ns) {
    ObcStatus s{};
    s.mono_ns = now_ns;
    s.state = state_.load(std::memory_order_relaxed);
    s.violation_mask = last_violation_;
    s.loop_jitter_ms = safety_.last_jitter_ms();
    s.engage_count = engage_count_;
    bus.write(s);
}

void OffboardController::publish_gps(ShmSeqSlot<GpsSample>& bus, uint64_t now_ns) {
    TelemetrySnapshot t{};
    GpsSample g{};
    g.mono_ns = now_ns;
    // gps_ok latches true once a fix is ever seen, so it alone cannot say the
    // coordinates belong to THIS instant. Stamping a frozen lat/lon with the
    // current time republished it as a valid present-tense fix every tick, and
    // the recording overlay burned those stale coordinates into evidence video
    // for as long as the stream stayed down (review CR-08). Publish the sample
    // with fix_ok=0 instead: consumers already treat that as "no position",
    // which is the honest answer.
    const bool have = fcu_.snapshot(t);
    const bool fix_fresh =
        have && (t.gps_ok != 0U) &&
        age_ns(now_ns, t.gpos_mono_ns) <= cfg_.limits.telem_flag_stale_ns;
    if (fix_fresh) {
        g.lat_deg = t.lat_deg;
        g.lon_deg = t.lon_deg;
        g.alt_m = t.abs_alt_m;
        g.fix_ok = 1;
    }
    bus.write(g);
}

} // namespace riposte
