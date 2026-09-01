#pragma once
#include "IDetector.h" // Frame, Detection

#include <cmath>

namespace riposte {

// T2 template-tracker boundary (RIPOSTE-TRACKER-REQ-001 TR-C). A Siamese
// (NanoTrack-class) tracker on the RK3588 NPU follows the PRIMARY target's
// appearance template every frame; SyntheticTemplateTracker stands in for SIL.
//
// Role limits (TR-6): the template NEVER feeds the track filter and NEVER
// re-anchors itself — it is anchored from detection-confirmed boxes only, and
// its output is used solely by TrackFusion as (a) a stand-in LOS on frames
// with no detection ("visual coast") and (b) a cross-check against detections.
// Anything else would let template drift soak into the track state.

// One template evaluation on one frame.
struct TemplateResult {
    bool valid = false; // a template is anchored and produced a response
    float cx = 0.F;     // response peak, frame-normalized
    float cy = 0.F;
    float w = 0.F; // scale estimate, frame-normalized (0 = unknown)
    float h = 0.F;
    float score = 0.F; // response confidence 0..1
};

class ITemplateTracker {
public:
    ITemplateTracker() = default;
    virtual ~ITemplateTracker() = default;
    ITemplateTracker(const ITemplateTracker&) = delete;
    ITemplateTracker& operator=(const ITemplateTracker&) = delete;
    ITemplateTracker(ITemplateTracker&&) = delete;
    ITemplateTracker& operator=(ITemplateTracker&&) = delete;

    virtual bool init() = 0;
    // (Re-)anchors the template on a DETECTION-CONFIRMED box (TR-6).
    virtual bool anchor(const Frame& f, const Detection& d) = 0;
    // Evaluates the template on this frame. out.valid=false when nothing is
    // anchored; returns false only on a device fault.
    virtual bool track(const Frame& f, TemplateResult& out) = 0;
    virtual void reset() = 0; // drop the template entirely
    virtual bool healthy() const = 0;
    virtual const char* name() const = 0;
};

// SIL stand-in: holds the anchored box and drifts it by a fixed per-frame
// offset with a fixed score. Good for plumbing and for exercising TrackFusion
// against a CONTROLLED drift; it sees no pixels, so it validates policy, not
// tracking quality.
class SyntheticTemplateTracker final : public ITemplateTracker {
public:
    // Per-frame drift in frame-normalized units, and the constant response
    // score reported while anchored.
    SyntheticTemplateTracker(float drift_x = 0.F, float drift_y = 0.F, float score = 0.9F)
        : drift_x_(drift_x), drift_y_(drift_y), score_(score) {}

    bool init() override { return true; }

    bool anchor(const Frame& /*f*/, const Detection& d) override {
        anchored_ = true;
        cx_ = d.cx;
        cy_ = d.cy;
        w_ = d.w;
        h_ = d.h;
        return true;
    }

    bool track(const Frame& /*f*/, TemplateResult& out) override {
        out = TemplateResult{};
        if (!anchored_) {
            return true; // nothing anchored: valid=false, not a fault
        }
        cx_ += drift_x_;
        cy_ += drift_y_;
        out.valid = true;
        out.cx = cx_;
        out.cy = cy_;
        out.w = w_;
        out.h = h_;
        out.score = score_;
        return true;
    }

    void reset() override { anchored_ = false; }
    bool healthy() const override { return true; }
    const char* name() const override { return "SyntheticTemplateTracker"; }

private:
    float drift_x_;
    float drift_y_;
    float score_;
    bool anchored_ = false;
    float cx_ = 0.F, cy_ = 0.F, w_ = 0.F, h_ = 0.F;
};

} // namespace riposte
