// SearchScheduler unit tests — the tiling search policy (REQ-001 R-5), the
// 5-frame/80 % confirmation window (R-6) and the periodic wide re-detect while
// tracking (R-7), plus the NV12 crop + coordinate remap that make a tiled
// inference pass land in full-frame coordinates.
//
// All pure logic: no camera, no NPU, no shm (G11.2/G17.13).
#include "IDetector.h"
#include "SearchScheduler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

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

SearchScheduler::Params test_params() {
    SearchScheduler::Params p;
    p.grid = 3;
    p.wide_dwell = 2;
    p.confirm_window = 5;
    p.confirm_hits = 4;
    p.recheck_period = 15;
    p.track_roi_scale = 4.F;
    p.track_roi_min = 0.15F;
    p.aspect = 16.F / 9.F;
    return p;
}

TrackCue miss() {
    return TrackCue{}; // alive=false: tracker holds nothing
}

TrackCue hit(float cx = 0.5F, float cy = 0.5F, float size = 0.02F) {
    TrackCue c;
    c.alive = true;
    c.hit = true;
    c.cx = cx;
    c.cy = cy;
    c.size = size;
    return c;
}

TrackCue coasting(float cx = 0.5F, float cy = 0.5F, float size = 0.02F) {
    TrackCue c = hit(cx, cy, size);
    c.hit = false; // alive but not associated this frame
    return c;
}

// --- R-5: wide pass first, then the tile sweep -----------------------------

int test_starts_wide() {
    const SearchScheduler s(test_params());
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_WIDE);
    CHECK(s.roi().is_full());
    return 0;
}

int test_falls_back_to_tiles_after_wide_dwell() {
    SearchScheduler s(test_params());
    s.update(miss()); // wide pass 1
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_WIDE);
    s.update(miss()); // wide pass 2 -> dwell reached
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_TILE);
    CHECK(!s.roi().is_full());
    return 0;
}

int test_tile_sweep_covers_every_cell_then_retries_wide() {
    SearchScheduler s(test_params());
    s.update(miss());
    s.update(miss()); // -> SEARCH_TILE, tile 0
    // Each of the 9 tiles must be visited exactly once, in sequence, and the
    // union of them must cover the frame.
    float area = 0.F;
    bool seen[9] = {false};
    for (int i = 0; i < 9; ++i) {
        CHECK(s.mode() == SearchScheduler::Mode::SEARCH_TILE);
        const int idx = s.tile_index();
        CHECK(idx == i);
        CHECK(!seen[idx]);
        seen[idx] = true;
        const Roi r = s.roi();
        CHECK(near(r.w, 1.F / 3.F) && near(r.h, 1.F / 3.F));
        CHECK(r.x >= 0.F && r.y >= 0.F && r.x + r.w <= 1.0001F && r.y + r.h <= 1.0001F);
        area += r.w * r.h;
        s.update(miss());
    }
    CHECK(near(area, 1.F, 1e-3F)); // the 9 tiles tile the whole frame
    // Sweep exhausted with nothing found: back to a wide pass.
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_WIDE);
    return 0;
}

int test_detection_during_tile_sweep_enters_confirm() {
    SearchScheduler s(test_params());
    s.update(miss());
    s.update(miss()); // -> SEARCH_TILE
    s.update(hit());
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    return 0;
}

// --- R-6: 4 of the last 5 frames --------------------------------------------

int test_four_of_five_confirms() {
    SearchScheduler s(test_params());
    s.update(hit()); // wide -> CONFIRM (frame 1 is the acquisition, not a window sample)
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    s.update(hit());
    s.update(hit());
    s.update(hit());
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM); // 3 hits: not yet
    s.update(hit());
    CHECK(s.mode() == SearchScheduler::Mode::TRACK); // 4th hit -> confirmed
    CHECK(s.confirmed());
    return 0;
}

int test_one_miss_still_confirms() {
    // 80 % of 5 means exactly one miss is tolerable.
    SearchScheduler s(test_params());
    s.update(hit()); // -> CONFIRM
    s.update(hit());
    s.update(coasting()); // one miss inside the window
    s.update(hit());
    s.update(hit());
    s.update(hit());
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    return 0;
}

int test_two_misses_abandons_confirmation() {
    // A flickering detection must not become the engaged target: once the 80 %
    // criterion is unreachable inside the window, go back to searching.
    SearchScheduler s(test_params());
    s.update(hit()); // -> CONFIRM
    s.update(coasting());
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    s.update(coasting());
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_WIDE);
    CHECK(!s.confirmed());
    return 0;
}

int test_tracker_dropping_target_resets_to_search() {
    SearchScheduler s(test_params());
    s.update(hit()); // -> CONFIRM
    s.update(hit());
    s.update(miss()); // tracker gave the track up entirely
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_WIDE);
    CHECK(s.window_hits() == 0); // window cleared for the next attempt
    return 0;
}

// --- TR-7: confirmation is bound to track identity -------------------------

TrackCue hit_id(uint32_t id) {
    TrackCue c = hit();
    c.track_id = id;
    return c;
}

int test_identity_change_restarts_confirmation() {
    SearchScheduler s(test_params());
    s.update(hit_id(1)); // SEARCH -> CONFIRM for id 1
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    s.update(hit_id(1));
    s.update(hit_id(1)); // two window hits accumulated for id 1
    // Primary handoff mid-confirmation: the hits so far belonged to id 1
    // (R-6 "same drone") — id 2 must start from zero, and specifically must
    // NOT reach TRACK by inheriting them.
    s.update(hit_id(2));
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    s.update(hit_id(2));
    s.update(hit_id(2));
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM); // 3/4 of id 2's own hits
    s.update(hit_id(2));
    CHECK(s.mode() == SearchScheduler::Mode::TRACK); // id 2 earned its own 4
    return 0;
}

int test_track_handoff_reconfirms_new_identity() {
    SearchScheduler s(test_params());
    for (int i = 0; i < 5; ++i) {
        s.update(hit_id(1)); // enter CONFIRM, then 4 hits -> TRACK
    }
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    CHECK(s.confirmed());
    // id 1 dies and id 2 is promoted in the same tracker update: TRACK status
    // must not be inherited — the scheduler reconfirms the new identity.
    s.update(hit_id(2));
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    CHECK(!s.confirmed());
    for (int i = 0; i < 3; ++i) {
        s.update(hit_id(2));
    }
    CHECK(s.mode() == SearchScheduler::Mode::TRACK); // 4 of its own hits
    return 0;
}

