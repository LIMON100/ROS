#pragma once
#include "AssocCost.h" // Embedding (T1 gallery)
#include "IDetector.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace riposte {

// Multi-target tracker: greedy per-track association (highest-score in-gate
// detection) in normalized image space + alpha-beta smoothing of center and
// pixel velocity. Keeps up to TRACKER_MAX_TRACKS concurrent tracks; a detection
// outside EVERY existing track's gate spawns a new track. A track becomes
// publish-eligible (valid) only after MIN_TRACK_HITS associated detections, so
// a one-frame glint never becomes the engaged target.
//
// One track is designated the PRIMARY (the engaged target), chosen as the
// LARGEST apparent target — for targets of comparable physical size that is the
// nearest one (REQ-001 R-8), with quality breaking ties. The choice stays open
// frame to frame until lock_primary(true) commits it; after that it is STICKY,
// so a momentarily better-looking neighbour never steals a committed
// control session. When the primary dies the choice reopens. current()/update()
// return the primary; TrackBus therefore stays single-target (backward
// compatible), while tracks() exposes the full set and confirmed_count() the
// number of targets.
class Tracker {
public:
    struct Track {
        bool valid = false; // CONFIRMED (hits >= MIN_TRACK_HITS): publish-eligible
        uint32_t id = 0;
        float cx = 0.5F, cy = 0.5F; // smoothed center (normalized)
        float vx = 0.F, vy = 0.F;   // normalized/frame
        float size = 0.F;           // smoothed bbox size, width-normalized units
        float quality = 0.F;        // 0..1
        int hits = 0;               // frames with an associated detection
        int misses = 0;
        // T1 appearance gallery (EMA of matched detection embeddings). Invalid
        // until the first embedded match; never required — see update() below.
        Embedding emb;
    };

    // aspect = image width/height (negotiated, not configured). Detection y/h
    // are height-normalized while x/w are width-normalized; the aspect converts
    // both into width-normalized units so gating is isotropic in pixels and
    // `size` is consistent with the estimator's width-based focal length.
    // target_cls is the detector class id to track; everything else is ignored.
    // It is configurable because the balloon test flights (REQ-001 T-2) run a
    // model whose target class is not the drone class.
    explicit Tracker(float aspect = 1.0F, int target_cls = 0)
        : aspect_(aspect), target_cls_(target_cls) {}

    // Advances all tracks using this frame's detections. dt_s is the interval
    // since the previous update (for velocity scaling / prediction). Returns the
    // primary track (an invalid Track if none is alive and confirmed).
    const Track& update(const std::vector<Detection>& dets, double dt_s);

    // T1 variant (TRACKER-REQ TR-B): `embs` is aligned with `dets` by index
    // (one embedding per detection, entries may be invalid). When at least one
    // candidate detection carries a valid embedding, association switches to
    // the fused cost (motion gate + appearance, global greedy matching) and
    // matched tracks update their appearance gallery. Empty or misaligned
    // `embs` — or none valid — runs the legacy motion/score path unchanged:
    // the embedder can only ever improve association, never break it (TR-3).
    const Track& update(const std::vector<Detection>& dets,
                        const std::vector<Embedding>& embs, double dt_s);

    // R-12 (S-15): how much to trust THIS frame's measurements relative to the
    // tuned baseline. The narrow channel resolves ~3.8x finer, so its samples
    // earn a larger alpha-beta gain and move the estimate further; wide samples
    // keep the flight-tuned values. Set before update(); clamped to [1, max] so
    // a configuration mistake cannot drive the filter unstable, and 1.0 (the
    // single-camera case) reproduces the previous behaviour exactly.
    void set_measurement_gain_scale(float scale);

    const Track& current() const { return primary(); }

    // Full track set (after update(); includes not-yet-confirmed tracks with
    // valid=false). For multi-target consumers, threat prioritisation, logging.
    const std::vector<Track>& tracks() const { return tracks_; }

    // Number of CONFIRMED tracks — the target count published on TrackBus and
    // logged (REQ-001 R-8). Unconfirmed tracks are excluded: a one-frame glint
    // is not a target the operator should see counted.
    std::size_t confirmed_count() const;

    // Commit the primary. While UNLOCKED the primary is re-chosen every frame
    // (largest apparent target wins), so a bigger target appearing a frame or
    // two after a smaller one still gets engaged first. Once LOCKED — which the
    // seeker does when the search scheduler confirms the target — the primary is
    // sticky until it dies. Locking is what turns "pick the biggest" into "and
    // then stay on it".
    void lock_primary(bool locked) { primary_locked_ = locked; }
    bool primary_locked() const { return primary_locked_; }

private:
    const Track& primary() const;
    void spawn(const Detection& d, const Embedding* e = nullptr);
    void reprioritize(); // re-choose primary if the current one died
    void acquire(Track& t, const Detection& d, uint32_t id) const;
    void smooth(Track& t, const Detection& d, double dt_s) const;
    float gain_scale_ = 1.F; // R-12 per-frame measurement trust
    // Highest-score in-gate detection for t among the still-unclaimed pool
    // entries; claims it (nulls its slot) and returns it, or nullptr if none.
    const Detection* claim_best(const Track& t, std::vector<const Detection*>& pool,
                                float gate2) const;

    float aspect_ = 1.0F; // image W/H
    int target_cls_ = 0;  // detector class id treated as the target
    std::vector<Track> tracks_;
    uint32_t next_id_ = 1;
    uint32_t primary_id_ = 0;     // 0 = none
    bool primary_locked_ = false; // see lock_primary()
    Track invalid_{};             // returned when no track is alive
};

} // namespace riposte
