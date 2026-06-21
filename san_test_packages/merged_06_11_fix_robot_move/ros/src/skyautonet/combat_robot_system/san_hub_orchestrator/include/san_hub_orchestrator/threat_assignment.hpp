// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.
//
// 2+2 cap-balanced nearest assignment — extracted from
// san_surveillance/sector_allocator.cpp:193-231 (load-balanced greedy).
// Pure C++17, no rclcpp — standalone unit-testable.
#ifndef SAN_HUB_ORCHESTRATOR__THREAT_ASSIGNMENT_HPP_
#define SAN_HUB_ORCHESTRATOR__THREAT_ASSIGNMENT_HPP_

#include <algorithm>
#include <vector>

namespace san_hub_orchestrator
{

/// Assign each of K robots to one of M targets, load-balanced with
/// cap = ceil(K/M). cost[i][j] = cost of robot i covering target j
/// (e.g. XY distance). Returns vector<int> size K, each in [0,M).
inline std::vector<int> assignWithCap(const std::vector<std::vector<float>> & cost)
{
  const size_t K = cost.size();
  std::vector<int> assign(K, -1);
  if (K == 0) {
    return assign;
  }
  const size_t M = cost[0].size();
  if (M == 0) {
    return assign;
  }
  const size_t cap = (K + M - 1) / M;
  std::vector<size_t> load(M, 0);

  struct Cand
  {
    size_t i;
    size_t j;
    float d;
  };
  std::vector<Cand> cands;
  cands.reserve(K * M);
  for (size_t i = 0; i < K; ++i) {
    for (size_t j = 0; j < M; ++j) {
      cands.push_back({i, j, cost[i][j]});
    }
  }
  std::sort(cands.begin(), cands.end(),
    [](const Cand & a, const Cand & b) {
      return a.d < b.d;
    });

  for (const auto & c : cands) {
    if (assign[c.i] != -1 || load[c.j] >= cap) {
      continue;
    }
    assign[c.i] = static_cast<int>(c.j);
    load[c.j]++;
  }
  for (size_t i = 0; i < K; ++i) {
    if (assign[i] != -1) {
      continue;
    }
    size_t j = std::min_element(load.begin(), load.end()) - load.begin();
    assign[i] = static_cast<int>(j);
    load[j]++;
  }
  return assign;
}

}  // namespace san_hub_orchestrator
#endif  // SAN_HUB_ORCHESTRATOR__THREAT_ASSIGNMENT_HPP_