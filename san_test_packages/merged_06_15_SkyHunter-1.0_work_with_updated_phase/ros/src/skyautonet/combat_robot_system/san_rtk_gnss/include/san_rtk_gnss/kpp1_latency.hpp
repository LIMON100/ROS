// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-014 D-055 KPP-1 latency helpers.
//
// Pure-logic helpers extracted from measure_kpp1_latency_node.cpp so
// the percentile + CSV-row formatters can be unit-tested without
// rclcpp / DDS / file I/O.

#ifndef SAN_RTK_GNSS__KPP1_LATENCY_HPP_
#define SAN_RTK_GNSS__KPP1_LATENCY_HPP_

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace san_rtk_gnss::kpp1
{

// Nearest-rank percentile (no interpolation) on a copy of `samples`.
// `pct` in [0, 100]. Throws on empty input or out-of-range pct.
inline double percentile(std::vector<double> samples, double pct)
{
  if (samples.empty()) {
    throw std::invalid_argument("percentile: empty samples");
  }
  if (pct < 0.0 || pct > 100.0) {
    throw std::invalid_argument("percentile: pct out of [0,100]");
  }
  std::sort(samples.begin(), samples.end());
  const auto n = samples.size();
  size_t idx = static_cast<size_t>((pct / 100.0) * static_cast<double>(n));
  if (idx >= n) {
    idx = n - 1;
  }
  return samples[idx];
}

struct LatencySummary
{
  size_t count;
  double min_us;
  double max_us;
  double mean_us;
  double p50_us;
  double p95_us;
  double p99_us;
};

inline LatencySummary summarise(std::vector<double> samples)
{
  if (samples.empty()) {
    throw std::invalid_argument("summarise: empty samples");
  }
  std::sort(samples.begin(), samples.end());
  double sum = 0.0;
  for (auto v : samples) {
    sum += v;
  }
  const auto n = samples.size();
  auto pick = [&](double pct) {
      size_t idx = static_cast<size_t>((pct / 100.0) * static_cast<double>(n));
      if (idx >= n) {idx = n - 1;}
      return samples[idx];
    };
  return LatencySummary{
    n,
    samples.front(),
    samples.back(),
    sum / static_cast<double>(n),
    pick(50.0),
    pick(95.0),
    pick(99.0),
  };
}

}  // namespace san_rtk_gnss::kpp1

#endif  // SAN_RTK_GNSS__KPP1_LATENCY_HPP_
