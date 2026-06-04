// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_hub_slam/pose_graph_optimizer.hpp"

#include <cmath>
#include <map>

#ifdef HAVE_G2O
#include <Eigen/Core>
#include <g2o/core/sparse_optimizer.h>
#include <g2o/core/block_solver.h>
#include <g2o/core/optimization_algorithm_levenberg.h>
#include <g2o/solvers/eigen/linear_solver_eigen.h>
#include <g2o/types/slam2d/vertex_se2.h>
#include <g2o/types/slam2d/edge_se2.h>
#endif

namespace san_hub_slam
{

PoseGraphOptimizer::PoseGraphOptimizer(
  const PoseGraphOptimizerParams & params)
: params_(params)
{}

void PoseGraphOptimizer::setNodePrior(uint32_t id, const Pose2D & prior)
{
  priors_[id] = prior;
  // The prior is the current best estimate until optimize() refines it, so
  // getPose() reflects an updated prior immediately. optimize() re-seeds
  // poses_ from priors_ on every call before any g2o refinement, so there
  // is no durable refined estimate to protect here.
  poses_[id] = prior;
}

void PoseGraphOptimizer::addEdge(const PoseEdge & edge)
{
  edges_.push_back(edge);
}

PoseEdge PoseGraphOptimizer::relativeEdge(
  uint32_t from, const Pose2D & a,
  uint32_t to, const Pose2D & b)
{
  const double ca = std::cos(a.theta);
  const double sa = std::sin(a.theta);
  const double rdx = b.x - a.x;
  const double rdy = b.y - a.y;
  PoseEdge e;
  e.from_node = from;
  e.to_node = to;
  e.dx = static_cast<float>(ca * rdx + sa * rdy);     // R(-aθ) * (b - a)
  e.dy = static_cast<float>(-sa * rdx + ca * rdy);
  double dth = b.theta - a.theta;
  while (dth > M_PI) {dth -= 2.0 * M_PI;}
  while (dth < -M_PI) {dth += 2.0 * M_PI;}
  e.dtheta = static_cast<float>(dth);
  return e;
}

Pose2D PoseGraphOptimizer::getPose(uint32_t id) const
{
  const auto it = poses_.find(id);
  if (it != poses_.end()) {return it->second;}
  const auto pit = priors_.find(id);
  if (pit != priors_.end()) {return pit->second;}
  return Pose2D{};
}

void PoseGraphOptimizer::clear()
{
  edges_.clear();
  priors_.clear();
  poses_.clear();
}

int PoseGraphOptimizer::optimize()
{
  // Default (and the only behaviour without g2o, or without loop-closure
  // edges): every node's optimized pose IS its prior. Ensure poses_ holds
  // an entry for every prior so getPose() is always defined.
  for (const auto & [id, prior] : priors_) {poses_[id] = prior;}

#ifdef HAVE_G2O
  // Need ≥1 edge to refine anything; with only priors the solution is the
  // priors themselves (handled above).
  if (edges_.empty() || priors_.size() < 2) {return 0;}

  g2o::SparseOptimizer optimizer;
  optimizer.setVerbose(false);

  using BlockSolverType = g2o::BlockSolver<g2o::BlockSolverTraits<3, 3>>;
  auto linear_solver =
    std::make_unique<g2o::LinearSolverEigen<BlockSolverType::PoseMatrixType>>();
  auto block_solver =
    std::make_unique<BlockSolverType>(std::move(linear_solver));
  auto * algorithm = new g2o::OptimizationAlgorithmLevenberg(
    std::move(block_solver));
  optimizer.setAlgorithm(algorithm);

  // 1+2. One VertexSE2 per registered node, initialised at its prior.
  std::map<uint32_t, g2o::VertexSE2 *> vmap;
  bool anchored = false;
  for (const auto & [id, prior] : priors_) {
    auto * v = new g2o::VertexSE2();
    v->setId(static_cast<int>(id));
    v->setEstimate(g2o::SE2(prior.x, prior.y, prior.theta));
    // Fix the lowest-id node so the otherwise gauge-free graph is
    // well-posed; unconstrained nodes then stay at their prior.
    if (!anchored) {v->setFixed(true); anchored = true;}
    optimizer.addVertex(v);
    vmap[id] = v;
  }

  // 3. One EdgeSE2 per inter-robot constraint (from → to measurement).
  for (const auto & e : edges_) {
    const auto fit = vmap.find(e.from_node);
    const auto tit = vmap.find(e.to_node);
    if (fit == vmap.end() || tit == vmap.end()) {continue;}
    auto * edge = new g2o::EdgeSE2();
    edge->setVertex(0, fit->second);
    edge->setVertex(1, tit->second);
    edge->setMeasurement(g2o::SE2(e.dx, e.dy, e.dtheta));
    edge->setInformation(Eigen::Matrix3d::Identity());
    optimizer.addEdge(edge);
  }

  optimizer.initializeOptimization();
  const int iters = optimizer.optimize(params_.max_iterations);

  // Read the refined estimates back into poses_.
  for (const auto & [id, v] : vmap) {
    const g2o::SE2 est = v->estimate();
    poses_[id] = Pose2D{est.translation().x(), est.translation().y(),
      est.rotation().angle()};
  }
  return iters;
#else
  (void)params_;
  return 0;
#endif
}

}  // namespace san_hub_slam
