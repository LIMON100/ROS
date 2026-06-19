// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 3 - global occupancy-grid aggregator.
//
// Maintains an 8-bit master grid (0 = free, 127 = unknown,
// 255 = occupied). Per-robot deltas land in via applyDelta().
//
// [DCN-2026-006 EXT D-021 + D-026]
//   v1.5.2 introduces Bayesian-style voting in place of the previous
//   last-write-wins merge. Each cell now keeps two counters
//   (free_votes, occupied_votes) that are bumped per incoming robot
//   delta; the master grid is recomputed at publish time by
//   thresholding the vote counts. This eliminates the v1.5.1
//   pathology where one robot's stale occupied sample would
//   silently override three robots' fresh free samples (last-write-
//   wins is order-dependent). It also exposes a "disagreement"
//   metric — count of cells where free_votes and occupied_votes
//   are both > 0 — which hub_slam_node publishes on
//   /diagnostics/hub_slam_audit for operator visibility (D-026).
//
// Pure C++ class - no rclcpp / lifecycle plumbing - so unit tests
// can drive it directly with hand-built PNG payloads.

#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace san_hub_slam
{

constexpr uint8_t GLOBAL_FREE = 0;
constexpr uint8_t GLOBAL_UNKNOWN = 127;
constexpr uint8_t GLOBAL_OCCUPIED = 255;

class MultirobotAggregator
{
public:
  MultirobotAggregator(
    int width = 280, int height = 280,
    float resolution_m = 0.05f);

  void setGeometry(int width, int height, float resolution_m);
  void clear();

  int width() const {return width_;}
  int height() const {return height_;}
  float resolution_m() const {return resolution_m_;}
  const std::vector<uint8_t> & globalGrid() const {return global_;}
  std::size_t contributingRobotCount() const
  {
    return contributing_.size();
  }

  // [DCN-2026-006 EXT D-026] Mismatch (disagreement) counters.
  //
  // mismatch_cell_count_ — set by the last recomputeGlobal(): how
  //   many cells in the master grid have both free_votes > 0 AND
  //   occupied_votes > 0. A non-zero value with N robots is
  //   inevitable on cells near robot boundaries; a sustained
  //   high value (>5% of contributing cells) indicates either
  //   alignment drift between robot SLAM frames or a clock-skew
  //   stale-sample bug — both worth surfacing to the operator.
  std::size_t mismatchCellCount() const {return mismatch_cell_count_;}
  std::size_t contributingCellCount() const
  {
    return contributing_cell_count_;
  }

  // [DCN-2026-006 EXT D-021] Recompute master grid from current vote
  // tallies. Called by hub_slam_node before each publish. Safe to
  // call repeatedly; idempotent.
  void recomputeGlobal();

  // Apply a per-robot delta in the PNG-encoded format produced by
  // san_slam. Cells whose pixel value is `DELTA_NO_CHANGE` are
  // skipped; FREE / OCCUPIED cells contribute to the vote count
  // for that cell.
  bool applyDelta(
    const std::string & robot_id,
    const std::vector<uint8_t> & png_bytes);

  // Variant useful for tests when the caller already has the
  // decoded uint8 grid (skips PNG round-trip). Cell-for-cell: assumes
  // the delta is already in the global frame at the global resolution.
  bool applyDeltaRaw(
    const std::string & robot_id,
    const std::vector<uint8_t> & grid);

  // [SLAM-1] World-frame-aware delta application. Projects each cell of a
  // robot's delta into the shared global grid using the robot's grid
  // origin (world x/y/theta of the delta's cell (0,0), from
  // SLAMLocalDelta.origin — refined by the pose-graph) and the delta's own
  // resolution. Unlike applyDelta(), the delta need NOT match the global
  // grid's size or resolution, and is correctly placed/rotated rather than
  // overlaid cell-for-cell. Each global cell receives at most one vote per
  // call (so a finer delta down-sampling onto one global cell is not
  // over-counted). FREE/OCCUPIED cells vote; others are skipped.
  bool applyDeltaAt(
    const std::string & robot_id,
    const std::vector<uint8_t> & png_bytes,
    double origin_x, double origin_y, double origin_theta,
    float delta_resolution_m);

  // Same as applyDeltaAt but takes an already-decoded grid, so a caller
  // that has decoded the PNG for another purpose (e.g. the hub's per-robot
  // submap accumulation) need not decode it twice. applyDeltaAt() decodes
  // and delegates here.
  bool applyDeltaAtDecoded(
    const std::string & robot_id,
    const std::vector<uint8_t> & grid, int grid_w, int grid_h,
    double origin_x, double origin_y, double origin_theta,
    float delta_resolution_m);

  // World coordinates of the global grid's cell (0,0). Default (0,0).
  void setGlobalOrigin(double x, double y) {origin_x_ = x; origin_y_ = y;}

  // PNG-encode the current global grid for SLAMAggregatedMap.
  // Implicitly calls recomputeGlobal() so the snapshot reflects the
  // latest vote state.
  std::vector<uint8_t> encodeGlobalPng();

  // Phase 7 deferred / R-3: encodeGlobalPng() runs OpenCV imencode
  // which is CPU-heavy; calling it under the producer/consumer
  // mutex blocks every applyDelta() on the publish thread. Callers
  // can instead grab a cheap memcpy snapshot under lock and then
  // PNG-encode it outside the lock with the static helper below.
  struct GridSnapshot
  {
    std::vector<uint8_t> grid;
    int width = 0;
    int height = 0;
    float resolution_m = 0.0f;
    std::size_t contributing_robots = 0;
    std::size_t mismatch_cells = 0;                     // D-026
    std::size_t contributing_cells = 0;
  };
  GridSnapshot snapshot();     // not const — calls recomputeGlobal()

  // Pure: encode an arbitrary uint8 grid as PNG. Mirrors the body of
  // encodeGlobalPng(), but takes the grid by reference so the caller
  // can pass a snapshot owned outside of any lock.
  static std::vector<uint8_t>
  encodePng(const std::vector<uint8_t> & grid, int width, int height);

  // Pure: decode a PNG-encoded uint8 grid into a row-major buffer with its
  // dimensions. Returns false on empty input or a decode failure. Shared by
  // the aggregator and the hub's per-robot submap accumulation.
  static bool decodePng(
    const std::vector<uint8_t> & png_bytes,
    std::vector<uint8_t> & out, int & width, int & height);

private:
  // [DCN-2026-006 EXT cleanup §5.1] In-class defaults guard against
  // a future refactor that adds another ctor.
  int width_ = 0;
  int height_ = 0;
  float resolution_m_ = 0.0f;
  double origin_x_ = 0.0;   // [SLAM-1] world coords of global cell (0,0)
  double origin_y_ = 0.0;
  std::vector<uint8_t> global_;
  std::set<std::string> contributing_;

  // [DCN-2026-006 EXT D-021] Per-cell vote tallies. Sized to match
  // global_; each cell keeps a free / occupied counter (uint16_t
  // is plenty — 4 robots × 30 publishes/min × 60 min/hr = 7200
  // tops, fits in 14 bits).
  std::vector<uint16_t> free_votes_;
  std::vector<uint16_t> occupied_votes_;

  // [DCN-2026-006 EXT D-026] Last recomputeGlobal() statistics.
  std::size_t mismatch_cell_count_ = 0;
  std::size_t contributing_cell_count_ = 0;
};

}  // namespace san_hub_slam
