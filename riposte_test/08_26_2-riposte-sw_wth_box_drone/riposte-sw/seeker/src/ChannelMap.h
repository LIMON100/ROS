#pragma once
#include "IDetector.h" // Roi, Detection

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace riposte {

// Wide<->narrow EO channel coordinate mapping (RIPOSTE-DUALEO-REQ-001 §3.6,
// design decision P4). The two cameras are nested-coaxial with parallel optical
// axes (LENS-REQ §2.2), so the same distant target sits at the same bearing in
// both — only the normalization differs (different HFOV) plus a small constant
// mounting-offset residual (Δaz, Δel) recovered by factory calibration.
//
// Far-field model: bearing(narrow) = bearing(wide) - offset; the ranges differ
// only in the second order (baseline parallax, negligible >~30 m), so distance
// does not enter. Distortion is assumed already corrected upstream. This is the
// pure-geometry half of the dual-EO handoff — no pixels, no camera, host-tested.
//
// Convention matches TargetEstimator: normalized center 0.5, az = (cx-0.5)*hfov
// (right +), el = (cy-0.5)*vfov (down +), vfov = hfov/aspect.
class ChannelMap {
public:
    struct Params {
        float wide_hfov_rad = 1.03F;    // ~59 deg (LENS L-W1)
        float narrow_hfov_rad = 0.269F; // ~15.4 deg (LENS L-N1)
        float aspect = 16.F / 9.F;      // image W/H (same sensor both channels)
        // Calibrated mounting-offset residual: narrow bearing = wide bearing
        // minus this. Zero for perfectly aligned axes; set from calibration.
        float offset_az_rad = 0.F;
        float offset_el_rad = 0.F;
    };

    // A mapped point plus whether it actually falls inside the destination
    // field of view. The narrow FOV is a small window at the wide centre, so a
    // wide-frame target near the edge maps OUTSIDE the narrow frame — the caller
    // must check in_fov before cropping/inferring on the narrow channel.
    struct Mapped {
        float cx = 0.5F;
        float cy = 0.5F;
        bool in_fov = false;
    };

    ChannelMap() = default;
    explicit ChannelMap(const Params& p) : p_(p) {}

    // Wide-channel normalized coords -> narrow-channel normalized coords.
    Mapped wide_to_narrow(float wide_cx, float wide_cy) const;
    // Narrow-channel normalized coords -> wide-channel normalized coords (the
    // handoff direction: a narrow detection remapped for the shared full-frame
    // tracker/estimator, which work in wide-channel coordinates).
    Mapped narrow_to_wide(float narrow_cx, float narrow_cy) const;

    // NORMALIZED SIZE across channels (S-13). Angular size is what is physical:
    // theta = size * hfov, so the same target measures hfov_wide/hfov_narrow
    // times LARGER in the narrow frame (~3.8x at the L-W1/L-N1 pair). Remapping
    // a narrow detection's centre without its size would feed TargetEstimator a
    // box ~4x too big — range = real_size * focal / bbox — so range would
    // collapse the instant a slot switched channel. Small-angle, matching the
    // linear az = (cx-0.5)*hfov model the estimator itself uses.
    float narrow_to_wide_size(float narrow_size) const {
        return narrow_size * (p_.narrow_hfov_rad / p_.wide_hfov_rad);
    }
    float wide_to_narrow_size(float wide_size) const {
        return wide_size * (p_.wide_hfov_rad / p_.narrow_hfov_rad);
    }

    // A cue-frame (wide) window expressed in narrow-channel coordinates, so a
    // NARROW slot can crop its own image (S-13). False when the window centre
    // falls outside the narrow field of view: the narrow channel is a small
    // window at the wide centre, so a target only slightly off-axis is not
    // visible there at all and the caller must fall back to the wide frame
    // rather than crop somewhere arbitrary.
    bool wide_roi_to_narrow(const Roi& in, Roi& out) const {
        const Mapped m = wide_to_narrow(in.x + (in.w * 0.5F), in.y + (in.h * 0.5F));
        if (!m.in_fov) {
            return false;
        }
        const float w = std::min(1.F, wide_to_narrow_size(in.w));
        const float h = std::min(1.F, wide_to_narrow_size(in.h));
        out.x = std::clamp(m.cx - (w * 0.5F), 0.F, 1.F - w);
        out.y = std::clamp(m.cy - (h * 0.5F), 0.F, 1.F - h);
        out.w = w;
        out.h = h;
        return true;
    }

    // Rewrites narrow-channel detections into the shared wide-channel
    // coordinates the tracker and estimator work in. Centre AND size both move;
    // dropping the size conversion would hand the monocular range estimate a
    // box ~4x too large. Detections that map outside the wide frame are dropped.
    void remap_detections_to_wide(std::vector<Detection>& dets) const {
        std::vector<char> kept;
        remap_and_mark(dets, kept);
    }

    // Same, keeping `aligned` (a per-detection parallel array — the T1
    // embeddings computed on the pre-remap boxes) index-aligned through those
    // drops. Misaligned arrays would pair detections with the wrong appearance
    // vectors downstream (TR-B); an `aligned` of a different length was never
    // aligned (e.g. the embedder produced nothing) and is left untouched.
    template <typename T>
    void remap_detections_to_wide(std::vector<Detection>& dets,
                                  std::vector<T>& aligned) const {
        std::vector<char> kept;
        const std::size_t before = dets.size();
        remap_and_mark(dets, kept);
        if (aligned.size() != before) {
            return;
        }
        std::size_t w = 0;
        for (std::size_t i = 0; i < before; ++i) {
            if (kept[i] != 0) {
                if (w != i) {
                    aligned[w] = aligned[i];
                }
                ++w;
            }
        }
        aligned.resize(w);
    }

    // Whether a narrow-channel frame may be PAIRED with this wide frame for one
    // perception tick (S-13). A narrow-slot measurement is published with the
    // WIDE frame's timestamp and differenced over the wide-loop dt, so the two
    // captures must describe (nearly) the same instant — an age-vs-now check
    // alone cannot see a narrow buffer that aged in its ring while the wide
    // grab stalled. Pure and symmetric, so the bound is host-testable.
    static bool frames_pairable(uint64_t wide_mono_ns, uint64_t narrow_mono_ns,
                                uint64_t max_skew_ns) {
        const uint64_t skew = (wide_mono_ns > narrow_mono_ns)
                                  ? wide_mono_ns - narrow_mono_ns
                                  : narrow_mono_ns - wide_mono_ns;
        return skew <= max_skew_ns;
    }

private:
    static bool inside(float cx, float cy) {
        return cx >= 0.F && cx <= 1.F && cy >= 0.F && cy <= 1.F;
    }
    // Remap in place, recording per-ORIGINAL-index survival in `kept` so the
    // aligned-array overload can compact a parallel vector identically.
    void remap_and_mark(std::vector<Detection>& dets, std::vector<char>& kept) const {
        kept.assign(dets.size(), 0);
        std::vector<Detection> out;
        out.reserve(dets.size());
        for (std::size_t i = 0; i < dets.size(); ++i) {
            Detection d = dets[i];
            const Mapped m = narrow_to_wide(d.cx, d.cy);
            if (!m.in_fov) {
                continue;
            }
            d.cx = m.cx;
            d.cy = m.cy;
            d.w = narrow_to_wide_size(d.w);
            d.h = narrow_to_wide_size(d.h);
            out.push_back(d);
            kept[i] = 1;
        }
        dets.swap(out);
    }
    Params p_{};
};

} // namespace riposte
