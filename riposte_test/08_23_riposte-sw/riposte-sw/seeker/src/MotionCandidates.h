#pragma once
#include "IDetector.h" // Frame

#include <cstddef>
#include <cstdint>
#include <vector>

namespace riposte {

// Wide-channel motion candidate generation (RIPOSTE-DUALEO-REQ-001 R-13).
//
// The wide channel's job includes reacquisition, but until now that depended
// entirely on the detector: with no model loaded — or a target outside the
// model's distribution — nothing produced a candidate at all. This module is
// the second path, built on PIXEL MOTION only, so it works without any AI.
//
//   frames t-1, t  ->  corners + block-matched correspondences
//                  ->  global ego-motion (RANSAC, similarity 4-DOF)
//                  ->  residual = |t - warp(t-1)|
//                  ->  threshold + connected components
//                  ->  small independently-moving blobs, scored
//
// Ego-motion compensation is the whole point: while the ownship moves, a plain
// frame difference marks the entire image as moving. Aligning the background
// first is what makes the residual mean "moved differently from the scene".
//
// The output is a HINT, never a track: it says "look here" and feeds the narrow
// channel's tile priority (R-15) and reacquisition (R-16). Promotion to a
// tracked object still goes through the R-6 confirmation window.
//
// Pure logic — no camera, no NPU, no OpenCV (S-9). Integer arithmetic
// throughout, so there is no NaN path to defend.
class MotionCandidates {
public:
    struct Params {
        // Ego-motion runs on the image decimated by this factor; the residual
        // is examined at full resolution (a distant target is only a few px).
        int decimate = 4;
        int fast_threshold = 20; // corner strength, luma step
        int patch_radius = 3;    // block-matching patch half-size (decimated px)
        int search_radius = 8;   // block-matching search half-range (decimated px)
        int min_correspondences = 12;
        int ransac_iters = 64;
        // Search half-range of the guided second pass, around the predicted
        // displacement. Small on purpose: the model already places it.
        int guided_radius = 3;
        float ransac_inlier_px = 1.5F; // inlier bound, decimated px
        float min_inlier_frac = 0.5F;  // else: scene unmodelled, emit nothing
        int residual_threshold = 24;   // full-resolution luma step
        float blob_min_frac = 0.0015F; // blob size band, fraction of image width
        float blob_max_frac = 0.08F;   // upper bound rejects parallax structure
        int max_candidates = 16;
    };

    // One independently-moving region, in FULL-FRAME normalized coordinates —
    // the same convention every downstream consumer (Tracker, ChannelMap,
    // SearchScheduler cue) already speaks.
    struct Candidate {
        float cx = 0.F, cy = 0.F; // centre
        float w = 0.F, h = 0.F;   // extent
        float score = 0.F;        // 0..1: residual strength x compactness
        int pixels = 0;           // thresholded pixels in the blob
    };

    // Estimated background transform between the two frames, in FULL-RESOLUTION
    // pixels: p_cur = R(theta) * s * p_prev + t. Reported so the caller can log
    // ego-motion magnitude and so tests can pin the estimate directly. The fit
    // itself runs on the decimated pair; `process` converts the translation
    // back (including the decimation box-centre term) so callers never have to
    // know the internal scale.
    struct EgoMotion {
        bool ok = false; // false: too few correspondences or no consensus
        float a = 1.F;   // s*cos(theta)
        float b = 0.F;   // s*sin(theta)
        float tx = 0.F;
        float ty = 0.F;
        int inliers = 0;
        int correspondences = 0;
    };

    struct Result {
        EgoMotion ego;
        std::vector<Candidate> candidates; // score-descending, capped
    };

    MotionCandidates() = default;
    explicit MotionCandidates(const Params& p) : p_(p) {}

    // Processes one consecutive pair. Both frames must be NV12 with identical
    // geometry (only the luma plane is read). Returns an empty candidate list —
    // never a guess — when the frames are unusable or the scene could not be
    // modelled: inventing candidates from a bad alignment is worse than none.
    //
    // Not reentrant: scratch buffers are reused across calls to keep the
    // per-frame allocation count at zero.
    Result process(const Frame& prev, const Frame& cur);

    const Params& params() const { return p_; }

private:
    // Corner + correspondence extraction on the decimated pair.
    struct Match {
        float px = 0.F, py = 0.F; // point in prev (decimated coords)
        float cx = 0.F, cy = 0.F; // matched point in cur
    };

    void decimate_luma(const Frame& f, std::vector<uint8_t>& out, int& dw, int& dh) const;
    void find_corners(const std::vector<uint8_t>& img, int w, int h, std::vector<int>& xs,
                      std::vector<int>& ys) const;
    // `guide`, when non-null, centres each corner's search on that model's
    // PREDICTED displacement instead of on zero. Rotation and scale displace a
    // point in proportion to its distance from the transform's fixed point, so
    // a single zero-centred window cannot cover a frame corner and the image
    // centre at once; the second, guided pass is what extends the usable
    // ego-motion range without paying for a huge search window.
    void match_corners(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                       int w, int h, const std::vector<int>& xs,
                       const std::vector<int>& ys, const EgoMotion* guide);
    EgoMotion fit_similarity() const;

    Params p_{};
    // Scratch, reused per call (no per-frame allocation).
    std::vector<uint8_t> prev_small_;
    std::vector<uint8_t> cur_small_;
    std::vector<int> corner_x_;
    std::vector<int> corner_y_;
    std::vector<Match> matches_;
    std::vector<int32_t> cost_map_; // block-matching cost surface for one corner
    // Residual magnitude at full resolution. The blob pass CONSUMES it — each
    // claimed pixel is zeroed — so no second visited buffer is needed for a
    // 5 MP frame.
    std::vector<uint8_t> residual_;
    std::vector<int32_t> stack_; // flood-fill stack (bounded by the mask size)
};

// Least-squares similarity transform from point correspondences (Umeyama's
// closed form for the 4-DOF case). Exposed because it is the numerically
// delicate step: with fewer than two well-separated pairs, or a degenerate
// (all-coincident) set, the normal equations are singular and the caller must
// get `false` rather than an infinity. Pure — unit-tested directly.
bool solve_similarity(const std::vector<float>& src_x, const std::vector<float>& src_y,
                      const std::vector<float>& dst_x, const std::vector<float>& dst_y,
                      float& a, float& b, float& tx, float& ty);

} // namespace riposte
