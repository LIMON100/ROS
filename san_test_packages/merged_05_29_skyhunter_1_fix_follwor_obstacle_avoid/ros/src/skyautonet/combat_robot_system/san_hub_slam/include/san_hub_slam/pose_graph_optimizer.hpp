// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 3 - g2o pose-graph optimizer.
//
// Production builds link against g2o for non-linear least-squares
// optimization of the multi-robot pose graph. CI / desktop builds
// where g2o is unavailable see the HAVE_G2O guard turn this into a
// no-op (the master grid still composes correctly from raw deltas).

#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace san_hub_slam
{

struct Pose2D
{
  double x = 0.0;
  double y = 0.0;
  double theta = 0.0;
};

struct PoseEdge
{
  uint32_t from_node = 0;
  uint32_t to_node = 0;
  float dx = 0.0f;
  float dy = 0.0f;
  float dtheta = 0.0f;
};

struct PoseGraphOptimizerParams
{
  int max_iterations = 50;
  double convergence_threshold = 1e-6;
};

// Multi-robot 2-D pose-graph aligner. Each robot is a node whose PRIOR is
// the grid origin it self-reports in SLAMLocalDelta. Inter-robot overlap
// constraints (loop closures) are added as PoseEdges; optimize() runs g2o
// (when linked) to produce a globally-consistent set of node poses that
// the hub aggregator uses to project each robot's delta into the shared
// grid. Without g2o — or before any loop-closure edge exists — getPose()
// returns the prior unchanged, i.e. each delta lands at its self-reported
// origin (the correct behaviour absent inter-robot constraints).
class PoseGraphOptimizer
{
public:
  explicit PoseGraphOptimizer(
    const PoseGraphOptimizerParams & params = PoseGraphOptimizerParams{});

  // Register / update a node's prior (its self-reported grid origin).
  void setNodePrior(uint32_t id, const Pose2D & prior);

  void addEdge(const PoseEdge & edge);
  std::size_t edgeCount() const {return edges_.size();}
  std::size_t nodeCount() const {return priors_.size();}

  // Drop only the loop-closure edges, keeping node priors. The hub
  // rebuilds the edge set each loop-closure cycle from the currently
  // confident matches, so stale constraints don't accumulate.
  void clearEdges() {edges_.clear();}

  // Build the relative-pose measurement for an EdgeSE2 from node `from`
  // (pose a) to node `to` (pose b): T = inv(SE2(a)) * SE2(b), i.e. b
  // expressed in a's frame. Pure/static so it is unit-testable.
  static PoseEdge relativeEdge(
    uint32_t from, const Pose2D & a,
    uint32_t to, const Pose2D & b);

  // Run optimization. Returns the number of iterations actually executed
  // (0 when there is nothing to optimize or g2o is unavailable). After it
  // returns, getPose() reflects the optimized estimates; with no edges or
  // no g2o it equals the prior.
  int optimize();

  // Optimized pose for a node (prior if never optimized / unknown node).
  Pose2D getPose(uint32_t id) const;

  void clear();

private:
  PoseGraphOptimizerParams params_;
  std::vector<PoseEdge> edges_;
  std::map<uint32_t, Pose2D> priors_;
  std::map<uint32_t, Pose2D> poses_;   // optimized estimates (= prior absent edges/g2o)
};

}  // namespace san_hub_slam
