// SAN v1.3 PHASE 3 - g2o pose-graph optimizer.
//
// Production builds link against g2o for non-linear least-squares
// optimization of the multi-robot pose graph. CI / desktop builds
// where g2o is unavailable see the HAVE_G2O guard turn this into a
// no-op (the master grid still composes correctly from raw deltas).

#pragma once

#include <cstdint>
#include <vector>

namespace san_hub_slam {

struct PoseEdge {
    uint32_t from_node = 0;
    uint32_t to_node = 0;
    float dx = 0.0f;
    float dy = 0.0f;
    float dtheta = 0.0f;
};

struct PoseGraphOptimizerParams {
    int max_iterations = 50;
    double convergence_threshold = 1e-6;
};

class PoseGraphOptimizer {
public:
    explicit PoseGraphOptimizer(
        const PoseGraphOptimizerParams& params = PoseGraphOptimizerParams{});

    void addEdge(const PoseEdge& edge);
    std::size_t edgeCount() const { return edges_.size(); }

    // Run optimization. Returns the number of iterations actually
    // executed. On builds without g2o, this is a no-op and returns 0.
    int optimize();

    void clear();

private:
    PoseGraphOptimizerParams params_;
    std::vector<PoseEdge> edges_;
};

}  // namespace san_hub_slam
