#include "SearchScheduler.h"

#include "IDetector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace riposte {

namespace {
// NV12 fourcc, matching what CameraIngest negotiates and the detectors expect.
constexpr uint32_t FOURCC_NV12 = 0x3231564E; // 'N','V','1','2'

int popcount32(uint32_t v) {
    int n = 0;
    while (v != 0U) {
        v &= v - 1U;
        ++n;
    }
    return n;
}

float clamp01(float v) {
    return std::max(0.F, std::min(1.F, v));
}

// Snaps a normalized edge to an even pixel index inside [0, extent].
int snap_even(float norm, int extent) {
    int px = static_cast<int>(std::lround(norm * static_cast<float>(extent)));
    px = std::max(0, std::min(extent, px));
    return px & ~1;
}
} // namespace

Detection remap_to_frame(const Detection& d, const Roi& roi) {
    Detection r = d;
    r.cx = roi.x + (d.cx * roi.w);
    r.cy = roi.y + (d.cy * roi.h);
    r.w = d.w * roi.w;
    r.h = d.h * roi.h;
    return r;
}

bool crop_nv12(const Frame& src, const Roi& roi, std::vector<uint8_t>& scratch,
               Frame& out, Roi& used) {
    if (src.data == nullptr || src.fourcc != FOURCC_NV12 || src.width <= 0 ||
        src.height <= 0) {
        return false;
    }
    const int x0 = snap_even(clamp01(roi.x), src.width);
    const int y0 = snap_even(clamp01(roi.y), src.height);
    int cw = snap_even(clamp01(roi.w), src.width);
    int ch = snap_even(clamp01(roi.h), src.height);
    cw = std::min(cw, src.width - x0);
    ch = std::min(ch, src.height - y0);
    const int cw16 = cw & ~15;
    cw = (cw16 >= 16) ? cw16 : (cw & ~1);
    ch &= ~1;
    if (cw < 2 || ch < 2) {
        return false; // degenerate request
    }

    const size_t stride =
        (src.stride != 0U) ? src.stride : static_cast<size_t>(src.width);
    const size_t cwz = static_cast<size_t>(cw);
    scratch.resize(cwz * static_cast<size_t>(ch) * 3U / 2U);

    // Luma rows.
    uint8_t* dst = scratch.data();
    for (int r = 0; r < ch; ++r) {
        const uint8_t* srow =
            src.data + (static_cast<size_t>(y0 + r) * stride) + static_cast<size_t>(x0);
        std::memcpy(dst, srow, cwz);
        dst += cwz;
    }
    // Interleaved chroma: half the rows, full width, and the x offset stays as-is
    // because a UV pair covers two luma columns (byte offset == luma column).
    const uint8_t* uv_plane = src.data + (static_cast<size_t>(src.height) * stride);
    for (int r = 0; r < ch / 2; ++r) {
        const uint8_t* srow = uv_plane + (static_cast<size_t>((y0 / 2) + r) * stride) +
                              static_cast<size_t>(x0);
        std::memcpy(dst, srow, cwz);
        dst += cwz;
    }

    used.x = static_cast<float>(x0) / static_cast<float>(src.width);
    used.y = static_cast<float>(y0) / static_cast<float>(src.height);
    used.w = static_cast<float>(cw) / static_cast<float>(src.width);
    used.h = static_cast<float>(ch) / static_cast<float>(src.height);

    out = src;
    out.data = scratch.data();
    out.width = cw;
    out.height = ch;
    out.stride = cwz;
    out.src_roi = used; // the crop carries where in the sensor frame it came from
    return true;
}

const char* SearchScheduler::mode_name() const {
    switch (mode_) {
        case Mode::SEARCH_WIDE:
            return "SEARCH_WIDE";
        case Mode::SEARCH_TILE:
            return "SEARCH_TILE";
        case Mode::CONFIRM:
            return "CONFIRM";
        case Mode::TRACK:
            return "TRACK";
    }
    return "?";
}

