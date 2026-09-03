// T1 ReID association cost tests (RIPOSTE-TRACKER-REQ-001 TR-B, host half).
//
// The scenario that justifies this whole layer is two similar targets crossing
// inside each other's motion gates: position alone cannot tell them apart, and
// a wrong pairing swaps the track IDs — on the engaged primary that means the
// ownship quietly changes targets. The tests here pin down (a) the gate
// staying authoritative, (b) appearance re-ranking and impostor rejection
// inside the gate, (c) byte-for-byte motion-only fallback when embeddings are
// missing or garbage, and (d) the crossing case end to end.
#include "AssocCost.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>
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

bool near(float a, float b, float tol = 1e-5F) {
    return std::fabs(a - b) <= tol;
}

const float NAN_F = std::numeric_limits<float>::quiet_NaN();

Embedding emb(std::vector<float> v) {
    Embedding e;
    e.v = std::move(v);
    l2_normalize(e.v);
    return e;
}

AssocParams params() {
    AssocParams p;
    p.gate2 = 0.01F; // gate radius 0.1 width-normalized
    p.aspect = 1.F;
    p.appearance_weight = 0.5F;
    p.appearance_reject = 0.7F;
    return p;
}

// ------------------------------------------------------------ normalize --

int test_normalize_produces_unit_norm() {
    std::vector<float> v{3.F, 4.F};
    CHECK(l2_normalize(v));
    CHECK(near(v[0], 0.6F) && near(v[1], 0.8F));
    CHECK(near((v[0] * v[0]) + (v[1] * v[1]), 1.F));
    return 0;
}

int test_normalize_rejects_garbage() {
    std::vector<float> zero{0.F, 0.F, 0.F};
    CHECK(!l2_normalize(zero));
    std::vector<float> nan{1.F, NAN_F};
    CHECK(!l2_normalize(nan));
    std::vector<float> empty;
    CHECK(!l2_normalize(empty));
    return 0;
}

// ---------------------------------------------------------- cosine dist --

int test_cosine_dist_identical_orthogonal_opposite() {
    const Embedding a = emb({1.F, 0.F});
    const Embedding b = emb({0.F, 1.F});
    const Embedding c = emb({-1.F, 0.F});
    float d = -1.F;
    CHECK(cosine_dist(a, a, d) && near(d, 0.F));
    CHECK(cosine_dist(a, b, d) && near(d, 1.F));
    CHECK(cosine_dist(a, c, d) && near(d, 2.F));
    return 0;
}

int test_cosine_dist_incomparable_pairs() {
    const Embedding a = emb({1.F, 0.F});
    const Embedding mismatched = emb({1.F, 0.F, 0.F});
    const Embedding invalid;
    float d = -1.F;
    CHECK(!cosine_dist(a, mismatched, d)); // dimension mismatch
    CHECK(!cosine_dist(a, invalid, d));    // empty embedding
    CHECK(!cosine_dist(invalid, invalid, d));
    return 0;
}

// ------------------------------------------------------- gallery update --

int test_gallery_adopts_first_then_smooths_and_stays_unit() {
    Embedding gallery;
    const Embedding first = emb({1.F, 0.F});
    update_embedding(gallery, first, 0.2F);
    CHECK(gallery.valid());
    float d = -1.F;
    CHECK(cosine_dist(gallery, first, d) && near(d, 0.F)); // adopted as-is

    const Embedding drifted = emb({0.F, 1.F});
    update_embedding(gallery, drifted, 0.2F);
    // Moved toward the new observation but nowhere near all the way...
    CHECK(cosine_dist(gallery, first, d) && d > 0.F && d < 0.5F);
    // ...and still unit norm after the blend.
    float n2 = 0.F;
    for (const float x : gallery.v) {
        n2 += x * x;
    }
    CHECK(near(n2, 1.F));

    // An invalid observation must not erase history.
    update_embedding(gallery, Embedding{}, 0.2F);
    CHECK(gallery.valid());
    return 0;
}

// ----------------------------------------------------------- assoc cost --

int test_gate_is_authoritative_over_appearance() {
    const AssocParams p = params();
    const Embedding same = emb({1.F, 0.F});
    AssocPoint const t{0.5F, 0.5F, &same};
    AssocPoint const d{0.7F, 0.5F, &same}; // 0.2 away, gate radius 0.1: outside
    // Identical appearance cannot rescue an out-of-gate pairing (TR-6).
    CHECK(assoc_cost(t, d, p) == ASSOC_REJECT);
    return 0;
}