int test_not_confirmed_until_track_mode() {
    // The publish gate: everything before TRACK is "no target" to the OBC.
    SearchScheduler s(test_params());
    CHECK(!s.confirmed());
    s.update(hit());
    CHECK(!s.confirmed()); // CONFIRM is not yet a commitment
    s.update(hit());
    s.update(hit());
    s.update(hit());
    CHECK(!s.confirmed());
    s.update(hit());
    CHECK(s.confirmed());
    return 0;
}

// --- TRACK behaviour: ROI, coasting, R-7 recheck -----------------------------

int confirm_into_track(SearchScheduler& s) {
    for (int i = 0; i < 5; ++i) {
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    return s.mode() == SearchScheduler::Mode::TRACK ? 0 : 1;
}

int test_track_roi_is_centred_and_pixel_square() {
    SearchScheduler s(test_params());
    CHECK(confirm_into_track(s) == 0);
    const Roi r = s.roi();
    CHECK(!r.is_full());
    CHECK(near(r.x + (r.w * 0.5F), 0.5F)); // centred on the target
    CHECK(near(r.y + (r.h * 0.5F), 0.5F));
    // Square in pixels: the height fraction is the width fraction x aspect.
    CHECK(near(r.h, r.w * test_params().aspect, 1e-3F));
    // Small target -> the floor applies rather than a degenerate window.
    CHECK(near(r.w, 0.15F));
    return 0;
}

int test_track_roi_stays_inside_frame_at_the_edge() {
    SearchScheduler s(test_params());
    for (int i = 0; i < 5; ++i) {
        s.update(hit(0.02F, 0.98F, 0.02F)); // target hard against a corner
    }
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    const Roi r = s.roi();
    CHECK(r.x >= 0.F && r.y >= 0.F);
    CHECK(r.x + r.w <= 1.0001F);
    CHECK(r.y + r.h <= 1.0001F);
    return 0;
}

int test_track_survives_short_coast() {
    // The tracker already coasts through brief dropouts; TRACK must not churn
    // back to CONFIRM on every missed frame.
    SearchScheduler s(test_params());
    CHECK(confirm_into_track(s) == 0);
    s.update(coasting());
    s.update(coasting());
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    s.update(miss()); // tracker finally dropped it
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_WIDE);
    return 0;
}

