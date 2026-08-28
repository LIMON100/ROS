#pragma once
#include "IDetector.h"

#include <cmath>

#include "riposte/Clock.h"

namespace riposte {

// SIL detector: a target that drifts across the frame and slowly grows (closing
// range). Lets the whole pipeline (Tracker -> Estimator -> TrackBus -> PN) run
// on a dev PC with RIPOSTE_WITH_HAILO=OFF.
//
// The target lives in SENSOR coordinates and is reported in the coordinates of
// whatever region it was handed (Frame::src_roi) — a target outside that region
// is simply not seen. Without that, a tiled search pass would "find" the target
// in every tile and the search policy could never be exercised in SIL.
class SyntheticDetector final : public IDetector {
public:
    bool init() override {
        t0_ns_ = mono_now_ns();
        return true;
    }

    bool detect(const Frame& f, std::vector<Detection>& out) override {
        out.clear();
        const double t = ns_to_s(mono_now_ns() - t0_ns_);
        // Target in full-sensor normalized coordinates.
        const float cx = 0.5F + (0.15F * static_cast<float>(std::sin(0.4 * t)));
        const float cy = 0.5F + (0.10F * static_cast<float>(std::cos(0.3 * t)));
        const float w = 0.04F + (0.010F * static_cast<float>(t) * 0.05F); // approaching
        const Roi& r = f.src_roi;
        if (r.w <= 0.F || r.h <= 0.F) {
            return true;
        }
        if (cx < r.x || cx > r.x + r.w || cy < r.y || cy > r.y + r.h) {
            return true; // outside the inspected region: nothing to report
        }
        Detection d{};
        d.cx = (cx - r.x) / r.w; // express it in the region's own coordinates
        d.cy = (cy - r.y) / r.h;
        d.w = w / r.w;
        d.h = w / r.h;
        d.score = 0.9F;
        d.cls = 0;
        out.push_back(d);
        return true;
    }

    bool healthy() const override { return true; }
    const char* name() const override { return "SyntheticDetector"; }

private:
    uint64_t t0_ns_ = 0;
};

} // namespace riposte