void SearchScheduler::enter(Mode m) {
    const Mode prev = mode_;
    mode_ = m;
    dwell_ = 0;
    frames_in_mode_ = 0;
    // The expansion belongs to the candidate being inspected; a new mode is a
    // new inspection, so the window starts at its nominal size (R-16).
    miss_run_ = 0;
    if (m == Mode::SEARCH_WIDE || m == Mode::SEARCH_TILE) {
        window_ = 0;
        window_len_ = 0;
        // The channel evidence belongs to the candidate that was being
        // confirmed; a restarted search has no candidate yet (R-11).
        wide_hit_ = false;
        narrow_hit_ = false;
        // The narrow sweep restarts only when the search itself restarts (a
        // fall back from CONFIRM/TRACK). The wide machine oscillates
        // WIDE<->TILE while searching, and resetting the narrow sweep on every
        // such transition would keep it re-visiting its first tiles forever.
        if (prev == Mode::CONFIRM || prev == Mode::TRACK) {
            tile_n_ = 0;
        }
    }
    if (m == Mode::SEARCH_TILE) {
        tile_ = 0; // a fallback to tiles always restarts the sweep
    }
}

bool SearchScheduler::track_recheck_now() const {
    // Close-range terminal phase (R-10): stretch the wide-recheck cadence so
    // the narrow channel keeps nearly every frame (~60 Hz inference) while the
    // periodic full-frame AI verification (R-7) still runs, just rarer.
    const int period = near_range_ ? p_.recheck_period_near : p_.recheck_period;
    return period > 0 && frames_in_mode_ > 0 && (frames_in_mode_ % period) == 0;
}

SearchScheduler::Channel SearchScheduler::channel() const {
    if (!p_.dual_eo) {
        return Channel::WIDE; // single camera: every slot is the one camera
    }
    switch (mode_) {
        case Mode::SEARCH_WIDE:
        case Mode::SEARCH_TILE:
        case Mode::CONFIRM:
            // 30 Hz each at 60 fps capture: even slots wide, odd slots narrow.
            return ((slot_ & 1U) == 0U) ? Channel::WIDE : Channel::NARROW;
        case Mode::TRACK:
            // Terminal precision lives on the narrow channel (halved LOS-rate
            // noise); the R-7 recheck slot goes to a wide full-frame pass and
            // the narrow simply skips that frame (one inference per frame).
            return track_recheck_now() ? Channel::WIDE : Channel::NARROW;
    }
    return Channel::WIDE;
}

const char* SearchScheduler::channel_name() const {
    return channel() == Channel::WIDE ? "wide" : "narrow";
}

Roi SearchScheduler::tile_roi(int index) const {
    const int g = std::max(1, p_.grid);
    const int col = index % g;
    const int row = (index / g) % g;
    // R-14 overlapping grid. Each tile is enlarged by `tile_overlap` and the
    // tiles are then spread so the first starts at 0 and the last ends at 1;
    // neighbours therefore overlap and the union still covers the frame
    // exactly. With overlap 0 this reduces to the original touching grid
    // (x = col/g, w = 1/g), so the previous behaviour is a special case rather
    // than something to keep separately.
    const float side =
        std::min(1.F, (1.F + std::max(0.F, p_.tile_overlap)) / static_cast<float>(g));
    const float span = (g > 1) ? (1.F - side) / static_cast<float>(g - 1) : 0.F;
    Roi r;
    r.x = static_cast<float>(col) * span;
    r.y = static_cast<float>(row) * span;
    r.w = side;
    r.h = side;
    return r;
}

Roi SearchScheduler::target_roi() const {
    // Square in PIXELS: the height fraction is the width fraction times the
    // aspect ratio, otherwise the window is squashed on non-square sensors.
    float side = std::max(p_.track_roi_min, cue_size_ * p_.track_roi_scale);
    // R-16: widen while detections are missing. The window is centred on the
    // tracker's PREDICTION, and the prediction drifts precisely while there is
    // nothing to correct it — so holding the window at its nominal size looks
    // for the target where it is least likely to still be. Bounded, because a
    // wider crop hands the model fewer pixels on the target.
    if (miss_run_ > 0 && p_.track_roi_growth > 1.F) {
        float grow = 1.F;
        for (int i = 0; i < miss_run_ && grow < 64.F; ++i) {
            grow *= p_.track_roi_growth;
        }
        side = std::min(side * grow, std::max(side, p_.track_roi_max));
    }
    const float w = std::min(1.F, side);
    const float h = std::min(1.F, side * p_.aspect);
    Roi r;
    r.x = clamp01(cue_cx_ - (w * 0.5F));
    r.y = clamp01(cue_cy_ - (h * 0.5F));
    r.w = w;
    r.h = h;
    // Keep the window inside the frame rather than letting it shrink at the edge:
    // a target near the border still needs its full neighbourhood inspected.
    r.x = std::min(r.x, 1.F - w);
    r.y = std::min(r.y, 1.F - h);
    return r;
}