int test_periodic_wide_recheck_while_tracking() {
    // R-7: every recheck_period frames the whole frame is re-inspected.
    SearchScheduler::Params p = test_params();
    p.recheck_period = 4;
    SearchScheduler s(p);
    for (int i = 0; i < 5; ++i) {
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    int wide_passes = 0;
    for (int i = 0; i < 13; ++i) {
        if (s.roi().is_full()) {
            ++wide_passes;
        }
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    // The entry frame is frame 0, so the rechecks land on the 4th, 8th and 12th
    // frame after entering TRACK — not on entry itself.
    CHECK(wide_passes == 3);
    return 0;
}

int test_recheck_disabled_by_zero_period() {
    SearchScheduler::Params p = test_params();
    p.recheck_period = 0;
    SearchScheduler s(p);
    for (int i = 0; i < 5; ++i) {
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    for (int i = 0; i < 20; ++i) {
        CHECK(!s.roi().is_full());
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    return 0;
}

// ---- R-14: overlapping tiles ------------------------------------------------

// The property that matters is not "tiles overlap" but its consequence: every
// point of the frame, INCLUDING a target sitting exactly on a seam, is wholly
// inside at least one tile. With touching tiles a target on the boundary is
// split between two crops, each half below the detector's size floor, and any
// box that does come back is truncated — which the monocular range estimate
// reads as a further-away target.
int test_overlapping_tiles_cover_every_seam() {
    SearchScheduler::Params p = test_params();
    p.grid = 3;
    p.tile_overlap = 0.12F;
    const SearchScheduler s(p);
    // A target of this normalized extent must fit wholly in some tile wherever
    // its centre falls — the seams are the interesting places.
    const float target = 0.03F;
    for (int i = 0; i <= 200; ++i) {
        const float cx = static_cast<float>(i) / 200.F;
        for (int j = 0; j <= 200; ++j) {
            const float cy = static_cast<float>(j) / 200.F;
            const float x0 = std::max(0.F, cx - (target * 0.5F));
            const float x1 = std::min(1.F, cx + (target * 0.5F));
            const float y0 = std::max(0.F, cy - (target * 0.5F));
            const float y1 = std::min(1.F, cy + (target * 0.5F));
            bool whole = false;
            for (int t = 0; t < p.grid * p.grid && !whole; ++t) {
                const Roi r = s.tile_at(t);
                whole = x0 >= r.x - 1e-5F && y0 >= r.y - 1e-5F &&
                        x1 <= r.x + r.w + 1e-5F && y1 <= r.y + r.h + 1e-5F;
            }
            CHECK(whole);
        }
    }
    return 0;
}

// Zero overlap must reproduce the original touching grid exactly, so the change
// is a strict extension rather than a retune of the search everyone flew with.
int test_zero_overlap_is_the_original_grid() {
    SearchScheduler::Params p = test_params();
    p.grid = 3;
    p.tile_overlap = 0.F;
    const SearchScheduler s(p);
    float area = 0.F;
    for (int t = 0; t < 9; ++t) {
        const int col = t % 3;
        const int row = t / 3; // grid row index, not a fractional quantity
        const Roi r = s.tile_at(t);
        CHECK(near(r.w, 1.F / 3.F));
        CHECK(near(r.h, 1.F / 3.F));
        CHECK(near(r.x, static_cast<float>(col) / 3.F));
        CHECK(near(r.y, static_cast<float>(row) / 3.F));
        area += r.w * r.h;
    }
    CHECK(near(area, 1.F, 1e-3F));
    return 0;
}

// Tiles must stay inside the frame at any overlap: a crop that runs off the
// edge is clamped by crop_nv12 to a smaller rectangle, and the remap would then
// place detections using a window the detector never saw.
// G16.6 deviation: a bounds matrix over overlaps x grid sizes; each CHECK pins
// one edge (G16.6) NOLINTNEXTLINE(readability-function-size)
int test_tiles_stay_inside_the_frame() {
    for (const float ov : {0.F, 0.1F, 0.3F, 0.9F}) {
        for (const int g : {1, 2, 3, 4}) {
            SearchScheduler::Params p = test_params();
            p.grid = g;
            p.tile_overlap = ov;
            const SearchScheduler s(p);
            for (int t = 0; t < g * g; ++t) {
                const Roi r = s.tile_at(t);
                CHECK(r.x >= -1e-5F && r.y >= -1e-5F);
                CHECK(r.x + r.w <= 1.F + 1e-5F);
                CHECK(r.y + r.h <= 1.F + 1e-5F);
                CHECK(r.w > 0.F && r.h > 0.F);
            }
        }
    }
    return 0;
}

// ---- R-16: the cue window widens while detections are missing --------------

// The window is centred on the tracker's PREDICTION, and the prediction drifts
// precisely while there is nothing correcting it. Holding the window at its
// nominal size searches where the target is least likely to still be.
int test_cue_window_grows_with_consecutive_misses() {
    SearchScheduler::Params p = test_params();
    p.track_roi_growth = 1.25F;
    p.track_roi_max = 0.6F;
    SearchScheduler s(p);
    for (int i = 0; i < 5; ++i) {
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    const float nominal = s.roi().w;
    CHECK(s.miss_run() == 0);

    s.update(coasting(0.5F, 0.5F, 0.02F));
    CHECK(s.miss_run() == 1);
    const float one_miss = s.roi().w;
    CHECK(one_miss > nominal);

    s.update(coasting(0.5F, 0.5F, 0.02F));
    CHECK(s.miss_run() == 2);
    CHECK(s.roi().w > one_miss); // keeps widening while nothing is found
    return 0;
}

// Expansion is not free: a wider crop resizes the target to fewer model-input
// pixels, so it must be bounded — an unbounded window would end up handing the
// detector the whole frame at exactly the moment it needs resolution most.
int test_cue_window_expansion_is_capped() {
    SearchScheduler::Params p = test_params();
    p.track_roi_growth = 1.5F;
    p.track_roi_max = 0.4F;
    SearchScheduler s(p);
    for (int i = 0; i < 5; ++i) {
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    for (int i = 0; i < 40; ++i) {
        s.update(coasting(0.5F, 0.5F, 0.02F));
        // The periodic R-7 slot deliberately inspects the WHOLE frame; the cap
        // bounds the cue window, not that re-validation pass.
        if (!s.roi().is_full()) {
            CHECK(s.roi().w <= p.track_roi_max + 1e-4F);
        }
        CHECK(s.roi().x >= -1e-5F);
        CHECK(s.roi().x + s.roi().w <= 1.F + 1e-5F);
    }
    return 0;
}

// A single detection means the prediction is corrected again, so the window
// returns to its nominal size: leaving it wide would keep feeding the detector
// a low-resolution crop long after the reason for widening had gone.
int test_a_hit_resets_the_expansion() {
    SearchScheduler::Params p = test_params();
    p.track_roi_growth = 1.25F;
    p.track_roi_max = 0.6F;
    SearchScheduler s(p);
    for (int i = 0; i < 5; ++i) {
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    const float nominal = s.roi().w;
    s.update(coasting(0.5F, 0.5F, 0.02F));
    s.update(coasting(0.5F, 0.5F, 0.02F));
    CHECK(s.roi().w > nominal);
    s.update(hit(0.5F, 0.5F, 0.02F));
    CHECK(s.miss_run() == 0);
    CHECK(near(s.roi().w, nominal, 1e-5F));
    return 0;
}

// Growth of 1 must reproduce the previous fixed-window behaviour exactly, so a
// profile that does not want expansion is not silently getting some.
int test_growth_one_disables_expansion() {
    SearchScheduler::Params p = test_params();
    p.track_roi_growth = 1.F;
    SearchScheduler s(p);
    for (int i = 0; i < 5; ++i) {
        s.update(hit(0.5F, 0.5F, 0.02F));
    }
    const float nominal = s.roi().w;
    for (int i = 0; i < 6; ++i) {
        s.update(coasting(0.5F, 0.5F, 0.02F));
        CHECK(near(s.roi().w, nominal, 1e-6F));
    }
    return 0;
}

// --- Coordinate remap --------------------------------------------------------

int test_remap_centre_of_tile() {
    // A detection at the centre of a tile must land at that tile's centre in
    // full-frame coordinates — otherwise every tiled detection is mis-aimed.
    Roi tile;
    tile.x = 1.F / 3.F;
    tile.y = 2.F / 3.F;
    tile.w = 1.F / 3.F;
    tile.h = 1.F / 3.F;
    Detection d{};
    d.cx = 0.5F;
    d.cy = 0.5F;
    d.w = 0.5F;
    d.h = 0.25F;
    d.score = 0.9F;
    d.cls = 0;
    const Detection r = remap_to_frame(d, tile);
    CHECK(near(r.cx, 0.5F));      // 1/3 + 0.5*1/3
    CHECK(near(r.cy, 5.F / 6.F)); // 2/3 + 0.5*1/3
    CHECK(near(r.w, 0.5F / 3.F)); // sizes scale with the crop
    CHECK(near(r.h, 0.25F / 3.F));
    CHECK(near(r.score, 0.9F) && r.cls == 0); // untouched fields survive
    return 0;
}

int test_remap_is_identity_on_full_frame() {
    const Roi full;
    Detection d{};
    d.cx = 0.3F;
    d.cy = 0.7F;
    d.w = 0.1F;
    d.h = 0.2F;
    const Detection r = remap_to_frame(d, full);
    CHECK(near(r.cx, 0.3F) && near(r.cy, 0.7F));
    CHECK(near(r.w, 0.1F) && near(r.h, 0.2F));
    return 0;
}

int test_remap_corners() {
    Roi tile;
    tile.x = 2.F / 3.F;
    tile.y = 0.F;
    tile.w = 1.F / 3.F;
    tile.h = 1.F / 3.F;
    Detection d{};
    d.cx = 0.F;
    d.cy = 1.F;
    const Detection r = remap_to_frame(d, tile);
    CHECK(near(r.cx, 2.F / 3.F)); // tile's left edge
    CHECK(near(r.cy, 1.F / 3.F)); // tile's bottom edge
    return 0;
}

// --- NV12 crop ---------------------------------------------------------------

constexpr uint32_t FOURCC_NV12 = 0x3231564E;

// Builds an NV12 frame whose luma byte encodes its column and chroma its row, so
// a crop can be checked to have taken the right pixels.
std::vector<uint8_t> make_nv12(int w, int h) {
    std::vector<uint8_t> buf(static_cast<size_t>(w) * static_cast<size_t>(h) * 3U / 2U);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            buf.at((static_cast<size_t>(y) * static_cast<size_t>(w)) +
                   static_cast<size_t>(x)) = static_cast<uint8_t>((x + (y * 7)) & 0xFF);
        }
    }
    const size_t uv = static_cast<size_t>(w) * static_cast<size_t>(h);
    for (int y = 0; y < h / 2; ++y) {
        for (int x = 0; x < w; ++x) {
            buf.at(uv + (static_cast<size_t>(y) * static_cast<size_t>(w)) +
                   static_cast<size_t>(x)) = static_cast<uint8_t>((x + (y * 3)) & 0xFF);
        }
    }
    return buf;
}

Frame nv12_frame(const std::vector<uint8_t>& buf, int w, int h) {
    Frame f{};
    f.mono_ns = 12345;
    f.width = w;
    f.height = h;
    f.data = buf.data();
    f.stride = static_cast<size_t>(w);
    f.fourcc = FOURCC_NV12;
    return f;
}

// G16.6 deviation: pixel-exact crop verification; each CHECK pins one byte address
// (G16.6) NOLINTNEXTLINE(readability-function-size)
int test_crop_dimensions_and_content() {
    const int w = 64;
    const int h = 48;
    const std::vector<uint8_t> buf = make_nv12(w, h);
    const Frame f = nv12_frame(buf, w, h);

    Roi roi;
    roi.x = 0.25F; // 16 px
    roi.y = 0.25F; // 12 px
    roi.w = 0.5F;  // 32 px
    roi.h = 0.5F;  // 24 px
    std::vector<uint8_t> scratch;
    Frame out{};
    Roi used{};
    CHECK(crop_nv12(f, roi, scratch, out, used));
    CHECK(out.width == 32 && out.height == 24);
    CHECK(out.stride == 32);
    CHECK(out.fourcc == FOURCC_NV12);
    CHECK(out.mono_ns == f.mono_ns); // timestamp carries through (freshness basis)
    CHECK(scratch.size() == 32U * 24U * 3U / 2U);
    // First luma pixel of the crop is source pixel (16, 12).
    CHECK(out.data[0] == static_cast<uint8_t>((16 + (12 * 7)) & 0xFF));
    // Last luma row (index 23) starts at source pixel (16, 12 + 23 = 35).
    CHECK(out.data[static_cast<size_t>(23) * 32] ==
          static_cast<uint8_t>((16 + (35 * 7)) & 0xFF));
    // First chroma byte is source UV row 6 (= y0/2), column 16.
    CHECK(out.data[static_cast<size_t>(32) * 24] ==
          static_cast<uint8_t>((16 + (6 * 3)) & 0xFF));
    CHECK(near(used.x, 0.25F) && near(used.y, 0.25F));
    CHECK(near(used.w, 0.5F) && near(used.h, 0.5F));
    return 0;
}

int test_crop_snaps_to_even_bounds() {
    // NV12 chroma is 2x2 subsampled: an odd offset would swap U and V. The crop
    // must snap and report what it actually took.
    const int w = 64;
    const int h = 48;
    const std::vector<uint8_t> buf = make_nv12(w, h);
    const Frame f = nv12_frame(buf, w, h);
    Roi roi;
    roi.x = 5.F / 64.F; // 5 px -> snaps to 4
    roi.y = 7.F / 48.F; // 7 px -> snaps to 6
    roi.w = 11.F / 64.F;
    roi.h = 9.F / 48.F;
    std::vector<uint8_t> scratch;
    Frame out{};
    Roi used{};
    CHECK(crop_nv12(f, roi, scratch, out, used));
    CHECK((out.width % 2) == 0 && (out.height % 2) == 0);
    CHECK(near(used.x, 4.F / 64.F) && near(used.y, 6.F / 48.F));
    CHECK(near(used.w, static_cast<float>(out.width) / 64.F));
    CHECK(out.data[0] == static_cast<uint8_t>((4 + (6 * 7)) & 0xFF));
    return 0;
}

int test_crop_clamps_to_frame() {
    const int w = 64;
    const int h = 48;
    const std::vector<uint8_t> buf = make_nv12(w, h);
    const Frame f = nv12_frame(buf, w, h);
    Roi roi;
    roi.x = 0.9F;
    roi.y = 0.9F;
    roi.w = 0.5F; // would run past the right/bottom edge
    roi.h = 0.5F;
    std::vector<uint8_t> scratch;
    Frame out{};
    Roi used{};
    CHECK(crop_nv12(f, roi, scratch, out, used));
    CHECK(out.width > 0 && out.height > 0);
    CHECK(used.x + used.w <= 1.0001F);
    CHECK(used.y + used.h <= 1.0001F);
    return 0;
}

int test_crop_rejects_degenerate_and_non_nv12() {
    const int w = 64;
    const int h = 48;
    const std::vector<uint8_t> buf = make_nv12(w, h);
    Frame f = nv12_frame(buf, w, h);
    std::vector<uint8_t> scratch;
    Frame out{};
    Roi used{};
    Roi tiny;
    tiny.w = 0.001F; // < 2 px
    tiny.h = 0.001F;
    CHECK(!crop_nv12(f, tiny, scratch, out, used));
    Roi ok;
    ok.w = 0.5F;
    ok.h = 0.5F;
    f.fourcc = 0x56595559; // YUYV: the crop only knows NV12
    CHECK(!crop_nv12(f, ok, scratch, out, used));
    f.fourcc = FOURCC_NV12;
    f.data = nullptr;
    CHECK(!crop_nv12(f, ok, scratch, out, used));
    return 0;
}

int test_crop_honours_padded_stride() {
    // A driver may hand back rows padded to an alignment; the crop must step by
    // stride, not width, or the image shears.
    const int w = 62;
    const int h = 16;
    const size_t stride = 64;
    std::vector<uint8_t> buf(stride * static_cast<size_t>(h) * 3U / 2U, 0xEE);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            buf.at((static_cast<size_t>(y) * stride) + static_cast<size_t>(x)) =
                static_cast<uint8_t>((x + (y * 7)) & 0xFF);
        }
    }
    Frame f{};
    f.width = w;
    f.height = h;
    f.data = buf.data();
    f.stride = stride;
    f.fourcc = FOURCC_NV12;
    Roi roi;
    roi.x = 8.F / 62.F;
    roi.y = 4.F / 16.F;
    roi.w = 16.F / 62.F;
    roi.h = 8.F / 16.F;
    std::vector<uint8_t> scratch;
    Frame out{};
    Roi used{};
    CHECK(crop_nv12(f, roi, scratch, out, used));
    CHECK(out.data[0] == static_cast<uint8_t>((8 + (4 * 7)) & 0xFF));
    // Second row of the crop comes from source row 5, not from padding.
    CHECK(out.data[static_cast<size_t>(out.stride)] ==
          static_cast<uint8_t>((8 + (5 * 7)) & 0xFF));
    return 0;
}

// ------------------------------------------------ S-11 channel allocation --

using Ch = SearchScheduler::Channel;

SearchScheduler::Params dual_params() {
    SearchScheduler::Params p = test_params();
    p.dual_eo = true;
    return p;
}

int test_single_camera_every_slot_is_wide() {
    // dual_eo off (the current single-camera build): allocation must be
    // invisible — every slot WIDE, in every mode.
    SearchScheduler s(test_params());
    for (int i = 0; i < 8; ++i) {
        CHECK(s.channel() == Ch::WIDE);
        s.update(miss());
    }
    for (int i = 0; i < 10; ++i) {
        CHECK(s.channel() == Ch::WIDE);
        s.update(hit());
    }
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    CHECK(s.channel() == Ch::WIDE);
    return 0;
}

// A hit carrying an estimated range, for the R-10 close-range boost gate.
TrackCue hit_range(float range_m) {
    TrackCue c = hit();
    c.range_m = range_m;
    return c;
}

// R-10 close-range boost: inside narrow_boost_range_m the wide recheck cadence
// stretches, so the narrow channel keeps far more consecutive slots. Far away
// (or range unknown) the normal cadence holds.
int test_dual_close_range_boosts_narrow() {
    SearchScheduler::Params p = dual_params();
    p.recheck_period = 4;        // frequent wide recheck when far
    p.recheck_period_near = 100; // ~never inside the boost range
    p.narrow_boost_range_m = 150.F;
    SearchScheduler s(p);
    for (int i = 0; i < 5; ++i) {
        s.update(hit_range(50.F)); // reach TRACK, already close
    }
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    // Close: over a long run the wide recheck is stretched away, so almost
    // every TRACK slot is narrow.
    int wide = 0;
    int narrow = 0;
    for (int i = 0; i < 40; ++i) {
        (s.channel() == Ch::WIDE ? wide : narrow)++;
        s.update(hit_range(50.F));
    }
    CHECK(narrow >= 39); // at most the frame-0-relative recheck, if any
    CHECK(wide <= 1);
    return 0;
}

// Wide (recheck) slots over `n` TRACK frames at the given range.
int track_wide_count(float range_m, int n) {
    SearchScheduler::Params p = dual_params();
    p.recheck_period = 4;        // frequent wide recheck when far
    p.recheck_period_near = 100; // ~never inside the boost range
    p.narrow_boost_range_m = 150.F;
    SearchScheduler s(p);
    for (int i = 0; i < 5; ++i) {
        s.update(hit_range(range_m)); // reach TRACK
    }
    int wide = 0;
    for (int i = 0; i < n; ++i) {
        if (s.channel() == Ch::WIDE) {
            ++wide;
        }
        s.update(hit_range(range_m));
    }
    return wide;
}

int test_dual_far_range_keeps_normal_recheck() {
    // Far: the period-4 wide recheck runs repeatedly over 12 frames — the
    // opposite of the close-range case, where it is stretched away.
    CHECK(track_wide_count(500.F, 12) >= 2);
    return 0;
}

int test_dual_unknown_range_no_boost() {
    // range_m == 0 (no valid estimate yet) must behave like far, not boosted.
    CHECK(track_wide_count(0.F, 12) == track_wide_count(500.F, 12));
    return 0;
}

int test_dual_search_alternates_wide_narrow() {
    SearchScheduler s(dual_params());
    for (int i = 0; i < 12; ++i) {
        CHECK(s.channel() == ((i % 2 == 0) ? Ch::WIDE : Ch::NARROW));
        s.update(miss());
    }
    return 0;
}

int test_dual_wide_dwell_counts_wide_slots_not_frames() {
    // wide_dwell=2 must mean 2 WIDE passes. With alternation that is 4 frames;
    // if dwell counted frames, the fallback would fire after 2 (i.e. after only
    // one actual wide pass).
    SearchScheduler s(dual_params());
    s.update(miss()); // frame 0: wide pass 1
    s.update(miss()); // frame 1: narrow slot
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_WIDE);
    s.update(miss()); // frame 2: wide pass 2 -> falls back to tiles
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_TILE);
    return 0;
}

int test_dual_narrow_runs_its_own_tile_sweep() {
    // The narrow channel sweeps its own 3x3 tiles on its slots, advancing only
    // on narrow slots and surviving the wide machine's WIDE<->TILE oscillation
    // (a reset there would keep re-visiting the first tiles forever).
    SearchScheduler s(dual_params());
    std::vector<int> narrow_tiles;
    for (int i = 0; i < 2 * 9; ++i) {
        if (s.channel() == Ch::NARROW) {
            const Roi r = s.roi();
            CHECK(!r.is_full()); // narrow search slot is always a tile
            // Recover the tile index from the ROI grid position.
            const int col = static_cast<int>(std::lround(r.x * 3.F));
            const int row = static_cast<int>(std::lround(r.y * 3.F));
            narrow_tiles.push_back((row * 3) + col);
        } else if (s.mode() == SearchScheduler::Mode::SEARCH_WIDE) {
            CHECK(s.roi().is_full()); // wide slot in wide mode: full pass
        }
        s.update(miss());
    }
    // 9 narrow slots in 18 frames: the sweep must have covered all 9 tiles.
    CHECK(narrow_tiles.size() == 9);
    for (int t = 0; t < 9; ++t) {
        bool seen = false;
        for (const int v : narrow_tiles) {
            seen = seen || (v == t);
        }
        CHECK(seen);
    }
    return 0;
}

int test_dual_track_gives_narrow_every_slot_with_wide_recheck() {
    SearchScheduler::Params p = dual_params();
    p.recheck_period = 5;
    SearchScheduler s(p);
    while (s.mode() != SearchScheduler::Mode::TRACK) {
        s.update(hit());
    }
    int wide_slots = 0;
    for (int i = 0; i < 12; ++i) {
        if (s.channel() == Ch::WIDE) {
            ++wide_slots;
            CHECK(s.roi().is_full()); // the recheck IS the wide slot
        } else {
            CHECK(!s.roi().is_full()); // narrow slot inspects the target ROI
        }
        s.update(hit());
    }
    CHECK(wide_slots == 2); // frames-in-mode 5 and 10 within 12 frames
    return 0;
}

int test_dual_confirm_alternates_on_target_region() {
    SearchScheduler s(dual_params());
    s.update(hit()); // SEARCH -> CONFIRM
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    bool saw_wide = false;
    bool saw_narrow = false;
    for (int i = 0; i < 2; ++i) {
        saw_wide = saw_wide || (s.channel() == Ch::WIDE);
        saw_narrow = saw_narrow || (s.channel() == Ch::NARROW);
        CHECK(!s.roi().is_full()); // both channels inspect the target region
        s.update(hit());
    }
    CHECK(saw_wide && saw_narrow);
    return 0;
}

} // namespace

