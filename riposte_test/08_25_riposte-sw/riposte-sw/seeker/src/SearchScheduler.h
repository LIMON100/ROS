#pragma once
#include "IDetector.h"

#include <cstdint>
#include <vector>

namespace riposte {

// `Roi` and `Frame::src_roi` live in IDetector.h — they are part of the image
// contract the detector boundary already speaks.

// Maps a detection produced on a CROPPED frame (coordinates normalized against
// the crop) back into full-frame normalized coordinates. Every consumer
// downstream of the detector — Tracker, TargetEstimator, the overlay — works in
// full-frame coordinates, so this runs on every detection before it is tracked.
Detection remap_to_frame(const Detection& d, const Roi& roi);

// Crops an NV12 frame to `roi` into `scratch`, producing a Frame that borrows it.
// The rectangle is snapped to EVEN pixel bounds (NV12 chroma is 2x2 subsampled,
// so an odd offset would swap the U and V samples) and the snapped rectangle is
// returned in `used` — remapping must use that, not the requested roi, or the
// detections land off by up to a pixel. Returns false if the request degenerates
// to less than 2x2 pixels or the source is not NV12.
bool crop_nv12(const Frame& src, const Roi& roi, std::vector<uint8_t>& scratch,
               Frame& out, Roi& used);

// What the tracker made of the previous frame, as the scheduler needs it.
struct TrackCue {
    bool alive = false; // tracker still holds a confirmed primary track
    // Which channel ACTUALLY produced this frame's detection (R-11 evidence).
    // Not the same as the allocated slot: a NARROW slot falls back to the wide
    // frame when the narrow camera missed its grab, or when the cue lies
    // outside the narrow field of view. Attributing such a hit to the narrow
    // channel would let cross-channel confirmation fire on evidence from ONE
    // channel — the exact thing R-11 exists to avoid.
    bool hit_from_narrow = false;
    bool hit = false;           // that primary was associated with a detection THIS frame
    uint32_t track_id = 0;      // WHICH track is primary (TR-7 identity binding)
    float cx = 0.5F, cy = 0.5F; // primary center, full-frame normalized
    float size = 0.F;           // primary bbox size, WIDTH-normalized (Tracker's unit)
    // Estimated target range in metres (0 = unknown). Drives the close-range
    // narrow-EO 60 Hz boost (R-10): inside narrow_boost_range_m the wide-recheck
    // cadence stretches so the narrow channel approaches every-frame inference.
    float range_m = 0.F;
};

// Decides which region of the frame the detector inspects each frame, and when a
// detection has been seen consistently enough to call it a tracked target
// (RIPOSTE-DUALEO-REQ-001 R-5/R-6/R-7).
//
//   SEARCH_WIDE  whole frame, resized to the model input. Cheapest pass, but a
//                distant target shrinks below the detector's floor here.
//   SEARCH_TILE  the frame split into a grid, ONE tile per frame, cycled. Each
//                tile reaches the model at native resolution, which is what makes
//                a small/distant target detectable at all (§3 of REQ-001).
//   CONFIRM      a detection appeared; inspect the region around it and count how
//                often it comes back. CONFIRM_HITS of the last CONFIRM_WINDOW
//                frames (4 of 5 = 80 %) promotes it to TRACK.
//   TRACK        committed to the target: inspect its neighbourhood, and every
//                recheck_period frames run a WIDE pass anyway so the commitment
//                is re-validated against the whole scene rather than drifting
//                inside a window that follows the tracker's own prediction.
//
// The scheduler owns no image data and calls nothing — it is a pure decision
// object, so the whole search policy is unit-testable without a camera or NPU.
//
// Dual-EO (S-11 / R-10, adaptive inference-rate allocation): both cameras
// capture at a fixed rate; the scheduler decides WHICH channel's frame gets
// this frame's single inference slot (the synchronous detect() pipeline fits
// exactly one per frame — REQ-001 §3.4). While searching/confirming the slot
// alternates wide/narrow (30 Hz each at 60 fps); once TRACK is confirmed the
// narrow channel takes every slot except the R-7 recheck, which goes to a wide
// full-frame pass (and the narrow simply skips that frame). Switching the
// SENSOR rate instead is forbidden: a V4L2 renegotiation right after track
// confirmation can outlive the coast limit and lose the track. With
// dual_eo=false (single camera) every slot is WIDE and behaviour is unchanged.
class SearchScheduler {
public:
    enum class Mode : uint8_t { SEARCH_WIDE, SEARCH_TILE, CONFIRM, TRACK };
    // Which camera's frame receives this frame's inference slot.
    enum class Channel : uint8_t { WIDE, NARROW };

