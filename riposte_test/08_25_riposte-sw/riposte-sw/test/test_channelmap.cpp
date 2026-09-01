// Wide<->narrow EO channel mapping tests (DUALEO-REQ §3.6 / P4).
//
// Pure geometry: the nested-coaxial dual-EO handoff maps a bearing between the
// two normalizations (wide 59 deg, narrow 15.4 deg) plus a calibrated mounting
// offset. The load-bearing properties: centre maps to centre, the narrow FOV is
// a small window so wide-edge targets fall OUTSIDE narrow, the mapping round-
// trips, and the calibration offset shifts the mapping the right way.
#include "ChannelMap.h"
#include "IDetector.h" // Roi, Detection

#include <cmath>
#include <cstdint>
#include <cstdio>
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

ChannelMap::Params params() {
    ChannelMap::Params p;
    p.wide_hfov_rad = 1.03F;
    p.narrow_hfov_rad = 0.269F;
    p.aspect = 16.F / 9.F;
    p.offset_az_rad = 0.F;
    p.offset_el_rad = 0.F;
    return p;
}

int test_centre_maps_to_centre() {
    const ChannelMap cm(params());
    const auto m = cm.wide_to_narrow(0.5F, 0.5F);
    CHECK(near(m.cx, 0.5F) && near(m.cy, 0.5F));
    CHECK(m.in_fov);
    return 0;
}

int test_narrow_fov_is_a_small_window_of_wide() {
    // A target a quarter-frame off wide centre in x is at bearing
    // 0.25*59deg ~ 14.75 deg — nearly the whole narrow half-FOV (7.7 deg), so it
    // maps well outside the narrow frame.
    const ChannelMap cm(params());
    const auto m = cm.wide_to_narrow(0.75F, 0.5F);
    CHECK(!m.in_fov);   // outside the narrow frame
    CHECK(m.cx > 1.0F); // mapped beyond the right edge
    return 0;
}

int test_small_wide_offset_stays_in_narrow() {
    // A target near wide centre (bearing within +-7.7 deg) IS inside narrow.
    // 0.5 + 0.05 of the wide frame = 0.05*59 = 2.95 deg < 7.7 deg half-FOV.
    const ChannelMap cm(params());
    const auto m = cm.wide_to_narrow(0.55F, 0.5F);
    CHECK(m.in_fov);
    // The same bearing spans a larger fraction of the narrower frame.
    const float wide_az = 0.05F * 1.03F;
    CHECK(near(m.cx, 0.5F + (wide_az / 0.269F), 1e-4F));
    CHECK(m.cx > 0.55F); // occupies more of the narrow frame than the wide one
    return 0;
}

int test_round_trip_is_identity() {
    const ChannelMap cm(params());
    const float pts[][2] = {{0.5F, 0.5F}, {0.52F, 0.48F}, {0.5F, 0.55F}, {0.47F, 0.51F}};
    for (const auto& pt : pts) {
        const auto n = cm.wide_to_narrow(pt[0], pt[1]);
        const auto back = cm.narrow_to_wide(n.cx, n.cy);
        CHECK(near(back.cx, pt[0], 1e-4F));
        CHECK(near(back.cy, pt[1], 1e-4F));
    }
    return 0;
}

int test_narrow_to_wide_shrinks_into_wide_frame() {
    // A target at the narrow frame edge (bearing +-7.7 deg) is only ~0.13 off
    // wide centre — a narrow detection remapped for the wide-frame tracker sits
    // comfortably inside the wide frame.
    const ChannelMap cm(params());
    const auto m = cm.narrow_to_wide(1.0F, 0.5F); // narrow right edge
    CHECK(m.in_fov);
    const float narrow_az = 0.5F * 0.269F; // half narrow HFOV
    CHECK(near(m.cx, 0.5F + (narrow_az / 1.03F), 1e-4F));
    CHECK(m.cx < 0.65F); // well inside the wide frame
    return 0;
}

