#pragma once
#include "ITemplateTracker.h"
#include "Tracker.h"

#include "riposte/Clock.h"

namespace riposte {

// T2 fusion policy (RIPOSTE-TRACKER-REQ-001 TR-C). Combines the detection-driven
// primary track (T0/T1) with the appearance-template tracker into the single
// LOS the guidance loop consumes, under the TR-6 rule that DETECTION IS ALWAYS
// AUTHORITATIVE:
//
//   detection present  -> output is the detection-anchored primary box.
//     The template is (re)anchored on that box on a cadence, and its own
//     response is compared against the detection as a drift cross-check.
//   detection absent   -> "visual coast": the template response stands in for
//     the LOS, at a DEGRADED quality flagged for the OBC (TR-5). If the
//     template is also unavailable, fall back to the motion coast the tracker
//     already does — byte for byte the pre-T2 behaviour (TR-3).
//
// The template never feeds the track filter and never re-anchors itself, so
// its drift can never soak into the track state. This class owns no pixels and
// no device — the template tracker is injected (Synthetic in SIL/tests, RKNN on
// target), so the whole policy is host-testable. The device-facing anchor()/
// track() calls are the only non-pure part, and they are behind the interface.
class TrackFusion {
public:
    struct Params {
        // Motion gate squared (width-normalized) for the drift cross-check —
        // same scale Tracker uses, so "template disagrees with detection" means
        // the same thing here as an out-of-gate association there.
        float gate2 = 0.F;
        float aspect = 1.F;
        // Re-anchor the template every N detection frames. NanoTrack-class track
        // is cheap; anchor is not — re-anchoring every frame would throw away
        // the template's whole point, never re-anchoring lets it drift.
        int reanchor_period = 15;
        // Consecutive cross-check disagreements before the template is force
        // re-anchored (its coast would be untrustworthy otherwise).
        int mismatch_max = 5;
        // Quality multiplier applied while coasting on the template alone, so
        // the OBC can tell a detection-anchored LOS from a visual-coast one.
        float coast_quality_scale = 0.6F;
    };

    struct Output {
        bool valid = false;
        float cx = 0.5F, cy = 0.5F; // full-frame normalized
        float w = 0.F, h = 0.F;
        float quality = 0.F;
        bool visual_coast = false; // true = template-only LOS (degraded, TR-5)
    };

    TrackFusion() = default;
    explicit TrackFusion(const Params& p) : p_(p) {}

    // Fuses this frame. `primary` is the tracker's primary track (valid=false
    // when none is alive); `detected` is whether the primary was associated
    // with a detection THIS frame (i.e. its box is fresh, not coasted). `tmpl`
    // and `f` are only touched through the interface. Call once per frame,
    // after Tracker::update().
    Output fuse(ITemplateTracker& tmpl, const Frame& f, const Tracker::Track& primary,
                bool detected);

    // For diagnostics / tests.
    bool anchored() const { return anchored_; }
    int mismatch_count() const { return mismatch_; }

private:
    Params p_{};
    bool anchored_ = false;
    // WHOSE box the template is anchored on (TR-7): a primary handoff makes
    // the anchor another target's appearance — it must be dropped, never
    // published as the new identity's visual coast.
    uint32_t anchored_id_ = 0;
    int mismatch_ = 0;   // consecutive cross-check disagreements
    int det_frames_ = 0; // detection frames since the current anchor
};

} // namespace riposte
