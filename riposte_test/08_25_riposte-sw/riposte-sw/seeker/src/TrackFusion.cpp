#include "TrackFusion.h"

#include "IDetector.h"
#include "ITemplateTracker.h"
#include "Tracker.h"

namespace riposte {

namespace {
// Same isotropic width-normalized metric Tracker::dist2 uses.
float dist2(float ax, float ay, float bx, float by, float aspect) {
    const float dx = ax - bx;
    const float dy = (ay - by) / aspect;
    return (dx * dx) + (dy * dy);
}

Detection box_of(const Tracker::Track& t, float aspect) {
    Detection d{};
    d.cx = t.cx;
    d.cy = t.cy;
    // Track carries a single WIDTH-normalized size; present it as a square box
    // (in pixels) for the template to anchor on. Detection.h is
    // HEIGHT-normalized, so the same pixel extent is size * aspect there —
    // h = size verbatim would anchor a box squashed by the sensor aspect.
    d.w = t.size;
    d.h = t.size * aspect;
    d.score = t.quality;
    d.cls = 0;
    return d;
}
} // namespace

TrackFusion::Output TrackFusion::fuse(ITemplateTracker& tmpl, const Frame& f,
                                      const Tracker::Track& primary, bool detected) {
    Output out;

    if (!primary.valid) {
        // No engaged target: drop the template so a stale anchor can never
        // resurface as a phantom coast on the next acquisition.
        tmpl.reset();
        anchored_ = false;
        anchored_id_ = 0;
        mismatch_ = 0;
        det_frames_ = 0;
        return out; // valid = false
    }

    // Identity handoff (TR-7): the anchor holds the PREVIOUS primary's
    // appearance. Publishing it under the new identity would coast on another
    // target's position, so it is dropped before anything else — the new
    // primary earns its own anchor from its own fresh detection below.
    if (anchored_ && primary.id != anchored_id_) {
        tmpl.reset();
        anchored_ = false;
        anchored_id_ = 0;
        mismatch_ = 0;
        det_frames_ = 0;
    }

    if (detected) {
        // --- Detection authoritative: the output IS the primary box (TR-6) ---
        out.valid = true;
        out.cx = primary.cx;
        out.cy = primary.cy;
        out.w = primary.size;
        out.h = primary.size;
        out.quality = primary.quality;
        out.visual_coast = false;

        // Drift cross-check: where does the template think the target is? A
        // persistent disagreement means the template has locked onto the
        // background and its coast can't be trusted.
        if (anchored_) {
            TemplateResult tr;
            const bool ok = tmpl.track(f, tr) && tr.valid;
            if (ok && dist2(tr.cx, tr.cy, primary.cx, primary.cy, p_.aspect) > p_.gate2) {
                ++mismatch_;
            } else if (ok) {
                mismatch_ = 0;
            }
        }
        ++det_frames_;

        // Re-anchor when: nothing anchored yet, the template drifted
        // (mismatch), or the cadence is due. Anchoring resets the drift state.
        const bool cadence_due =
            p_.reanchor_period > 0 && det_frames_ >= p_.reanchor_period;
        if (!anchored_ || mismatch_ >= p_.mismatch_max || cadence_due) {
            const Detection box = box_of(primary, p_.aspect);
            anchored_ = tmpl.anchor(f, box); // anchor may fail on a device fault
            anchored_id_ = anchored_ ? primary.id : 0;
            mismatch_ = 0;
            det_frames_ = 0;
        }
        return out;
    }

    // --- Detection absent: try to coast on the template (TR-2) ---------------
    if (anchored_) {
        TemplateResult tr;
        if (tmpl.track(f, tr) && tr.valid) {
            out.valid = true;
            out.cx = tr.cx;
            out.cy = tr.cy;
            out.w = (tr.w > 0.F) ? tr.w : primary.size;
            out.h = (tr.h > 0.F) ? tr.h : primary.size;
            // Degraded quality, flagged: the OBC must be able to treat a
            // visual-coast LOS more conservatively than a detected one (TR-5).
            out.quality = primary.quality * p_.coast_quality_scale;
            out.visual_coast = true;
            return out;
        }
    }

    // Template unavailable (never anchored, or device/track failure): fall back
    // to the motion coast the tracker already produced — the pre-T2 behaviour,
    // unchanged (TR-3). The tracker has already advanced primary by its velocity.
    out.valid = primary.valid;
    out.cx = primary.cx;
    out.cy = primary.cy;
    out.w = primary.size;
    out.h = primary.size;
    out.quality = primary.quality;
    out.visual_coast = false;
    return out;
}

} // namespace riposte
