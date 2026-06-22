// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.3 — Phase-7 audit F2: PoseGraphOptimizer API contract.
//
// PoseGraphOptimizer is the planned home of multi-robot loop-closure
// optimization (DCN-2026-006). The production implementation requires
// a vertex-registry + edge-construction layer that is NOT yet wired
// (see DEFERRED comment in pose_graph_optimizer.cpp::optimize()).
//
// These tests pin the existing API contract so any refactor that
// breaks addEdge/clear/edgeCount/optimize signatures is caught at
// build time, and so the no-op behavior on an empty graph is
// reproducible across HAVE_G2O and non-HAVE_G2O builds:
//
//   P1  default-constructed optimizer is empty
//   P2  addEdge appends; edgeCount reflects count
//   P3  clear() empties
//   P4  optimize() returns 0 on empty graph (both HAVE_G2O paths)
//   P5  optimize() does not throw on populated graph
//       (HAVE_G2O: dispatches g2o; non-HAVE_G2O: no-op)
//
// Pure-logic, no rclcpp. Runs in <5 ms.

#include <gtest/gtest.h>

#include <cmath>

#include "san_hub_slam/pose_graph_optimizer.hpp"

using san_hub_slam::PoseGraphOptimizer;
using san_hub_slam::PoseGraphOptimizerParams;
using san_hub_slam::PoseEdge;

TEST(PoseGraphOptimizer, P1_DefaultConstructIsEmpty) {
  PoseGraphOptimizer opt;
  EXPECT_EQ(opt.edgeCount(), 0u);
  // optimize on empty graph: 0 iterations (HAVE_G2O early-returns;
  // non-HAVE_G2O is unconditionally 0).
  EXPECT_EQ(opt.optimize(), 0);
}

TEST(PoseGraphOptimizer, P2_AddEdgeIncrementsCount) {
  PoseGraphOptimizer opt;
  PoseEdge e;
  e.from_node = 1;
  e.to_node = 2;
  e.dx = 1.0f;
  e.dy = 0.5f;
  e.dtheta = 0.1f;

  opt.addEdge(e);
  EXPECT_EQ(opt.edgeCount(), 1u);

  opt.addEdge(e);
  opt.addEdge(e);
  EXPECT_EQ(opt.edgeCount(), 3u);
}

TEST(PoseGraphOptimizer, P3_ClearEmptiesGraph) {
  PoseGraphOptimizer opt;
  PoseEdge e;
  opt.addEdge(e);
  opt.addEdge(e);
  ASSERT_EQ(opt.edgeCount(), 2u);

  opt.clear();
  EXPECT_EQ(opt.edgeCount(), 0u);
  EXPECT_EQ(opt.optimize(), 0)
    << "optimize after clear should return 0 (empty graph again)";
}

TEST(PoseGraphOptimizer, P4_CustomParamsHeldThroughLifetime) {
  PoseGraphOptimizerParams params;
  params.max_iterations = 17;
  params.convergence_threshold = 1e-4;

  PoseGraphOptimizer opt(params);
  // No public getter — we exercise the optimize() path which
  // consumes params_.max_iterations (HAVE_G2O builds). On non-
  // HAVE_G2O the test still verifies the constructor doesn't
  // crash with non-default params.
  EXPECT_EQ(opt.edgeCount(), 0u);
  EXPECT_EQ(opt.optimize(), 0);
}

TEST(PoseGraphOptimizer, P5_OptimizeWithEdgesButNoPriorsDoesNotThrow) {
  PoseGraphOptimizer opt;
  // Edges referencing nodes that were never given a prior. optimize()
  // needs ≥2 registered node priors to build the g2o graph, so with
  // priors absent it returns 0 on both HAVE_G2O and stub builds.
  // Must not throw / crash regardless.
  PoseEdge a; a.from_node = 1; a.to_node = 2; a.dx = 0.5f;
  PoseEdge b; b.from_node = 2; b.to_node = 3; b.dx = 0.5f;
  PoseEdge c; c.from_node = 3; c.to_node = 1; c.dx = -1.0f;     // loop
  opt.addEdge(a);
  opt.addEdge(b);
  opt.addEdge(c);
  ASSERT_EQ(opt.edgeCount(), 3u);

  int iters = 0;
  EXPECT_NO_THROW(iters = opt.optimize());
  EXPECT_EQ(iters, 0)
    << "no node priors registered → nothing to optimize";
}

// ─── [SLAM-1] Prior registration + pose pass-through ───────────────────
//
// The hub_slam aligner uses each robot's self-reported grid origin as
// its pose-graph PRIOR. Absent inter-robot loop-closure edges (none are
// detected yet) — or on a build without g2o — getPose() must return that
// prior unchanged, so each delta lands at its self-reported origin.

TEST(PoseGraphOptimizer, P6_PriorPassThroughWithoutEdges) {
  PoseGraphOptimizer opt;
  opt.setNodePrior(1, san_hub_slam::Pose2D{1.0, 2.0, 0.5});
  opt.setNodePrior(7, san_hub_slam::Pose2D{-3.0, 4.0, -1.2});
  EXPECT_EQ(opt.nodeCount(), 2u);
  EXPECT_EQ(opt.edgeCount(), 0u);

  // No edges → optimize is a no-op; each node's pose equals its prior
  // on every build (HAVE_G2O early-returns at priors<2 / edges empty,
  // stub returns 0).
  EXPECT_EQ(opt.optimize(), 0);

  const auto p1 = opt.getPose(1);
  EXPECT_DOUBLE_EQ(p1.x, 1.0);
  EXPECT_DOUBLE_EQ(p1.y, 2.0);
  EXPECT_DOUBLE_EQ(p1.theta, 0.5);

  const auto p7 = opt.getPose(7);
  EXPECT_DOUBLE_EQ(p7.x, -3.0);
  EXPECT_DOUBLE_EQ(p7.y, 4.0);
  EXPECT_DOUBLE_EQ(p7.theta, -1.2);
}

