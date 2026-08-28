#include "Tracker.h"

#include "AssocCost.h"
#include "IDetector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "riposte/Tunables.h"

namespace riposte {

namespace {
// TRACKER_GATE_PX is defined on this reference sensor width; the gate is thus a
// constant FRACTION of image width (~ constant fraction of horizontal FOV) on
// other sensors rather than an absolute pixel count.
constexpr float GATE_REF_WIDTH_PX = 1280.0F;

// Squared distance in width-normalized units: y (height-normalized) is scaled
// by 1/aspect so the metric is isotropic in pixels for any sensor shape.
float dist2(float ax, float ay, float bx, float by, float aspect) {
    const float dx = ax - bx;
    const float dy = (ay - by) / aspect;
    return (dx * dx) + (dy * dy);
}
// Association gate radius squared, in width-normalized units (see dist2).
float gate_norm2() {
    const float g = tun::TRACKER_GATE_PX / GATE_REF_WIDTH_PX;
    return g * g;
}
} // namespace

void Tracker::acquire(Track& t, const Detection& d, uint32_t id) const {
    t = Track{};
    t.id = id;
    t.cx = d.cx;
    t.cy = d.cy;
    // Width-normalized size: d.w is width-normalized but d.h is height-
    // normalized, so h converts via /aspect (matches the estimator's
    // width-based focal length).
    t.size = std::max(d.w, d.h / aspect_);
    t.hits = 1;
    t.valid = (t.hits >= tun::MIN_TRACK_HITS); // one hit: not yet confirmed
    t.misses = 0;
    t.quality = std::min(1.0F, 0.5F * d.score); // fresh track: 0.5*0 + 0.5*score
}

void Tracker::set_measurement_gain_scale(float scale) {
    // Only ever increases trust, and only up to the ceiling: R-12 exists to use
    // the narrow channel's finer resolution, not to let a config value retune
    // the filter.
    gain_scale_ = std::clamp(scale, 1.F, tun::NARROW_GAIN_SCALE);
}

void Tracker::smooth(Track& t, const Detection& d, double dt_s) const {
    const float a = std::min(tun::TRACKER_ALPHA * gain_scale_, tun::TRACKER_ALPHA_MAX);
    const float b = std::min(tun::TRACKER_BETA * gain_scale_, tun::TRACKER_BETA_MAX);
    const float rx = d.cx - t.cx;
    const float ry = d.cy - t.cy;
    t.cx += a * rx;
    t.cy += a * ry;
    if (dt_s > 1e-3) {
        t.vx += b * rx; // alpha-beta: residual feeds velocity
        t.vy += b * ry;
    }
    t.size += a * (std::max(d.w, d.h / aspect_) - t.size);
    ++t.hits;
    t.valid = (t.hits >= tun::MIN_TRACK_HITS); // confirmed once seen enough
    t.misses = 0;
    t.quality = std::min(1.0F, (0.5F * t.quality) + (0.5F * d.score));
}

const Detection* Tracker::claim_best(const Track& t, std::vector<const Detection*>& pool,
                                     float gate2) const {
    const Detection** best = nullptr; // slot holding the winner (so we can null it)
    float best_score = -1.F;
    for (auto& slot : pool) {
        if (slot == nullptr) {
            continue; // already claimed by a higher-priority track
        }
        if (dist2(slot->cx, slot->cy, t.cx, t.cy, aspect_) > gate2) {
            continue;
        }
        if (slot->score > best_score) {
            best_score = slot->score;
            best = &slot;
        }
    }
    if (best == nullptr) {
        return nullptr;
    }
    const Detection* d = *best;
    *best = nullptr; // claim it
    return d;
}

void Tracker::spawn(const Detection& d, const Embedding* e) {
    if (tracks_.size() >= static_cast<std::size_t>(tun::TRACKER_MAX_TRACKS)) {
        return; // capacity reached; ignore extra targets
    }
    Track t;
    acquire(t, d, next_id_++);
    if (e != nullptr && e->valid()) {
        t.emb = *e; // seed the gallery from the spawning detection
    }
    tracks_.push_back(t);
}

const Tracker::Track& Tracker::primary() const {
    for (const auto& t : tracks_) {
        if (t.id == primary_id_) {
            return t;
        }
    }
    return invalid_;
}

void Tracker::reprioritize() {
    const Track* cur = nullptr;
    for (const auto& t : tracks_) {
        if (t.id == primary_id_) {
            cur = &t;
            break;
        }
    }
    // Sticky only AFTER the control session has committed to this target: once
    // committed, a momentarily better-looking neighbour must not steal it. Before
    // that the choice is still open every frame, which is what lets the LARGEST
    // target win when several appear over consecutive frames rather than
    // whichever one happened to confirm first.
    if (cur != nullptr && primary_locked_) {
        return;
    }
    const Track* best = nullptr;
    for (const auto& t : tracks_) {
        if (!t.valid) {
            continue; // unconfirmed tracks are not primary-eligible
        }
        // Largest apparent size first (REQ-001 R-8): for targets of comparable
        // physical size the biggest bbox is the nearest one, so this is "deal
        // with the closest threat first". Quality breaks ties, which keeps the
        // single-target and equal-size behaviour identical to before.
        if (best == nullptr || t.size > best->size ||
            (t.size == best->size && t.quality > best->quality)) {
            best = &t;
        }
    }
    // Pre-lock hysteresis: a live incumbent yields only to a challenger that is
    // DECISIVELY larger. Without the margin, two similar-size tracks flip rank
    // on per-frame box noise, and every flip restarts the scheduler's
    // confirmation window (TR-7 identity binding) — with both targets alive,
    // confirmation then never completes and nothing is ever published.
    if (cur != nullptr && best != nullptr && best->id != cur->id &&
        best->size < cur->size * tun::TRACKER_PRIMARY_SWITCH_MARGIN) {
        return; // incumbent stands: challenger not decisively larger (R-8)
    }
    primary_id_ = (best != nullptr) ? best->id : 0;
}

std::size_t Tracker::confirmed_count() const {
    return static_cast<std::size_t>(std::count_if(
        tracks_.begin(), tracks_.end(), [](const Track& t) { return t.valid; }));
}

const Tracker::Track& Tracker::update(const std::vector<Detection>& dets, double dt_s) {
    static const std::vector<Embedding> NO_EMBEDDINGS;
    return update(dets, NO_EMBEDDINGS, dt_s);
}

// G16.6 deviation: the associate/gate/spawn/prune sequence is one data flow per frame
// (G16.6) NOLINTNEXTLINE(readability-function-size)
const Tracker::Track& Tracker::update(const std::vector<Detection>& dets,
                                      const std::vector<Embedding>& embs, double dt_s) {
    const float gate2 = gate_norm2();

    // 1) Predict every track forward (constant velocity) so gating uses the
    //    expected position.
    for (auto& t : tracks_) {
        t.cx += t.vx;
        t.cy += t.vy;
    }

    // 2) Candidate pool: this frame's target-class detections, as indices into
    //    `dets` (so a candidate can reach its aligned embedding) plus a
    //    claimable pointer view for the legacy path and the spawn stage.
    std::vector<int> cand;
    cand.reserve(dets.size());
    for (std::size_t i = 0; i < dets.size(); ++i) {
        if (dets[i].cls == target_cls_) {
            cand.push_back(static_cast<int>(i));
        }
    }
    std::vector<const Detection*> pool;
    pool.reserve(cand.size());
    for (const int idx : cand) {
        pool.push_back(&dets[idx]);
    }

    // T1 eligibility (TR-B): embeddings aligned with dets AND at least one
    // candidate actually carries one. Anything else — no embedder, device
    // fault, misaligned call — takes the legacy path unchanged (TR-3).
    bool use_t1 = embs.size() == dets.size();
    if (use_t1) {
        use_t1 = std::any_of(cand.begin(), cand.end(),
                             [&](int idx) { return embs[idx].valid(); });
    }

    if (use_t1) {
        // 3/4-T1) Fused-cost association (motion gate + appearance), matched
        // globally by ascending cost — the gate is the same gate2/dist2 the
        // legacy path uses, so T0 and T1 agree on which pairings exist at all.
        AssocParams ap;
        ap.gate2 = gate2;
        ap.aspect = aspect_;
        ap.appearance_weight = tun::ASSOC_APPEARANCE_WEIGHT;
        ap.appearance_reject = tun::ASSOC_APPEARANCE_REJECT;
        std::vector<AssocPoint> tps;
        tps.reserve(tracks_.size());
        for (const auto& t : tracks_) {
            tps.push_back(AssocPoint{t.cx, t.cy, &t.emb});
        }
        std::vector<AssocPoint> dps;
        dps.reserve(cand.size());
        for (const int idx : cand) {
            dps.push_back(AssocPoint{dets[idx].cx, dets[idx].cy, &embs[idx]});
        }
        const auto matches =
            assoc_greedy_match(assoc_matrix(tps, dps, ap), tps.size(), dps.size());
        std::vector<bool> matched(tracks_.size(), false);
        for (const auto& m : matches) {
            Track& t = tracks_[static_cast<std::size_t>(m.first)];
            const int idx = cand[static_cast<std::size_t>(m.second)];
            smooth(t, dets[idx], dt_s);
            update_embedding(t.emb, embs[idx], tun::ASSOC_EMBED_EMA_ALPHA);
            matched[static_cast<std::size_t>(m.first)] = true;
            pool[static_cast<std::size_t>(m.second)] = nullptr; // claimed
        }
        for (std::size_t i = 0; i < tracks_.size(); ++i) {
            Track& t = tracks_[i];
            if (!matched[i] && ++t.misses <= tun::TRACKER_MAX_MISSES) {
                t.quality *= 0.7F; // coast: decay confidence while unmatched
            }
        }
    } else {
        // 3) Association order: primary first (the engaged target keeps its
        //    pick), then remaining tracks by descending quality.
        std::vector<Track*> order;
        order.reserve(tracks_.size());
        for (auto& t : tracks_) {
            order.push_back(&t);
        }
        std::sort(order.begin(), order.end(), [&](const Track* a, const Track* b) {
            const bool pa = a->id == primary_id_;
            const bool pb = b->id == primary_id_;
            return (pa != pb) ? pa : (a->quality > b->quality);
        });

        // 4) Per-track association: highest-score in-gate detection.
        for (Track* t : order) {
            const Detection* d = claim_best(*t, pool, gate2);
            if (d != nullptr) {
                smooth(*t, *d, dt_s);
            } else if (++t->misses <= tun::TRACKER_MAX_MISSES) {
                t->quality *= 0.7F; // coast: decay confidence while unmatched
            }
        }
    }

    // 5) Drop dead tracks (misses budget exceeded).
    tracks_.erase(
        std::remove_if(tracks_.begin(), tracks_.end(),
                       [](const Track& t) { return t.misses > tun::TRACKER_MAX_MISSES; }),
        tracks_.end());

    // 6) Spawn: an unclaimed detection outside EVERY surviving track's gate is a
    //    new target (duplicate detections of an existing target stay in-gate).
    //    With T1 active the new track's gallery is seeded from the detection.
    for (std::size_t k = 0; k < pool.size(); ++k) {
        const Detection* d = pool[k];
        if (d == nullptr) {
            continue; // claimed by an existing track
        }
        const bool near_existing =
            std::any_of(tracks_.begin(), tracks_.end(), [&](const Track& t) {
                return dist2(d->cx, d->cy, t.cx, t.cy, aspect_) <= gate2;
            });
        if (!near_existing) {
            spawn(*d, use_t1 ? &embs[cand[k]] : nullptr);
        }
    }

    // 7) Primary: sticky while alive, else highest-quality survivor.
    reprioritize();
    return primary();
}

} // namespace riposte
