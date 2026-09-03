#include "sources/BalloonPatrolSource.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "riposte/Clock.h"
#include "riposte/Log.h"
#include "riposte/Types.h"

namespace riposte {

bool validate(const BalloonPatrolSource::Params& p, std::string& err) {
    if (!(p.approach_speed_mps > 0.F)) {
        err = "patrol.approach_speed_mps must be > 0";
        return false;
    }
    if (!(p.search_yaw_rate_rps > 0.F)) {
        err = "patrol.search_yaw_rate_rps must be > 0";
        return false;
    }
    if (!(p.alt_rate_mps > 0.F)) {
        err = "patrol.alt_rate_mps must be > 0";
        return false;
    }
    if (!(p.reach_range_m > 0.F)) {
        err = "patrol.reach_range_m must be > 0";
        return false;
    }
    // The soft margin is a divisor in the slow-down blend, and the turn margin
    // must sit strictly inside it — TURN_AWAY exits only once the vehicle is
    // clear of the SOFT margin, so turn >= soft could never terminate.
    if (!(p.soft_margin_m > 0.F)) {
        err = "patrol.soft_margin_m must be > 0";
        return false;
    }
    if (p.turn_margin_m < 0.F || p.turn_margin_m >= p.soft_margin_m) {
        err = "patrol.turn_margin_m must be in [0, soft_margin_m)";
        return false;
    }
    if (p.min_alt_m < 0.F || !(p.max_alt_m > p.min_alt_m)) {
        err = "patrol altitude band requires 0 <= min_alt < max_alt";
        return false;
    }
    // The patrol altitude itself must live inside the band the approach clamp
    // enforces — outside it, desired_alt() commands an altitude alt_command()
    // then chases across the safety band every SEARCH tick.
    if (p.patrol_alt_m < p.min_alt_m || p.patrol_alt_m > p.max_alt_m) {
        err = "patrol.alt_m must be within [safety.alt_min, safety.alt_max]";
        return false;
    }
    err.clear();
    return true;
}

const char* BalloonPatrolSource::phase_name(Phase p) {
    switch (p) {
        case Phase::SEARCH:
            return "SEARCH";
        case Phase::APPROACH:
            return "APPROACH";
        case Phase::TURN_AWAY:
            return "TURN_AWAY";
    }
    return "?";
}

void BalloonPatrolSource::set_phase(Phase p, uint64_t now_ns, const char* why) {
    if (phase_ == p) {
        return;
    }
    RLOG_INFO("patrol", "%s -> %s (%s)", phase_name(phase_), phase_name(p), why);
    phase_ = p;
    phase_since_ns_ = now_ns;
}

void BalloonPatrolSource::on_engage() {
    phase_ = Phase::SEARCH;
    phase_since_ns_ = 0;
    last_tick_ns_ = 0;
    search_yaw_rad_ = 0.F;
    visited_.clear();
    guidance_.on_engage();
    RLOG_INFO("patrol", "engage: patrol state reset");
}

bool BalloonPatrolSource::is_visited(uint32_t track_id, uint64_t now_ns) const {
    return std::any_of(visited_.begin(), visited_.end(), [&](const auto& v) {
        return v.track_id == track_id &&
               age_ns(now_ns, v.when_ns) < params_.visited_cooldown_ns;
    });
}

void BalloonPatrolSource::mark_visited(uint32_t track_id, uint64_t now_ns) {
    for (auto& v : visited_) {
        if (v.track_id == track_id) {
            v.when_ns = now_ns;
            return;
        }
    }
    // Drop entries that have aged out before growing the list, so a long sortie
    // over many balloons cannot grow it without bound.
    visited_.erase(std::remove_if(visited_.begin(), visited_.end(),
                                  [&](const Visited& v) {
                                      return age_ns(now_ns, v.when_ns) >=
                                             params_.visited_cooldown_ns;
                                  }),
                   visited_.end());
    visited_.push_back(Visited{track_id, now_ns});
}

float BalloonPatrolSource::alt_command(const TelemetrySnapshot& t,
                                       float desired_alt_m) const {
    const float err = desired_alt_m - t.rel_alt_m; // + means climb needed
    return std::max(-params_.alt_rate_mps, std::min(params_.alt_rate_mps, -err));
}

float BalloonPatrolSource::desired_alt(const TelemetrySnapshot& t) const {
    if (phase_ != Phase::APPROACH || !guidance_.last_track_valid()) {
        return params_.patrol_alt_m;
    }
    // rel_pos_frd_m[2] is DOWN-positive: the target sits this far below us. Match
    // its altitude so the line of sight stays near the boresight as the range
    // closes; otherwise the elevation angle grows without bound and a body-fixed
    // camera loses the target in the last few metres of the approach.
    const float down = guidance_.last_track().rel_pos_frd_m[2];
    const float target_alt = t.rel_alt_m - down;
    return std::max(params_.min_alt_m, std::min(params_.max_alt_m, target_alt));
}

float BalloonPatrolSource::target_range_m() const {
    if (!guidance_.last_track_valid()) {
        return -1.F;
    }
    const TrackState& ts = guidance_.last_track();
    const float x = ts.rel_pos_frd_m[0];
    const float y = ts.rel_pos_frd_m[1];
    const float z = ts.rel_pos_frd_m[2];
    return std::sqrt((x * x) + (y * y) + (z * z));
}

// G16.6 deviation: the phase machine reads as one piece: transitions then per-phase
// command (G16.6) NOLINTNEXTLINE(readability-function-size)
bool BalloonPatrolSource::compute(const TelemetrySnapshot& t, uint64_t now_ns,
                                  VelocitySetpointNed& out) {
    out = VelocitySetpointNed{};
    const double dt_s =
        (last_tick_ns_ != 0U) ? ns_to_s(age_ns(now_ns, last_tick_ns_)) : 0.0;
    last_tick_ns_ = now_ns;
    if (phase_since_ns_ == 0U) {
        phase_since_ns_ = now_ns;
        search_yaw_rad_ = t.yaw_rad;
    }

    // A patrol without a boundary has nothing to keep it inside the range, and
    // this source exists precisely to respect one. Refusing to guide converges
    // on disengage (SM-7) rather than flying an unbounded search.
    if (!params_.fence.valid()) {
        RLOG_WARN("patrol", "no geofence configured — cannot patrol");
        return false;
    }

    const float pos_n = t.pos_ned_m[0];
    const float pos_e = t.pos_ned_m[1];
    const float edge_m = params_.fence.distance_to_edge(pos_n, pos_e);
    float in_n = 0.F;
    float in_e = 0.F;
    params_.fence.inward(pos_n, pos_e, in_n, in_e);

    // Guidance runs every tick regardless of phase so the cached track stays
    // current (and so `visited` ids are recognised the moment they reappear).
    VelocitySetpointNed tracking{};
    const bool have_track = guidance_.compute(t, now_ns, tracking);
    const uint32_t track_id =
        guidance_.last_track_valid() ? guidance_.last_track().track_id : 0U;
    const bool usable_track =
        have_track && guidance_.last_track_valid() && !is_visited(track_id, now_ns);

    // --- Phase transitions, decided BEFORE the command is generated so the
    // --- setpoint always belongs to the phase we are actually in. Deciding
    // --- after would emit one tick of the previous phase's command on every
    // --- transition — including a tick of "keep approaching" after the vehicle
    // --- has already decided the fence is too close.
    const float range_m = target_range_m();
    switch (phase_) {
        case Phase::SEARCH:
            if (usable_track) {
                set_phase(Phase::APPROACH, now_ns, "balloon acquired");
            }
            break;

        case Phase::APPROACH:
            if (!usable_track) {
                search_yaw_rad_ = t.yaw_rad;
                set_phase(Phase::SEARCH, now_ns,
                          have_track ? "target already serviced" : "track lost");
            } else if (range_m >= 0.F && range_m <= params_.reach_range_m) {
                mark_visited(track_id, now_ns);
                set_phase(Phase::TURN_AWAY, now_ns, "balloon reached");
            } else if (edge_m <= params_.turn_margin_m) {
                // T-5: give this balloon up rather than chase it across the
                // fence, and remember it so the turn does not re-acquire it.
                mark_visited(track_id, now_ns);
                set_phase(Phase::TURN_AWAY, now_ns, "fence boundary reached");
            }
            break;

        case Phase::TURN_AWAY: {
            const bool clear = edge_m > params_.soft_margin_m;
            const bool dwelt = age_ns(now_ns, phase_since_ns_) >= params_.turn_dwell_ns;
            if (clear && dwelt) {
                search_yaw_rad_ = std::atan2(in_e, in_n);
                set_phase(Phase::SEARCH, now_ns, "clear of boundary");
            }
            break;
        }
    }

    // --- Command generation for the current phase. The vertical command is
    // shared: hold the patrol altitude, except while approaching, when it tracks
    // the target's altitude to keep it inside the camera's vertical FOV.
    out.vd_mps = alt_command(t, desired_alt(t));

    switch (phase_) {
        case Phase::SEARCH: {
            // Yaw sweep in place. The vehicle holds station; only the heading
            // moves, so a search near the edge cannot drift across it.
            search_yaw_rad_ += params_.search_yaw_rate_rps * static_cast<float>(dt_s);
            out.yaw_rad = search_yaw_rad_;
            // Drift back toward the middle if the search left us near the edge.
            if (edge_m < params_.soft_margin_m) {
                const float k = std::min(1.F, (params_.soft_margin_m - edge_m) /
                                                  std::max(1e-3F, params_.soft_margin_m));
                out.vn_mps = in_n * k * params_.approach_speed_mps * 0.5F;
                out.ve_mps = in_e * k * params_.approach_speed_mps * 0.5F;
            }
            return true;
        }

        case Phase::APPROACH: {
            // Proportional slow-down through the soft margin: the command is
            // blended toward "inward" as the edge nears, so the vehicle both
            // decelerates and starts curving back before the hard limit.
            const float k = std::min(1.F, std::max(0.F, edge_m / params_.soft_margin_m));
            const float speed = params_.approach_speed_mps;
            const float ph = std::sqrt((tracking.vn_mps * tracking.vn_mps) +
                                       (tracking.ve_mps * tracking.ve_mps));
            float un = 0.F;
            float ue = 0.F;
            if (ph > 1e-3F) {
                un = tracking.vn_mps / ph;
                ue = tracking.ve_mps / ph;
            }
            out.vn_mps = ((un * k) + (in_n * (1.F - k))) * speed;
            out.ve_mps = ((ue * k) + (in_e * (1.F - k))) * speed;
            out.yaw_rad = tracking.yaw_rad; // keep the balloon in view
            return true;
        }

        case Phase::TURN_AWAY:
            out.vn_mps = in_n * params_.approach_speed_mps * 0.5F;
            out.ve_mps = in_e * params_.approach_speed_mps * 0.5F;
            out.yaw_rad = std::atan2(in_e, in_n); // face the middle of the range
            return true;
    }
    return true;
}

} // namespace riposte
