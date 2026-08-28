#include "FcuLink.h"

#ifndef RIPOSTE_WITH_MAVSDK
#include <algorithm> // std::max — SIL vehicle model only
#endif
#include <cmath>
#include <memory>
#include <string>

#include "riposte/Clock.h"
#include "riposte/Log.h"
#include "riposte/SeqSlot.h"
#include "riposte/Types.h"

// Two implementations selected at compile time:
//   RIPOSTE_WITH_MAVSDK=ON  -> real MAVSDK (target / SITL)
//   otherwise               -> SIL stub vehicle (dev PC, no PX4)
// The header is identical to both; the control logic above never knows which.

#ifdef RIPOSTE_WITH_MAVSDK
#include <mavsdk/component_type.h>
#include <mavsdk/connection_result.h>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

#include <atomic>
#include <exception>
#include <mutex>
#endif

namespace riposte {

#ifdef RIPOSTE_WITH_MAVSDK
// ===========================================================================
// Real MAVSDK implementation (target / PX4 SITL).
//
// Threading contract (G1): MAVSDK runs its own callback threads. Every
// telemetry callback only updates `shadow` under `mtx` and republishes it into
// the LocalSeqSlot; the 20 Hz control thread reads exclusively through
// snapshot() (torn-read safe). No sync MAVSDK call is ever made from a MAVSDK
// callback (deadlocks — see mavsdk.h note; G6.9).
// ===========================================================================
namespace {
constexpr float RAD_TO_DEG = 57.295779513F;
constexpr double TELEM_RATE_HZ = 40.0; // 2x control rate so SM-1 has margin

// Every float crossing the MAVLink boundary must be finite before it can touch
// the snapshot (OBC-SDD §6, P0-04): NaN coordinates make every geofence
// comparison false and sail straight through the limits. A non-finite sample
// is dropped WHOLE — its stream stamp does not advance, so persistent garbage
// ages into staleness and trips the field-level SM-1 checks instead.
template <typename... F>
bool all_finite(F... v) {
    return (std::isfinite(v) && ...);
}
} // namespace

struct FcuLink::Impl {
    // Destruction order is load-bearing (G5.4): members are destroyed in
    // reverse declaration order. Plugins (declared last) must die before
    // `mavsdk` (their owner session), and `mavsdk` — which stops the callback
    // threads — must die before `mtx`/`shadow`/`telem`, which those callbacks
    // touch. Hence: slots first, mavsdk next, system/plugins last.
    std::mutex mtx;
    TelemetrySnapshot shadow{};
    LocalSeqSlot<TelemetrySnapshot> telem;
    std::atomic<bool> discovered{false};
    // Claims the wiring slot exactly once. Kept separate from `discovered` so
    // that flag can be published AFTER wiring completes (review CR-06): it is
    // what the FSM polls, and it used to go true before the subscriptions
    // existed.
    std::atomic<bool> claimed{false};

    mavsdk::Mavsdk mavsdk{
        mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::CompanionComputer}};
    mavsdk::Mavsdk::NewSystemHandle new_system_handle;
    std::shared_ptr<mavsdk::System> system;
    std::unique_ptr<mavsdk::Telemetry> telemetry;
    std::unique_ptr<mavsdk::Offboard> offboard;
    std::unique_ptr<mavsdk::Action> action;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    // Closing the new-system subscription is the FIRST thing destruction does
    // (review CR-06). Declaration order alone does not close this hole: the
    // callback runs on a mavsdk thread and assigns `system` and the plugins,
    // which are destroyed BEFORE `mavsdk` — the member whose destructor stops
    // those threads. A discovery landing in that window would write through
    // pointers that are already gone. Unsubscribing here means the callback can
    // no longer fire once destruction has begun.
    ~Impl() { mavsdk.unsubscribe_on_new_system(new_system_handle); }

    // Publish the shadow into the seqslot. Callers hold `mtx`.
    void publish_locked() { telem.write(shadow); }

