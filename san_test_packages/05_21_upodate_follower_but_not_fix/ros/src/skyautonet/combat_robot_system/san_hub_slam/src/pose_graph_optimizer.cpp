#include "san_hub_slam/pose_graph_optimizer.hpp"

#ifdef HAVE_G2O
#include <g2o/core/sparse_optimizer.h>
#include <g2o/core/block_solver.h>
#include <g2o/core/optimization_algorithm_levenberg.h>
#include <g2o/solvers/eigen/linear_solver_eigen.h>
#include <g2o/types/slam2d/vertex_se2.h>
#include <g2o/types/slam2d/edge_se2.h>
#endif

namespace san_hub_slam {

PoseGraphOptimizer::PoseGraphOptimizer(
    const PoseGraphOptimizerParams& params)
    : params_(params)
{}

void PoseGraphOptimizer::addEdge(const PoseEdge& edge) {
    edges_.push_back(edge);
}

void PoseGraphOptimizer::clear() {
    edges_.clear();
}

int PoseGraphOptimizer::optimize() {
#ifdef HAVE_G2O
    if (edges_.empty()) return 0;
    g2o::SparseOptimizer optimizer;
    optimizer.setVerbose(false);

    using BlockSolverType = g2o::BlockSolver<g2o::BlockSolverTraits<3, 3>>;
    auto linear_solver =
        std::make_unique<g2o::LinearSolverEigen<BlockSolverType::PoseMatrixType>>();
    auto block_solver =
        std::make_unique<BlockSolverType>(std::move(linear_solver));
    auto algorithm = new g2o::OptimizationAlgorithmLevenberg(
        std::move(block_solver));
    optimizer.setAlgorithm(algorithm);

    // TODO: populate vertices + edges from edges_; production
    // implementation would resolve graph node IDs from a separate
    // vertex registry. The dispatch + parameter-driven optimization
    // path is exercised regardless.
    optimizer.initializeOptimization();
    const int iters = optimizer.optimize(params_.max_iterations);
    return iters;
#else
    // No-op when g2o is not available - return iteration count of 0
    // so callers can branch on it.
    (void)params_;
    return 0;
#endif
}

}  // namespace san_hub_slam
