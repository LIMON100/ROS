// T2 template-fusion policy tests (RIPOSTE-TRACKER-REQ-001 TR-C, host half).
//
// The policy's whole job is to add a "visual coast" without ever letting the
// template override or corrupt the detection-driven track (TR-6). These tests
// drive it through the SyntheticTemplateTracker — which sees no pixels and
// drifts by a controlled offset — so they pin the POLICY: detection stays
// authoritative, coast is degraded and flagged, drift is caught, and a dead
// template falls back to the motion coast byte for byte.
#include "IDetector.h"
#include "ITemplateTracker.h"
#include "TrackFusion.h"
#include "Tracker.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace riposte;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
int checks = 0;

#define CHECK(c)                                                    \
    do {                                                            \
        ++checks;                                                   \
        if (!(c)) {                                                 \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            return 1;                                               \
        }                                                           \
    } while (0)

bool near(float a, float b, float tol = 1e-4F) {
    return std::fabs(a - b) <= tol;
}

TrackFusion::Params params() {
    TrackFusion::Params p;
    p.gate2 = 0.01F; // gate radius 0.1
    p.aspect = 1.F;
    p.reanchor_period = 15;
    p.mismatch_max = 5;
    p.coast_quality_scale = 0.6F;
    return p;
}

Tracker::Track primary(float cx, float cy, float size, float quality, bool valid = true) {
    Tracker::Track t;
    t.valid = valid;
    t.id = 1;
    t.cx = cx;
    t.cy = cy;
    t.size = size;
    t.quality = quality;
    return t;
}

const Frame FRAME{}; // Synthetic tracker ignores pixels

// -------------------------------------------------------------- no target --

int test_no_primary_resets_and_reports_invalid() {
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl;
    // Anchor something first...
    fusion.fuse(tmpl, FRAME, primary(0.5F, 0.5F, 0.05F, 0.9F), true);
    CHECK(fusion.anchored());
    // ...then the target dies: the template must be dropped so it can't coast
    // a phantom on the next acquisition.
    const auto out = fusion.fuse(tmpl, FRAME, primary(0.F, 0.F, 0.F, 0.F, false), false);
    CHECK(!out.valid);
    CHECK(!fusion.anchored());
    return 0;
}

// ------------------------------------------------- detection authoritative --

int test_detection_frame_outputs_primary_and_anchors() {
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl(0.02F, 0.F, 0.9F); // drifts right if ever trusted
    const auto out = fusion.fuse(tmpl, FRAME, primary(0.4F, 0.6F, 0.05F, 0.8F), true);
    CHECK(out.valid);
    CHECK(!out.visual_coast);
    CHECK(near(out.cx, 0.4F) && near(out.cy, 0.6F)); // detection box, NOT drift
    CHECK(near(out.quality, 0.8F));                  // full quality
    CHECK(fusion.anchored());                        // template seeded for coast
    return 0;
}

int test_detection_always_wins_even_if_template_drifted() {
    // Even after the template has been left to drift, a detection frame's
    // output is the detection — the template can never override it (TR-6).
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl(0.05F, 0.05F, 0.9F);
    for (int i = 0; i < 4; ++i) {
        const auto out = fusion.fuse(tmpl, FRAME, primary(0.5F, 0.5F, 0.05F, 0.9F), true);
        CHECK(near(out.cx, 0.5F) && near(out.cy, 0.5F));
        CHECK(!out.visual_coast);
    }
    return 0;
}

// -------------------------------------------------------------- visual coast --

int test_visual_coast_uses_template_at_degraded_quality() {
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl(0.01F, 0.F, 0.85F); // drifts +0.01/frame in x
    // Detection frame anchors the template at 0.50.
    fusion.fuse(tmpl, FRAME, primary(0.50F, 0.5F, 0.05F, 0.9F), true);
    // Detection drops: the template response (now 0.51) stands in.
    const auto out =
        fusion.fuse(tmpl, FRAME, primary(0.50F, 0.5F, 0.05F, 0.9F), /*detected=*/false);
    CHECK(out.valid);
    CHECK(out.visual_coast);               // flagged for the OBC (TR-5)
    CHECK(out.cx > 0.505F);                // followed the template, not the stale box
    CHECK(near(out.quality, 0.9F * 0.6F)); // degraded
    return 0;
}

