#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace riposte {

// One detection in image space. Normalized [0,1] coords so downstream geometry
// is independent of sensor resolution.
struct Detection {
    float cx, cy; // bbox center, normalized
    float w, h;   // bbox size, normalized
    float score;  // 0..1
    int cls;      // class id (0 = target drone)
};

// Normalized sub-rectangle of a frame; (x, y) is the TOP-LEFT corner and all
// four fields are in [0,1] against their own axis (x/w against width, y/h
// against height) — the same split convention Detection uses.
struct Roi {
    float x = 0.F, y = 0.F, w = 1.F, h = 1.F;
    bool is_full() const { return x <= 0.F && y <= 0.F && w >= 1.F && h >= 1.F; }
};

struct Frame {
    uint64_t mono_ns{}; // capture timestamp (freshness basis)
    int width{};
    int height{};
    const uint8_t* data{}; // borrowed; valid only during detect() call
    size_t stride{};
    uint32_t fourcc{}; // pixel format (e.g. NV12)
    // Which part of the SENSOR frame these pixels are. A whole-frame pass leaves
    // it full; a tiled/ROI pass carries the crop's placement. Real detectors
    // ignore it (they see only the pixels they were handed, and the caller
    // remaps their output) — it exists so a frame carries its own provenance,
    // which the SIL detector needs to place a target faithfully.
    Roi src_roi;
};

// Inference boundary. HailoDetector wraps HailoRT on the target; SyntheticDetector
// fabricates detections for SIL. A detector fault is a local seeker fault and must
// never reach the flight-control path except via TrackBus staleness (SM-7).
class IDetector {
public:
    IDetector() = default;
    virtual ~IDetector() = default;
    IDetector(const IDetector&) = delete;
    IDetector& operator=(const IDetector&) = delete;
    IDetector(IDetector&&) = delete;
    IDetector& operator=(IDetector&&) = delete;
    virtual bool init() = 0;
    virtual bool detect(const Frame& f, std::vector<Detection>& out) = 0;
    virtual bool healthy() const = 0;
    virtual const char* name() const = 0;
};

} // namespace riposte
