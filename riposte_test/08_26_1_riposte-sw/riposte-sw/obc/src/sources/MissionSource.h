#pragma once
#include "GuidanceSource.h"
#include "ISetpointSource.h"
#include "Rendezvous.h"

#include <cstdint>
#include <string>

namespace riposte {

// Mission-level velocity source for the tablet/external-cue scenario:
//   target cue -> climb to mission altitude -> hover settle -> fly to cue point
//   -> seeker-guided approach when visual track exists -> hold at point
//   -> return home after operator timeout -> descend/land.
class MissionSource final : public ISetpointSource {
public:
    struct Params {
        float takeoff_alt_m = 20.F;
        float takeoff_rate_mps = 1.5F;
        float cruise_speed_mps = 5.F;
        float hover_radius_m = 3.F;
        float search_radius_m = 12.F;
        float land_rate_mps = 0.8F;
        float land_alt_m = 0.35F;
        uint64_t hover_settle_ns = 3'000'000'000ULL;
        uint64_t operator_timeout_ns = 60'000'000'000ULL;
        // 0 disables cue staleness checking. Default: 120 s.
        uint64_t target_stale_ns = 120'000'000'000ULL;
        // Mission altitude ceiling, mirrored from safety.alt_max: a cue above it
        // is flown at this altitude instead of climbing into an SM-3 violation.
        float max_alt_m = 60.F;
    };

    MissionSource();
    explicit MissionSource(const Params& params);

    bool compute(const TelemetrySnapshot& t, uint64_t now_ns,
                 VelocitySetpointNed& out) override;
    const char* name() const override { return "MissionSource"; }
    void on_engage() override;
    void set_mission_target(const MissionTarget& target) override;
    void operator_hold(uint64_t now_ns) override;
    void return_home(uint64_t now_ns) override;
    bool requests_land() const override;
    bool requests_disarm(const TelemetrySnapshot& t) const override;

private:
    enum class Phase : uint8_t {
        WAIT_TARGET,
        TAKEOFF,
        HOVER_AFTER_TAKEOFF,
        TRANSIT_TO_CUE,
        SEEKER_APPROACH,
        HOLD_AT_CUE,
        RETURN_HOME,
        LANDING
    };

    static const char* phase_name(Phase p);
    void set_phase(Phase p, uint64_t now_ns, const char* why);
    static float norm2(float n, float e);
    static float clampf(float v, float lo, float hi);
    bool goto_xy_alt(const TelemetrySnapshot& t, float target_n, float target_e,
                     float rel_alt_m, float speed_mps, VelocitySetpointNed& out) const;
    static void hover_with_yaw(float yaw_rad, VelocitySetpointNed& out);
    // Lead-closure aim point for the current cue (R-2). Not const: it
    // rate-limits its own logging.
    void aim_point(const TelemetrySnapshot& t, uint64_t now_ns, float& aim_n,
                   float& aim_e);

    Params params_{};
    GuidanceSource seeker_guidance_;
    MissionTarget target_{};
    float home_ned_[3] = {0.F, 0.F, 0.F};
    bool home_captured_ = false;
    Phase phase_ = Phase::WAIT_TARGET;
    uint64_t phase_since_ns_ = 0;
    uint64_t last_operator_cmd_ns_ = 0;
    bool last_rendezvous_ok_ = true; // log rate-limiting for aim_point()
    float last_lead_m_ = 0.F;
};

// Rejects an out-of-range mission configuration at startup (AGENTS §7.9), the
// same pattern as SafetyMonitor::Limits. The phase machine assumes every speed
// and rate is positive: cruise_speed_mps < 0 makes goto_xy_alt fly directly
// AWAY from its point, land_rate_mps < 0 makes LANDING climb (so
// requests_disarm() never fires and the session runs to the SM-8 timebox), and
// takeoff_rate_mps < 0 inverts the vertical clamp band. RETURN_HOME flies at
// takeoff_alt_m UNclamped, so it must also sit inside the mirrored ceiling.
// Returns true if every field is sane; otherwise false with `err` set to the
// first specific reason. Durations are validated at the config boundary.
bool validate(const MissionSource::Params& p, std::string& err);

} // namespace riposte
