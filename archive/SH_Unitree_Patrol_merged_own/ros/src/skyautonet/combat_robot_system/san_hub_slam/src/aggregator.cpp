#include "san_hub_slam/aggregator.hpp"

#include <algorithm>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>

namespace san_hub_slam {

namespace {
// Static-analysis hardening: clamp incoming geometry so a misconfigured
// parameter (negative or absurdly large width/height) can't allocate a
// pathological vector. UBSAN would flag the signed-int multiplication;
// the runtime cost is one branch.
constexpr int kMaxAxisCells = 4096;   // 4096×4096 8-bit grid = 16 MiB

inline std::size_t safeCellCount(int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    if (width > kMaxAxisCells || height > kMaxAxisCells) return 0;
    return static_cast<std::size_t>(width) *
           static_cast<std::size_t>(height);
}
}  // namespace

MultirobotAggregator::MultirobotAggregator(int width, int height,
                                            float resolution_m)
    : width_(std::max(0, width)),
      height_(std::max(0, height)),
      resolution_m_(resolution_m),
      global_(safeCellCount(width, height), GLOBAL_UNKNOWN),
      free_votes_(safeCellCount(width, height), 0),
      occupied_votes_(safeCellCount(width, height), 0)
{}

void MultirobotAggregator::setGeometry(int width, int height,
                                        float resolution_m)
{
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    resolution_m_ = resolution_m;
    const auto n = safeCellCount(width, height);
    global_.assign(n, GLOBAL_UNKNOWN);
    // [DCN-2026-006 EXT D-021] Reset vote tallies on geometry change.
    free_votes_.assign(n, 0);
    occupied_votes_.assign(n, 0);
    contributing_.clear();
    mismatch_cell_count_     = 0;
    contributing_cell_count_ = 0;
}

void MultirobotAggregator::clear() {
    std::fill(global_.begin(), global_.end(), GLOBAL_UNKNOWN);
    std::fill(free_votes_.begin(),     free_votes_.end(),     0);
    std::fill(occupied_votes_.begin(), occupied_votes_.end(), 0);
    contributing_.clear();
    mismatch_cell_count_     = 0;
    contributing_cell_count_ = 0;
}

bool MultirobotAggregator::applyDelta(const std::string& robot_id,
                                      const std::vector<uint8_t>& png_bytes)
{
    if (png_bytes.empty()) return false;
    cv::Mat encoded(1, static_cast<int>(png_bytes.size()), CV_8UC1,
                    const_cast<uint8_t*>(png_bytes.data()));
    cv::Mat decoded = cv::imdecode(encoded, cv::IMREAD_GRAYSCALE);
    if (decoded.empty()) return false;
    if (decoded.cols != width_ || decoded.rows != height_) {
        return false;
    }
    if (!decoded.isContinuous()) {
        decoded = decoded.clone();
    }
    const std::size_t expected =
        static_cast<std::size_t>(width_) *
        static_cast<std::size_t>(height_);
    if (decoded.total() != expected) return false;
    std::vector<uint8_t> grid(decoded.data, decoded.data + expected);
    return applyDeltaRaw(robot_id, grid);
}

bool MultirobotAggregator::applyDeltaRaw(const std::string& robot_id,
                                          const std::vector<uint8_t>& grid)
{
    if (grid.size() != global_.size()) return false;
    contributing_.insert(robot_id);

    // [DCN-2026-006 EXT D-021] Vote-based merge — no last-write-wins.
    //
    // Each robot's delta increments either free_votes_ or
    // occupied_votes_ for every non-unknown cell it reports. The
    // master grid itself is NOT updated here; the publish path
    // (recomputeGlobal) translates the votes into the master grid
    // by majority threshold. This makes the merge order-independent
    // and lets us surface per-cell disagreement (D-026).
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const uint8_t v = grid[i];
        if (v == GLOBAL_UNKNOWN) continue;
        if (v == GLOBAL_FREE) {
            // Saturating increment guards against the absurd long-run
            // case where a single robot publishes for hours; uint16_t
            // saturates at 65535 which we never expect to hit (4
            // robots × 12 publishes/min × 60 min/hr × 24 hr ≈ 70k —
            // boundary). Caller should call clear() periodically.
            if (free_votes_[i] < UINT16_MAX) free_votes_[i]++;
        } else if (v == GLOBAL_OCCUPIED) {
            if (occupied_votes_[i] < UINT16_MAX) occupied_votes_[i]++;
        }
        // Any other value (legacy intermediate confidences) is
        // treated as unknown — strict three-state contract.
    }
    return true;
}

// [DCN-2026-006 EXT D-021] Vote → master grid translation.
//
// Bayesian-style majority vote per cell:
//   - both votes 0     → UNKNOWN
//   - free  > occupied → FREE
//   - occupied > free  → OCCUPIED
//   - tie (rare)       → preserve previous master to avoid flicker;
//                        UNKNOWN if no previous data.
//
// Also tallies the disagreement metric (D-026): cells where both
// counters are > 0 are counted as "mismatched" — operator-visible
// signal that two or more robots saw the same cell differently.
void MultirobotAggregator::recomputeGlobal() {
    std::size_t mismatched     = 0;
    std::size_t contributing_n = 0;
    for (std::size_t i = 0; i < global_.size(); ++i) {
        const uint16_t f = free_votes_[i];
        const uint16_t o = occupied_votes_[i];
        if (f == 0 && o == 0) {
            global_[i] = GLOBAL_UNKNOWN;
            continue;
        }
        contributing_n++;
        if (f > 0 && o > 0) mismatched++;
        if      (f > o)  global_[i] = GLOBAL_FREE;
        else if (o > f)  global_[i] = GLOBAL_OCCUPIED;
        // tie → leave global_[i] unchanged (anti-flicker).
    }
    mismatch_cell_count_     = mismatched;
    contributing_cell_count_ = contributing_n;
}

std::vector<uint8_t> MultirobotAggregator::encodeGlobalPng() {
    recomputeGlobal();   // [D-021] always publish the latest vote outcome
    return encodePng(global_, width_, height_);
}

MultirobotAggregator::GridSnapshot
MultirobotAggregator::snapshot() {
    recomputeGlobal();   // [D-021]
    GridSnapshot s;
    s.grid                = global_;          // copy
    s.width               = width_;
    s.height              = height_;
    s.resolution_m        = resolution_m_;
    s.contributing_robots = contributing_.size();
    s.mismatch_cells      = mismatch_cell_count_;    // D-026
    s.contributing_cells  = contributing_cell_count_;
    return s;
}

std::vector<uint8_t> MultirobotAggregator::encodePng(
    const std::vector<uint8_t>& grid, int width, int height) {
    if (static_cast<std::size_t>(width) * static_cast<std::size_t>(height)
        != grid.size()) {
        return {};
    }
    cv::Mat img(height, width, CV_8UC1,
                 const_cast<uint8_t*>(grid.data()));
    std::vector<uint8_t> buf;
    cv::imencode(".png", img, buf);
    return buf;
}

}  // namespace san_hub_slam