// ---- R-11: cross-channel confirmation (S-15) ----
// The two channels have independent optics, resolution and noise, so hits from
// BOTH are stronger evidence than the same count from one. That lowers the bar;
// it must never raise it.

int test_dual_channel_agreement_confirms_sooner() {
    SearchScheduler::Params p = dual_params();
    p.confirm_window = 5;
    p.confirm_hits = 4;
    p.dual_confirm_relax = 2; // agreement -> need 2 instead of 4
    SearchScheduler s(p);

    s.update(hit()); // SEARCH_WIDE sees it -> CONFIRM (slot 0 was WIDE)
    CHECK(s.mode_name() == std::string("CONFIRM"));
    CHECK(!s.dual_confirmed());

    // Slot 1 is the NARROW slot in dual-EO: a hit that actually came from the
    // narrow image is the second channel.
    CHECK(s.channel() == Ch::NARROW);
    TrackCue n = hit();
    n.hit_from_narrow = true;
    s.update(n);
    CHECK(s.dual_confirmed());

    // One more wide hit reaches 2 in-window hits, which the relaxed criterion
    // accepts — the single-channel rule would still be waiting for 4.
    s.update(hit());
    CHECK(s.confirmed());
    return 0;
}

int test_single_channel_still_needs_the_full_window() {
    // Same scheduler, but every hit comes from the WIDE slots only: the bar
    // must stay at confirm_hits. This is the half that must NOT change —
    // trading misses for false positives would defeat the purpose.
    SearchScheduler::Params p = dual_params();
    p.confirm_window = 5;
    p.confirm_hits = 4;
    p.dual_confirm_relax = 2;
    SearchScheduler s(p);

    s.update(hit()); // -> CONFIRM
    int wide_hits = 1;
    for (int i = 0; i < 3 && !s.confirmed(); ++i) {
        // Feed a hit on wide slots and a MISS on narrow ones, so only one
        // channel ever contributes evidence.
        if (s.channel() == Ch::NARROW) {
            TrackCue c = miss();
            c.alive = true; // tracker still holds it; this frame just had no hit
            s.update(c);
        } else {
            s.update(hit());
            ++wide_hits;
        }
    }
    CHECK(!s.dual_confirmed());
    // With 2 wide hits inside a 5-window and no agreement, 4 are still required.
    CHECK(!s.confirmed() || wide_hits >= p.confirm_hits);
    return 0;
}

