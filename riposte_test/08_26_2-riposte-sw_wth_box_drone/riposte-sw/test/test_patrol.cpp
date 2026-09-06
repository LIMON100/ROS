// Balloon-trial behaviour tests (RIPOSTE-DUALEO-REQ-001 T-1..T-5):
//   Geofence            — the 50x50 m boundary model: containment, distance,
//                         GPS -> local NED projection, inward direction.
//   SM-10               — the hard limit built on it.
//   BalloonPatrolSource — search / approach / turn-away, driven through the REAL
//                         compute() with TrackState injected into the TrackBus.
//
// The patrol source composes a GuidanceSource, which reads the seeker's shm
// TrackBus, so these tests publish into that bus exactly as the seeker would.
#include "Geofence.h"
#include "SafetyMonitor.h"

#include <math.h>
#include <unistd.h> // getpid, unlink

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "riposte/Clock.h"
#include "riposte/Config.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"
#include "sources/BalloonPatrolSource.h"
#include "sources/MissionSource.h"

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

bool near(float a, float b, float tol = 1e-3F) {
    return std::fabs(a - b) <= tol;
}

// ---------------------------------------------------------------- Geofence --

int test_square_containment() {
    Geofence f;
    f.set_square(50.F, 0.F, 0.F); // 50 x 50 m centred on the origin (T-1 default)
    CHECK(f.valid());
    CHECK(f.vertex_count() == 4);
    CHECK(f.contains(0.F, 0.F));
    CHECK(f.contains(24.F, 24.F));
    CHECK(f.contains(-24.F, -24.F));
    CHECK(!f.contains(26.F, 0.F));
    CHECK(!f.contains(0.F, 26.F));
    CHECK(!f.contains(-30.F, -30.F));
    return 0;
}

int test_square_distance_sign_and_value() {
    Geofence f;
    f.set_square(50.F, 0.F, 0.F);
    CHECK(near(f.distance_to_edge(0.F, 0.F), 25.F));  // centre: half the side
    CHECK(near(f.distance_to_edge(20.F, 0.F), 5.F));  // 5 m from the north edge
    CHECK(near(f.distance_to_edge(0.F, -20.F), 5.F)); // 5 m from the west edge
    CHECK(f.distance_to_edge(30.F, 0.F) < 0.F);       // outside -> negative
    CHECK(near(f.distance_to_edge(30.F, 0.F), -5.F));
    return 0;
}

int test_offset_square_and_centroid() {
    Geofence f;
    f.set_square(50.F, 100.F, -40.F); // fence about an arbitrary engage point
    float cn = 0.F;
    float ce = 0.F;
    f.centroid(cn, ce);
    CHECK(near(cn, 100.F) && near(ce, -40.F));
    CHECK(f.contains(100.F, -40.F));
    CHECK(!f.contains(100.F, 0.F));
    return 0;
}

int test_inward_points_at_centre() {
    Geofence f;
    f.set_square(50.F, 0.F, 0.F);
    float dn = 0.F;
    float de = 0.F;
    f.inward(20.F, 0.F, dn, de); // near the north edge -> push south
    CHECK(near(dn, -1.F) && near(de, 0.F));
    f.inward(0.F, -20.F, dn, de); // near the west edge -> push east
    CHECK(near(dn, 0.F) && near(de, 1.F));
    // At the exact centre all edges are equidistant (a tie): inward is any one
    // valid interior direction — a unit vector, not the old artifactual zero.
    f.inward(0.F, 0.F, dn, de);
    CHECK(near(std::sqrt((dn * dn) + (de * de)), 1.F));
    return 0;
}

