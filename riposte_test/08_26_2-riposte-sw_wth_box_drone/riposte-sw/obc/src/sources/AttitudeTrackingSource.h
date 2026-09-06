#pragma once
#include "IAttitudeSource.h"

#include "riposte/Clock.h"
#include "riposte/SeqSlot.h"

namespace riposte {

// Attitude-mode terminal steering: reads the seeker TrackBus and points the
// airframe at the target — yaw to face it, nose-down pitch to accelerate toward
// it, thrust biased by target elevation. This is the pitch/yaw control law for
// tracking. Same freshness/quality gate and coast policy as GuidanceSource (SM-7
// in-source half): an invalid/low-quality bus sample steers on the cached
// last-valid track within the coast budget; beyond it the source emits a safe
// LEVEL, hover-thrust hold and returns false so the controller disengages.
class AttitudeTrackingSource final : public IAttitudeSource {
public:
    struct Params {
        float hover_thrust = 0.50F;
        float track_pitch_deg = 15.0F;  // forward nose-down lean, ADDED to the
                                        // LOS-elevation tracking term (R-9)
        float thrust_elev_gain = 0.15F; // thrust bias per unit target elevation
    };

    explicit AttitudeTrackingSource(Params p);

    bool compute(const TelemetrySnapshot& t, uint64_t now_ns,
                 AttitudeSetpoint& out) override;
    const char* name() const override { return "AttitudeTrackingSource"; }
    // Session boundary: drop the cached track and its detection clock so a new
    // control session cannot steer on the previous session's last target.
    void on_engage() override {
        last_valid_ = false;
        last_track_ = TrackState{};
        last_detection_ns_ = 0;
    }

private:
    Params p_;
    ShmSeqSlot<TrackState> track_bus_;
    bool last_valid_ = false;
    TrackState last_track_{}; // last VALID sample; geometry source during coast
    // Timestamp of the last DETECTION-ANCHORED sample (visual_coast == 0). The
    // coast window is measured from THIS, not from last_track_.mono_ns, so a
    // stream of fresh template-only "visual coast" samples cannot hold the
    // control session open past the coast budget (TRACKER-REQ TR-D-b, P2-04) — the
    // same detection-anchored invariant GuidanceSource already enforces.
    // 0 = no detection-anchored track yet.
    uint64_t last_detection_ns_ = 0;
};

} // namespace riposte
