#include "Rendezvous.h"

#include <cmath>
#include <cstdint>

#include "riposte/Clock.h"

namespace riposte {

namespace {
float dot3(const float a[3], const float b[3]) {
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

// Smallest strictly-positive root of a*t^2 + b*t + c, or -1 if there is none.
// Handles a == 0 (target and ownship at equal speed) as the linear case.
float smallest_positive_root(double a, double b, double c) {
    constexpr double EPS = 1e-9;
    if (std::fabs(a) < EPS) {
        if (std::fabs(b) < EPS) {
            return -1.F; // degenerate: no dependence on t
        }
        const double t = -c / b;
        return (t > EPS) ? static_cast<float>(t) : -1.F;
    }
    const double disc = (b * b) - (4.0 * a * c);
    if (disc < 0.0) {
        return -1.F; // never meets
    }
    const double sq = std::sqrt(disc);
    const double t1 = (-b - sq) / (2.0 * a);
    const double t2 = (-b + sq) / (2.0 * a);
    // The two roots come out in an order that depends on sign(a), so pick by
    // value rather than assuming t1 < t2.
    double best = -1.0;
    if (t1 > EPS) {
        best = t1;
    }
    if (t2 > EPS && (best < 0.0 || t2 < best)) {
        best = t2;
    }
    return (best > 0.0) ? static_cast<float>(best) : -1.F;
}
} // namespace

RendezvousResult solve_rendezvous(const float own_ned_m[3], const RendezvousCue& cue,
                                  float speed_mps, uint64_t now_ns) {
    RendezvousResult r;
    // Age the cue forward: it arrives at a low rate, so by the time it is acted
    // on the target has already moved. Everything below works from the target's
    // position NOW, not from where it was when the GCS looked at it.
    const float age_s = (cue.mono_ns != 0U)
                            ? static_cast<float>(ns_to_s(age_ns(now_ns, cue.mono_ns)))
                            : 0.F;
    float p_now[3];
    for (int i = 0; i < 3; ++i) {
        p_now[i] = cue.pos_ned_m[i] + (cue.vel_ned_mps[i] * age_s);
        r.point_ned_m[i] = p_now[i]; // direct-tracking fallback unless solved below
    }

    if (!(speed_mps > 0.F)) {
        return r; // no closing speed: nothing to solve, fall back to the target
    }

    // |R + V t| = s t  ->  (|V|^2 - s^2) t^2 + 2 (R.V) t + |R|^2 = 0
    float rel[3];
    for (int i = 0; i < 3; ++i) {
        rel[i] = p_now[i] - own_ned_m[i];
    }
    const double vv = dot3(cue.vel_ned_mps, cue.vel_ned_mps);
    const double rv = dot3(rel, cue.vel_ned_mps);
    const double rr = dot3(rel, rel);
    const double s2 = static_cast<double>(speed_mps) * static_cast<double>(speed_mps);

    if (rr < 1e-6) {
        r.ok = true; // already on top of it
        return r;
    }
    // A static cue has no lead to compute: fly at it.
    if (vv < 1e-9) {
        r.ok = true;
        r.t_go_s = static_cast<float>(std::sqrt(rr) / speed_mps);
        return r;
    }

    const float t = smallest_positive_root(vv - s2, 2.0 * rv, rr);
    if (t < 0.F || t > RENDEZVOUS_MAX_TGO_S) {
        // Either unreachable (faster target, opening geometry) or a solution so
        // far out that the constant-velocity assumption is fiction. Keep the
        // direct-tracking fallback already in point_ned_m and say so.
        return r;
    }

    float lead2 = 0.F;
    for (int i = 0; i < 3; ++i) {
        const float aim = p_now[i] + (cue.vel_ned_mps[i] * t);
        lead2 += (aim - p_now[i]) * (aim - p_now[i]);
        r.point_ned_m[i] = aim;
    }
    r.ok = true;
    r.t_go_s = t;
    r.lead_m = std::sqrt(lead2);
    return r;
}

} // namespace riposte