// P2-03: for a CONCAVE range the centroid can fall outside the polygon, so the
// old toward-centroid inward could point a boundary-hugging vehicle further
// OUT. The nearest-edge inward must always point somewhere INSIDE.
int test_inward_stays_inside_for_concave_polygon() {
    Geofence f;
    // The same L shape as the containment test (centroid = (20,20), the notch
    // corner — right where the old method misbehaved).
    const double lat0 = 37.0;
    const double lon0 = 127.0;
    const double m_lat = 1.0 / 111320.0;
    const double m_lon = m_lat / std::cos(lat0 * M_PI / 180.0);
    std::vector<Geofence::GeoPoint> const v = {
        {lat0, lon0},
        {lat0 + (40.0 * m_lat), lon0},
        {lat0 + (40.0 * m_lat), lon0 + (20.0 * m_lon)},
        {lat0 + (20.0 * m_lat), lon0 + (20.0 * m_lon)},
        {lat0 + (20.0 * m_lat), lon0 + (40.0 * m_lon)},
        {lat0, lon0 + (40.0 * m_lon)},
    };
    CHECK(f.set_polygon_gps(v, lat0, lon0, 0.F, 0.F));
    // Points in each arm, near the notch edge; a small step along inward must
    // stay contained (the old centroid vector would have failed near the notch).
    const float pts[][2] = {{30.F, 18.F}, {35.F, 15.F}, {18.F, 30.F}, {10.F, 35.F}};
    for (const auto& p : pts) {
        float dn = 0.F;
        float de = 0.F;
        f.inward(p[0], p[1], dn, de);
        CHECK(near(std::sqrt((dn * dn) + (de * de)), 1.F));      // unit direction
        CHECK(f.contains(p[0] + (dn * 2.F), p[1] + (de * 2.F))); // heads inside
    }
    return 0;
}

int test_concave_polygon_containment() {
    // Even-odd ray casting must handle a non-convex range outline, not just the
    // default box (a surveyed field is rarely a rectangle).
    Geofence f;
    // An L shape: (0,0) (40,0) (40,20) (20,20) (20,40) (0,40)
    std::vector<Geofence::GeoPoint> const unused;
    (void)unused;
    // Build it directly through the GPS path with a degenerate projection is
    // awkward; use the square then verify the general path separately below.
    // Here we exercise contains() on an explicit L via set_polygon_gps with a
    // reference that makes 1 deg lat = 111320 m irrelevant: use tiny offsets.
    const double lat0 = 37.0;
    const double lon0 = 127.0;
    const double m_lat = 1.0 / 111320.0;
    const double m_lon = m_lat / std::cos(lat0 * M_PI / 180.0);
    std::vector<Geofence::GeoPoint> const v = {
        {lat0, lon0},
        {lat0 + (40.0 * m_lat), lon0},
        {lat0 + (40.0 * m_lat), lon0 + (20.0 * m_lon)},
        {lat0 + (20.0 * m_lat), lon0 + (20.0 * m_lon)},
        {lat0 + (20.0 * m_lat), lon0 + (40.0 * m_lon)},
        {lat0, lon0 + (40.0 * m_lon)},
    };
    CHECK(f.set_polygon_gps(v, lat0, lon0, 0.F, 0.F));
    CHECK(f.vertex_count() == 6);
    CHECK(f.contains(10.F, 10.F));  // in the lower arm
    CHECK(f.contains(30.F, 10.F));  // in the upper arm
    CHECK(!f.contains(30.F, 30.F)); // in the notch -> OUTSIDE
    CHECK(!f.contains(50.F, 10.F)); // beyond the top
    return 0;
}

int test_gps_projection_scales_correctly() {
    Geofence f;
    const double lat0 = 37.5;
    const double lon0 = 127.0;
    const double m_lat = 1.0 / 111320.0;
    const double m_lon = m_lat / std::cos(lat0 * M_PI / 180.0);
    // A 50 m square in GPS, anchored so its south-west corner is the reference.
    std::vector<Geofence::GeoPoint> const v = {
        {lat0, lon0},
        {lat0 + (50.0 * m_lat), lon0},
        {lat0 + (50.0 * m_lat), lon0 + (50.0 * m_lon)},
        {lat0, lon0 + (50.0 * m_lon)},
    };
    // Reference maps to local NED (10, -5): the projection must carry that offset.
    CHECK(f.set_polygon_gps(v, lat0, lon0, 10.F, -5.F));
    float cn = 0.F;
    float ce = 0.F;
    f.centroid(cn, ce);
    CHECK(near(cn, 35.F, 0.5F)); // 10 + 25
    CHECK(near(ce, 20.F, 0.5F)); // -5 + 25
    CHECK(f.contains(35.F, 20.F));
    CHECK(!f.contains(70.F, 20.F));
    return 0;
}

