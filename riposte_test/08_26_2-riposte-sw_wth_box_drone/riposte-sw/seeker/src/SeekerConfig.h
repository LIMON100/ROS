#pragma once
#include <string>

namespace riposte {

// Validated seeker configuration (AGENTS §7.9 / SAN R7.x: reject bad config at
// startup, never run on it). main() reads the raw INI values into this struct
// and calls validate() before building anything; an out-of-range value fails
// the process fast with a precise message instead of surfacing later as a
// divide-by-zero (grid=0), a mask overflow (confirm_window>32), or plausible-
// but-wrong detections (score_thr outside [0,1]).
//
// Pure logic — no Config, no shm — so the range rules are host unit-tested
// (test/test_seekerconfig.cpp) independently of INI parsing.
struct SeekerConfig {
    // Detector.
    float score_thr = 0.4F; // [0,1]
    float nms_iou = 0.45F;  // [0,1]
    int model_size = 640;   // >= 32, even
    int num_classes = 1;    // >= 1
    int target_class = 0;   // [0, num_classes)
    // Search scheduler.
    int grid = 3;                // >= 1, and grid*grid must not overflow
    int confirm_window = 10;     // [1, 32] (bitmask width)
    int confirm_hits = 8;        // [1, confirm_window]
    int recheck_period = 30;     // >= 0 (0 disables)
    int wide_dwell = 2;          // >= 1 (wide passes before the tile sweep)
    float track_roi_scale = 4.F; // > 0
    float track_roi_min = 0.15F; // (0, 1]
    // R-14 tile overlap: fractional enlargement of each tile, [0, 1). A target
    // on a seam is split between two crops and detected in neither.
    float tile_overlap = 0.12F;
    // R-16 cue-window expansion while detections are missing: growth per
    // consecutive miss (>= 1, 1 disables) and its cap as a frame fraction.
    float track_roi_growth = 1.25F;
    float track_roi_max = 0.60F;
    // R-10 close-range narrow boost. A NEGATIVE near-recheck period silently
    // disabled the R-7 authoritative re-detect inside the boost range (review
    // CR-07, reproduced), so it is range-checked like any other cadence.
    float narrow_boost_range_m = 150.F; // >= 0 (0 disables the boost)
    int recheck_period_near = 90;       // > 0 whenever the boost is enabled
    // T1 embedding deadline (TR-4). Positive and inside the frame budget: a
    // deadline longer than a frame period bounds nothing.
    double embed_deadline_ms = 6.0; // (0, frame period]
    // S-11 dual-EO inference allocation (P4). When on, the narrow channel is
    // part of the perception pipeline and its geometry must be sane.
    bool dual_eo = false;
    float hfov_rad = 1.05F;         // wide channel, > 0
    float narrow_hfov_rad = 0.269F; // narrow channel, 0 < this < hfov_rad
    // Camera (requested; the driver may renegotiate, but the request must be
    // sane).
    int width = 1280; // >= 2
    int height = 720; // >= 2
    // Recording (optional path).
    double record_fps = 30.0;       // > 0
    double record_segment_s = 30.0; // > 0 (rotation period)
    double record_disk_high = 0.80; // (0,1)
    double record_disk_free = 0.10; // (0,1), and < record_disk_high
};

// Returns true if every field is in range; otherwise false with `err` set to a
// single specific reason (the first failure found).
bool validate(const SeekerConfig& c, std::string& err);

} // namespace riposte
