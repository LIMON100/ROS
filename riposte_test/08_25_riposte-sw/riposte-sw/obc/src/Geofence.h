#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace riposte {

// A convex-or-concave polygon flight boundary in LOCAL NED metres.
//
// The test-flight boundary is specified in GPS (RIPOSTE-DUALEO-REQ-001 T-1: a
// 50 x 50 m box), but every flight decision is made in the EKF's local NED
// frame, so the polygon is projected ONCE — at engage, against a telemetry
// sample that carries both representations of the same instant — and used in
// metres afterwards. Re-projecting per tick would drift with GPS noise.
//
// This is a boundary MODEL, not a policy: SafetyMonitor uses it as a hard limit
// (SM-10) while the patrol behaviour uses the distance to slow down and turn
// away before ever reaching it.
class Geofence {
public:
    struct GeoPoint {
        double lat_deg = 0.0;
        double lon_deg = 0.0;
    };

    // Axis-aligned square of `side_m`, centred on a local NED point. The default
    // test boundary (T-1) when no GPS polygon is configured.
    void set_square(float side_m, float centre_n, float centre_e);

    // Projects GPS vertices into local NED using an equirectangular projection
    // anchored at (ref_lat, ref_lon) <-> (ref_n, ref_e) — the two coordinate
    // representations of ONE telemetry sample. Accurate to well under a metre
    // over a test range this size. Needs >= 3 vertices.
    bool set_polygon_gps(const std::vector<GeoPoint>& verts, double ref_lat,
                         double ref_lon, float ref_n, float ref_e);

    // Parses the "lat,lon; lat,lon; ..." vertex list of fence.polygon
    // (RIPOSTE-DUALEO-REQ-001 T-1). Fail-closed (AGENTS §7.9): an empty/blank
    // string means "no polygon configured" (out empty, returns true); anything
    // else must parse COMPLETELY — >= 3 vertices, each a finite in-range
    // "lat,lon" pair — or this returns false with the reason in `err` so the
    // caller refuses startup instead of flying with a silently dropped SM-10
    // boundary.
    static bool parse_polygon(const std::string& text, std::vector<GeoPoint>& out,
                              std::string& err);

    bool valid() const { return n_.size() >= 3; }
    void clear();

    // Winding-agnostic containment (even-odd ray cast).
    bool contains(float n, float e) const;

    // Distance to the nearest edge, POSITIVE inside the polygon and negative
    // outside. This is what the patrol behaviour throttles on, so the sign
    // convention matters: "how much room is left".
    float distance_to_edge(float n, float e) const;

    // Unit vector in the direction that increases the signed distance to the
    // boundary — the way to turn to get deeper inside when the edge is close.
    // Derived from the NEAREST EDGE, not the centroid, so it stays correct for
    // a concave range whose centroid can lie outside the polygon (P2-03).
    // Falls back to (0,0) only for an unconfigured/degenerate fence.
    void inward(float n, float e, float& dn, float& de) const;

    std::size_t vertex_count() const { return n_.size(); }
    void centroid(float& n, float& e) const;

private:
    // Vertices in local NED, parallel arrays (north, east).
    std::vector<float> n_;
    std::vector<float> e_;
};

} // namespace riposte