    // Registration-only wiring; runs inside the new-system callback, so it
    // must stay non-blocking (subscribe_* and *_async are; sync calls are not).
    // Split in two (each is one idea and each stayed over the size limit as a
    // single function): the high-rate motion streams, then the ~1 Hz flag
    // streams whose staleness SM-1 judges field by field.
    void wire_motion_subscriptions() {
        telemetry = std::make_unique<mavsdk::Telemetry>(system);
        offboard = std::make_unique<mavsdk::Offboard>(system);
        action = std::make_unique<mavsdk::Action>(system);

        telemetry->set_rate_position_velocity_ned_async(
            TELEM_RATE_HZ, [](mavsdk::Telemetry::Result r) {
                if (r != mavsdk::Telemetry::Result::Success) {
                    RLOG_WARN("fcu",
                              "set_rate(position_velocity_ned) failed (%d) — "
                              "SM-1 staleness margin reduced",
                              static_cast<int>(r));
                }
            });

        // Every callback stamps ITS OWN stream (OBC-SDD §6 SM-1 field-level,
        // P0-04): one shared stamp once let a live position feed hide stopped
        // mode/armed/altitude/battery streams from every freshness check.
        telemetry->subscribe_position_velocity_ned(
            [this](mavsdk::Telemetry::PositionVelocityNed pv) {
                if (!all_finite(pv.position.north_m, pv.position.east_m,
                                pv.position.down_m, pv.velocity.north_m_s,
                                pv.velocity.east_m_s, pv.velocity.down_m_s)) {
                    return; // dropped whole; the stream ages into SM-1
                }
                std::lock_guard<std::mutex> const lock(mtx);
                shadow.pos_ned_m[0] = pv.position.north_m;
                shadow.pos_ned_m[1] = pv.position.east_m;
                shadow.pos_ned_m[2] = pv.position.down_m;
                shadow.vel_ned_mps[0] = pv.velocity.north_m_s;
                shadow.vel_ned_mps[1] = pv.velocity.east_m_s;
                shadow.vel_ned_mps[2] = pv.velocity.down_m_s;
                shadow.mono_ns = mono_now_ns();
                publish_locked();
            });
        telemetry->subscribe_attitude_euler([this](mavsdk::Telemetry::EulerAngle e) {
            if (!all_finite(e.roll_deg, e.pitch_deg, e.yaw_deg)) {
                return;
            }
            std::lock_guard<std::mutex> const lock(mtx);
            shadow.roll_rad = e.roll_deg / RAD_TO_DEG;
            shadow.pitch_rad = e.pitch_deg / RAD_TO_DEG;
            shadow.yaw_rad = e.yaw_deg / RAD_TO_DEG;
            shadow.att_mono_ns = mono_now_ns();
            publish_locked();
        });
        telemetry->subscribe_position([this](mavsdk::Telemetry::Position p) {
            if (!all_finite(p.relative_altitude_m, p.absolute_altitude_m) ||
                !all_finite(p.latitude_deg, p.longitude_deg)) {
                return;
            }
            std::lock_guard<std::mutex> const lock(mtx);
            shadow.rel_alt_m = p.relative_altitude_m;
            shadow.lat_deg = p.latitude_deg;
            shadow.lon_deg = p.longitude_deg;
            shadow.abs_alt_m = p.absolute_altitude_m;
            // Last received value only — consumers must pair it with a fresh
            // gpos_mono_ns; the flag alone would latch true forever (P0-04).
            shadow.gps_ok = 1;
            shadow.gpos_mono_ns = mono_now_ns();
            publish_locked();
        });
    }

