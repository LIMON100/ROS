#include "sources/MissionSource.h"

#include "Rendezvous.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "riposte/Clock.h"
#include "riposte/Log.h"
#include "riposte/Types.h"

namespace riposte {

bool validate(const MissionSource::Params& p, std::string& err) {
    if (!(p.max_alt_m > 0.F)) {
        err = "mission ceiling (safety.alt_max mirror) must be > 0";
        return false;
    }
    if (!(p.takeoff_alt_m > 0.F) || p.takeoff_alt_m > p.max_alt_m) {
        err =
            "mission.takeoff_alt_m must be in (0, safety.alt_max] — "
            "RETURN_HOME flies at this altitude unclamped";
        return false;
    }
    if (!(p.takeoff_rate_mps > 0.F)) {
        err = "mission.takeoff_rate_mps must be > 0";
        return false;
    }
    if (!(p.cruise_speed_mps > 0.F)) {
        err = "mission.cruise_speed_mps must be > 0";
        return false;
    }
    if (!(p.hover_radius_m > 0.F)) {
        err = "mission.hover_radius_m must be > 0";
        return false;
    }
    if (!(p.search_radius_m > 0.F)) {
        err = "mission.search_radius_m must be > 0";
        return false;
    }
    if (!(p.land_rate_mps > 0.F)) {
        err = "mission.land_rate_mps must be > 0";
        return false;
    }
    if (p.land_alt_m < 0.F) {
        err = "mission.land_alt_m must be >= 0";
        return false;
    }
    err.clear();
    return true;
}

MissionSource::MissionSource() = default;
MissionSource::MissionSource(const Params& params) : params_(params) {}

const char* MissionSource::phase_name(Phase p) {
    switch (p) {
        case Phase::WAIT_TARGET:
            return "WAIT_TARGET";
        case Phase::TAKEOFF:
            return "TAKEOFF";
        case Phase::HOVER_AFTER_TAKEOFF:
            return "HOVER_AFTER_TAKEOFF";
        case Phase::TRANSIT_TO_CUE:
            return "TRANSIT_TO_CUE";
        case Phase::SEEKER_APPROACH:
            return "SEEKER_APPROACH";
        case Phase::HOLD_AT_CUE:
            return "HOLD_AT_CUE";
        case Phase::RETURN_HOME:
            return "RETURN_HOME";
        case Phase::LANDING:
            return "LANDING";
    }
    return "?";
}

void MissionSource::set_phase(Phase p, uint64_t now_ns, const char* why) {
    if (phase_ == p) {
        return;
    }
    RLOG_INFO("mission", "%s -> %s (%s)", phase_name(phase_), phase_name(p), why);
    phase_ = p;
    phase_since_ns_ = now_ns;
}

float MissionSource::norm2(float n, float e) {
    return std::sqrt((n * n) + (e * e));
}

float MissionSource::clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

void MissionSource::on_engage() {
    // Per-control session state only. The cue (target_) is deliberately kept: the
    // TARGET command that carries it is what requests the engage, and it is
    // delivered before this hook runs.
    home_captured_ = false;
    phase_ = Phase::WAIT_TARGET;
    phase_since_ns_ = 0;
    last_operator_cmd_ns_ = 0;
    seeker_guidance_.on_engage();
    RLOG_INFO("mission", "engage: home and phase reset");
}

void MissionSource::set_mission_target(const MissionTarget& target) {
    target_ = target;
    target_.valid = 1;
    RLOG_INFO("mission",
              "target accepted seq=%u ned=(%.1f, %.1f, %.1f) vel=(%.1f, %.1f, %.1f) "
              "heading=%.2f",
              target_.seq, target_.pos_ned_m[0], target_.pos_ned_m[1],
              target_.pos_ned_m[2], target_.vel_ned_mps[0], target_.vel_ned_mps[1],
              target_.vel_ned_mps[2], target_.heading_rad);
}

// Where to fly for this cue: the lead-rendezvous point when the cue reports the
// target moving (R-2), otherwise the cue point itself. Recomputed every tick so
// the aim point keeps up as both the vehicle and the extrapolated target move.
void MissionSource::aim_point(const TelemetrySnapshot& t, uint64_t now_ns, float& aim_n,
                              float& aim_e) {
    RendezvousCue cue;
    cue.pos_ned_m[0] = target_.pos_ned_m[0];
    cue.pos_ned_m[1] = target_.pos_ned_m[1];
    cue.pos_ned_m[2] = target_.pos_ned_m[2];
    cue.vel_ned_mps[0] = target_.vel_ned_mps[0];
    cue.vel_ned_mps[1] = target_.vel_ned_mps[1];
    cue.vel_ned_mps[2] = target_.vel_ned_mps[2];
    cue.mono_ns = target_.mono_ns;
    const RendezvousResult r =
        solve_rendezvous(t.pos_ned_m, cue, params_.cruise_speed_mps, now_ns);
    aim_n = r.point_ned_m[0];
    aim_e = r.point_ned_m[1];
    // Log only on a meaningful change: this runs at 20 Hz.
    if (r.ok != last_rendezvous_ok_ || std::fabs(r.lead_m - last_lead_m_) > 2.F) {
        last_rendezvous_ok_ = r.ok;
        last_lead_m_ = r.lead_m;
        if (r.ok && r.lead_m > 0.5F) {
            RLOG_INFO("mission", "closure lead %.0fm, t_go %.1fs", r.lead_m, r.t_go_s);
        } else if (!r.ok) {
            RLOG_WARN("mission",
                      "no rendezvous solution (target faster/opening or too far "
                      "ahead) — tracking current position");
        }
    }
}

void MissionSource::operator_hold(uint64_t now_ns) {
    last_operator_cmd_ns_ = now_ns;
    set_phase(Phase::HOLD_AT_CUE, now_ns, "operator hold");
}

void MissionSource::return_home(uint64_t now_ns) {
    last_operator_cmd_ns_ = now_ns;
    set_phase(Phase::RETURN_HOME, now_ns, "operator return-home");
}

bool MissionSource::requests_land() const {
    return phase_ == Phase::LANDING;
}

bool MissionSource::requests_disarm(const TelemetrySnapshot& t) const {
    return phase_ == Phase::LANDING && t.rel_alt_m <= params_.land_alt_m;
}

void MissionSource::hover_with_yaw(float yaw_rad, VelocitySetpointNed& out) {
    out = VelocitySetpointNed{};
    out.yaw_rad = yaw_rad;
}

bool MissionSource::goto_xy_alt(const TelemetrySnapshot& t, float target_n,
                                float target_e, float rel_alt_m, float speed_mps,
                                VelocitySetpointNed& out) const {
    out = VelocitySetpointNed{};
    const float dn = target_n - t.pos_ned_m[0];
    const float de = target_e - t.pos_ned_m[1];
    const float dist = norm2(dn, de);
    if (dist > 0.2F) {
        // Proportional slow-down inside `speed_mps` metres of the point.
        const float v = std::min(speed_mps, dist);
        out.vn_mps = (dn / dist) * v;
        out.ve_mps = (de / dist) * v;
        out.yaw_rad = std::atan2(de, dn);
    } else {
        out.yaw_rad = target_.heading_rad;
    }
    const float alt_err = rel_alt_m - t.rel_alt_m; // + means climb needed
    out.vd_mps = clampf(-alt_err, -params_.takeoff_rate_mps, params_.land_rate_mps);
    return true;
}

bool MissionSource::compute(const TelemetrySnapshot& t, uint64_t now_ns,
                            VelocitySetpointNed& out) {
    out = VelocitySetpointNed{};
    if (!home_captured_) {
        home_ned_[0] = t.pos_ned_m[0];
        home_ned_[1] = t.pos_ned_m[1];
        home_ned_[2] = t.pos_ned_m[2];
        home_captured_ = true;
        last_operator_cmd_ns_ = now_ns;
        phase_since_ns_ = now_ns;
        RLOG_INFO("mission", "home captured ned=(%.1f, %.1f, %.1f)", home_ned_[0],
                  home_ned_[1], home_ned_[2]);
    }

    if (target_.valid == 0U) {
        hover_with_yaw(t.yaw_rad, out);
        return true; // safe hold while waiting for tablet/external target
    }
    if (params_.target_stale_ns != 0U &&
        age_ns(now_ns, target_.mono_ns) > params_.target_stale_ns &&
        phase_ != Phase::RETURN_HOME && phase_ != Phase::LANDING) {
        set_phase(Phase::RETURN_HOME, now_ns, "target cue stale");
    }
    if (phase_ == Phase::WAIT_TARGET) {
        set_phase(Phase::TAKEOFF, now_ns, "target cue received");
    }

    // Cue altitude, bounded by the mission ceiling: an out-of-range cue must be
    // flown at the highest legal altitude, not climbed into an SM-3 disengage.
    const float cue_alt_m = clampf(std::max(params_.takeoff_alt_m, -target_.pos_ned_m[2]),
                                   0.F, params_.max_alt_m);
    // Aim at the lead-rendezvous point (R-2); for a static cue this is the cue.
    float aim_n = target_.pos_ned_m[0];
    float aim_e = target_.pos_ned_m[1];
    aim_point(t, now_ns, aim_n, aim_e);
    // "Arrived" is judged against the AIM point, not the cue's original
    // position: with a moving target the cue point is somewhere the target has
    // already left, and stopping there would end the transit in empty air.
    const float d_to_cue = norm2(aim_n - t.pos_ned_m[0], aim_e - t.pos_ned_m[1]);
    const float d_to_home =
        norm2(home_ned_[0] - t.pos_ned_m[0], home_ned_[1] - t.pos_ned_m[1]);

    switch (phase_) {
        case Phase::WAIT_TARGET:
            hover_with_yaw(t.yaw_rad, out);
            return true;

        case Phase::TAKEOFF:
            (void)goto_xy_alt(t, t.pos_ned_m[0], t.pos_ned_m[1], cue_alt_m,
                              params_.cruise_speed_mps, out);
            out.yaw_rad = target_.heading_rad;
            if (std::fabs(cue_alt_m - t.rel_alt_m) <= 0.75F) {
                set_phase(Phase::HOVER_AFTER_TAKEOFF, now_ns, "target altitude reached");
            }
            return true;

        case Phase::HOVER_AFTER_TAKEOFF:
            hover_with_yaw(target_.heading_rad, out);
            if (now_ns - phase_since_ns_ >= params_.hover_settle_ns) {
                set_phase(Phase::TRANSIT_TO_CUE, now_ns, "hover settle complete");
            }
            return true;

        case Phase::TRANSIT_TO_CUE: {
            // If the seeker already has a usable visual track, let the visual source
            // guide.
            VelocitySetpointNed visual{};
            if (seeker_guidance_.compute(t, now_ns, visual)) {
                out = visual;
                set_phase(Phase::SEEKER_APPROACH, now_ns, "seeker target acquired");
                return true;
            }
            (void)goto_xy_alt(t, aim_n, aim_e, cue_alt_m, params_.cruise_speed_mps, out);
            if (d_to_cue <= params_.search_radius_m) {
                set_phase(Phase::HOLD_AT_CUE, now_ns, "cue point reached");
                last_operator_cmd_ns_ = now_ns;
            }
            return true;
        }

        case Phase::SEEKER_APPROACH: {
            VelocitySetpointNed visual{};
            if (seeker_guidance_.compute(t, now_ns, visual)) {
                out = visual;
                return true;
            }
            set_phase(Phase::HOLD_AT_CUE, now_ns, "seeker lost; hold at cue");
            last_operator_cmd_ns_ = now_ns;
            hover_with_yaw(target_.heading_rad, out);
            return true;
        }

        case Phase::HOLD_AT_CUE:
            hover_with_yaw(target_.heading_rad, out);
            if (now_ns - last_operator_cmd_ns_ >= params_.operator_timeout_ns) {
                set_phase(Phase::RETURN_HOME, now_ns, "operator timeout");
            }
            return true;

        case Phase::RETURN_HOME:
            (void)goto_xy_alt(t, home_ned_[0], home_ned_[1], params_.takeoff_alt_m,
                              params_.cruise_speed_mps, out);
            if (d_to_home <= params_.hover_radius_m) {
                set_phase(Phase::LANDING, now_ns, "home reached");
            }
            return true;

        case Phase::LANDING:
            // Reached only when the FC refused the LAND command (otherwise the
            // controller is in AUTO_LANDING). Keep descending all the way down —
            // holding a hover just above the ground would never touch down, and
            // requests_disarm() would then wait forever.
            hover_with_yaw(t.yaw_rad, out);
            out.vd_mps = (t.rel_alt_m <= params_.land_alt_m)
                             ? std::min(params_.land_rate_mps, 0.3F) // touchdown creep
                             : params_.land_rate_mps;                // positive down
            return true;
    }
    return true;
}

} // namespace riposte
