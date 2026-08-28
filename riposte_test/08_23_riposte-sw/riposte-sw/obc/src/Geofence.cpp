#include "Geofence.h"

#include <math.h>

#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace riposte {

namespace {
// Metres per degree of latitude (WGS-84 mean). Longitude scales by cos(lat).
constexpr double M_PER_DEG_LAT = 111320.0;

// Closest point on segment [a, b] to p, returned in (cn, ce); the squared
// distance is the return value. A point, a segment and the out point are 8
// scalars by nature (matches seg_dist2's scalar-param style) — the parameter
// count trips readability-function-size, which the next line suppresses.
// NOLINTNEXTLINE(readability-function-size)
float seg_closest(float pn, float pe, float an, float ae, float bn, float be, float& cn,
                  float& ce) {
    const float vn = bn - an;
    const float ve = be - ae;
    const float wn = pn - an;
    const float we = pe - ae;
    const float len2 = (vn * vn) + (ve * ve);
    float t = 0.F;
    if (len2 > 1e-12F) {
        t = ((wn * vn) + (we * ve)) / len2;
        t = std::fmax(0.F, std::fmin(1.F, t)); // clamp to the segment
    }
    cn = an + (t * vn);
    ce = ae + (t * ve);
    const float dn = pn - cn;
    const float de = pe - ce;
    return (dn * dn) + (de * de);
}

// Squared distance from p to the segment [a, b].
float seg_dist2(float pn, float pe, float an, float ae, float bn, float be) {
    float cn = 0.F;
    float ce = 0.F;
    return seg_closest(pn, pe, an, ae, bn, be, cn, ce);
}

std::string trim_ws(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) {
        return "";
    }
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Whole-string finite degree value within [lo, hi]; false on any leftover
// characters ("37,127,5" must not silently become 37 — G3).
bool parse_deg(const std::string& s, double lo, double hi, double& out) {
    const char* start = s.c_str();
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod(start, &end);
    if (end == start || *end != '\0' || errno == ERANGE || !std::isfinite(v) || v < lo ||
        v > hi) {
        return false;
    }
    out = v;
    return true;
}
} // namespace

void Geofence::clear() {
    n_.clear();
    e_.clear();
}

void Geofence::set_square(float side_m, float centre_n, float centre_e) {
    const float h = side_m * 0.5F;
    n_ = {centre_n - h, centre_n + h, centre_n + h, centre_n - h};
    e_ = {centre_e - h, centre_e - h, centre_e + h, centre_e + h};
}

bool Geofence::set_polygon_gps(const std::vector<GeoPoint>& verts, double ref_lat,
                               double ref_lon, float ref_n, float ref_e) {
    if (verts.size() < 3) {
        return false;
    }
    const double m_per_deg_lon = M_PER_DEG_LAT * std::cos(ref_lat * M_PI / 180.0);
    n_.clear();
    e_.clear();
    n_.reserve(verts.size());
    e_.reserve(verts.size());
    for (const auto& v : verts) {
        n_.push_back(ref_n + static_cast<float>((v.lat_deg - ref_lat) * M_PER_DEG_LAT));
        e_.push_back(ref_e + static_cast<float>((v.lon_deg - ref_lon) * m_per_deg_lon));
    }
    return true;
}

bool Geofence::contains(float n, float e) const {
    if (!valid()) {
        return true; // unconfigured fence constrains nothing (callers gate on valid())
    }
    // Even-odd ray cast along +east. The half-open edge test (one endpoint
    // strictly above the ray, one at-or-below) counts a vertex exactly once, so
    // a point level with a vertex is not double-counted.
    bool inside = false;
    const std::size_t cnt = n_.size();
    for (std::size_t i = 0, j = cnt - 1; i < cnt; j = i++) {
        const bool straddles = (n_[i] > n) != (n_[j] > n);
        if (!straddles) {
            continue;
        }
        const float dn = n_[j] - n_[i];
        if (std::fabs(dn) < 1e-12F) {
            continue;
        }
        const float x = e_[i] + (((n - n_[i]) / dn) * (e_[j] - e_[i]));
        if (e < x) {
            inside = !inside;
        }
    }
    return inside;
}

