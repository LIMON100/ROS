#pragma once
#include "ISetpointSource.h"
#include "TargetImm.h"

#include <cstdint>

#include "riposte/Clock.h"
#include "riposte/SeqSlot.h"
#include "riposte/Types.h"

namespace riposte {

// L4 guidance folded into the OBC (design decision A-3): reading track freshness
// and producing the setpoint happen inside the deterministic control thread, so
// the instant the seeker loses the target the controller sees it and converges
// to disengage (SM-7) instead of coasting on a stale command.
//
// Pipeline per tick:
//   1) Read newest TrackState from TrackBus (shm SeqSlot); a valid fresh sample
//      updates the cached last-valid track (and re-seeds the lead filter on a
//      track_id change — never difference the LOS across two aircraft).
//   2) Freshness/quality gate. Invalid/low-quality bus samples coast on the
//      CACHED track within the coast budget; beyond it -> compute() = false.
//   3) Rotate relative target from body FRD to NED (full ZYX attitude DCM).
//   4) Tracking + discrete LOS-delta lead -> commanded closing velocity (NED).
class GuidanceSource final : public ISetpointSource {
public:
    GuidanceSource();

    bool compute(const TelemetrySnapshot& t, uint64_t now_ns,
                 VelocitySetpointNed& out) override;
    const char* name() const override { return "GuidanceSource"; }
    // Drop the previous control session's cached track and lead history: the first
    // tick of a new control session must not coast on (or lead from) a target seen
    // minutes ago at another location.
    void on_engage() override {
        last_valid_ = false;
        last_track_ = TrackState{};
        last_detection_ns_ = 0;
        have_prev_los_ = false;
        imm_.reset();
        imm_prev_ns_ = 0;
    }

    // Exposed for supervisor/logging, and for sources that COMPOSE guidance
    // (MissionSource, BalloonPatrolSource) and need the target geometry —
    // range, identity — that compute() folds into a velocity. Meaningful only
    // while last_track_valid().
    const TrackState& last_track() const { return last_track_; }
    bool last_track_valid() const { return last_valid_; }
    uint64_t last_track_age_ns(uint64_t now_ns) const {
        return age_ns(now_ns, last_track_.mono_ns);
    }

private:
    ShmSeqSlot<TrackState> track_bus_;
    bool last_valid_ = false;
    TrackState last_track_{}; // last VALID sample; geometry source during coast
    // Timestamp of the last DETECTION-ANCHORED valid sample (visual_coast == 0).
    // The coast budget is measured from THIS, not from last_track_.mono_ns, so a
    // stream of fresh-but-template-only "visual coast" samples cannot hold the
    // control session open past the coast window — SM-7 still fires (TRACKER-REQ
    // TR-D-b). 0 = no detection-anchored track yet.
    uint64_t last_detection_ns_ = 0;
    float prev_los_ned_[3] = {0, 0, 0};
    bool have_prev_los_ = false;
    // Moving-target IMM estimator (ESTIMATION-REQ EST-P4 / P2-07). Runs HERE, in
    // the OBC, because only the OBC holds own-vehicle telemetry — the seeker
    // never touches the FC (I2). Two constant-acceleration models (smooth /
    // agile) blended by likelihood track a maneuvering target with less lag than
    // a single fixed-Q filter. It refines the cached track's relative VELOCITY
    // (own motion removed) from the seeker's raw single-tick difference; the
    // flight-tuned PN geometry above keeps using the seeker's position
    // untouched, so guidance behaviour is unchanged and only downstream velocity
    // consumers (lead/closure) and the EST-6 quality degrade see the change.
    TargetImm imm_;
    uint64_t imm_prev_ns_ = 0;
};

} // namespace riposte
