// riposte-obc (L3 control + L4 guidance + L5 authorization) — SDD-001 / SAD-001.
//
// Single deterministic control thread at 20 Hz. The command channel runs on the
// main thread and only latches requests into the controller; every state
// transition and every setpoint send happens inside the control tick (G1/G2).

#include "Geofence.h"
#include "OffboardController.h"
#include "SafetyMonitor.h"
#include "SetpointStreamer.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "riposte/CommandBus.h"
#include "riposte/Config.h"
#include "riposte/Log.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"
#include "sources/AttitudeTrackingSource.h"
#include "sources/BalloonPatrolSource.h"
#include "sources/GuidanceSource.h"
#include "sources/IAttitudeSource.h"
#include "sources/ISetpointSource.h"
#include "sources/MissionSource.h"
#include "sources/TestAttitudeSource.h"
#include "sources/TestPatternSource.h"

using namespace riposte;

namespace {
// Cooperative-shutdown flag (G7.7): set by the signal handler, observed by the
// control loop. Mutable by design — a global is the only channel a signal
// handler can reach.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_run{true};
void on_signal(int /*unused*/) {
    g_run.store(false);
}

// `err` is set (and false returned) when a duration is unusable — see
// Config::get_duration_ns. Returning the limits by reference keeps the caller's
// "refuse to start on a bad safety value" path in one place (CR-02).
bool limits_from_config(const Config& c, SafetyMonitor::Limits& out, std::string& err) {
    SafetyMonitor::Limits l;
    l.vmax_h =
        static_cast<float>(c.get_double("safety.vmax_h", tun::VMAX_HORIZONTAL_MPS));
    l.vmax_v = static_cast<float>(c.get_double("safety.vmax_v", tun::VMAX_VERTICAL_MPS));
    l.geofence_r =
        static_cast<float>(c.get_double("safety.geofence_r", tun::GEOFENCE_RADIUS_M));
    l.alt_min = static_cast<float>(c.get_double("safety.alt_min", tun::ALT_MIN_M));
    l.alt_max = static_cast<float>(c.get_double("safety.alt_max", tun::ALT_MAX_M));
    l.telem_stale_ns = tun::TELEM_STALE_NS;
    // SM-1 field-level bound for the low-rate streams (OBC-SDD §6, P0-04).
    if (!c.get_duration_ns("safety.telem_flag_stale_s",
                           static_cast<double>(tun::TELEM_FLAG_STALE_NS) * 1e-9,
                           l.telem_flag_stale_ns, err) ||
        !c.get_duration_ns("safety.engage_timebox_s",
                           static_cast<double>(tun::ENGAGE_TIMEBOX_NS) * 1e-9,
                           l.engage_timebox_ns, err)) {
        return false;
    }
    l.jitter_budget_frac = tun::JITTER_BUDGET_FRAC;
    l.jitter_max_consec = tun::JITTER_MAX_CONSEC;
    l.period_ns = tun::CONTROL_PERIOD_NS;
    l.att_max_tilt_deg = static_cast<float>(
        c.get_double("safety.att_max_tilt_deg", tun::ATT_MAX_TILT_DEG));
    l.att_min_thrust =
        static_cast<float>(c.get_double("safety.att_min_thrust", tun::ATT_MIN_THRUST));
    l.att_max_thrust =
        static_cast<float>(c.get_double("safety.att_max_thrust", tun::ATT_MAX_THRUST));
    // SM-9 battery gate: fractions 0..1 (FcuLink already normalized MAVSDK v3's
    // 0..100 percent at the boundary). 0 disables the respective check.
    l.bat_engage_min_frac = static_cast<float>(
        c.get_double("safety.bat_engage_min_frac", tun::BAT_ENGAGE_MIN_FRAC));
    l.bat_land_frac =
        static_cast<float>(c.get_double("safety.bat_land_frac", tun::BAT_LAND_FRAC));
    out = l;
    return true;
}

bool is_attitude_mode(const std::string& src) {
    return src == "attitude" || src == "attitude_test";
}

bool patrol_params_from_config(const Config& c, BalloonPatrolSource::Params& out,
                               std::string& err) {
    BalloonPatrolSource::Params pp;
    pp.patrol_alt_m = static_cast<float>(c.get_double("patrol.alt_m", 5.0));
    pp.approach_speed_mps =
        static_cast<float>(c.get_double("patrol.approach_speed_mps", 3.0));
    pp.search_yaw_rate_rps =
        static_cast<float>(c.get_double("patrol.search_yaw_rate_rps", 0.5));
    pp.soft_margin_m = static_cast<float>(c.get_double("patrol.soft_margin_m", 10.0));
    pp.turn_margin_m = static_cast<float>(c.get_double("patrol.turn_margin_m", 3.0));
    pp.reach_range_m = static_cast<float>(c.get_double("patrol.reach_range_m", 3.0));
    pp.alt_rate_mps = static_cast<float>(c.get_double("patrol.alt_rate_mps", 1.0));
    // Mirror the SM-3 altitude band (same keys/defaults limits_from_config
    // reads, same mirror MissionSource gets for its ceiling). Without it the
    // approach clamp runs on the COMPILED [3, 15] m defaults: with a lower
    // configured ceiling, chasing a high target commands a climb straight into
    // an SM-3 violation instead of clamping at the boundary.
    pp.min_alt_m = static_cast<float>(c.get_double("safety.alt_min", tun::ALT_MIN_M));
    pp.max_alt_m = static_cast<float>(c.get_double("safety.alt_max", tun::ALT_MAX_M));
    if (!c.get_duration_ns("patrol.visited_cooldown_s", 30.0, pp.visited_cooldown_ns,
                           err) ||
        !c.get_duration_ns("patrol.turn_dwell_s", 3.0, pp.turn_dwell_ns, err)) {
        return false;
    }
    // Range-check the numerics the durations beside them already get: a sign
    // typo in a speed/rate inverts boundary-recovery or landing motion with no
    // diagnostic (SafetyMonitor::clamp bounds magnitude only).
    if (!validate(pp, err)) {
        return false;
    }
    out = pp;
    return true;
}

// Velocity-mode setpoint source (hover/constant/circle test patterns, or guidance).
std::unique_ptr<ISetpointSource> make_velocity_source(const Config& cfg,
                                                      const std::string& src) {
    if (src == "guidance") {
        return std::make_unique<GuidanceSource>();
    }
    if (src == "mission") {
        MissionSource::Params mp;
        mp.takeoff_alt_m =
            static_cast<float>(cfg.get_double("mission.takeoff_alt_m", 20.0));
        mp.takeoff_rate_mps =
            static_cast<float>(cfg.get_double("mission.takeoff_rate_mps", 1.5));
        mp.cruise_speed_mps =
            static_cast<float>(cfg.get_double("mission.cruise_speed_mps", 5.0));
        mp.hover_radius_m =
            static_cast<float>(cfg.get_double("mission.hover_radius_m", 3.0));
        mp.search_radius_m =
            static_cast<float>(cfg.get_double("mission.search_radius_m", 12.0));
        mp.land_rate_mps =
            static_cast<float>(cfg.get_double("mission.land_rate_mps", 0.8));
        mp.land_alt_m = static_cast<float>(cfg.get_double("mission.land_alt_m", 0.35));
        std::string derr;
        if (!cfg.get_duration_ns("mission.hover_settle_s", 3.0, mp.hover_settle_ns,
                                 derr) ||
            !cfg.get_duration_ns("mission.operator_timeout_s", 60.0,
                                 mp.operator_timeout_ns, derr) ||
            !cfg.get_duration_ns("mission.target_stale_s", 120.0, mp.target_stale_ns,
                                 derr)) {
            RLOG_ERROR("obc", "config: %s", derr.c_str());
            return nullptr; // refuse to start rather than run an unbounded timer
        }
        // Mirror the SM-3 ceiling so a high cue is clamped rather than flown
        // into a geofence violation.
        mp.max_alt_m =
            static_cast<float>(cfg.get_double("safety.alt_max", tun::ALT_MAX_M));
        // Range-check the numerics (speeds/rates/radii) — the durations above
        // are already refused at the config boundary, but a sign typo in
        // cruise_speed_mps or land_rate_mps inverted transit/landing motion
        // with no diagnostic at all.
        if (!validate(mp, derr)) {
            RLOG_ERROR("obc", "config: %s", derr.c_str());
            return nullptr;
        }
        return std::make_unique<MissionSource>(mp);
    }
    if (src == "balloon") {
        BalloonPatrolSource::Params pp;
        std::string derr;
        if (!patrol_params_from_config(cfg, pp, derr)) {
            RLOG_ERROR("obc", "config: %s", derr.c_str());
            return nullptr;
        }
        return std::make_unique<BalloonPatrolSource>(pp);
    }
    if (src == "circle") {
        return std::make_unique<TestPatternSource>(TestPatternSource::Pattern::CIRCLE);
    }
    if (src == "constant") {
        return std::make_unique<TestPatternSource>(TestPatternSource::Pattern::CONSTANT);
    }
    return std::make_unique<TestPatternSource>(TestPatternSource::Pattern::HOVER);
}

// Attitude-mode (pitch/yaw) source: fixed test command or track-driven tracking.
std::unique_ptr<IAttitudeSource> make_attitude_source(const Config& c,
                                                      const std::string& src) {
    if (src == "attitude_test") {
        TestAttitudeSource::Params ap;
        ap.roll_deg = static_cast<float>(c.get_double("obc.att_roll_deg", 0.0));
        ap.pitch_deg = static_cast<float>(c.get_double("obc.att_pitch_deg", 0.0));
        ap.yaw_deg = static_cast<float>(c.get_double("obc.att_yaw_deg", 0.0));
        ap.thrust =
            static_cast<float>(c.get_double("obc.att_thrust", tun::ATT_HOVER_THRUST));
        return std::make_unique<TestAttitudeSource>(ap);
    }
    AttitudeTrackingSource::Params pp;
    pp.hover_thrust =
        static_cast<float>(c.get_double("obc.att_hover_thrust", tun::ATT_HOVER_THRUST));
    pp.track_pitch_deg = static_cast<float>(
        c.get_double("obc.att_track_pitch_deg", tun::ATT_TRACK_PITCH_DEG));
    pp.thrust_elev_gain = static_cast<float>(
        c.get_double("obc.att_thrust_elev_gain", tun::ATT_THRUST_ELEV_GAIN));
    return std::make_unique<AttitudeTrackingSource>(pp);
}
} // namespace

