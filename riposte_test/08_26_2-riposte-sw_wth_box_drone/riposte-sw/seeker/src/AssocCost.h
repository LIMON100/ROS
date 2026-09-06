#pragma once
#include <cstddef>
#include <utility>
#include <vector>

namespace riposte {

// Association cost fusion for the T1 ReID layer (RIPOSTE-TRACKER-REQ-001 TR-B):
// motion gate + appearance distance combined into one cost matrix, plus the
// greedy matcher that consumes it.
//
// This is the HOST-TESTED half of TR-B. It knows nothing about the RK NPU —
// embeddings arrive as plain float vectors from whatever produced them (the
// RKNN ReID net on the target, synthetic vectors in SIL/tests), the same
// isolation the detector keeps between ModelIo and HailoRT.
//
// Design rules (TR-1/TR-3/TR-6):
//  - The MOTION GATE is authoritative: appearance can never rescue a pairing
//    the gate rejected. Appearance only re-ranks candidates INSIDE the gate,
//    and hard-rejects an in-gate impostor whose appearance is clearly wrong
//    (the crossing-targets ID switch, TR-1).
//  - Missing/invalid/mismatched embeddings degrade to motion-only cost — the
//    T0 behaviour, byte for byte (TR-3). Garbage embeddings (NaN, zero norm)
//    count as missing, never as evidence.

// L2-normalized appearance vector. Dimension is model-dependent and carried at
// runtime; an empty vector means "no embedding" (degrade to motion-only).
struct Embedding {
    std::vector<float> v;
    bool valid() const { return !v.empty(); }
};

// Normalizes in place to unit L2 norm. Returns false (leaving the vector
// UNTOUCHED but the caller must treat it as invalid) when the norm is zero,
// non-finite, or any element is non-finite — device output is untrusted.
bool l2_normalize(std::vector<float>& v);

// Cosine distance in [0, 2] between two L2-normalized embeddings. Returns
// false when the pair is not comparable (either invalid, dimension mismatch,
// or a non-finite result) — the caller falls back to motion-only cost.
bool cosine_dist(const Embedding& a, const Embedding& b, float& out);

// EMA update of a track's gallery embedding toward a fresh detection
// embedding, re-normalized after the blend (an unnormalized gallery would make
// cosine distances drift). First valid observation is copied as-is. A gallery
// smooths over single-frame appearance noise (motion blur, partial occlusion)
// instead of trusting the newest crop alone.
void update_embedding(Embedding& gallery, const Embedding& fresh, float alpha);

struct AssocParams {
    // Motion gate radius squared, width-normalized units — the SAME quantity
    // Tracker's dist2/gate uses, so T0 and T1 gate identically.
    float gate2 = 0.F;
    float aspect = 1.F; // image W/H, for the isotropic metric
    // Appearance term weight: cost = motion_dist2/gate2 + weight * cosine_dist.
    float appearance_weight = 0.5F;
    // In-gate hard reject: both embeddings valid and cosine_dist above this
    // means "different object standing where mine should be" (TR-1).
    float appearance_reject = 0.7F;
};

// Sentinel for "pairing not allowed"; compares greater than every real cost.
extern const float ASSOC_REJECT;

// One side of a candidate pairing: predicted track position (or detection
// position) in normalized image coordinates, plus its embedding (may be null
// or invalid — motion-only fallback).
struct AssocPoint {
    float cx = 0.F;
    float cy = 0.F;
    const Embedding* emb = nullptr;
};

// Cost of pairing `track` with `det` under `p`. ASSOC_REJECT outside the
// motion gate or on appearance hard-reject; otherwise
// motion_dist2/gate2 (in [0,1]) + weight * cosine_dist, motion-only when the
// embeddings are not comparable.
float assoc_cost(const AssocPoint& track, const AssocPoint& det, const AssocParams& p);

// Row-major cost matrix: cost(track i, det j) at [i * dets.size() + j].
std::vector<float> assoc_matrix(const std::vector<AssocPoint>& tracks,
                                const std::vector<AssocPoint>& dets,
                                const AssocParams& p);

// Greedy global minimum-cost matching: repeatedly commit the cheapest
// remaining (track, det) pair until only ASSOC_REJECT entries remain. Globally
// greedy — unlike a fixed per-track claiming order it cannot hand track A its
// neighbour's detection just because A was processed first, which is exactly
// the crossing case TR-1 exists for. Optimal assignment (Hungarian) is
// DEFERRED until a bench shows greedy losing real pairings.
// Returns (track_index, det_index) pairs; unmatched detections are the
// caller's spawn candidates, unmatched tracks its misses.
std::vector<std::pair<int, int>> assoc_greedy_match(const std::vector<float>& cost,
                                                    std::size_t num_tracks,
                                                    std::size_t num_dets);

} // namespace riposte