int test_mounting_offset_shifts_the_mapping() {
    // A positive az offset means the narrow axis points slightly right of wide,
    // so a wide-centre target appears LEFT of narrow centre.
    ChannelMap::Params p = params();
    p.offset_az_rad = 0.02F; // ~1.1 deg
    const ChannelMap cm(p);
    const auto m = cm.wide_to_narrow(0.5F, 0.5F);
    CHECK(m.cx < 0.5F); // shifted left by the offset
    CHECK(near(m.cx, 0.5F - (0.02F / 0.269F), 1e-4F));
    // Round trip still holds with a nonzero offset.
    const auto back = cm.narrow_to_wide(m.cx, m.cy);
    CHECK(near(back.cx, 0.5F, 1e-4F));
    return 0;
}

int test_vertical_axis_uses_vfov() {
    // el uses vfov = hfov/aspect, so a vertical wide offset maps through the
    // narrow vfov, not hfov.
    const ChannelMap cm(params());
    const auto m = cm.wide_to_narrow(0.5F, 0.53F);
    const float wide_vfov = 1.03F / (16.F / 9.F);
    const float narrow_vfov = 0.269F / (16.F / 9.F);
    const float el = 0.03F * wide_vfov;
    CHECK(near(m.cy, 0.5F + (el / narrow_vfov), 1e-4F));
    CHECK(near(m.cx, 0.5F, 1e-4F)); // no horizontal coupling
    return 0;
}

// Size must cross channels with the centre (S-13). A narrow detection remapped
// centre-only would hand TargetEstimator a box ~4x too large, and range =
// real_size * focal / bbox_px would collapse the moment a slot switched
// channel — a range step with no physical cause.
int test_size_scales_with_the_fov_ratio() {
    const ChannelMap m{ChannelMap::Params{}};
    const ChannelMap::Params p{};
    const float ratio = p.narrow_hfov_rad / p.wide_hfov_rad; // ~0.26

    // The same target: bigger in the narrow frame, by exactly the FOV ratio.
    const float narrow_size = 0.40F;
    const float wide_size = m.narrow_to_wide_size(narrow_size);
    CHECK(wide_size < narrow_size);
    CHECK(std::fabs(wide_size - (narrow_size * ratio)) < 1e-6F);

    // Round trip is identity, so a handoff in either direction is lossless.
    CHECK(std::fabs(m.wide_to_narrow_size(wide_size) - narrow_size) < 1e-5F);

    // Angular size is the invariant: size * hfov must match across channels.
    CHECK(std::fabs((narrow_size * p.narrow_hfov_rad) - (wide_size * p.wide_hfov_rad)) <
          1e-6F);
    return 0;
}

// The ROI handoff (S-13): a cue window lives in WIDE coordinates, so a narrow
// slot must move it before cropping — and must be told when the target is not
// on the narrow channel at all.
int test_cue_window_maps_into_the_narrow_frame() {
    const ChannelMap m{ChannelMap::Params{}};
    Roi out{};

    // Centred window: lands centred, and grows because the narrow frame covers
    // less sky per normalized unit.
    Roi in;
    in.x = 0.45F;
    in.y = 0.45F;
    in.w = 0.10F;
    in.h = 0.10F;
    CHECK(m.wide_roi_to_narrow(in, out));
    CHECK(std::fabs((out.x + (out.w * 0.5F)) - 0.5F) < 1e-3F);
    CHECK(out.w > in.w);
    CHECK(out.x >= 0.F && out.y >= 0.F);
    CHECK(out.x + out.w <= 1.0001F);

    // Off-axis target: outside the narrow field of view entirely, so the caller
    // is told to stay on the wide frame instead of cropping somewhere wrong.
    in.x = 0.90F;
    in.y = 0.45F;
    CHECK(!m.wide_roi_to_narrow(in, out));
    return 0;
}

// Detections come back in narrow coordinates and must reach the tracker in wide
// ones, size included.
int test_detection_remap_moves_centre_and_size() {
    const ChannelMap m{ChannelMap::Params{}};
    const ChannelMap::Params p{};
    std::vector<Detection> dets;
    Detection d{};
    d.cx = 0.5F;
    d.cy = 0.5F;
    d.w = 0.40F;
    d.h = 0.40F;
    d.score = 0.9F;
    d.cls = 0;
    dets.push_back(d);
    // A second detection at the narrow frame edge maps to a wide position that
    // is still inside the wide frame (the narrow FOV is a subset), so both live.
    Detection edge = d;
    edge.cx = 0.95F;
    dets.push_back(edge);

    m.remap_detections_to_wide(dets);
    CHECK(dets.size() == 2);
    // Centre stays centre; size shrinks by exactly the FOV ratio.
    CHECK(std::fabs(dets[0].cx - 0.5F) < 1e-6F);
    CHECK(std::fabs(dets[0].w - (0.40F * (p.narrow_hfov_rad / p.wide_hfov_rad))) < 1e-6F);
    CHECK(dets[0].h < 0.40F);
    // The edge detection moved toward the wide centre, not past it.
    CHECK(dets[1].cx > 0.5F && dets[1].cx < 0.95F);
    return 0;
}