int test_motion_only_when_embeddings_missing_or_garbage() {
    const AssocParams p = params();
    AssocPoint t{0.5F, 0.5F, nullptr};
    AssocPoint d{0.55F, 0.5F, nullptr}; // dist2 = 0.0025, gate2 = 0.01
    const float motion_only = assoc_cost(t, d, p);
    CHECK(near(motion_only, 0.25F)); // m2/gate2, no appearance term

    // One side missing: same motion-only cost (TR-3 fallback).
    const Embedding e = emb({1.F, 0.F});
    t.emb = &e;
    CHECK(near(assoc_cost(t, d, p), motion_only));

    // Garbage embedding (never normalized, raw NaN) counts as missing too.
    Embedding garbage;
    garbage.v = {NAN_F, 1.F};
    d.emb = &garbage;
    CHECK(near(assoc_cost(t, d, p), motion_only));
    return 0;
}

int test_appearance_reranks_inside_the_gate() {
    const AssocParams p = params();
    const Embedding target = emb({1.F, 0.F});
    const Embedding similar = emb({0.95F, 0.312F}); // small angle off
    AssocPoint const t{0.5F, 0.5F, &target};
    AssocPoint const d{0.55F, 0.5F, &similar};
    const float cost = assoc_cost(t, d, p);
    float app = -1.F;
    CHECK(cosine_dist(target, similar, app));
    CHECK(near(cost, 0.25F + (p.appearance_weight * app)));
    return 0;
}

int test_in_gate_impostor_is_hard_rejected() {
    const AssocParams p = params();
    const Embedding mine = emb({1.F, 0.F});
    const Embedding other = emb({0.F, 1.F}); // cosine dist 1.0 > reject 0.7
    AssocPoint const t{0.5F, 0.5F, &mine};
    AssocPoint const d{0.5F, 0.5F, &other}; // EXACTLY where the track predicts
    // Perfect motion agreement, wrong object: reject (TR-1). This is the
    // crossing impostor standing on my predicted position.
    CHECK(assoc_cost(t, d, p) == ASSOC_REJECT);
    return 0;
}

int test_degenerate_gate_rejects_everything() {
    AssocParams p = params();
    p.gate2 = 0.F;
    AssocPoint const t{0.5F, 0.5F, nullptr};
    AssocPoint const d{0.5F, 0.5F, nullptr};
    CHECK(assoc_cost(t, d, p) == ASSOC_REJECT);
    return 0;
}

int test_aspect_makes_the_gate_isotropic_in_pixels() {
    AssocParams p = params();
    p.aspect = 16.F / 9.F;
    AssocPoint const t{0.5F, 0.5F, nullptr};
    // Same PIXEL offset expressed in each axis' own normalization: a height-
    // normalized dy shrinks by 1/aspect in width units.
    AssocPoint const dx{0.5F + 0.09F, 0.5F, nullptr};
    AssocPoint const dy{0.5F, 0.5F + (0.09F * p.aspect), nullptr};
    const float cx = assoc_cost(t, dx, p);
    const float cy = assoc_cost(t, dy, p);
    CHECK(cx < ASSOC_REJECT && cy < ASSOC_REJECT);
    CHECK(near(cx, cy, 1e-4F));
    return 0;
}

// -------------------------------------------------------- greedy match --

int test_match_trivial_and_empty() {
    const AssocParams p = params();
    AssocPoint const t{0.5F, 0.5F, nullptr};
    AssocPoint const d{0.52F, 0.5F, nullptr};
    const auto cost = assoc_matrix({t}, {d}, p);
    const auto m = assoc_greedy_match(cost, 1, 1);
    CHECK(m.size() == 1 && m[0].first == 0 && m[0].second == 0);
    CHECK(assoc_greedy_match({}, 0, 3).empty());
    CHECK(assoc_greedy_match({}, 3, 0).empty());
    CHECK(assoc_greedy_match({1.F}, 2, 2).empty()); // size mismatch: fail closed
    return 0;
}

int test_match_leaves_rejected_pairs_unmatched() {
    const AssocParams p = params();
    // One det in track 0's gate, the second det far from both tracks.
    const std::vector<AssocPoint> tracks{{0.2F, 0.2F, nullptr}, {0.8F, 0.8F, nullptr}};
    const std::vector<AssocPoint> dets{{0.21F, 0.2F, nullptr}, {0.5F, 0.5F, nullptr}};
    const auto m = assoc_greedy_match(assoc_matrix(tracks, dets, p), 2, 2);
    CHECK(m.size() == 1);
    CHECK(m[0].first == 0 && m[0].second == 0);
    // det 1 stays unmatched (spawn candidate), track 1 stays unmatched (miss).
    return 0;
}