TEST(PoseGraphOptimizer, P7_GetPoseReflectsPriorImmediately) {
  PoseGraphOptimizer opt;
  opt.setNodePrior(3, san_hub_slam::Pose2D{5.0, -5.0, 0.0});
  // getPose works right after setNodePrior, before any optimize().
  auto p = opt.getPose(3);
  EXPECT_DOUBLE_EQ(p.x, 5.0);
  EXPECT_DOUBLE_EQ(p.y, -5.0);

  // Updating the prior is reflected immediately (no stale pose).
  opt.setNodePrior(3, san_hub_slam::Pose2D{9.0, 8.0, 0.0});
  EXPECT_EQ(opt.nodeCount(), 1u);
  p = opt.getPose(3);
  EXPECT_DOUBLE_EQ(p.x, 9.0);
  EXPECT_DOUBLE_EQ(p.y, 8.0);
}

TEST(PoseGraphOptimizer, P8_UnknownNodeReturnsDefaultPose) {
  PoseGraphOptimizer opt;
  const auto p = opt.getPose(42);
  EXPECT_DOUBLE_EQ(p.x, 0.0);
  EXPECT_DOUBLE_EQ(p.y, 0.0);
  EXPECT_DOUBLE_EQ(p.theta, 0.0);
}

TEST(PoseGraphOptimizer, P10_ClearEdgesKeepsPriors) {
  // The hub rebuilds the edge set each loop-closure cycle; clearEdges()
  // must drop edges WITHOUT discarding node priors.
  PoseGraphOptimizer opt;
  opt.setNodePrior(1, san_hub_slam::Pose2D{1.0, 0.0, 0.0});
  opt.setNodePrior(2, san_hub_slam::Pose2D{2.0, 0.0, 0.0});
  PoseEdge e; e.from_node = 1; e.to_node = 2; e.dx = 1.0f;
  opt.addEdge(e);
  ASSERT_EQ(opt.edgeCount(), 1u);
  ASSERT_EQ(opt.nodeCount(), 2u);

  opt.clearEdges();
  EXPECT_EQ(opt.edgeCount(), 0u);
  EXPECT_EQ(opt.nodeCount(), 2u);     // priors preserved
  EXPECT_DOUBLE_EQ(opt.getPose(1).x, 1.0);
}

TEST(PoseGraphOptimizer, P11_RelativeEdgeComputesTransform) {
  // θ=0: relative is just the world-frame delta.
  auto e0 = PoseGraphOptimizer::relativeEdge(
    1, san_hub_slam::Pose2D{1.0, 2.0, 0.0},
    2, san_hub_slam::Pose2D{3.0, 2.0, 0.0});
  EXPECT_EQ(e0.from_node, 1u);
  EXPECT_EQ(e0.to_node, 2u);
  EXPECT_NEAR(e0.dx, 2.0f, 1e-5);
  EXPECT_NEAR(e0.dy, 0.0f, 1e-5);
  EXPECT_NEAR(e0.dtheta, 0.0f, 1e-5);

  // a faces +90°: a world-frame +x offset of b becomes -y in a's frame.
  const double h = M_PI / 2.0;
  auto e1 = PoseGraphOptimizer::relativeEdge(
    1, san_hub_slam::Pose2D{0.0, 0.0, h},
    2, san_hub_slam::Pose2D{1.0, 0.0, h});
  EXPECT_NEAR(e1.dx, 0.0f, 1e-5);
  EXPECT_NEAR(e1.dy, -1.0f, 1e-5);
  EXPECT_NEAR(e1.dtheta, 0.0f, 1e-5);
}

TEST(PoseGraphOptimizer, P9_PriorsPlusEdgeOptimizeRunsAndStaysFinite) {
  // Two node priors + one connecting edge: this is the path that builds
  // the g2o graph (VertexSE2 per prior, the lowest-id fixed, one EdgeSE2)
  // and reads estimates back. On a HAVE_G2O build it runs Levenberg–
  // Marquardt; on the stub it returns the priors. The cross-build
  // invariant we can assert: it must not throw and every pose stays
  // finite (the anchored node never moves from its prior).
  PoseGraphOptimizer opt;
  opt.setNodePrior(1, san_hub_slam::Pose2D{0.0, 0.0, 0.0});
  opt.setNodePrior(2, san_hub_slam::Pose2D{1.0, 0.0, 0.0});
  PoseEdge e;
  e.from_node = 1;
  e.to_node = 2;
  e.dx = 1.0f;     // node 2 sits 1 m ahead of node 1 in x
  opt.addEdge(e);

  int iters = 0;
  EXPECT_NO_THROW(iters = opt.optimize());
  EXPECT_GE(iters, 0);

  const auto p1 = opt.getPose(1);
  const auto p2 = opt.getPose(2);
  // Anchored node (lowest id) is fixed at its prior on every build.
  EXPECT_DOUBLE_EQ(p1.x, 0.0);
  EXPECT_DOUBLE_EQ(p1.y, 0.0);
  // node 2 stays finite and near its prior / the 1 m constraint.
  EXPECT_TRUE(std::isfinite(p2.x));
  EXPECT_TRUE(std::isfinite(p2.y));
  EXPECT_TRUE(std::isfinite(p2.theta));
  EXPECT_NEAR(p2.x, 1.0, 0.5);
}