int test_identity_change_restarts_channel_evidence() {
    // TR-7: the window belongs to an identity. A new primary must not inherit
    // the previous target's cross-channel evidence.
    const SearchScheduler::Params p = dual_params();
    SearchScheduler s(p);
    TrackCue a = hit();
    a.track_id = 7;
    s.update(a); // -> CONFIRM on track 7 (wide slot)
    TrackCue b = hit();
    b.track_id = 7;
    b.hit_from_narrow = true;
    s.update(b); // narrow-sourced hit -> agreement for track 7
    CHECK(s.dual_confirmed());

    TrackCue other = hit();
    other.track_id = 9; // primary replaced
    s.update(other);
    CHECK(!s.dual_confirmed());
    return 0;
}

int test_relaxation_never_confirms_on_one_frame() {
    // Floor of 2: one detection is a glint. Even with a huge relax value the
    // scheduler must not confirm off a single hit.
    SearchScheduler::Params p = dual_params();
    p.confirm_hits = 4;
    p.dual_confirm_relax = 99;
    SearchScheduler s(p);
    s.update(hit()); // -> CONFIRM, one wide hit so far
    CHECK(!s.confirmed());
    return 0;
}

// The slot's ALLOCATION is not the evidence. A narrow slot falls back to the
// wide frame when the narrow camera missed its grab or the cue is outside the
// narrow field of view; a hit from that frame is wide evidence. Attributing it
// to the narrow channel would let the relaxed threshold fire on one channel —
// which is exactly the false-positive risk R-11 is supposed to reduce.
int test_fallback_slot_does_not_count_as_narrow_evidence() {
    SearchScheduler::Params p = dual_params();
    p.confirm_window = 5;
    p.confirm_hits = 4;
    p.dual_confirm_relax = 2;
    SearchScheduler s(p);

    s.update(hit());                  // wide slot discovers it -> CONFIRM
    CHECK(s.channel() == Ch::NARROW); // this slot is ALLOCATED to narrow...
    TrackCue fell_back = hit();
    fell_back.hit_from_narrow = false; // ...but the frame inferred on was wide
    s.update(fell_back);
    CHECK(!s.dual_confirmed()); // no agreement: both hits came from one channel
    CHECK(!s.confirmed());      // and the bar is therefore not relaxed
    return 0;
}