int test_invalid_polygon_rejected() {
    Geofence f;
    std::vector<Geofence::GeoPoint> const two = {{37.0, 127.0}, {37.001, 127.0}};
    CHECK(!f.set_polygon_gps(two, 37.0, 127.0, 0.F, 0.F));
    CHECK(!f.valid());
    // An unconfigured fence constrains nothing — callers gate on valid().
    CHECK(f.contains(1e6F, 1e6F));
    return 0;
}

// ------------------------------------------------- fence.polygon config path --

// G16.6 deviation: reject-matrix enumeration; one CHECK per malformed input (G16.6)
// NOLINTNEXTLINE(readability-function-size)
int test_parse_polygon_grammar() {
    std::vector<Geofence::GeoPoint> v;
    std::string err;
    // Empty/blank = no polygon configured — fence off, NOT an error.
    CHECK(Geofence::parse_polygon("", v, err));
    CHECK(v.empty());
    CHECK(Geofence::parse_polygon("  \t ", v, err));
    CHECK(v.empty());
    // Documented format, with loose spacing and a trailing separator.
    CHECK(Geofence::parse_polygon(" 37.1,127.1; 37.2 , 127.1 ;37.2,127.2; ", v, err));
    CHECK(v.size() == 3);
    CHECK(v[1].lat_deg == 37.2);
    CHECK(v[2].lon_deg == 127.2);
    // Fail-closed (AGENTS §7.9): a configured polygon must parse COMPLETELY.
    CHECK(!Geofence::parse_polygon("37.1,127.1; 37.2,127.1", v, err)); // 2 verts
    CHECK(v.empty());
    CHECK(!err.empty());
    CHECK(!Geofence::parse_polygon("37.1,127.1; garbage; 37.2,127.2", v, err));
    CHECK(!Geofence::parse_polygon("37.1,127.1; 37.2; 37.2,127.2", v, err));
    CHECK(!Geofence::parse_polygon("37.1,127.1,5; 37.2,127.1; 37.2,127.2", v, err));
    CHECK(!Geofence::parse_polygon("91.0,127.1; 37.2,127.1; 37.2,127.2", v, err));
    CHECK(!Geofence::parse_polygon("nan,127.1; 37.2,127.1; 37.2,127.2", v, err));
    CHECK(!Geofence::parse_polygon("37.1,inf; 37.2,127.1; 37.2,127.2", v, err));
    return 0;
}

// -------------------------------------------------------------------- SM-10 --

TelemetrySnapshot engaged_snapshot(uint64_t now_ns) {
    TelemetrySnapshot t{};
    t.stamp_all_streams(now_ns); // field-level SM-1: all streams fresh
    t.rel_alt_m = 5.F;
    t.armed = 1;
    t.in_offboard = 1;
    t.position_ok = 1;
    t.connected = 1;
    t.gps_ok = 1;
    t.battery_frac = 0.9F;
    t.battery_ok = 1;
    return t;
}

SafetyMonitor::Limits patrol_limits() {
    SafetyMonitor::Limits l;
    l.vmax_h = 8.F;
    l.vmax_v = 3.F;
    l.geofence_r = 150.F;
    l.alt_min = 2.F;
    l.alt_max = 60.F;
    l.telem_stale_ns = 500'000'000ULL;
    l.telem_flag_stale_ns = 3'000'000'000ULL;
    l.engage_timebox_ns = 600'000'000'000ULL;
    return l;
}