// G16.6 deviation: linear process wiring; the logic already lives in helpers
// (G16.4/G16.6) NOLINTNEXTLINE(readability-function-size)
int main(int argc, char** argv) {
    (void)std::signal(SIGINT, on_signal); // prior handler is irrelevant here
    (void)std::signal(SIGTERM, on_signal);

    Config cfg;
    if (argc > 1 && !cfg.load(argv[1])) {
        // Fail-closed (AGENTS §7.9): the named config carries the safety limits,
        // fence and operator token — running on compiled defaults instead of
        // what the operator configured is exactly the silent-downgrade the
        // startup gates exist to prevent.
        RLOG_ERROR("obc", "cannot load config %s — refusing to start on defaults",
                   argv[1]);
        return 1;
    }

    // Source selection: hover/constant/circle/guidance drive the velocity path;
    // attitude/attitude_test drive the pitch/yaw attitude path.
    const std::string src = cfg.get_str("obc.source", "hover");
    const bool att_mode = is_attitude_mode(src);
    std::unique_ptr<ISetpointSource> vel_source;
    std::unique_ptr<IAttitudeSource> att_source;
    if (att_mode) {
        att_source = make_attitude_source(cfg, src);
    } else {
        vel_source = make_velocity_source(cfg, src);
    }
    if ((att_mode ? att_source == nullptr : vel_source == nullptr)) {
        return 1; // the factory already logged the specific config error
    }
    RLOG_INFO("obc", "setpoint source = %s (%s mode)",
              att_mode ? att_source->name() : vel_source->name(),
              att_mode ? "attitude" : "velocity");

    OffboardController::Config occ;
    occ.connection_url = cfg.get_str("obc.connection_url", "udpin://0.0.0.0:14540");
    // Reject an out-of-range safety configuration before arming anything
    // (AGENTS §7.9): flight-safety limits must never run on a bad value. The
    // durations are checked while still doubles (CR-02) — a negative one used
    // to become a huge unsigned bound that passed validate() and disabled the
    // monitor it feeds.
    {
        std::string err;
        if (!limits_from_config(cfg, occ.limits, err)) {
            RLOG_ERROR("obc", "invalid safety config: %s", err.c_str());
            return 1;
        }
        if (!validate(occ.limits, err)) {
            RLOG_ERROR("obc", "invalid safety config: %s", err.c_str());
            return 1;
        }
    }
    occ.operator_token = cfg.get_str("obc.operator_token", "");
    occ.attitude_mode = att_mode;
    // SM-10 boundary vertices (DUALEO-REQ T-1). Fail-closed (AGENTS §7.9): a
    // configured polygon that does not parse is a refused startup, NEVER a
    // silently disabled fence. Projection into local NED happens at ENGAGE,
    // inside the controller, against a telemetry sample carrying both
    // coordinate representations of one instant.
    {
        std::string err;
        if (!Geofence::parse_polygon(cfg.get_str("fence.polygon", ""), occ.fence_polygon,
                                     err)) {
            RLOG_ERROR("obc", "invalid fence.polygon: %s", err.c_str());
            return 1;
        }
    }
    occ.fence_side_m = static_cast<float>(cfg.get_double("fence.side_m", 0.0));

    OffboardController controller;
    if (!controller.init(occ, vel_source.get(), att_source.get())) {
        RLOG_ERROR("obc", "controller init failed");
        return 1;
    }

    // Status/GPS buses out. Unlike the seeker's TrackBus these are not flight
    // inputs — a failure degrades supervision and the recording overlay, so it
    // is logged and the flight continues rather than aborting the process.
    ShmSeqSlot<ObcStatus> status_bus;
    if (!status_bus.open(tun::SHM_OBC_STATUS, ShmSeqSlot<ObcStatus>::Role::WRITER)) {
        RLOG_WARN("obc", "status bus open failed (%s) — supervisor blind",
                  tun::SHM_OBC_STATUS);
    }

    // GPS bus out (seeker stamps it onto recorded video — logging only).
    ShmSeqSlot<GpsSample> gps_bus;
    if (!gps_bus.open(tun::SHM_GPS, ShmSeqSlot<GpsSample>::Role::WRITER)) {
        RLOG_WARN("obc", "gps bus open failed (%s) — video overlay without GPS",
                  tun::SHM_GPS);
    }

    // Command channel (engage/disengage). Polled from the control tick to avoid
    // cross-thread FSM mutation; here we only forward datagrams into the
    // controller's atomic request latches.
    const std::string cmd_socket = cfg.get_str("obc.cmd_socket", tun::OBC_CMD_SOCKET);
    CommandServer cmd_srv;
    if (!cmd_srv.open(cmd_socket)) {
        RLOG_WARN("obc", "command socket open failed (%s) — engage disabled",
                  cmd_socket.c_str());
    }

    // Control thread: 20 Hz fixed, SCHED_FIFO, pinned to a big core.
    SetpointStreamer::Params sp;
    sp.period_ns = tun::CONTROL_PERIOD_NS;
    sp.rt_priority = static_cast<int>(cfg.get_int("obc.rt_priority", 80));
    sp.cpu_affinity = static_cast<int>(cfg.get_int("obc.cpu_affinity", -1));

    // Every config key has been read by this point. A value that was PRESENT
    // but unparseable silently became its compiled default above — for a
    // safety limit (e.g. a legacy inline ';' comment on safety.alt_max) that
    // is a fail-open the validation gates cannot see, because the compiled
    // default itself is in range. Surface each one and refuse to start
    // (AGENTS §7.9: a mistyped safety value is an operator error to surface).
    if (!cfg.parse_failures().empty()) {
        for (const auto& e : cfg.parse_failures()) {
            RLOG_ERROR("obc", "config: %s", e.c_str());
        }
        RLOG_ERROR("obc", "refusing to start on unparseable config values");
        return 1;
    }

    std::thread control([&] {
        SetpointStreamer::run(
            sp,
            [&](uint64_t now_ns) {
                // Dispatch EVERY queued valid command this tick, bounded
                // (P1-01): with one command per tick, a >20 Hz flood of
                // structurally-valid bad-token ENGAGEs starves the valid
                // DISENGAGE queued behind them. The FSM latches requests, so
                // multiple dispatches per tick are safe, and the tick handler's
                // existing priority (disengage wins) resolves same-tick pairs.
                for (int i = 0; i < CommandServer::MAX_DRAIN_PER_POLL; ++i) {
                    const auto c = cmd_srv.poll();
                    if (!c) {
                        break;
                    }
                    controller.on_command(*c);
                }
                controller.tick(now_ns);
                controller.publish_status(status_bus, now_ns);
                controller.publish_gps(gps_bus, now_ns);
            },
            g_run);
    });

    // Main thread: 1 Hz human-readable state line.
    while (g_run.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        RLOG_INFO("obc", "state=%s", to_string(controller.state()));
    }

    if (control.joinable()) {
        control.join();
    }
    // SIGINT/SIGTERM mid-control session (e.g. systemctl stop): the control thread
    // is gone, so nothing streams anymore. Best-effort commanded Hold now beats
    // leaving PX4 to its offboard-loss failsafe. No-op unless engaged.
    controller.shutdown_hold();
    RLOG_INFO("obc", "shutdown");
    return 0;
}
