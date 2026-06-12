// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-014 D-055 gtest for KPP-1 helpers.

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "san_rtk_gnss/kpp1_latency.hpp"

using san_rtk_gnss::kpp1::percentile;
using san_rtk_gnss::kpp1::summarise;

TEST(Kpp1Percentile, NearestRankIndexing) {
  // 1..100. Nearest-rank: idx = static_cast<size_t>(pct/100 * N).
  std::vector<double> s;
  s.reserve(100);
  for (int i = 1; i <= 100; ++i) {
    s.push_back(static_cast<double>(i));
  }
  EXPECT_DOUBLE_EQ(percentile(s, 50.0), 51.0);
  EXPECT_DOUBLE_EQ(percentile(s, 95.0), 96.0);
  EXPECT_DOUBLE_EQ(percentile(s, 99.0), 100.0);
  EXPECT_DOUBLE_EQ(percentile(s, 100.0), 100.0);   // clamped to last
  EXPECT_DOUBLE_EQ(percentile(s, 0.0), 1.0);       // first element
}

TEST(Kpp1Percentile, SingleSampleAllPercentilesAreThatSample) {
  std::vector<double> s{42.0};
  EXPECT_DOUBLE_EQ(percentile(s, 0.0), 42.0);
  EXPECT_DOUBLE_EQ(percentile(s, 50.0), 42.0);
  EXPECT_DOUBLE_EQ(percentile(s, 99.0), 42.0);
}

TEST(Kpp1Percentile, UnsortedInputOk) {
  std::vector<double> s{9.0, 1.0, 3.0, 7.0, 5.0};
  // Sorted: 1,3,5,7,9 (n=5). pct=50 → idx 2 → 5.
  EXPECT_DOUBLE_EQ(percentile(s, 50.0), 5.0);
  EXPECT_DOUBLE_EQ(percentile(s, 80.0), 9.0);  // idx 4 → 9
}

TEST(Kpp1Percentile, EmptyThrows) {
  std::vector<double> s;
  EXPECT_THROW(percentile(s, 50.0), std::invalid_argument);
}

TEST(Kpp1Percentile, OutOfRangeThrows) {
  std::vector<double> s{1.0, 2.0, 3.0};
  EXPECT_THROW(percentile(s, -1.0), std::invalid_argument);
  EXPECT_THROW(percentile(s, 100.1), std::invalid_argument);
}

TEST(Kpp1Summary, AggregatesAllFields) {
  std::vector<double> s;
  s.reserve(100);
  for (int i = 1; i <= 100; ++i) {
    s.push_back(static_cast<double>(i));
  }
  const auto agg = summarise(s);
  EXPECT_EQ(agg.count, 100u);
  EXPECT_DOUBLE_EQ(agg.min_us, 1.0);
  EXPECT_DOUBLE_EQ(agg.max_us, 100.0);
  EXPECT_DOUBLE_EQ(agg.mean_us, 50.5);
  EXPECT_DOUBLE_EQ(agg.p50_us, 51.0);
  EXPECT_DOUBLE_EQ(agg.p95_us, 96.0);
  EXPECT_DOUBLE_EQ(agg.p99_us, 100.0);
}

TEST(Kpp1Summary, EmptyThrows) {
  std::vector<double> s;
  EXPECT_THROW(summarise(s), std::invalid_argument);
}
