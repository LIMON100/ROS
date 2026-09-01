// Seeker config range-validation tests (AGENTS §7.9): a bad configuration must
// be rejected at startup with a specific reason, not run into a divide-by-zero
// (grid=0), a bitmask overflow (confirm_window>32), or a silently wrong
// detector (score_thr outside [0,1]).
#include "SeekerConfig.h"

#include <cstdio>
#include <string>

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

int test_defaults_are_valid() {
    SeekerConfig const c; // struct defaults mirror the Tunables/POC config
    std::string err;
    CHECK(validate(c, err));
    CHECK(err.empty());
    return 0;
}

// G16.6 deviation: reject-matrix enumeration; one CHECK per range violation (G16.6)
// NOLINTNEXTLINE(readability-function-size)
int test_rejects_and_reports_each_range() {
    // Each mutation must fail with a NON-empty reason; the default is valid so
    // exactly one field is out of range per case.
    struct Case {
        const char* what{};
        SeekerConfig c;
    };
    auto base = [] { return SeekerConfig{}; };

    // Helper: mutate, validate, expect failure with a message.
    auto expect_bad = [](SeekerConfig c) -> bool {
        std::string err;
        return !validate(c, err) && !err.empty();
    };

    SeekerConfig c;
    c = base();
    c.score_thr = 1.5F;
    CHECK(expect_bad(c));
    c = base();
    c.score_thr = -0.1F;
    CHECK(expect_bad(c));
    c = base();
    c.nms_iou = 2.0F;
    CHECK(expect_bad(c));
    c = base();
    c.model_size = 0;
    CHECK(expect_bad(c)); // divide/degenerate
    c = base();
    c.model_size = 641;
    CHECK(expect_bad(c)); // odd
    c = base();
    c.num_classes = 0;
    CHECK(expect_bad(c));
    c = base();
    c.target_class = 5;
    c.num_classes = 2;
    CHECK(expect_bad(c));
    c = base();
    c.target_class = -1;
    CHECK(expect_bad(c));
    c = base();
    c.grid = 0;
    CHECK(expect_bad(c)); // divide-by-zero
    c = base();
    c.confirm_window = 33;
    CHECK(expect_bad(c)); // mask overflow
    c = base();
    c.confirm_window = 0;
    CHECK(expect_bad(c));
    c = base();
    c.confirm_hits = 11;
    c.confirm_window = 10;
    CHECK(expect_bad(c));
    c = base();
    c.confirm_hits = 0;
    CHECK(expect_bad(c));
    c = base();
    c.recheck_period = -1;
    CHECK(expect_bad(c));
    c = base();
    c.track_roi_scale = 0.F;
    CHECK(expect_bad(c));
    c = base();
    c.track_roi_min = 0.F;
    CHECK(expect_bad(c));
    c = base();
    c.track_roi_min = 1.5F;
    CHECK(expect_bad(c));
    c = base();
    c.width = 1;
    CHECK(expect_bad(c));
    c = base();
    c.height = 0;
    CHECK(expect_bad(c));
    c = base();
    c.record_fps = 0.0;
    CHECK(expect_bad(c));
    c = base();
    c.record_disk_high = 1.0;
    CHECK(expect_bad(c));
    c = base();
    c.record_disk_free = 0.0;
    CHECK(expect_bad(c));
    // free >= high (both individually in (0,1)) must also fail.
    c = base();
    c.record_disk_free = 0.5;
    c.record_disk_high = 0.4;
    CHECK(expect_bad(c));
    return 0;
}

int test_boundary_values_accepted() {
    // The inclusive edges are valid: score/iou at 0 and 1, confirm_window 32,
    // confirm_hits == window, target_class == num_classes-1, roi_min == 1.
    SeekerConfig c;
    c.score_thr = 0.F;
    c.nms_iou = 1.F;
    c.confirm_window = 32;
    c.confirm_hits = 32;
    c.num_classes = 3;
    c.target_class = 2;
    c.track_roi_min = 1.0F;
    // The expansion cap is coupled to the floor: a cap below the nominal window
    // would mean "expansion configured but inert", which is the kind of silent
    // no-op the startup gate exists to surface (R-16).
    c.track_roi_max = 1.0F;
    c.recheck_period = 0; // disabled is allowed
    std::string err;
    CHECK(validate(c, err));
    return 0;
}

// The keys added after review CR-07: each of these was reachable at runtime
// while being absent from the validated struct, so a negative value silently
// disabled the property it feeds rather than refusing startup.
int test_rejects_the_cadence_and_deadline_keys() {
    std::string err;
    {
        SeekerConfig c;
        c.recheck_period_near = -1; // reproduced: seeker reached TRACK with this
        CHECK(!validate(c, err));
        CHECK(err.find("track_recheck_period_near") != std::string::npos);
    }
    {
        SeekerConfig c;
        c.narrow_boost_range_m = 0.F; // boost off: the near cadence is unused
        c.recheck_period_near = -1;
        CHECK(validate(c, err));
    }
    {
        SeekerConfig c;
        c.embed_deadline_ms = -5.0; // TR-4 bound must be positive
        CHECK(!validate(c, err));
        CHECK(err.find("embed_deadline_ms") != std::string::npos);
    }
    {
        SeekerConfig c;
        // A deadline longer than a frame period cannot bound the frame it is
        // supposed to protect.
        c.embed_deadline_ms = 100.0;
        CHECK(!validate(c, err));
    }
    {
        SeekerConfig c;
        c.record_segment_s = -1.0;
        CHECK(!validate(c, err));
    }
    {
        SeekerConfig c;
        c.wide_dwell = 0;
        CHECK(!validate(c, err));
    }
    {
        SeekerConfig c;
        c.grid = 100000; // grid*grid overflows the signed tile count
        CHECK(!validate(c, err));
    }
    return 0;
}

// Dual-EO geometry (P4). A narrow HFOV that is not strictly inside the wide one
// is not a second channel: the ChannelMap ratio inverts and remapped sizes — so
// monocular range — come out wrong in a way that still looks plausible.
int test_dual_eo_requires_a_narrower_second_channel() {
    std::string err;
    {
        SeekerConfig c;
        c.dual_eo = true; // defaults are a valid wide/narrow pair
        CHECK(validate(c, err));
    }
    {
        SeekerConfig c;
        c.dual_eo = true;
        c.narrow_hfov_rad = c.hfov_rad; // not narrower
        CHECK(!validate(c, err));
        CHECK(err.find("narrow_hfov_rad") != std::string::npos);
    }
    {
        SeekerConfig c;
        c.dual_eo = true;
        c.narrow_hfov_rad = -0.1F;
        CHECK(!validate(c, err));
    }
    {
        // Off: the pair is unused, so a nonsense value cannot matter.
        SeekerConfig c;
        c.narrow_hfov_rad = 99.F;
        CHECK(validate(c, err));
    }
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_defaults_are_valid();
    rc = rc != 0 ? rc : test_rejects_and_reports_each_range();
    rc = rc != 0 ? rc : test_boundary_values_accepted();
    rc = rc != 0 ? rc : test_rejects_the_cadence_and_deadline_keys();
    rc = rc != 0 ? rc : test_dual_eo_requires_a_narrower_second_channel();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_seekerconfig: %d checks passed\n", checks);
    return 0;
}