    struct Params {
        int grid = 3;            // grid x grid tiles in SEARCH_TILE
        int wide_dwell = 2;      // wide passes before falling back to tiles
        int confirm_window = 5;  // R-6 window length, frames
        int confirm_hits = 4;    // hits required within it (>= 80 %)
        int recheck_period = 15; // R-7 wide re-detect cadence while tracking
        // Stretched recheck cadence inside narrow_boost_range_m (R-10 close-range
        // narrow 60 Hz boost). >= recheck_period; equal disables the boost.
        int recheck_period_near = 15;
        float narrow_boost_range_m = 0.F; // 0 = boost off
        float track_roi_scale = 4.F;      // target ROI = size x this
        float track_roi_min = 0.15F;      // ...but never narrower than this
        // R-14: fractional enlargement of each tile so neighbours overlap; 0
        // is the original touching grid. A target on a seam is otherwise split
        // between two crops and detected in neither.
        float tile_overlap = 0.F;
        // R-16: per-consecutive-miss growth of the cue window, and its cap.
        // Growth costs resolution (a wider crop resizes the target smaller), so
        // it is bounded rather than run to the full frame.
        float track_roi_growth = 1.F; // 1 = no expansion (previous behaviour)
        float track_roi_max = 1.F;    // frame-width fraction
        float aspect = 16.F / 9.F;    // image W/H, to keep ROIs square in pixels
        bool dual_eo = false;         // S-11 allocation on (P4 pipeline wiring)
        // R-11: hits required drops by this much once both channels agree
        // (floor 2). Never rises — see dual_confirmed().
        int dual_confirm_relax = 2;
    };

    explicit SearchScheduler(const Params& p) : p_(p) {}
    SearchScheduler() = default;

    // S-11: channel whose frame gets the inference slot for the frame about to
    // be processed. Always WIDE when dual_eo is off. Stable until update().
    Channel channel() const;
    const char* channel_name() const;

    // Region to run inference on for the frame about to be processed, in the
    // coordinates of the channel() frame. (Cross-channel coordinate mapping is
    // the P4 pipeline's job — extrinsic calibration, REQ-001 DEFERRED.)
    Roi roi() const;

    // Whether roi() is expressed in the CUE's frame — the shared wide-channel
    // coordinates the tracker works in — rather than being a channel-local tile
    // of the frame about to be cropped. A NARROW slot must map a cue-frame ROI
    // through ChannelMap before cropping the narrow image (S-13); a tile ROI is
    // pure grid fractions and applies to whichever frame it is handed.
    bool roi_is_cue_window() const;

    // Hits needed to confirm, after the R-11 cross-channel relaxation.
    int confirm_hits_required() const;
    void tick_confirm(const TrackCue& cue);

    // Feed back what the tracker made of the frame just processed. Drives every
    // mode transition; call exactly once per processed frame.
    void update(const TrackCue& cue);

    Mode mode() const { return mode_; }
    // Consecutive frames the confirmed/confirming target has gone undetected.
    // Drives the R-16 window expansion; reset by any hit and on mode entry.
    int miss_run() const { return miss_run_; }
    // True once the target has met the R-6 confirmation criterion. Until then the
    // seeker publishes valid=0: a target that has not been seen consistently is
    // not something the OBC should be guiding on.
    bool confirmed() const { return mode_ == Mode::TRACK; }

    // R-11: the current track was seen by BOTH channels inside its confirmation
    // window. Stronger evidence than the same hit count from one channel — the
    // two channels have independent optics, resolution and noise, so a false
    // positive rarely reappears at the same bearing in the other. Published on
    // the TrackBus so downstream can see WHY a track was believed.
    bool dual_confirmed() const { return wide_hit_ && narrow_hit_; }
    int window_hits() const;
    int tile_index() const { return tile_; }
    // The grid rectangle for tile `index`, exposed so the seam coverage the
    // overlap exists for can be checked directly rather than inferred from a
    // sweep (R-14).
    Roi tile_at(int index) const { return tile_roi(index); }
    const char* mode_name() const;

private:
    void enter(Mode m);
    Roi tile_roi(int index) const;
    Roi target_roi() const;
    bool track_recheck_now() const;

    Params p_{};
    Mode mode_ = Mode::SEARCH_WIDE;
    int tile_ = 0;   // next tile to inspect in SEARCH_TILE (wide channel)
    int tile_n_ = 0; // narrow channel's own continuous tile sweep (S-11 search)
    int dwell_ = 0;  // frames spent in the current search mode
    int frames_in_mode_ = 0;
    uint32_t slot_ = 0;   // frame parity for the S-11 wide/narrow alternation
    uint32_t window_ = 0; // bit k = hit on the k-th most recent frame
    int window_len_ = 0;  // frames accumulated since entering CONFIRM/TRACK
    // Terminal phase (R-10 close-range boost): set from the cue range each
    // update; stretches the wide-recheck cadence so narrow approaches 60 Hz.
    bool near_range_ = false;
    int miss_run_ = 0; // consecutive misses on the current candidate (R-16)
    // The identity the window is accumulating FOR (TR-7): R-6 requires the
    // SAME target across the window, so a primary handoff restarts it.
    uint32_t cue_id_ = 0;
    // Which channels produced a HIT for the identity being confirmed (R-11).
    // Reset with the window, so they always describe the current candidate.
    bool wide_hit_ = false;
    bool narrow_hit_ = false;
    float cue_cx_ = 0.5F, cue_cy_ = 0.5F, cue_size_ = 0.F;
};

} // namespace riposte