int test_visual_coast_tracks_across_several_frames() {
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl(0.01F, 0.F, 0.85F);
    fusion.fuse(tmpl, FRAME, primary(0.50F, 0.5F, 0.05F, 0.9F), true); // anchor @0.50
    float last = 0.50F;
    for (int i = 0; i < 3; ++i) {
        const auto out =
            fusion.fuse(tmpl, FRAME, primary(0.50F, 0.5F, 0.05F, 0.9F), false);
        CHECK(out.valid && out.visual_coast);
        CHECK(out.cx > last); // LOS keeps advancing with the template
        last = out.cx;
    }
    return 0;
}

// ------------------------------------------------- motion-coast fallback --

int test_no_template_falls_back_to_motion_coast() {
    // Template never anchored (no prior detection frame) and target is coasting:
    // output must be the tracker's own predicted box, undegraded, not flagged —
    // exactly the pre-T2 behaviour (TR-3).
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl;
    const auto out =
        fusion.fuse(tmpl, FRAME, primary(0.30F, 0.30F, 0.05F, 0.7F), /*detected=*/false);
    CHECK(out.valid);
    CHECK(!out.visual_coast);
    CHECK(near(out.cx, 0.30F) && near(out.cy, 0.30F));
    CHECK(near(out.quality, 0.7F)); // NOT degraded — this is the motion path
    return 0;
}

int test_reset_template_stops_coasting() {
    // A device-faulted template (track returns valid=false) must not coast;
    // fall back to motion. Simulated by resetting the anchor out from under it.
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl(0.01F, 0.F, 0.8F);
    fusion.fuse(tmpl, FRAME, primary(0.5F, 0.5F, 0.05F, 0.9F), true);
    tmpl.reset(); // template loses its anchor (stand-in for a fault)
    const auto out = fusion.fuse(tmpl, FRAME, primary(0.5F, 0.5F, 0.05F, 0.9F), false);
    // fusion still thinks it is anchored, asks the template, gets valid=false,
    // and must fall back to motion (undegraded, unflagged).
    CHECK(out.valid && !out.visual_coast);
    CHECK(near(out.quality, 0.9F));
    return 0;
}

// -------------------------------------------------- drift cross-check --

// ------------------------------------------------- TR-7 identity handoff --

Tracker::Track primary_id(uint32_t id, float cx, float cy) {
    Tracker::Track t = primary(cx, cy, 0.05F, 0.9F);
    t.id = id;
    return t;
}

int test_identity_handoff_drops_the_old_anchor() {
    // Anchor on id 1 at (0.2, 0.2); the primary hands off to id 2 at
    // (0.8, 0.8). On id 2's first MISS the old template must NOT publish
    // id 1's position as id 2's visual coast — the anchor holds another
    // target's appearance (TR-7).
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl; // no drift: reports the anchor spot verbatim
    fusion.fuse(tmpl, FRAME, primary_id(1, 0.2F, 0.2F), true);
    CHECK(fusion.anchored());
    const auto out = fusion.fuse(tmpl, FRAME, primary_id(2, 0.8F, 0.8F), false);
    CHECK(!fusion.anchored()); // old identity's anchor dropped
    CHECK(out.valid);
    CHECK(!out.visual_coast); // motion-coast fallback, not the old template
    CHECK(near(out.cx, 0.8F) && near(out.cy, 0.8F)); // id 2's own coasted box
    return 0;
}

int test_identity_handoff_then_new_anchor_on_detection() {
    TrackFusion fusion(params());
    SyntheticTemplateTracker tmpl;
    fusion.fuse(tmpl, FRAME, primary_id(1, 0.2F, 0.2F), true);
    // Handoff on a DETECTION frame: id 2 anchors its own template right away.
    const auto out = fusion.fuse(tmpl, FRAME, primary_id(2, 0.8F, 0.8F), true);
    CHECK(out.valid && !out.visual_coast);
    CHECK(fusion.anchored());
    // A subsequent miss coasts on id 2's OWN anchor position, not id 1's.
    const auto coast = fusion.fuse(tmpl, FRAME, primary_id(2, 0.8F, 0.8F), false);
    CHECK(coast.valid && coast.visual_coast);
    CHECK(near(coast.cx, 0.8F));
    return 0;
}