// The paired overload keeps a per-detection parallel array (the T1 embeddings,
// computed on the PRE-remap boxes) index-aligned through FOV drops — otherwise
// surviving detections pair with the wrong appearance vectors downstream (TR-B).
int test_detection_remap_keeps_aligned_array_in_step() {
    ChannelMap::Params p{};
    p.offset_az_rad = 0.45F; // extreme misalignment: the narrow right edge maps
                             // outside the wide frame and is dropped
    const ChannelMap m{p};
    Detection keep{};
    keep.cx = 0.2F;
    keep.cy = 0.5F;
    keep.w = 0.1F;
    keep.h = 0.1F;
    keep.score = 0.7F;
    Detection drop = keep;
    drop.cx = 1.0F;
    drop.score = 0.9F;
    std::vector<Detection> dets{drop, keep};
    std::vector<int> aligned{90, 20}; // stand-in for per-detection embeddings
    m.remap_detections_to_wide(dets, aligned);
    CHECK(dets.size() == 1);
    CHECK(aligned.size() == 1);
    CHECK(aligned[0] == 20); // the survivor kept ITS entry, not the dropped one's
    CHECK(std::fabs(dets[0].score - 0.7F) < 1e-6F);

    // A parallel array of the WRONG length was never aligned (e.g. the embedder
    // produced nothing): it is left untouched rather than guessed at.
    std::vector<Detection> dets2{drop, keep};
    std::vector<int> misaligned{1};
    m.remap_detections_to_wide(dets2, misaligned);
    CHECK(dets2.size() == 1);
    CHECK(misaligned.size() == 1 && misaligned[0] == 1);
    return 0;
}

// Cross-channel pairing bound (S-13): a narrow measurement is published under
// the WIDE frame's timestamp, so the two captures must describe nearly the
// same instant — an age-vs-now guard cannot see a narrow buffer that aged in
// its ring while the wide grab stalled.
int test_frames_pairable_bounds_cross_channel_skew() {
    const uint64_t max_skew = 50'000'000ULL;
    CHECK(ChannelMap::frames_pairable(1'000'000'000ULL, 1'000'000'000ULL, max_skew));
    CHECK(ChannelMap::frames_pairable(1'050'000'000ULL, 1'000'000'000ULL, max_skew));
    CHECK(!ChannelMap::frames_pairable(1'050'000'001ULL, 1'000'000'000ULL, max_skew));
    // Symmetric: a narrow frame NEWER than the wide one is bounded identically.
    CHECK(!ChannelMap::frames_pairable(1'000'000'000ULL, 1'050'000'001ULL, max_skew));
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_centre_maps_to_centre();
    rc = rc != 0 ? rc : test_narrow_fov_is_a_small_window_of_wide();
    rc = rc != 0 ? rc : test_small_wide_offset_stays_in_narrow();
    rc = rc != 0 ? rc : test_round_trip_is_identity();
    rc = rc != 0 ? rc : test_narrow_to_wide_shrinks_into_wide_frame();
    rc = rc != 0 ? rc : test_mounting_offset_shifts_the_mapping();
    rc = rc != 0 ? rc : test_vertical_axis_uses_vfov();
    rc = rc != 0 ? rc : test_size_scales_with_the_fov_ratio();
    rc = rc != 0 ? rc : test_cue_window_maps_into_the_narrow_frame();
    rc = rc != 0 ? rc : test_detection_remap_moves_centre_and_size();
    rc = rc != 0 ? rc : test_detection_remap_keeps_aligned_array_in_step();
    rc = rc != 0 ? rc : test_frames_pairable_bounds_cross_channel_skew();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_channelmap: %d checks passed\n", checks);
    return 0;
}
