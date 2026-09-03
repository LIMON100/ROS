#include "AssocCost.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace riposte {

const float ASSOC_REJECT = std::numeric_limits<float>::infinity();

bool l2_normalize(std::vector<float>& v) {
    if (v.empty()) {
        return false;
    }
    float sum2 = 0.F;
    for (const float x : v) {
        if (!std::isfinite(x)) {
            return false; // untrusted device output: NaN is not evidence
        }
        sum2 += x * x;
    }
    if (!(sum2 > 0.F) || !std::isfinite(sum2)) {
        return false; // zero (or overflowed) norm cannot be normalized
    }
    const float inv = 1.F / std::sqrt(sum2);
    for (float& x : v) {
        x *= inv;
    }
    return true;
}

bool cosine_dist(const Embedding& a, const Embedding& b, float& out) {
    if (!a.valid() || !b.valid() || a.v.size() != b.v.size()) {
        return false; // not comparable: caller degrades to motion-only (TR-3)
    }
    float dot = 0.F;
    for (std::size_t i = 0; i < a.v.size(); ++i) {
        dot += a.v[i] * b.v[i];
    }
    if (!std::isfinite(dot)) {
        return false;
    }
    // Unit inputs put dot in [-1, 1]; clamp against rounding before the
    // subtraction so the distance stays inside [0, 2].
    out = 1.F - std::min(1.F, std::max(-1.F, dot));
    return true;
}

void update_embedding(Embedding& gallery, const Embedding& fresh, float alpha) {
    if (!fresh.valid()) {
        return; // nothing observed; the gallery keeps its history
    }
    if (!gallery.valid() || gallery.v.size() != fresh.v.size()) {
        gallery = fresh; // first observation (or model change): adopt as-is
        return;
    }
    for (std::size_t i = 0; i < gallery.v.size(); ++i) {
        gallery.v[i] += alpha * (fresh.v[i] - gallery.v[i]);
    }
    // The blend of two unit vectors is shorter than unit; renormalize so
    // cosine distances against the gallery keep their scale. A degenerate
    // blend (opposite vectors cancelling out) invalidates the gallery rather
    // than leaving a garbage direction in it.
    if (!l2_normalize(gallery.v)) {
        gallery.v.clear();
    }
}

namespace {
// Same isotropic width-normalized metric as Tracker::dist2 — T0 and T1 must
// gate identically or the layers disagree about which pairings exist at all.
float motion_dist2(const AssocPoint& a, const AssocPoint& b, float aspect) {
    const float dx = a.cx - b.cx;
    const float dy = (a.cy - b.cy) / aspect;
    return (dx * dx) + (dy * dy);
}
} // namespace

float assoc_cost(const AssocPoint& track, const AssocPoint& det, const AssocParams& p) {
    if (!(p.gate2 > 0.F)) {
        return ASSOC_REJECT; // degenerate gate: nothing can pair
    }
    const float m2 = motion_dist2(track, det, p.aspect);
    if (m2 > p.gate2) {
        // The gate is authoritative (TR-6): appearance never rescues a pairing
        // the motion model rules out.
        return ASSOC_REJECT;
    }
    float const cost = m2 / p.gate2; // [0, 1] inside the gate

    float app = 0.F;
    const bool comparable = track.emb != nullptr && det.emb != nullptr &&
                            cosine_dist(*track.emb, *det.emb, app);
    if (!comparable) {
        return cost; // motion-only: byte-for-byte the T0 behaviour (TR-3)
    }
    if (app > p.appearance_reject) {
        // In the gate but looking like a different object: the crossing-targets
        // impostor (TR-1). Rejecting here is what prevents the ID switch.
        return ASSOC_REJECT;
    }
    return cost + (p.appearance_weight * app);
}

std::vector<float> assoc_matrix(const std::vector<AssocPoint>& tracks,
                                const std::vector<AssocPoint>& dets,
                                const AssocParams& p) {
    std::vector<float> cost(tracks.size() * dets.size(), ASSOC_REJECT);
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        for (std::size_t j = 0; j < dets.size(); ++j) {
            cost[(i * dets.size()) + j] = assoc_cost(tracks[i], dets[j], p);
        }
    }
    return cost;
}

std::vector<std::pair<int, int>> assoc_greedy_match(const std::vector<float>& cost,
                                                    std::size_t num_tracks,
                                                    std::size_t num_dets) {
    std::vector<std::pair<int, int>> out;
    if (num_tracks == 0 || num_dets == 0 || cost.size() != num_tracks * num_dets) {
        return out; // size mismatch is a caller bug; match nothing, fail closed
    }
    std::vector<bool> track_used(num_tracks, false);
    std::vector<bool> det_used(num_dets, false);
    const std::size_t rounds = std::min(num_tracks, num_dets);
    for (std::size_t r = 0; r < rounds; ++r) {
        float best = ASSOC_REJECT;
        std::size_t bi = 0;
        std::size_t bj = 0;
        for (std::size_t i = 0; i < num_tracks; ++i) {
            if (track_used[i]) {
                continue;
            }
            for (std::size_t j = 0; j < num_dets; ++j) {
                if (det_used[j]) {
                    continue;
                }
                const float c = cost[(i * num_dets) + j];
                if (c < best) {
                    best = c;
                    bi = i;
                    bj = j;
                }
            }
        }
        if (!(best < ASSOC_REJECT)) {
            break; // only rejected pairings remain
        }
        track_used[bi] = true;
        det_used[bj] = true;
        out.emplace_back(static_cast<int>(bi), static_cast<int>(bj));
    }
    return out;
}

} // namespace riposte
