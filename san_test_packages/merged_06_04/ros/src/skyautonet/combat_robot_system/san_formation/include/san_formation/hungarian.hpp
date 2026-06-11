// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Hungarian algorithm (assignment problem) for formation slot
// allocation per SDD-SWARM §7.4.
//
// Given N robots at current poses and N target slot offsets, find the
// minimum-total-cost 1-to-1 assignment of robots → slots.
//
// Implementation: O(n³) Kuhn-Munkres on square cost matrix. Pure
// C++17, no external deps, fully standalone testable (no rclcpp).
//
// Use:
//   std::vector<std::vector<double>> cost(N, std::vector<double>(N));
//   // fill cost[i][j] = distance(robot_i, slot_j)
//   auto assignment = solveAssignment(cost);
//   // assignment[i] = slot index assigned to robot i

#ifndef SAN_FORMATION__HUNGARIAN_HPP_
#define SAN_FORMATION__HUNGARIAN_HPP_

#include <cstddef>
#include <limits>
#include <vector>

namespace san_formation
{

/// Solve the assignment problem on a square N×N cost matrix.
///
/// Returns a vector `assignment` of length N where assignment[i] is
/// the column (slot) assigned to row (robot) i. Total cost is
/// minimized.
///
/// Cost values must be finite non-negative. INF cells (impossible
/// matches) can be encoded as a very large value (e.g. 1e9).
///
/// Time: O(n³). Space: O(n²).
///
/// Throws std::invalid_argument if matrix is empty, non-square, or
/// contains a row/column where all entries are INF (no feasible
/// assignment).
std::vector<size_t> solveAssignment(
  const std::vector<std::vector<double>> & cost);

/// Convenience: compute the total cost of a given assignment.
double assignmentCost(
  const std::vector<std::vector<double>> & cost,
  const std::vector<size_t> & assignment);

/// Sentinel used to mark "infeasible" cell. Anything >= INF is
/// treated as forbidden by the solver.
constexpr double INF = 1e18;

}  // namespace san_formation

#endif  // SAN_FORMATION__HUNGARIAN_HPP_
