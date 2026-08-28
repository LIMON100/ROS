#pragma once
#include "Tracker.h"

#include "riposte/Types.h"

namespace riposte {

// Converts a smoothed image-space track into a relative target state in the
// vehicle BODY FRD frame, published on TrackBus.
//
// Geometry (POC, monocular):
//   - Camera pinhole with known horizontal FOV -> line-of-sight angles from the
//     normalized bbox center.
//   - Range from apparent target size: range = (real_size * focal_px) / bbox_px.
//     Crude but sufficient to seed PN; accuracy validation is DEFERRED and a
//     range sensor may be added (RIPOSTE-SAD-001 §13).
//   - Relative velocity from the range/LOS time-derivative (finite difference).
class TargetEstimator {
public:
    struct Params {
        float hfov_rad = 1.05F; // ~60 deg horizontal FOV
        float aspect = 16.F / 9.F;
        float target_size_m = 0.35F; // assumed non-cooperative target drone span
    };

    explicit TargetEstimator(Params p) : p_(p) {}

    // Fills `out` from the current track. Returns false if the track is invalid
    // or below quality threshold (=> seeker publishes valid=0).
    bool estimate(const Tracker::Track& trk, uint64_t frame_mono_ns, double dt_s,
                  TrackState& out);

private:
    Params p_;
    bool have_prev_ = false;
    float prev_pos_[3] = {0, 0, 0};
    uint64_t prev_ns_ = 0;
    uint32_t prev_id_ = 0; // track the differentiator was seeded from
};

} // namespace riposte