    void wire_flag_subscriptions() {
        telemetry->subscribe_armed([this](bool armed) {
            std::lock_guard<std::mutex> const lock(mtx);
            shadow.armed = armed ? 1 : 0;
            shadow.armed_mono_ns = mono_now_ns();
            publish_locked();
        });
        telemetry->subscribe_landed_state([this](mavsdk::Telemetry::LandedState ls) {
            std::lock_guard<std::mutex> const lock(mtx);
            shadow.landed = (ls == mavsdk::Telemetry::LandedState::OnGround) ? 1 : 0;
            shadow.landed_mono_ns = mono_now_ns();
            publish_locked();
        });
        telemetry->subscribe_flight_mode([this](mavsdk::Telemetry::FlightMode fm) {
            std::lock_guard<std::mutex> const lock(mtx);
            shadow.in_offboard = (fm == mavsdk::Telemetry::FlightMode::Offboard) ? 1 : 0;
            shadow.mode_mono_ns = mono_now_ns();
            publish_locked();
        });
        telemetry->subscribe_health([this](mavsdk::Telemetry::Health h) {
            std::lock_guard<std::mutex> const lock(mtx);
            shadow.position_ok = h.is_local_position_ok ? 1 : 0;
            shadow.health_mono_ns = mono_now_ns();
            publish_locked();
        });
        telemetry->subscribe_battery([this](mavsdk::Telemetry::Battery b) {
            // MAVSDK v3: remaining_percent is 0..100 PERCENT (v1/v2 was a 0..1
            // fraction). Normalize here, at the boundary, so the rest of the
            // system only ever sees a fraction — the same unit mixup silently
            // defeated the battery gates in the flight-test app until PX4 SITL
            // caught it (2026-07-17). Out-of-range/NaN marks the battery
            // unknown rather than clamping: SM-9 treats unknown conservatively
            // at engage and leniently in flight.
            std::lock_guard<std::mutex> const lock(mtx);
            const float pct = b.remaining_percent;
            if (std::isfinite(pct) && pct >= 0.F && pct <= 100.F) {
                shadow.battery_frac = pct / 100.F;
                shadow.battery_ok = 1;
            } else {
                shadow.battery_ok = 0;
            }
            shadow.bat_mono_ns = mono_now_ns();
            publish_locked();
        });
        system->subscribe_is_connected([this](bool connected) {
            std::lock_guard<std::mutex> const lock(mtx);
            shadow.connected = connected ? 1 : 0;
            publish_locked();
            if (!connected) {
                RLOG_WARN("fcu", "FC heartbeat lost — SM-1 will disengage on staleness");
            }
        });

        {
            std::lock_guard<std::mutex> const lock(mtx);
            shadow.connected = 1;
            publish_locked();
        }
    }

    void wire_subscriptions() {
        wire_motion_subscriptions();
        wire_flag_subscriptions();
    }
};

FcuLink::FcuLink() : impl_(std::make_unique<Impl>()) {}
FcuLink::~FcuLink() = default;

bool FcuLink::connect(const std::string& url) {
    RLOG_INFO("fcu", "connect %s (MAVSDK)", url.c_str());
    const auto res = impl_->mavsdk.add_any_connection(url);
    if (res != mavsdk::ConnectionResult::Success) {
        RLOG_ERROR("fcu", "add_any_connection('%s') failed (%d) — FC unreachable",
                   url.c_str(), static_cast<int>(res));
        return false;
    }
    // Discovery is asynchronous: the FSM polls discovered() from CONNECTING and
    // applies its own CONNECT_TIMEOUT_NS (G7.3 — no unbounded wait here).
    impl_->new_system_handle = impl_->mavsdk.subscribe_on_new_system([this]() {
        // Runs on a mavsdk thread. An exception escaping here would cross the
        // vendor's callback boundary — at best terminate, at worst whatever the
        // library does with a throwing callback — so the failure is contained
        // as "not discovered" and the FSM's connect timeout handles it (CR-06).
        try {
            for (const auto& sys : impl_->mavsdk.systems()) {
                if (!sys->has_autopilot() || !sys->is_connected()) {
                    continue;
                }
                // Wire exactly once; later systems (e.g. GCS peers) are ignored.
                bool expected = false;
                if (!impl_->claimed.compare_exchange_strong(expected, true)) {
                    return;
                }
                impl_->system = sys;
                impl_->wire_subscriptions();
                // Published only now, with release ordering: discovered() is
                // what the FSM polls to leave CONNECTING, and setting it before
                // the subscriptions existed advertised a link that could not
                // yet deliver telemetry.
                impl_->discovered.store(true, std::memory_order_release);
                RLOG_INFO("fcu", "autopilot discovered");
                return;
            }
        } catch (const std::exception& e) {
            RLOG_ERROR("fcu", "discovery callback threw (%s); link not established",
                       e.what());
        } catch (...) {
            RLOG_ERROR("fcu", "discovery callback threw; link not established");
        }
    });
    return true;
}
bool FcuLink::discovered() const {
    return impl_->discovered.load(std::memory_order_acquire);
}

bool FcuLink::snapshot(TelemetrySnapshot& out) const {
    return impl_->telem.read(out);
}

// Non-blocking: MAVSDK queues the setpoint message immediately (no ack wait),
// so this is safe inside the 20 Hz control tick.
bool FcuLink::send_velocity_ned(const VelocitySetpointNed& sp) {
    if (!impl_->offboard) {
        return false;
    }
    const mavsdk::Offboard::VelocityNedYaw v{sp.vn_mps, sp.ve_mps, sp.vd_mps,
                                             sp.yaw_rad * RAD_TO_DEG};
    return impl_->offboard->set_velocity_ned(v) == mavsdk::Offboard::Result::Success;
}