int test_global_greedy_beats_per_track_order() {
    const AssocParams p = params();
    // Track 0 processed first would claim det 0 (in its gate, and track 0's
    // nearest), but track 1 sits strictly closer to det 0. Global greedy
    // commits the cheapest pair (track 1, det 0) first, leaving det 1 for
    // track 0 — both matched; a fixed per-track order would give track 0 the
    // wrong detection.
    const std::vector<AssocPoint> tracks{{0.50F, 0.5F, nullptr}, {0.455F, 0.5F, nullptr}};
    const std::vector<AssocPoint> dets{{0.47F, 0.5F, nullptr}, {0.55F, 0.5F, nullptr}};
    const auto m = assoc_greedy_match(assoc_matrix(tracks, dets, p), 2, 2);
    CHECK(m.size() == 2);
    int det_of_track[2] = {-1, -1};
    for (const auto& pr : m) {
        det_of_track[pr.first] = pr.second;
    }
    CHECK(det_of_track[1] == 0); // track 1 keeps its only candidate
    CHECK(det_of_track[0] == 1);
    return 0;
}

int test_crossing_targets_keep_ids_via_appearance() {
    // THE TR-1 scenario. Two targets cross: both detections are inside both
    // tracks' gates and the swapped pairing is the MOTION-cheaper one. With
    // embeddings, appearance re-ranks and each track keeps its own target.
    AssocParams p = params();
    p.gate2 = 0.04F; // wide gate: everything is in range of everything
    const Embedding red = emb({1.F, 0.F, 0.F});
    const Embedding blue = emb({0.F, 1.F, 0.F});

    // Tracks predicted where they were heading; the targets have just crossed,
    // so each detection now lies CLOSER to the other track's prediction.
    const std::vector<AssocPoint> tracks{{0.48F, 0.5F, &red}, {0.52F, 0.5F, &blue}};
    const std::vector<AssocPoint> dets{{0.53F, 0.5F, &red}, {0.47F, 0.5F, &blue}};

    // Sanity: motion alone really does prefer the swap.
    const AssocParams motion_only{p.gate2, p.aspect, 0.F, 2.F};
    std::vector<AssocPoint> bare_t = tracks;
    std::vector<AssocPoint> bare_d = dets;
    for (auto& a : bare_t) {
        a.emb = nullptr;
    }
    for (auto& a : bare_d) {
        a.emb = nullptr;
    }
    const auto swapped =
        assoc_greedy_match(assoc_matrix(bare_t, bare_d, motion_only), 2, 2);
    CHECK(swapped.size() == 2);
    for (const auto& pr : swapped) {
        CHECK(pr.first != pr.second); // motion-only: IDs switch
    }

    // With appearance the impostor pairings are hard-rejected (cosine dist 1.0
    // between red and blue > 0.7) and each track keeps its own target.
    const auto kept = assoc_greedy_match(assoc_matrix(tracks, dets, p), 2, 2);
    CHECK(kept.size() == 2);
    for (const auto& pr : kept) {
        CHECK(pr.first == pr.second); // IDs preserved
    }
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_normalize_produces_unit_norm();
    rc = rc != 0 ? rc : test_normalize_rejects_garbage();
    rc = rc != 0 ? rc : test_cosine_dist_identical_orthogonal_opposite();
    rc = rc != 0 ? rc : test_cosine_dist_incomparable_pairs();
    rc = rc != 0 ? rc : test_gallery_adopts_first_then_smooths_and_stays_unit();
    rc = rc != 0 ? rc : test_gate_is_authoritative_over_appearance();
    rc = rc != 0 ? rc : test_motion_only_when_embeddings_missing_or_garbage();
    rc = rc != 0 ? rc : test_appearance_reranks_inside_the_gate();
    rc = rc != 0 ? rc : test_in_gate_impostor_is_hard_rejected();
    rc = rc != 0 ? rc : test_degenerate_gate_rejects_everything();
    rc = rc != 0 ? rc : test_aspect_makes_the_gate_isotropic_in_pixels();
    rc = rc != 0 ? rc : test_match_trivial_and_empty();
    rc = rc != 0 ? rc : test_match_leaves_rejected_pairs_unmatched();
    rc = rc != 0 ? rc : test_global_greedy_beats_per_track_order();
    rc = rc != 0 ? rc : test_crossing_targets_keep_ids_via_appearance();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_assoc: %d checks passed\n", checks);
    return 0;
}