// The same rule at the TRACK identity handoff — the one attribution site the
// original fix missed. In dual-EO TRACK nearly every slot is ALLOCATED narrow,
// so a new primary promoted on a slot whose inference fell back to the wide
// frame would latch narrow evidence from a wide detection, and the very next
// wide hit would relax the bar on single-channel evidence.
int test_track_handoff_attributes_evidence_to_inferred_channel() {
    SearchScheduler s(dual_params());
    for (int i = 0; i < 5; ++i) {
        s.update(hit_id(1)); // enter CONFIRM, then 4 hits -> TRACK
    }
    CHECK(s.mode() == SearchScheduler::Mode::TRACK);
    CHECK(s.channel() == Ch::NARROW); // TRACK slot is allocated to narrow...
    TrackCue promoted = hit_id(2);    // ...primary handoff on this slot,
    promoted.hit_from_narrow = false; // ...but the frame inferred on was wide
    s.update(promoted);
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    CHECK(!s.dual_confirmed()); // allocation must not fabricate narrow evidence
    s.update(hit_id(2));        // a later wide hit
    CHECK(!s.dual_confirmed()); // still one channel only: no relaxation
    return 0;
}

// Re-entering CONFIRM with a track that is merely ALIVE (coasting after an
// abandoned window, within the tracker's miss budget) must not latch channel
// evidence: a frame with no detection saw nothing, and hit_from_narrow on a
// hit-less cue only describes the slot, not a detection.
int test_reentry_alive_without_hit_latches_no_evidence() {
    SearchScheduler s(dual_params()); // window 5, hits 4
    s.update(hit_id(3));              // discovery -> CONFIRM (wide evidence)
    TrackCue alive = hit_id(3);
    alive.hit = false; // coasting: alive, no detection this frame
    s.update(alive);
    s.update(alive); // two misses: criterion unreachable -> abandon
    CHECK(s.mode() == SearchScheduler::Mode::SEARCH_WIDE);
    TrackCue reentry = hit_id(3);
    reentry.hit = false;
    reentry.hit_from_narrow = true; // slot-derived; there was NO detection
    s.update(reentry);              // coasting track re-enters CONFIRM
    CHECK(s.mode() == SearchScheduler::Mode::CONFIRM);
    CHECK(!s.dual_confirmed()); // no hit, no evidence
    s.update(hit_id(3));        // one real wide hit
    CHECK(!s.dual_confirmed()); // still single-channel: the bar stays up
    return 0;
}