// R-11 confirmation threshold. Cross-channel agreement lowers the bar; nothing
// raises it. The floor of 2 keeps a single frame from ever confirming a track:
// one detection is a glint, two in a window is a target.
int SearchScheduler::confirm_hits_required() const {
    if (!p_.dual_eo || !dual_confirmed()) {
        return p_.confirm_hits;
    }
    return std::max(2, p_.confirm_hits - p_.dual_confirm_relax);
}

bool SearchScheduler::roi_is_cue_window() const {
    // CONFIRM and the tracking slots of TRACK aim at the cue; everything else
    // is a tile sweep or a full frame.
    return mode_ == Mode::CONFIRM || (mode_ == Mode::TRACK && !track_recheck_now());
}

Roi SearchScheduler::roi() const {
    // On a narrow-channel search slot the narrow camera runs its OWN continuous
    // tile sweep (REQ-001 §4: narrow tiles are what reach 300 m), regardless of
    // whether the wide channel is in its wide pass or its tile fallback.
    const bool narrow_slot = p_.dual_eo && channel() == Channel::NARROW;
    switch (mode_) {
        case Mode::SEARCH_WIDE:
            return narrow_slot ? tile_roi(tile_n_) : Roi{};
        case Mode::SEARCH_TILE:
            return narrow_slot ? tile_roi(tile_n_) : tile_roi(tile_);
        case Mode::CONFIRM:
            return target_roi();
        case Mode::TRACK:
            // R-7: periodically re-detect over the whole frame so a committed
            // track is re-validated against the full scene, not just the window
            // the tracker's own prediction is steering. In dual-EO this slot is
            // the wide channel's (see channel()).
            if (track_recheck_now()) {
                return Roi{};
            }
            return target_roi();
    }
    return Roi{};
}

int SearchScheduler::window_hits() const {
    const int n = std::max(1, p_.confirm_window);
    const uint32_t mask =
        (n >= 32) ? 0xFFFFFFFFU : ((1U << static_cast<unsigned>(n)) - 1U);
    return popcount32(window_ & mask);
}

// The confirmation window (R-6/TR-7/R-11), split out so update() stays inside
// the size budget. It is one idea: does the evidence justify TRACK yet.
void SearchScheduler::tick_confirm(const TrackCue& cue) {
    if (!cue.alive) {
        enter(Mode::SEARCH_WIDE); // tracker dropped it; start over
        return;
    }
    // R-6 demands the SAME target across the window (TR-7): if the
    // primary identity changed mid-confirmation, the hits so far
    // belong to the previous target — the new one starts from zero.
    if (cue.track_id != cue_id_) {
        window_ = 0;
        window_len_ = 0;
        wide_hit_ = false;
        narrow_hit_ = false;
        cue_id_ = cue.track_id;
    }
    window_ = (window_ << 1U) | (cue.hit ? 1U : 0U);
    window_len_ = std::min(window_len_ + 1, p_.confirm_window);
    miss_run_ = cue.hit ? 0 : (miss_run_ + 1); // R-16 window expansion
    // R-11: remember WHICH channel saw it, so agreement across the two
    // can be recognized below.
    if (cue.hit) {
        (cue.hit_from_narrow ? narrow_hit_ : wide_hit_) = true;
    }
    const int hits = window_hits();
    const int need = confirm_hits_required();
    if (hits >= need) {
        enter(Mode::TRACK);
        return;
    }
    // Give up as soon as the criterion has become unreachable inside the
    // window, rather than lingering on a target that keeps flickering.
    // Judged against the SAME threshold, or a relaxed criterion could be
    // abandoned while still reachable.
    const int misses = window_len_ - hits;
    if (misses > p_.confirm_window - need) {
        enter(Mode::SEARCH_WIDE);
    }
}