// Attitude offboard setpoint (roll/pitch/yaw deg + thrust). Non-blocking like
// send_velocity_ned; PX4 accepts either stream, not both in one session.
bool FcuLink::send_attitude(const AttitudeSetpoint& sp) {
    if (!impl_->offboard) {
        return false;
    }
    const mavsdk::Offboard::Attitude a{sp.roll_deg, sp.pitch_deg, sp.yaw_deg, sp.thrust};
    return impl_->offboard->set_attitude(a) == mavsdk::Offboard::Result::Success;
}

// start/stop/hold are sync (wait for FC ack, bounded by MAVSDK's own timeout).
// They run in the control thread only at engage/disengage boundaries; the one
// delayed tick they may cause is visible to SM-5 but below its 3-consecutive
// trigger, and correctness beats a single-tick jitter at a mode change.
bool FcuLink::start_offboard() {
    if (!impl_->offboard) {
        return false;
    }
    const auto r = impl_->offboard->start();
    if (r != mavsdk::Offboard::Result::Success) {
        RLOG_ERROR("fcu", "offboard start failed (%d) — engage aborted",
                   static_cast<int>(r));
        return false;
    }
    return true;
}
bool FcuLink::stop_offboard() {
    if (!impl_->offboard) {
        return false;
    }
    const auto r = impl_->offboard->stop();
    if (r != mavsdk::Offboard::Result::Success) {
        RLOG_WARN("fcu", "offboard stop failed (%d) — falling back to hold()",
                  static_cast<int>(r));
        return false;
    }
    return true;
}
bool FcuLink::hold() {
    if (!impl_->action) {
        return false;
    }
    const auto r = impl_->action->hold();
    if (r != mavsdk::Action::Result::Success) {
        RLOG_ERROR("fcu", "hold failed (%d) — stream stops, PX4 failsafe takes over",
                   static_cast<int>(r));
        return false;
    }
    return true;
}

bool FcuLink::land() {
    if (!impl_->action) {
        return false;
    }
    const auto r = impl_->action->land();
    if (r != mavsdk::Action::Result::Success) {
        RLOG_ERROR("fcu", "land failed (%d)", static_cast<int>(r));
        return false;
    }
    return true;
}

bool FcuLink::disarm() {
    if (!impl_->action) {
        return false;
    }
    const auto r = impl_->action->disarm();
    if (r != mavsdk::Action::Result::Success) {
        RLOG_WARN("fcu", "disarm failed (%d)", static_cast<int>(r));
        return false;
    }
    return true;
}
void FcuLink::sil_step(double /*dt_s*/) {} // no-op with a real FC

#else
// ===========================================================================
// SIL stub: minimal single-integrator vehicle so the FSM/safety loop is fully
// exercisable on a dev PC. Velocity setpoints move a virtual position in NED.
// ===========================================================================
namespace {
// SIL attitude->motion gains: a crude kinematic model so the attitude control
// path is exercisable on a dev PC (real dynamics come from PX4 SIH in SITL).
constexpr float SIL_PITCH_TO_MPS = 0.20F; // fwd speed per deg of nose-down pitch
constexpr float SIL_ROLL_TO_MPS = 0.20F;  // right speed per deg of right roll
constexpr float SIL_THRUST_TO_MPS = 6.0F; // climb rate per unit thrust above hover
constexpr float SIL_HOVER = 0.50F;        // hover thrust
constexpr float SIL_DEG_TO_RAD = 0.01745329252F;
} // namespace

struct FcuLink::Impl {
    LocalSeqSlot<TelemetrySnapshot> telem;
    TelemetrySnapshot state{};
    VelocitySetpointNed cmd{};
    AttitudeSetpoint att_cmd{};
    bool att_mode = false; // last setpoint was attitude, not velocity
    bool offboard = false;
    bool connected = false;
    bool landing = false;

    void publish() {
        // The SIL stub synthesizes every field in one place, so every stream
        // is by definition current (the real FcuLink stamps per callback).
        state.stamp_all_streams(mono_now_ns());
        telem.write(state);
    }
};

FcuLink::FcuLink() : impl_(std::make_unique<Impl>()) {}
FcuLink::~FcuLink() = default;