// ---- R-11 reference-model sweep (property test over every state path) ------
//
// The safety property behind R-11 is not "these three sites attribute
// correctly" but "`dual_confirmed()` is true EXACTLY when the identity being
// confirmed has a real detection from EACH channel". Two site bugs (the TRACK
// identity handoff attributing by slot allocation, search re-entry latching on
// a hit-less cue) each satisfied the per-site tests of their day and still
// broke that property, so it is pinned against an independent reference model
// derived from the requirement rather than from the implementation:
//
//   evidence is created ONLY by a real detection (`cue.hit`), attributed to the
//   channel the frame was actually INFERRED on (`cue.hit_from_narrow`, not the
//   slot allocation), belongs to the identity being confirmed, and is cleared
//   whenever the search restarts or the identity changes. It is frozen on entry
//   to TRACK — `dual_confirmed` records how the target WAS confirmed, so the
//   published flag does not drift afterwards.
//
// The cue sequence mirrors what the seeker's main loop feeds the scheduler:
// the slot comes from `channel()` read before the update, and a narrow slot
// falls back to the wide frame when the narrow grab misses or the cue leaves
// the narrow FOV — which is exactly how slot and evidence-channel diverge.
//
// G16.6 deviation: one walk drives every transition, and splitting it would
// break the sequence the property is stated over (G16.6)
// NOLINTNEXTLINE(readability-function-size)
int test_r11_matches_reference_model() {
    SearchScheduler::Params p = dual_params();
    p.confirm_window = 5;
    p.confirm_hits = 4;
    p.dual_confirm_relax = 2;
    SearchScheduler s(p);
    // Deterministic LCG: a fixed pseudo-random walk, so a failure reproduces
    // exactly and the sequence is long enough to reach every transition.
    uint32_t rng = 0x5EEDU;
    auto next = [&rng]() {
        rng = (rng * 1103515245U) + 12345U;
        return (rng >> 16U) & 0x7FFFU;
    };
    // Reference evidence for the identity currently under confirmation.
    bool ref_wide = false;
    bool ref_narrow = false;
    uint32_t ref_id = 0;
    // Identity is held for a RUN of frames: with the identity changing every
    // frame the confirmation window restarts constantly and TRACK is never
    // reached — the coverage assertion below exists because an earlier version
    // of this sweep did exactly that and passed vacuously.
    // The walk alternates PRESENT and LOST phases that each last many frames:
    // a target that reappears the frame after it was dropped never lets the
    // wide dwell expire, so the tile sweep — one of the paths evidence is
    // cleared on — would never be exercised.
    uint32_t id = 1;
    bool present = true;
    int run_left = 14;
    bool seen_mode[4] = {false, false, false, false};
    for (int i = 0; i < 8000; ++i) {
        const uint32_t r = next();
        // What main.cpp does: read the allocated channel BEFORE the update.
        const bool narrow_slot = s.channel() == Ch::NARROW;
        // The narrow frame is not always usable (grab miss / outside its FOV):
        // the slot then falls back to the wide frame, so the evidence channel
        // is wide even though the ALLOCATION was narrow.
        const bool inferred_narrow = narrow_slot && ((r % 8U) != 0U);
        TrackCue c;
        c.alive = present && ((r % 29U) != 0U);    // phase + rare single-frame drop
        c.hit = c.alive && ((r / 16U) % 6U) != 0U; // both channels do detect
        c.track_id = id;
        c.cx = 0.5F;
        c.cy = 0.5F;
        c.size = 0.02F;
        c.hit_from_narrow = inferred_narrow; // the frame actually inferred on
        const SearchScheduler::Mode processed_in = s.mode(); // mode that handles it
        s.update(c);
        const SearchScheduler::Mode mode = s.mode();
        seen_mode[static_cast<int>(mode)] = true;

        // Advance the reference model. It keys off the mode the frame was
        // PROCESSED in (the state machine's own dispatch), because the frame
        // that completes confirmation is handled by the CONFIRM rules even
        // though the scheduler is already in TRACK when it returns.
        auto clear = [&]() {
            ref_wide = false;
            ref_narrow = false;
            ref_id = 0;
        };
        auto latch = [&]() {
            if (c.hit) { // ONLY a real detection is evidence
                (inferred_narrow ? ref_narrow : ref_wide) = true;
            }
        };
        if (processed_in == SearchScheduler::Mode::SEARCH_WIDE ||
            processed_in == SearchScheduler::Mode::SEARCH_TILE) {
            clear(); // searching: no candidate yet
            if (c.alive) {
                ref_id = c.track_id; // discovery: this identity starts its window
                latch();
            }
        } else if (processed_in == SearchScheduler::Mode::CONFIRM) {
            if (!c.alive) {
                clear(); // tracker dropped it: back to searching
            } else {
                if (c.track_id != ref_id) { // a new identity earns its own evidence
                    clear();
                    ref_id = c.track_id;
                }
                latch();
                if (mode == SearchScheduler::Mode::SEARCH_WIDE ||
                    mode == SearchScheduler::Mode::SEARCH_TILE) {
                    clear(); // window became unreachable: candidate abandoned
                }
            }
        } else { // TRACK
            if (!c.alive) {
                clear();
            } else if (c.track_id != ref_id) { // identity handoff -> reconfirm
                clear();
                ref_id = c.track_id;
                latch();
            }
            // Same identity: evidence is frozen at the confirmation that made it.
        }

        CHECK(s.dual_confirmed() == (ref_wide && ref_narrow));
        const int expect_need = (ref_wide && ref_narrow)
                                    ? p.confirm_hits - p.dual_confirm_relax
                                    : p.confirm_hits;
        CHECK(s.confirm_hits_required() == expect_need);
        if (--run_left <= 0) {
            const uint32_t q = next();
            if (present && (q % 3U) != 0U) {
                id = 1U + (id % 3U); // handoff with no loss: from CONFIRM and TRACK
            } else {
                present = !present; // lost <-> reacquired (drives the tile sweep)
                if (present) {
                    id = 1U + (id % 3U);
                }
            }
            run_left = 5 + static_cast<int>((q / 128U) % 20U);
        }
    }
    // Coverage: a sweep that never reaches TRACK (or never falls back to the
    // tile sweep) proves nothing about the paths the defects lived on.
    for (const bool seen : seen_mode) {
        CHECK(seen);
    }
    return 0;
}

