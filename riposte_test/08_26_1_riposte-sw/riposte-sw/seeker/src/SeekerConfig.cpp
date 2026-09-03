#include "SeekerConfig.h"

#include <string>

#include "riposte/Tunables.h"

namespace riposte {

namespace {
bool in01(float v) {
    return v >= 0.F && v <= 1.F;
}

// Split out of validate() so each group stays readable and testable as one
// idea; the caller runs them in order and reports the first failure.
bool validate_detector(const SeekerConfig& c, std::string& err) {
    // Detector.
    if (!in01(c.score_thr)) {
        err = "seeker.score_thr must be in [0,1]";
        return false;
    }
    if (!in01(c.nms_iou)) {
        err = "seeker.nms_iou must be in [0,1]";
        return false;
    }
    if (c.model_size < 32 || (c.model_size % 2) != 0) {
        err = "seeker.model_size must be an even integer >= 32";
        return false;
    }
    if (c.num_classes < 1) {
        err = "seeker.model_classes must be >= 1";
        return false;
    }
    if (c.target_class < 0 || c.target_class >= c.num_classes) {
        err = "seeker.target_class must be in [0, model_classes)";
        return false;
    }
    return true;
}

bool validate_scheduler(const SeekerConfig& c, std::string& err) {
    // Search scheduler.
    // The sweep addresses grid*grid tiles as a signed int; an absurd grid would
    // overflow that product long before it produced a usable sweep (CR-07).
    constexpr int MAX_GRID = 64;
    if (c.grid < 1 || c.grid > MAX_GRID) {
        err = "seeker.search_grid must be in [1, 64]";
        return false;
    }
    if (c.wide_dwell < 1) {
        err = "seeker.search_wide_dwell must be >= 1";
        return false;
    }
    if (c.confirm_window < 1 || c.confirm_window > 32) {
        err = "seeker.confirm_window must be in [1, 32]";
        return false;
    }
    if (c.confirm_hits < 1 || c.confirm_hits > c.confirm_window) {
        err = "seeker.confirm_hits must be in [1, confirm_window]";
        return false;
    }
    if (c.recheck_period < 0) {
        err = "seeker.track_recheck_period must be >= 0";
        return false;
    }
    if (!(c.track_roi_scale > 0.F)) {
        err = "seeker.track_roi_scale must be > 0";
        return false;
    }
    if (!(c.track_roi_min > 0.F) || c.track_roi_min > 1.F) {
        err = "seeker.track_roi_min must be in (0, 1]";
        return false;
    }
    // R-14: an overlap of 1 would double every tile, defeating the point of
    // tiling (native-resolution crops); negative is meaningless.
    if (c.tile_overlap < 0.F || c.tile_overlap >= 1.F) {
        err = "seeker.tile_overlap must be in [0, 1)";
        return false;
    }
    // R-16: growth below 1 would SHRINK the window while the target is being
    // lost, which is the opposite of the intent.
    if (!(c.track_roi_growth >= 1.F)) {
        err = "seeker.track_roi_growth must be >= 1 (1 disables expansion)";
        return false;
    }
    if (!(c.track_roi_max >= c.track_roi_min) || c.track_roi_max > 1.F) {
        err = "seeker.track_roi_max must be in [track_roi_min, 1]";
        return false;
    }
    return true;
}

bool validate_camera_and_recording(const SeekerConfig& c, std::string& err) {
    // Camera.
    if (c.width < 2 || c.height < 2) {
        err = "seeker.width/height must be >= 2";
        return false;
    }
    // Recording.
    if (!(c.record_fps > 0.0)) {
        err = "seeker.record_fps must be > 0";
        return false;
    }
    if (!(c.record_disk_high > 0.0) || c.record_disk_high >= 1.0) {
        err = "seeker.record_disk_high must be in (0, 1)";
        return false;
    }
    if (!(c.record_disk_free > 0.0) || c.record_disk_free >= 1.0) {
        err = "seeker.record_disk_free must be in (0, 1)";
        return false;
    }
    if (c.record_disk_free >= c.record_disk_high) {
        err = "seeker.record_disk_free must be < record_disk_high";
        return false;
    }
    if (!(c.record_segment_s > 0.0)) {
        err = "seeker.record_segment_s must be > 0";
        return false;
    }
    // R-10 boost (CR-07): with the boost enabled the near cadence is what keeps
    // the authoritative full-frame re-detect running (R-7); a non-positive value
    // switched it off entirely, which is the opposite of a safe default.
    if (!(c.narrow_boost_range_m >= 0.F)) {
        err = "seeker.narrow_boost_range_m must be >= 0";
        return false;
    }
    if (c.narrow_boost_range_m > 0.F && c.recheck_period_near < 1) {
        err = "seeker.track_recheck_period_near must be >= 1 when the boost is enabled";
        return false;
    }
    // S-11 dual-EO allocation (P4 wired, 2026-08-17). The capability gate that
    // used to reject this outright is gone; what remains is the geometry the
    // handoff needs. A narrow HFOV that is not strictly inside the wide one is
    // not a second channel — the ChannelMap ratio would invert and remapped
    // sizes (and therefore monocular range) would be wrong in a way that still
    // looks plausible.
    if (c.dual_eo) {
        if (!(c.narrow_hfov_rad > 0.F) || c.narrow_hfov_rad >= c.hfov_rad) {
            err = "seeker.narrow_hfov_rad must be > 0 and < seeker.hfov_rad";
            return false;
        }
    }
    // TR-4: the deadline exists to bound the perception thread, so it must be
    // positive and no longer than one frame period.
    const double frame_ms = static_cast<double>(tun::FRAME_PERIOD_NS) * 1e-6;
    if (!(c.embed_deadline_ms > 0.0) || c.embed_deadline_ms > frame_ms) {
        err = "seeker.embed_deadline_ms must be in (0, one frame period]";
        return false;
    }
    return true;
}
} // namespace

bool validate(const SeekerConfig& c, std::string& err) {
    if (!validate_detector(c, err) || !validate_scheduler(c, err) ||
        !validate_camera_and_recording(c, err)) {
        return false;
    }
    err.clear();
    return true;
}

} // namespace riposte