int test_anchor_box_preserves_pixel_square_aspect() {
    // Detection.h is HEIGHT-normalized: anchoring a pixel-square box on a
    // 16:9 sensor needs h = size * aspect; size verbatim would anchor a box
    // squashed by the sensor aspect.
    TrackFusion::Params p = params();
    p.aspect = 16.F / 9.F;
    TrackFusion fusion(p);
    SyntheticTemplateTracker tmpl;
    fusion.fuse(tmpl, FRAME, primary_id(1, 0.5F, 0.5F), true); // size 0.05
    TemplateResult tr;
    CHECK(tmpl.track(FRAME, tr) && tr.valid);
    CHECK(near(tr.w, 0.05F));
    CHECK(near(tr.h, 0.05F * (16.F / 9.F)));
    return 0;
}

int test_persistent_disagreement_forces_reanchor() {
    // The template drifts far from the detection every frame. After mismatch_max
    // disagreements the fusion must force a re-anchor, resetting the counter —
    // so a drifted template is corrected rather than trusted on the next coast.
    TrackFusion::Params p = params();
    p.mismatch_max = 3;
    p.reanchor_period = 999; // isolate the mismatch path from the cadence
    TrackFusion fusion(p);
    // Template jumps 0.2/frame — always outside the 0.1 gate from a stationary
    // detection at 0.5.
    SyntheticTemplateTracker tmpl(0.2F, 0.F, 0.9F);

    // Frame 1 anchors (det_frames resets, no track cross-check yet).
    fusion.fuse(tmpl, FRAME, primary(0.5F, 0.5F, 0.05F, 0.9F), true);
    // Now each detection frame: template.track drifts away -> mismatch++.
    for (int i = 0; i < 2; ++i) {
        fusion.fuse(tmpl, FRAME, primary(0.5F, 0.5F, 0.05F, 0.9F), true);
        CHECK(fusion.mismatch_count() > 0);
    }
    // The 3rd disagreement hits mismatch_max -> force re-anchor -> counter clears.
    fusion.fuse(tmpl, FRAME, primary(0.5F, 0.5F, 0.05F, 0.9F), true);
    CHECK(fusion.mismatch_count() == 0);
    CHECK(fusion.anchored());
    return 0;
}

int test_agreeing_template_keeps_mismatch_at_zero() {
    TrackFusion fusion(params());
    // No drift: the template stays on the detection, so the cross-check always
    // agrees and never triggers a spurious re-anchor.
    SyntheticTemplateTracker tmpl(0.F, 0.F, 0.9F);
    for (int i = 0; i < 6; ++i) {
        fusion.fuse(tmpl, FRAME, primary(0.5F, 0.5F, 0.05F, 0.9F), true);
        CHECK(fusion.mismatch_count() == 0);
    }
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_no_primary_resets_and_reports_invalid();
    rc = rc != 0 ? rc : test_detection_frame_outputs_primary_and_anchors();
    rc = rc != 0 ? rc : test_detection_always_wins_even_if_template_drifted();
    rc = rc != 0 ? rc : test_visual_coast_uses_template_at_degraded_quality();
    rc = rc != 0 ? rc : test_visual_coast_tracks_across_several_frames();
    rc = rc != 0 ? rc : test_no_template_falls_back_to_motion_coast();
    rc = rc != 0 ? rc : test_reset_template_stops_coasting();
    rc = rc != 0 ? rc : test_identity_handoff_drops_the_old_anchor();
    rc = rc != 0 ? rc : test_identity_handoff_then_new_anchor_on_detection();
    rc = rc != 0 ? rc : test_anchor_box_preserves_pixel_square_aspect();
    rc = rc != 0 ? rc : test_persistent_disagreement_forces_reanchor();
    rc = rc != 0 ? rc : test_agreeing_template_keeps_mismatch_at_zero();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_fusion: %d checks passed\n", checks);
    return 0;
}