// The mirror of the sweep: genuine two-channel agreement MUST still relax the
// bar — a fix that simply never latched evidence would pass the sweeps above.
int test_r11_genuine_agreement_still_relaxes() {
    SearchScheduler::Params p = dual_params();
    p.confirm_window = 5;
    p.confirm_hits = 4;
    p.dual_confirm_relax = 2;
    SearchScheduler s(p);
    const TrackCue wide = hit_id(1);
    s.update(wide); // discovery on the wide channel -> CONFIRM
    CHECK(s.confirm_hits_required() == 4);
    TrackCue narrow = hit_id(1);
    narrow.hit_from_narrow = true; // the narrow channel sees the SAME identity
    s.update(narrow);
    CHECK(s.dual_confirmed());
    CHECK(s.confirm_hits_required() == 2); // relaxed, floor respected
    return 0;
}

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_starts_wide();
    rc = rc != 0 ? rc : test_falls_back_to_tiles_after_wide_dwell();
    rc = rc != 0 ? rc : test_tile_sweep_covers_every_cell_then_retries_wide();
    rc = rc != 0 ? rc : test_detection_during_tile_sweep_enters_confirm();
    rc = rc != 0 ? rc : test_four_of_five_confirms();
    rc = rc != 0 ? rc : test_one_miss_still_confirms();
    rc = rc != 0 ? rc : test_two_misses_abandons_confirmation();
    rc = rc != 0 ? rc : test_tracker_dropping_target_resets_to_search();
    rc = rc != 0 ? rc : test_identity_change_restarts_confirmation();
    rc = rc != 0 ? rc : test_track_handoff_reconfirms_new_identity();
    rc = rc != 0 ? rc : test_not_confirmed_until_track_mode();
    rc = rc != 0 ? rc : test_track_roi_is_centred_and_pixel_square();
    rc = rc != 0 ? rc : test_track_roi_stays_inside_frame_at_the_edge();
    rc = rc != 0 ? rc : test_track_survives_short_coast();
    rc = rc != 0 ? rc : test_periodic_wide_recheck_while_tracking();
    rc = rc != 0 ? rc : test_recheck_disabled_by_zero_period();
    rc = rc != 0 ? rc : test_overlapping_tiles_cover_every_seam();
    rc = rc != 0 ? rc : test_zero_overlap_is_the_original_grid();
    rc = rc != 0 ? rc : test_tiles_stay_inside_the_frame();
    rc = rc != 0 ? rc : test_cue_window_grows_with_consecutive_misses();
    rc = rc != 0 ? rc : test_cue_window_expansion_is_capped();
    rc = rc != 0 ? rc : test_a_hit_resets_the_expansion();
    rc = rc != 0 ? rc : test_growth_one_disables_expansion();
    rc = rc != 0 ? rc : test_remap_centre_of_tile();
    rc = rc != 0 ? rc : test_remap_is_identity_on_full_frame();
    rc = rc != 0 ? rc : test_remap_corners();
    rc = rc != 0 ? rc : test_crop_dimensions_and_content();
    rc = rc != 0 ? rc : test_crop_snaps_to_even_bounds();
    rc = rc != 0 ? rc : test_crop_clamps_to_frame();
    rc = rc != 0 ? rc : test_crop_rejects_degenerate_and_non_nv12();
    rc = rc != 0 ? rc : test_crop_honours_padded_stride();
    rc = rc != 0 ? rc : test_single_camera_every_slot_is_wide();
    rc = rc != 0 ? rc : test_dual_channel_agreement_confirms_sooner();
    rc = rc != 0 ? rc : test_single_channel_still_needs_the_full_window();
    rc = rc != 0 ? rc : test_identity_change_restarts_channel_evidence();
    rc = rc != 0 ? rc : test_relaxation_never_confirms_on_one_frame();
    rc = rc != 0 ? rc : test_track_handoff_attributes_evidence_to_inferred_channel();
    rc = rc != 0 ? rc : test_reentry_alive_without_hit_latches_no_evidence();
    rc = rc != 0 ? rc : test_fallback_slot_does_not_count_as_narrow_evidence();
    rc = rc != 0 ? rc : test_r11_matches_reference_model();
    rc = rc != 0 ? rc : test_r11_genuine_agreement_still_relaxes();
    rc = rc != 0 ? rc : test_dual_search_alternates_wide_narrow();
    rc = rc != 0 ? rc : test_dual_wide_dwell_counts_wide_slots_not_frames();
    rc = rc != 0 ? rc : test_dual_narrow_runs_its_own_tile_sweep();
    rc = rc != 0 ? rc : test_dual_track_gives_narrow_every_slot_with_wide_recheck();
    rc = rc != 0 ? rc : test_dual_close_range_boosts_narrow();
    rc = rc != 0 ? rc : test_dual_far_range_keeps_normal_recheck();
    rc = rc != 0 ? rc : test_dual_unknown_range_no_boost();
    rc = rc != 0 ? rc : test_dual_confirm_alternates_on_target_region();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_search: %d checks passed\n", checks);
    return 0;
}