float Geofence::distance_to_edge(float n, float e) const {
    if (!valid()) {
        return 0.F;
    }
    float best2 = -1.F;
    const std::size_t cnt = n_.size();
    for (std::size_t i = 0, j = cnt - 1; i < cnt; j = i++) {
        const float d2 = seg_dist2(n, e, n_[i], e_[i], n_[j], e_[j]);
        if (best2 < 0.F || d2 < best2) {
            best2 = d2;
        }
    }
    const float d = std::sqrt(std::fmax(0.F, best2));
    return contains(n, e) ? d : -d;
}

void Geofence::centroid(float& n, float& e) const {
    n = 0.F;
    e = 0.F;
    if (n_.empty()) {
        return;
    }
    for (std::size_t i = 0; i < n_.size(); ++i) {
        n += n_[i];
        e += e_[i];
    }
    const float inv = 1.F / static_cast<float>(n_.size());
    n *= inv;
    e *= inv;
}

void Geofence::inward(float n, float e, float& dn, float& de) const {
    dn = 0.F;
    de = 0.F;
    if (!valid()) {
        return;
    }
    // Direction that increases the signed distance to the boundary — i.e. the
    // way to turn to get deeper inside (P2-03). This is the negative gradient of
    // the nearest-EDGE distance, NOT the direction to the centroid: for a
    // concave (U/L) range the centroid can lie OUTSIDE the polygon, so the old
    // toward-centroid vector could point a boundary-hugging vehicle further out.
    // Working from the nearest edge is local and correct for any winding.
    std::size_t best = 0;
    float best2 = -1.F;
    float bcn = 0.F;
    float bce = 0.F;
    const std::size_t cnt = n_.size();
    for (std::size_t i = 0, j = cnt - 1; i < cnt; j = i++) {
        float cn = 0.F;
        float ce = 0.F;
        const float d2 = seg_closest(n, e, n_[i], e_[i], n_[j], e_[j], cn, ce);
        if (best2 < 0.F || d2 < best2) {
            best2 = d2;
            best = i;
            bcn = cn;
            bce = ce;
        }
    }
    // Away from the nearest boundary point when inside; back toward it when
    // outside — both increase the signed distance (re-enter / go deeper).
    float rn = n - bcn;
    float re = e - bce;
    if (!contains(n, e)) {
        rn = -rn;
        re = -re;
    }
    const float len = std::sqrt((rn * rn) + (re * re));
    if (len < 1e-6F) {
        // On the boundary (query == closest point): use the nearest edge's
        // inward normal, oriented by probing which side is contained. best is in
        // [0, cnt) and cnt >= 3 (valid()), so this indexes the previous vertex
        // without a modulo (and without the analyzer's divide-by-zero worry).
        const std::size_t j = (best == 0) ? (cnt - 1) : (best - 1);
        const float en = n_[best] - n_[j];
        const float ee = e_[best] - e_[j];
        // Two candidate normals; pick the one whose small step lands inside.
        float nn = -ee;
        float ne = en;
        const float nl = std::sqrt((nn * nn) + (ne * ne));
        if (nl < 1e-6F) {
            return;
        }
        nn /= nl;
        ne /= nl;
        if (!contains(n + (nn * 0.1F), e + (ne * 0.1F))) {
            nn = -nn;
            ne = -ne;
        }
        dn = nn;
        de = ne;
        return;
    }
    dn = rn / len;
    de = re / len;
}

bool Geofence::parse_polygon(const std::string& text, std::vector<GeoPoint>& out,
                             std::string& err) {
    out.clear();
    err.clear();
    const std::string body = trim_ws(text);
    if (body.empty()) {
        return true; // no polygon configured — fence stays off, not an error
    }
    std::stringstream ss(body);
    std::string item;
    while (std::getline(ss, item, ';')) {
        const std::string v = trim_ws(item);
        if (v.empty()) {
            continue; // trailing/doubled separator
        }
        const std::size_t comma = v.find(',');
        GeoPoint p;
        if (comma == std::string::npos ||
            !parse_deg(trim_ws(v.substr(0, comma)), -90.0, 90.0, p.lat_deg) ||
            !parse_deg(trim_ws(v.substr(comma + 1)), -180.0, 180.0, p.lon_deg)) {
            err = "bad vertex '" + v + "' (want finite in-range \"lat,lon\")";
            out.clear();
            return false;
        }
        out.push_back(p);
    }
    if (out.size() < 3) {
        err = "needs >= 3 vertices (" + std::to_string(out.size()) + " parsed)";
        out.clear();
        return false;
    }
    return true;
}

} // namespace riposte
