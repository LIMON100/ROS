#pragma once
#include "AssocCost.h" // Embedding
#include "IDetector.h" // Frame, Detection

#include <cstdint>
#include <vector>

namespace riposte {

// T1 appearance-embedder boundary (RIPOSTE-TRACKER-REQ-001 TR-A). RknnEmbedder
// wraps the RK3588 NPU on the target; SyntheticEmbedder stands in for SIL. An
// embedder can only ever IMPROVE association: any failure — device fault,
// single bad crop, missing model — surfaces as invalid embeddings, and the
// association cost degrades to motion-only, byte for byte the T0 behaviour
// (TR-3). Nothing downstream may treat a missing embedding as an error.
class IEmbedder {
public:
    IEmbedder() = default;
    virtual ~IEmbedder() = default;
    IEmbedder(const IEmbedder&) = delete;
    IEmbedder& operator=(const IEmbedder&) = delete;
    IEmbedder(IEmbedder&&) = delete;
    IEmbedder& operator=(IEmbedder&&) = delete;

    virtual bool init() = 0;
    // One embedding per detection, aligned by index (out.size() == dets.size()
    // on success); individual entries may be invalid (empty) when that crop
    // failed. Returns false only on a device-level fault — out is cleared and
    // the frame proceeds motion-only.
    virtual bool embed(const Frame& f, const std::vector<Detection>& dets,
                       std::vector<Embedding>& out) = 0;
    virtual bool healthy() const = 0;
    virtual const char* name() const = 0;
};

// SIL stand-in: a deterministic unit vector seeded from the detection's CLASS
// and coarsely-quantized POSITION. Good enough to exercise the plumbing
// (alignment, fallback, timing) — NOT for ID-retention benchmarks: identity
// follows position, so crossing targets exchange "appearance" at exactly the
// moment a real embedder would tell them apart. The K-1 bench needs the real
// model (or a gz-bridge identity channel).
class SyntheticEmbedder final : public IEmbedder {
public:
    bool init() override { return true; }

    bool embed(const Frame& /*f*/, const std::vector<Detection>& dets,
               std::vector<Embedding>& out) override {
        out.clear();
        out.reserve(dets.size());
        for (const auto& d : dets) {
            out.push_back(make(d));
        }
        return true;
    }

    bool healthy() const override { return true; }
    const char* name() const override { return "SyntheticEmbedder"; }

private:
    static Embedding make(const Detection& d) {
        // Coarse grid so small frame-to-frame motion keeps the seed stable.
        constexpr float GRID = 4.F;
        const auto qx = static_cast<uint32_t>(d.cx * GRID);
        const auto qy = static_cast<uint32_t>(d.cy * GRID);
        uint32_t seed = (static_cast<uint32_t>(d.cls) * 92821U) ^ (qx * 486187739U) ^
                        (qy * 2654435761U);
        Embedding e;
        e.v.resize(16);
        for (float& x : e.v) {
            seed = (seed * 1664525U) + 1013904223U; // LCG: deterministic per seed
            x = (static_cast<float>(seed >> 8U) / 8388608.F) - 1.F; // [-1, 1)
        }
        if (!l2_normalize(e.v)) {
            e.v.clear(); // astronomically unlikely all-zero draw: invalid
        }
        return e;
    }
};

} // namespace riposte