void SearchScheduler::update(const TrackCue& cue) {
    // The channel the frame just processed was allocated to — captured BEFORE
    // any counter moves, so it matches what channel()/roi() said pre-frame.
    const bool narrow_slot = p_.dual_eo && channel() == Channel::NARROW;
    if (cue.alive) {
        cue_cx_ = cue.cx;
        cue_cy_ = cue.cy;
        cue_size_ = cue.size;
    }
    // R-10 close-range boost gate: inside narrow_boost_range_m (and only with a
    // known range) the terminal phase is active. Off when range is unknown
    // (0) or the boost is disabled, so it never over-commits on a bad estimate.
    near_range_ = p_.narrow_boost_range_m > 0.F && cue.alive && cue.range_m > 0.F &&
                  cue.range_m <= p_.narrow_boost_range_m;
    ++frames_in_mode_;
    ++slot_;

    switch (mode_) {
        case Mode::SEARCH_WIDE:
            if (cue.alive) {
                cue_id_ = cue.track_id; // the identity this window confirms (TR-7)
                enter(Mode::CONFIRM);
                // The discovering frame is evidence from ITS channel (R-11).
                // It does not enter the hit window — that starts empty, as it
                // always has — but it did see this candidate, and pretending
                // otherwise would make agreement need one extra frame.
                // Only an actual HIT is evidence: a track can re-enter CONFIRM
                // merely alive (coasting after an abandoned window, within the
                // tracker's miss budget), and a frame with no detection saw
                // nothing — latching a channel bit from it would let the
                // relaxed threshold fire on single-channel evidence.
                if (cue.hit) {
                    (cue.hit_from_narrow ? narrow_hit_ : wide_hit_) = true;
                }
                return;
            }
            // A narrow slot advances only the narrow sweep; the wide machine's
            // dwell counts WIDE passes, not frames, so alternation does not
            // halve its meaning.
            if (narrow_slot) {
                tile_n_ = (tile_n_ + 1) % (p_.grid * p_.grid);
                return;
            }
            // Nothing at wide resolution: fall back to the tile sweep, where a
            // distant target reaches the model large enough to be detected.
            if (++dwell_ >= p_.wide_dwell) {
                enter(Mode::SEARCH_TILE);
            }
            return;

        case Mode::SEARCH_TILE:
            if (cue.alive) {
                cue_id_ = cue.track_id; // the identity this window confirms (TR-7)
                enter(Mode::CONFIRM);
                if (cue.hit) { // R-11, hit-gated — see SEARCH_WIDE above
                    (cue.hit_from_narrow ? narrow_hit_ : wide_hit_) = true;
                }
                return;
            }
            if (narrow_slot) {
                tile_n_ = (tile_n_ + 1) % (p_.grid * p_.grid);
                return;
            }
            tile_ = tile_ + 1;
            if (tile_ >= p_.grid * p_.grid) {
                enter(Mode::SEARCH_WIDE); // swept every tile; retry wide
            }
            return;

        case Mode::CONFIRM:
            tick_confirm(cue);
            return;

        case Mode::TRACK:
            // Identity handoff (TR-7): the primary died and another track was
            // promoted in the same update. The new target has never been
            // through ITS confirmation window — it must not inherit the old
            // one's TRACK status, so confirmation restarts for it.
            if (cue.alive && cue.track_id != cue_id_) {
                cue_id_ = cue.track_id;
                enter(Mode::CONFIRM);
                window_ = cue.hit ? 1U : 0U; // this frame is its first sample
                window_len_ = 1;
                // Evidence restarts with the identity (R-11/TR-7), attributed
                // to the channel that ACTUALLY produced the detection — not
                // the slot's allocation, which falls back to the wide frame
                // when the narrow grab missed or the cue left the narrow FOV.
                wide_hit_ = false;
                narrow_hit_ = false;
                if (cue.hit) {
                    (cue.hit_from_narrow ? narrow_hit_ : wide_hit_) = true;
                }
                return;
            }
            window_ = (window_ << 1U) | (cue.hit ? 1U : 0U);
            window_len_ = std::min(window_len_ + 1, p_.confirm_window);
            miss_run_ = cue.hit ? 0 : (miss_run_ + 1); // R-16 window expansion
            // Leave TRACK only when the tracker itself has given the target up:
            // it already coasts through short dropouts (TRACKER_MAX_MISSES), and
            // re-running confirmation on every brief miss would just churn.
            if (!cue.alive) {
                enter(Mode::SEARCH_WIDE);
            }
            return;
    }
}

} // namespace riposte
