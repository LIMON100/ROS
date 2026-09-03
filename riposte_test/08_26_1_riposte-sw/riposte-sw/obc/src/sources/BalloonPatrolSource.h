#pragma once
#include "Geofence.h"
#include "GuidanceSource.h"
#include "ISetpointSource.h"

#include <cstdint>
#include <string>
#include <vector>

namespace riposte {

// Test-flight behaviour for the balloon trials (RIPOSTE-DUALEO-REQ-001 T-1..T-5):
// inside a surveyed geofence, find a red balloon, fly at it, and when the fence
// gets close, slow down, turn back inward and go looking for a different one.
//
//   SEARCH      hold patrol altitude, yaw slowly, wait for a CONFIRMED track
//      │ track acquired (and not one we already visited)
//   APPROACH    guidance-driven tracking, speed scaled down as the fence nears
//      │ reached it / fence too close / track lost
//   TURN_AWAY   drive inward, yaw toward the middle of the range, then SEARCH
//
// Two safety properties this source deliberately does NOT own: the control session
// still requires the operator token (D-2), and SM-10 still ends the flight if
// the boundary is actually crossed. This layer's job is to make that never
// happen — a behaviour that reaches the hard fence has already failed.
//
// "Already visited" is tracked by TRACK ID, not position: the seeker's tracker
// keeps a sticky primary, so ignoring the id we just serviced is what lets the
// vehicle turn away and let a different balloon become primary.
class BalloonPatrolSource final : public ISetpointSource {
public:
    struct Params {
        float patrol_alt_m = 5.F;       // T-2: balloons fly at 3 m and up
        float approach_speed_mps = 3.F; // gentle: the range is 50 m across
        float search_yaw_rate_rps = 0.5F;
        float soft_margin_m = 10.F; // T-5: start slowing this far from the edge
        float turn_margin_m = 3.F;  // ...and give the target up at this point
        float reach_range_m = 3.F;  // close enough to call the balloon serviced
        float alt_rate_mps = 1.0F;  // climb/descend rate holding patrol altitude
        // While approaching, the vehicle matches the TARGET's altitude instead of
        // holding the patrol altitude, bounded by this band. A body-fixed camera
        // has a limited vertical field of view, so closing in on a target that
        // sits above or below the flight altitude drives the line of sight out
        // of frame in the last few metres — the target is lost exactly when it
        // is about to be reached (seen in the Gazebo trial: elevation went past
        // the vertical FOV at ~3 m range and the approach never completed).
        float min_alt_m = 3.0F;
        float max_alt_m = 15.0F;
        uint64_t visited_cooldown_ns = 30'000'000'000ULL; // ignore a serviced id
        uint64_t turn_dwell_ns = 3'000'000'000ULL;        // minimum time turning away
        Geofence fence;                                   // must be valid()
    };

    BalloonPatrolSource() = default;
    explicit BalloonPatrolSource(const Params& p) : params_(p) {}

    bool compute(const TelemetrySnapshot& t, uint64_t now_ns,
                 VelocitySetpointNed& out) override;
    const char* name() const override { return "BalloonPatrolSource"; }
    void on_engage() override;
    void set_fence(const Geofence& fence) override { params_.fence = fence; }

    // Test/logging observability.
    enum class Phase : uint8_t { SEARCH, APPROACH, TURN_AWAY };
    Phase phase() const { return phase_; }
    static const char* phase_name(Phase p);
    std::size_t visited_count() const { return visited_.size(); }

private:
    struct Visited {
        uint32_t track_id = 0;
        uint64_t when_ns = 0;
    };

    void set_phase(Phase p, uint64_t now_ns, const char* why);
    bool is_visited(uint32_t track_id, uint64_t now_ns) const;
    void mark_visited(uint32_t track_id, uint64_t now_ns);
    // Rate-limited vertical command toward `desired_alt_m` (AGL).
    float alt_command(const TelemetrySnapshot& t, float desired_alt_m) const;
    // Altitude to hold this tick: the patrol altitude, or the target's altitude
    // while approaching so the target stays in the camera's vertical FOV.
    float desired_alt(const TelemetrySnapshot& t) const;
    // Range to the current target, or -1 when there is no usable track.
    float target_range_m() const;

    Params params_{};
    GuidanceSource guidance_;
    Phase phase_ = Phase::SEARCH;
    uint64_t phase_since_ns_ = 0;
    float search_yaw_rad_ = 0.F;
    uint64_t last_tick_ns_ = 0;
    std::vector<Visited> visited_;
};

// Rejects an out-of-range patrol configuration at startup (AGENTS §7.9), the
// same pattern as SafetyMonitor::Limits. The compute() maths assumes every
// speed/rate/margin is positive: a sign typo in approach_speed_mps inverts the
// boundary-recovery direction (TURN_AWAY then drives OUTWARD across the
// boundary until SM-10 ends the session), a non-positive soft margin divides
// by ~0 in the slow-down blend, and a patrol altitude outside the configured
// band commands a climb/descent the altitude clamp then fights forever.
// Returns true if every field is sane; otherwise false with `err` set to the
// first specific reason. The fence and durations are validated elsewhere.
bool validate(const BalloonPatrolSource::Params& p, std::string& err);

} // namespace riposte