bool FcuLink::connect(const std::string& url) {
    RLOG_INFO("fcu", "connect %s (SIL stub)", url.c_str());
    impl_->connected = true;
    impl_->state.connected = 1;
    impl_->state.armed = 1; // SIL: assume armed on the bench
    impl_->state.position_ok = 1;
    impl_->state.rel_alt_m = 10.0F; // start at 10 m
    impl_->state.landed = 0;
    // SIL: a fixed reference fix (Daejeon) so the recording overlay has GPS.
    impl_->state.lat_deg = 36.350411;
    impl_->state.lon_deg = 127.384548;
    impl_->state.abs_alt_m = 80.0F;
    impl_->state.gps_ok = 1;
    // SIL: full, readable battery so the SM-9 engage gate passes on the bench
    // (mirrors the armed=1 bench assumption above).
    impl_->state.battery_frac = 1.0F;
    impl_->state.battery_ok = 1;
    impl_->publish();
    return true;
}
bool FcuLink::discovered() const {
    return impl_->connected;
}

bool FcuLink::snapshot(TelemetrySnapshot& out) const {
    return impl_->telem.read(out);
}

bool FcuLink::send_velocity_ned(const VelocitySetpointNed& sp) {
    impl_->cmd = sp;
    impl_->att_mode = false;
    return true;
}

bool FcuLink::send_attitude(const AttitudeSetpoint& sp) {
    impl_->att_cmd = sp;
    impl_->att_mode = true;
    return true;
}

bool FcuLink::start_offboard() {
    impl_->offboard = true;
    impl_->state.in_offboard = 1;
    impl_->publish();
    return true;
}
bool FcuLink::stop_offboard() {
    impl_->offboard = false;
    impl_->state.in_offboard = 0;
    impl_->publish();
    return true;
}
bool FcuLink::hold() {
    impl_->cmd = VelocitySetpointNed{};
    return true;
}

bool FcuLink::land() {
    impl_->landing = true;
    impl_->offboard = false;
    impl_->state.in_offboard = 0;
    impl_->publish();
    return true;
}

bool FcuLink::disarm() {
    impl_->state.armed = 0;
    impl_->publish();
    return true;
}

void FcuLink::sil_step(double dt_s) {
    auto& s = impl_->state;
    if (impl_->landing) {
        s.vel_ned_mps[0] = 0.F;
        s.vel_ned_mps[1] = 0.F;
        s.vel_ned_mps[2] = 0.8F;
        s.pos_ned_m[2] += static_cast<float>(s.vel_ned_mps[2] * dt_s);
        s.rel_alt_m =
            std::max(0.F, s.rel_alt_m - static_cast<float>(s.vel_ned_mps[2] * dt_s));
        if (s.rel_alt_m <= 0.02F) {
            impl_->landing = false;
            s.landed = 1;
            s.vel_ned_mps[2] = 0.F;
        }
    } else if (impl_->offboard) {
        float vn = 0.F;
        float ve = 0.F;
        float vd = 0.F;
        if (impl_->att_mode) {
            // Attitude -> NED velocity via a crude kinematic model: nose-down
            // pitch drives forward, right roll drives right, thrust sets climb.
            const auto& a = impl_->att_cmd;
            const float yaw = a.yaw_deg * SIL_DEG_TO_RAD;
            const float fwd = -a.pitch_deg * SIL_PITCH_TO_MPS;
            const float lat = a.roll_deg * SIL_ROLL_TO_MPS;
            const float climb = (a.thrust - SIL_HOVER) * SIL_THRUST_TO_MPS;
            vn = (fwd * std::cos(yaw)) - (lat * std::sin(yaw));
            ve = (fwd * std::sin(yaw)) + (lat * std::cos(yaw));
            vd = -climb;
            s.yaw_rad = yaw;
        } else {
            vn = impl_->cmd.vn_mps;
            ve = impl_->cmd.ve_mps;
            vd = impl_->cmd.vd_mps;
            s.yaw_rad = impl_->cmd.yaw_rad;
        }
        s.pos_ned_m[0] += vn * static_cast<float>(dt_s);
        s.pos_ned_m[1] += ve * static_cast<float>(dt_s);
        s.pos_ned_m[2] += vd * static_cast<float>(dt_s);
        s.rel_alt_m -= vd * static_cast<float>(dt_s); // down reduces altitude
        s.vel_ned_mps[0] = vn;
        s.vel_ned_mps[1] = ve;
        s.vel_ned_mps[2] = vd;
    }
    impl_->publish();
}
#endif

} // namespace riposte
