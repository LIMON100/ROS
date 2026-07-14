// SAN v1.5 — Hungarian algorithm implementation.
//
// Classic Kuhn-Munkres O(n³) augmenting-path variant.
// Reference: "The Hungarian Method" — H. W. Kuhn (1955),
// J. Munkres "Algorithms for the Assignment and Transportation
// Problems" (1957).

#include "san_formation/hungarian.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace san_formation {

namespace {

// Internal sentinel for "no assignment / column unvisited".
constexpr size_t NIL = static_cast<size_t>(-1);

}  // namespace

std::vector<size_t> solveAssignment(
    const std::vector<std::vector<double>>& cost) {
  const size_t n = cost.size();
  if (n == 0) {
    throw std::invalid_argument(
        "solveAssignment: empty cost matrix");
  }
  for (const auto& row : cost) {
    if (row.size() != n) {
      throw std::invalid_argument(
          "solveAssignment: matrix not square");
    }
  }

  // Jonker-Volgenant / O(n³) Hungarian — uses potentials u, v
  // (reduced cost). Standard 1-indexed presentation; we keep 0-index
  // outside and use 1..n internally.
  //
  // u[i], v[j] : potentials such that cost[i][j] - u[i] - v[j] >= 0
  // p[j]       : row assigned to column j (NIL = unassigned)
  // way[j]     : back-pointer for augmenting path

  std::vector<double> u(n + 1, 0.0);
  std::vector<double> v(n + 1, 0.0);
  std::vector<size_t> p(n + 1, NIL);
  std::vector<size_t> way(n + 1, NIL);

  for (size_t i = 1; i <= n; ++i) {
    p[0] = i;
    size_t j0 = 0;
    std::vector<double> minv(n + 1, INF);
    std::vector<bool>   used(n + 1, false);
    do {
      used[j0] = true;
      const size_t i0 = p[j0];
      double delta = INF;
      size_t j1 = NIL;
      for (size_t j = 1; j <= n; ++j) {
        if (used[j]) continue;
        const double c = cost[i0 - 1][j - 1];
        const double cur = c - u[i0] - v[j];
        if (cur < minv[j]) {
          minv[j] = cur;
          way[j]  = j0;
        }
        if (minv[j] < delta) {
          delta = minv[j];
          j1    = j;
        }
      }
      if (j1 == NIL) {
        throw std::invalid_argument(
            "solveAssignment: no feasible matching "
            "(check for INF rows/columns)");
      }
      for (size_t j = 0; j <= n; ++j) {
        if (used[j]) {
          u[p[j]] += delta;
          v[j]    -= delta;
        } else {
          minv[j] -= delta;
        }
      }
      j0 = j1;
    } while (p[j0] != NIL);
    // Augment along the path
    do {
      const size_t j1 = way[j0];
      p[j0] = p[j1];
      j0    = j1;
    } while (j0 != 0);
  }

  // Build 0-indexed result
  std::vector<size_t> assignment(n);
  for (size_t j = 1; j <= n; ++j) {
    if (p[j] != NIL) {
      assignment[p[j] - 1] = j - 1;
    }
  }
  return assignment;
}

double assignmentCost(const std::vector<std::vector<double>>& cost,
                       const std::vector<size_t>& assignment) {
  double total = 0.0;
  for (size_t i = 0; i < assignment.size(); ++i) {
    total += cost[i][assignment[i]];
  }
  return total;
}

}  // namespace san_formation