int test_sm10_fires_outside_polygon() {
    const uint64_t now = 10'000'000'000ULL;
    SafetyMonitor sm;
    sm.configure(patrol_limits());
    Geofence f;
    f.set_square(50.F, 0.F, 0.F);
    sm.set_fence(f);
    TelemetrySnapshot t = engaged_snapshot(now);
    sm.capture_home(t);
    CHECK((sm.evaluate(t, ObcState::OFFBOARD_ACTIVE, now, 0, now - 1'000'000'000ULL, true)
               .violation_mask &
           SB_FENCE_POLY) == 0U);
    t.pos_ned_m[0] = 30.F; // 5 m outside the north edge
    const SafetyVerdict v =
        sm.evaluate(t, ObcState::OFFBOARD_ACTIVE, now, 0, now - 1'000'000'000ULL, true);
    CHECK((v.violation_mask & SB_FENCE_POLY) != 0U);
    CHECK(!v.ok);
    // SM-3's radius (150 m) is untouched by this: the two limits are independent.
    CHECK((v.violation_mask & SB_GEOFENCE) == 0U);
    return 0;
}

int test_ini_polygon_to_sm10_end_to_end() {
    // The full configuration path: the DOCUMENTED ini polygon format
    // (';'-separated vertices, '#' inline comment) through Config::load ->
    // Geofence::parse_polygon -> set_polygon_gps -> the SM-10 verdict. This is
    // the path a ';'-inline-comment rule in the loader once silently broke, so
    // it is pinned end-to-end and not per-stage only.
    const std::string path = "/tmp/riposte_patrol_fence_" +
                             std::to_string(static_cast<long>(getpid())) + ".ini";
    {
        std::ofstream out(path);
        out << "[fence]\n"
               "# ~50 x 50 m box anchored at 37.0000, 127.0000 (T-1)\n"
               "polygon = 37.00000,127.00000; 37.00045,127.00000; "
               "37.00045,127.00056; 37.00000,127.00056  # surveyed corners\n"
               "side_m  = 0\n";
    }
    Config cfg;
    const bool loaded = cfg.load(path);
    ::unlink(path.c_str());
    CHECK(loaded);

    std::vector<Geofence::GeoPoint> verts;
    std::string err;
    CHECK(Geofence::parse_polygon(cfg.get_str("fence.polygon", ""), verts, err));
    CHECK(verts.size() == 4);

    Geofence f;
    CHECK(f.set_polygon_gps(verts, 37.0, 127.0, 0.F, 0.F));
    CHECK(f.valid());

    const uint64_t now = 10'000'000'000ULL;
    SafetyMonitor sm;
    sm.configure(patrol_limits());
    sm.set_fence(f);
    TelemetrySnapshot t = engaged_snapshot(now);
    t.pos_ned_m[0] = 25.F; // mid-box
    t.pos_ned_m[1] = 25.F;
    sm.capture_home(t);
    CHECK((sm.evaluate(t, ObcState::OFFBOARD_ACTIVE, now, 0, now - 1'000'000'000ULL, true)
               .violation_mask &
           SB_FENCE_POLY) == 0U);
    t.pos_ned_m[0] = 100.F; // well past the north edge (~50 m)
    const SafetyVerdict v =
        sm.evaluate(t, ObcState::OFFBOARD_ACTIVE, now, 0, now - 1'000'000'000ULL, true);
    CHECK((v.violation_mask & SB_FENCE_POLY) != 0U);
    CHECK(!v.ok);
    return 0;
}

int test_sm10_silent_without_a_fence() {
    const uint64_t now = 10'000'000'000ULL;
    SafetyMonitor sm;
    sm.configure(patrol_limits());
    TelemetrySnapshot t = engaged_snapshot(now);
    sm.capture_home(t);
    t.pos_ned_m[0] = 1000.F; // far away, but no polygon configured
    CHECK((sm.evaluate(t, ObcState::OFFBOARD_ACTIVE, now, 0, now - 1'000'000'000ULL, true)
               .violation_mask &
           SB_FENCE_POLY) == 0U);
    return 0;
}

int test_sm10_not_checked_before_engage() {
    const uint64_t now = 10'000'000'000ULL;
    SafetyMonitor sm;
    sm.configure(patrol_limits());
    Geofence f;
    f.set_square(50.F, 0.F, 0.F);
    sm.set_fence(f);
    TelemetrySnapshot t = engaged_snapshot(now);
    t.pos_ned_m[0] = 100.F;
    // PRESTREAM: the active-only checks stay off (same rule as SM-3).
    CHECK((sm.evaluate(t, ObcState::PRESTREAM, now, 0, 0, true).violation_mask &
           SB_FENCE_POLY) == 0U);
    return 0;
}

// ---------------------------------------------------- BalloonPatrolSource --

// Publishes a TrackState the way the seeker does, so GuidanceSource sees it.
class TrackPublisher {
public:
    bool open() {
        return bus_.open(tun::SHM_TRACK, ShmSeqSlot<TrackState>::Role::WRITER);
    }
    void publish(uint64_t mono_ns, uint32_t id, float fwd_m, float right_m,
                 float quality = 0.9F) {
        TrackState ts{};
        ts.mono_ns = mono_ns;
        ts.seq = ++seq_;
        ts.track_id = id;
        ts.rel_pos_frd_m[0] = fwd_m;
        ts.rel_pos_frd_m[1] = right_m;
        ts.rel_pos_frd_m[2] = 0.F;
        ts.quality = quality;
        ts.valid = 1;
        bus_.write(ts);
    }
    // Same, with an explicit vertical (FRD down-positive) offset.
    void publish_with_down(uint64_t mono_ns, uint32_t id, float fwd_m, float right_m,
                           float down_m, float quality = 0.9F) {
        TrackState ts{};
        ts.mono_ns = mono_ns;
        ts.seq = ++seq_;
        ts.track_id = id;
        ts.rel_pos_frd_m[0] = fwd_m;
        ts.rel_pos_frd_m[1] = right_m;
        ts.rel_pos_frd_m[2] = down_m;
        ts.quality = quality;
        ts.valid = 1;
        bus_.write(ts);
    }
    void publish_invalid(uint64_t mono_ns) {
        TrackState ts{};
        ts.mono_ns = mono_ns;
        ts.seq = ++seq_;
        ts.valid = 0;
        bus_.write(ts);
    }

private:
    ShmSeqSlot<TrackState> bus_;
    uint32_t seq_ = 0;
};

BalloonPatrolSource::Params patrol_params() {
    BalloonPatrolSource::Params p;
    p.patrol_alt_m = 5.F;
    p.approach_speed_mps = 3.F;
    p.search_yaw_rate_rps = 0.5F;
    p.soft_margin_m = 10.F;
    p.turn_margin_m = 3.F;
    p.reach_range_m = 3.F;
    p.alt_rate_mps = 1.F;
    p.visited_cooldown_ns = 30'000'000'000ULL;
    p.turn_dwell_ns = 1'000'000'000ULL;
    p.fence.set_square(50.F, 0.F, 0.F);
    return p;
}

// ---- source parameter validation (startup gates) ---------------------------
// The compute() maths assumes positive speeds/rates/margins; a sign typo used
// to invert boundary-recovery or landing motion with no diagnostic at all
// (SafetyMonitor::clamp bounds magnitude only). These pin the reject matrix.

// G16.6 deviation: reject-matrix enumeration; one CHECK per bad value (G16.6)
// NOLINTNEXTLINE(readability-function-size)
int test_patrol_params_validation() {
    auto bad = [](const BalloonPatrolSource::Params& p) {
        std::string err;
        return !validate(p, err) && !err.empty();
    };
    std::string err;
    CHECK(validate(patrol_params(), err)); // the test fixture itself is sane
    CHECK(err.empty());
    BalloonPatrolSource::Params p = patrol_params();
    p.approach_speed_mps = -3.F; // inverts TURN_AWAY: drives OUTWARD
    CHECK(bad(p));
    p = patrol_params();
    p.search_yaw_rate_rps = 0.F;
    CHECK(bad(p));
    p = patrol_params();
    p.alt_rate_mps = -1.F; // inverts the climb/descend clamp band
    CHECK(bad(p));
    p = patrol_params();
    p.soft_margin_m = 0.F; // divisor in the slow-down blend
    CHECK(bad(p));
    p = patrol_params();
    p.turn_margin_m = p.soft_margin_m; // TURN_AWAY could never exit
    CHECK(bad(p));
    p = patrol_params();
    p.reach_range_m = 0.F;
    CHECK(bad(p));
    p = patrol_params();
    p.patrol_alt_m = p.max_alt_m + 5.F; // patrol altitude outside the band
    CHECK(bad(p));
    p = patrol_params();
    p.max_alt_m = p.min_alt_m; // inverted/empty altitude band
    CHECK(bad(p));
    return 0;
}

// G16.6 deviation: reject-matrix enumeration; one CHECK per bad value (G16.6)
// NOLINTNEXTLINE(readability-function-size)
int test_mission_params_validation() {
    auto bad = [](const MissionSource::Params& p) {
        std::string err;
        return !validate(p, err) && !err.empty();
    };
    std::string err;
    CHECK(validate(MissionSource::Params{}, err)); // compiled defaults are sane
    MissionSource::Params p;
    p.cruise_speed_mps = -5.F; // goto_xy_alt flies AWAY from its point
    CHECK(bad(p));
    p = MissionSource::Params{};
    p.land_rate_mps = -0.8F; // LANDING climbs; disarm never fires
    CHECK(bad(p));
    p = MissionSource::Params{};
    p.takeoff_rate_mps = 0.F;
    CHECK(bad(p));
    p = MissionSource::Params{};
    p.takeoff_alt_m = p.max_alt_m + 10.F; // RETURN_HOME altitude is unclamped
    CHECK(bad(p));
    p = MissionSource::Params{};
    p.hover_radius_m = 0.F;
    CHECK(bad(p));
    p = MissionSource::Params{};
    p.search_radius_m = -1.F;
    CHECK(bad(p));
    p = MissionSource::Params{};
    p.land_alt_m = -0.1F;
    CHECK(bad(p));
    return 0;
}

int test_refuses_to_guide_without_a_fence() {
    // A patrol with no boundary has nothing keeping it inside the range; it must
    // decline rather than fly an unbounded search (converges on SM-7 disengage).
    BalloonPatrolSource::Params p = patrol_params();
    p.fence.clear();
    BalloonPatrolSource s(p);
    TelemetrySnapshot const t = engaged_snapshot(1'000'000'000ULL);
    VelocitySetpointNed out{};
    CHECK(!s.compute(t, 1'000'000'000ULL, out));
    return 0;
}

int test_searches_then_approaches(TrackPublisher& pub) {
    BalloonPatrolSource s(patrol_params());
    s.on_engage();
    uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    VelocitySetpointNed out{};

    pub.publish_invalid(now);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::SEARCH);
    // Station-keeping while searching: no horizontal command in the middle of
    // the range, so a search can never drift across the boundary.
    CHECK(near(out.vn_mps, 0.F) && near(out.ve_mps, 0.F));

    // A balloon appears 20 m ahead.
    now += 50'000'000ULL;
    t.mono_ns = now;
    pub.publish(now, 1, 20.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);
    CHECK(out.vn_mps > 1.F); // flying at it
    return 0;
}

// G16.6 deviation: scripted multi-phase scenario; splitting hides the timeline (G16.6)
// NOLINTNEXTLINE(readability-function-size)
int test_reaching_a_balloon_marks_it_visited(TrackPublisher& pub) {
    BalloonPatrolSource s(patrol_params());
    s.on_engage();
    uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    VelocitySetpointNed out{};

    pub.publish(now, 7, 20.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);

    // Closed to within the reach range -> serviced.
    now += 50'000'000ULL;
    t.mono_ns = now;
    pub.publish(now, 7, 2.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::TURN_AWAY);
    CHECK(s.visited_count() == 1);

    // Clear of the boundary and past the dwell -> back to searching...
    now += 2'000'000'000ULL;
    t.mono_ns = now;
    pub.publish(now, 7, 2.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::SEARCH);

    // ...and the SAME balloon must not be re-acquired (T-5: go find another).
    now += 50'000'000ULL;
    t.mono_ns = now;
    pub.publish(now, 7, 15.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::SEARCH);

    // A DIFFERENT balloon is fair game.
    now += 50'000'000ULL;
    t.mono_ns = now;
    pub.publish(now, 8, 15.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);
    return 0;
}

int test_slows_down_near_the_boundary(TrackPublisher& pub) {
    // T-5: the approach speed is scaled down through the soft margin.
    BalloonPatrolSource s(patrol_params());
    s.on_engage();
    uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    VelocitySetpointNed out{};

    pub.publish(now, 1, 20.F, 0.F);
    CHECK(s.compute(t, now, out)); // at the centre: 25 m of room
    const float fast = std::sqrt((out.vn_mps * out.vn_mps) + (out.ve_mps * out.ve_mps));

    now += 50'000'000ULL;
    t.mono_ns = now;
    t.pos_ned_m[0] = 19.F; // 6 m from the north edge: inside the soft margin
    pub.publish(now, 1, 20.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);
    const float slow_n = out.vn_mps;
    // The northward (outward) component must have been pulled back.
    CHECK(slow_n < fast);
    return 0;
}

int test_gives_up_the_target_at_the_turn_margin(TrackPublisher& pub) {
    BalloonPatrolSource s(patrol_params());
    s.on_engage();
    uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    VelocitySetpointNed out{};

    pub.publish(now, 3, 20.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);

    now += 50'000'000ULL;
    t.mono_ns = now;
    t.pos_ned_m[0] = 23.F; // 2 m of room: inside the turn margin
    pub.publish(now, 3, 20.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::TURN_AWAY);
    CHECK(s.visited_count() == 1); // the balloon beyond the fence is written off
    // Turning away drives back INTO the range (southward here) and faces inward.
    CHECK(out.vn_mps < 0.F);
    CHECK(near(out.yaw_rad, static_cast<float>(M_PI))); // due south
    return 0;
}

int test_holds_patrol_altitude(TrackPublisher& pub) {
    BalloonPatrolSource s(patrol_params());
    s.on_engage();
    const uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    VelocitySetpointNed out{};
    pub.publish_invalid(now);

    t.rel_alt_m = 2.F; // below the patrol altitude -> climb (negative down)
    CHECK(s.compute(t, now, out));
    CHECK(out.vd_mps < 0.F);

    t.rel_alt_m = 9.F; // above it -> descend
    CHECK(s.compute(t, now, out));
    CHECK(out.vd_mps > 0.F);

    t.rel_alt_m = 5.F; // on it -> hold
    CHECK(s.compute(t, now, out));
    CHECK(near(out.vd_mps, 0.F, 0.05F));
    return 0;
}

int test_approach_matches_the_target_altitude(TrackPublisher& pub) {
    // Regression from the Gazebo trial: closing on a target that sits below the
    // patrol altitude drove the line of sight past the camera's vertical FOV in
    // the last few metres, so the approach never completed. While approaching,
    // the vertical command must track the TARGET's altitude, not the patrol one.
    BalloonPatrolSource s(patrol_params()); // patrol_alt 5 m
    s.on_engage();
    const uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    t.rel_alt_m = 5.F; // already at patrol altitude: no patrol-hold command
    VelocitySetpointNed out{};

    // Target 15 m ahead and 2 m BELOW (rel FRD down = +2).
    TrackState const ts{};
    (void)ts;
    pub.publish(now, 11, 15.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);

    // Re-publish with a vertical offset and step again.
    ShmSeqSlot<TrackState> const w;
    (void)w;
    pub.publish_with_down(now + 50'000'000ULL, 11, 15.F, 0.F, 2.F);
    t.mono_ns = now + 50'000'000ULL;
    CHECK(s.compute(t, now + 50'000'000ULL, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);
    CHECK(out.vd_mps > 0.F); // descend toward the target (down is positive)

    // Target ABOVE instead -> climb.
    pub.publish_with_down(now + 100'000'000ULL, 11, 15.F, 0.F, -2.F);
    t.mono_ns = now + 100'000'000ULL;
    CHECK(s.compute(t, now + 100'000'000ULL, out));
    CHECK(out.vd_mps < 0.F);
    return 0;
}

int test_approach_altitude_is_bounded(TrackPublisher& pub) {
    // Chasing a very low target must not fly into the ground: the matched
    // altitude is clamped to the configured band.
    BalloonPatrolSource::Params p = patrol_params();
    p.min_alt_m = 3.F;
    BalloonPatrolSource s(p);
    s.on_engage();
    const uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    t.rel_alt_m = 3.F; // already at the floor
    VelocitySetpointNed out{};
    pub.publish_with_down(now, 12, 15.F, 0.F, 10.F); // target 10 m below the floor
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);
    CHECK(out.vd_mps <= 0.01F); // must not descend below the floor
    return 0;
}

int test_losing_the_track_returns_to_search(TrackPublisher& pub) {
    BalloonPatrolSource s(patrol_params());
    s.on_engage();
    uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    VelocitySetpointNed out{};

    pub.publish(now, 5, 20.F, 0.F);
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::APPROACH);

    // The seeker loses it and the coast budget runs out.
    now += tun::TRACK_STALE_NS + tun::TRACK_COAST_NS + 100'000'000ULL;
    t.mono_ns = now;
    CHECK(s.compute(t, now, out));
    CHECK(s.phase() == BalloonPatrolSource::Phase::SEARCH);
    // Still guiding (station-keeping search), NOT a disengage: losing one
    // balloon is normal in a patrol, unlike in an closure.
    return 0;
}

int test_on_engage_clears_visited(TrackPublisher& pub) {
    BalloonPatrolSource s(patrol_params());
    s.on_engage();
    uint64_t now = mono_now_ns();
    TelemetrySnapshot t = engaged_snapshot(now);
    VelocitySetpointNed out{};
    pub.publish(now, 9, 20.F, 0.F);
    (void)s.compute(t, now, out);
    now += 50'000'000ULL;
    t.mono_ns = now;
    pub.publish(now, 9, 2.F, 0.F);
    (void)s.compute(t, now, out);
    CHECK(s.visited_count() == 1);
    s.on_engage();
    CHECK(s.visited_count() == 0);
    CHECK(s.phase() == BalloonPatrolSource::Phase::SEARCH);
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_square_containment();
    rc = rc != 0 ? rc : test_square_distance_sign_and_value();
    rc = rc != 0 ? rc : test_offset_square_and_centroid();
    rc = rc != 0 ? rc : test_inward_points_at_centre();
    rc = rc != 0 ? rc : test_inward_stays_inside_for_concave_polygon();
    rc = rc != 0 ? rc : test_concave_polygon_containment();
    rc = rc != 0 ? rc : test_gps_projection_scales_correctly();
    rc = rc != 0 ? rc : test_invalid_polygon_rejected();
    rc = rc != 0 ? rc : test_parse_polygon_grammar();
    rc = rc != 0 ? rc : test_sm10_fires_outside_polygon();
    rc = rc != 0 ? rc : test_ini_polygon_to_sm10_end_to_end();
    rc = rc != 0 ? rc : test_sm10_silent_without_a_fence();
    rc = rc != 0 ? rc : test_sm10_not_checked_before_engage();
    rc = rc != 0 ? rc : test_patrol_params_validation();
    rc = rc != 0 ? rc : test_mission_params_validation();
    rc = rc != 0 ? rc : test_refuses_to_guide_without_a_fence();

    TrackPublisher pub;
    if (!pub.open()) {
        std::printf("FAIL: cannot open TrackBus shm\n");
        return 1;
    }
    rc = rc != 0 ? rc : test_searches_then_approaches(pub);
    rc = rc != 0 ? rc : test_reaching_a_balloon_marks_it_visited(pub);
    rc = rc != 0 ? rc : test_slows_down_near_the_boundary(pub);
    rc = rc != 0 ? rc : test_gives_up_the_target_at_the_turn_margin(pub);
    rc = rc != 0 ? rc : test_holds_patrol_altitude(pub);
    rc = rc != 0 ? rc : test_approach_matches_the_target_altitude(pub);
    rc = rc != 0 ? rc : test_approach_altitude_is_bounded(pub);
    rc = rc != 0 ? rc : test_losing_the_track_returns_to_search(pub);
    rc = rc != 0 ? rc : test_on_engage_clears_visited(pub);
    ShmSeqSlot<TrackState>::unlink(tun::SHM_TRACK);
    if (rc != 0) {
        return rc;
    }
    std::printf("test_patrol: %d checks passed\n", checks);
    return 0;
}
