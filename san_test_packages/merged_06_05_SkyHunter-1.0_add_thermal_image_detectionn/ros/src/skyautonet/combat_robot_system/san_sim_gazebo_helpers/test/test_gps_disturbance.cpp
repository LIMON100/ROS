// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — GPS disturbance unit tests.

#include <gtest/gtest.h>

#include <cmath>

#include "san_sim_gazebo_helpers/gps_disturbance.hpp"

using namespace san_sim_gazebo_helpers;

static GpsFix baseFix()
{
  GpsFix f;
  f.latitude_deg = 37.5665;     // Seoul-ish
  f.longitude_deg = 126.9780;
  f.altitude_m = 100.0;
  f.status = 0;
  return f;
}

// ─── Jump ───────────────────────────────────────────────────────────────

TEST(GpsDisturbance, T1_JumpAppliesAtScheduledTime) {
  GpsDisturbance d;
  JumpConfig jc;
  jc.at_sim_time_s = 10.0;
  jc.east_offset_m = 2.0;
  jc.recovery_time_s = 0.0;     // step holds forever
  d.withJump(jc);

  const auto before = d.apply(baseFix(), 9.99);
  ASSERT_TRUE(before.has_value());
  EXPECT_NEAR(before->longitude_deg, baseFix().longitude_deg, 1e-9);

  const auto after = d.apply(baseFix(), 10.01);
  ASSERT_TRUE(after.has_value());
  // 2m east @ 37° latitude → 2 / (111412 * cos(37°)) ≈ 2.25e-5 deg.
  EXPECT_GT(after->longitude_deg, baseFix().longitude_deg);
  EXPECT_NEAR(
    after->longitude_deg - baseFix().longitude_deg,
    2.0 / (111412.84 * std::cos(37.5665 * M_PI / 180.0)),
    5e-7);
}

TEST(GpsDisturbance, T2_JumpDecaysAfterRecovery) {
  GpsDisturbance d;
  JumpConfig jc;
  jc.at_sim_time_s = 5.0;
  jc.east_offset_m = 5.0;
  jc.recovery_time_s = 2.0;
  d.withJump(jc);

  const auto peak = d.apply(baseFix(), 5.0);
  const auto mid = d.apply(baseFix(), 6.0);
  const auto post = d.apply(baseFix(), 10.0);   // beyond recovery
  ASSERT_TRUE(peak && mid && post);
  EXPECT_GT(
    peak->longitude_deg - baseFix().longitude_deg,
    mid->longitude_deg - baseFix().longitude_deg);
  EXPECT_NEAR(post->longitude_deg, baseFix().longitude_deg, 1e-12);
}

// ─── Dropout ────────────────────────────────────────────────────────────

TEST(GpsDisturbance, T3_DropoutReturnsNullopt) {
  GpsDisturbance d;
  DropoutConfig dc;
  dc.start_at_s = 4.0;
  dc.duration_s = 2.0;
  d.withDropout(dc);

  EXPECT_TRUE(d.apply(baseFix(), 3.5).has_value());
  EXPECT_FALSE(d.apply(baseFix(), 4.5).has_value());
  EXPECT_FALSE(d.apply(baseFix(), 5.5).has_value());
  EXPECT_TRUE(d.apply(baseFix(), 6.5).has_value());
}

// ─── Noise ──────────────────────────────────────────────────────────────

TEST(GpsDisturbance, T4_NoiseChangesValuesButCenteredAtZero) {
  GpsDisturbance d;
  NoiseConfig nc;
  nc.east_std_m = 0.3;
  nc.north_std_m = 0.3;
  nc.altitude_std_m = 0.6;
  nc.rng_seed = 100;
  d.withNoise(nc);

  // Sample 1000 times and verify mean ≈ baseline (Law of Large Numbers).
  double sum_lon = 0.0, sum_lat = 0.0, sum_alt = 0.0;
  const int N = 1000;
  for (int i = 0; i < N; ++i) {
    const auto out = d.apply(baseFix(), 0.01 * i);
    ASSERT_TRUE(out.has_value());
    sum_lon += (out->longitude_deg - baseFix().longitude_deg);
    sum_lat += (out->latitude_deg - baseFix().latitude_deg);
    sum_alt += (out->altitude_m - baseFix().altitude_m);
  }
  // Mean should be small (within ~ 3σ/sqrt(N)).
  EXPECT_LT(std::abs(sum_lon / N), 1e-5);
  EXPECT_LT(std::abs(sum_lat / N), 1e-5);
  EXPECT_LT(std::abs(sum_alt / N), 0.1);
}

// ─── Deterministic given seed ───────────────────────────────────────────

TEST(GpsDisturbance, T5_DeterministicGivenSeed) {
  auto build = []() {
      GpsDisturbance d;
      NoiseConfig nc;
      nc.east_std_m = 0.5;
      nc.rng_seed = 999;
      d.withNoise(nc);
      return d;
    };
  auto a = build();
  auto b = build();
  for (int i = 0; i < 20; ++i) {
    const auto sa = a.apply(baseFix(), 0.05 * i);
    const auto sb = b.apply(baseFix(), 0.05 * i);
    ASSERT_TRUE(sa && sb);
    EXPECT_NEAR(sa->longitude_deg, sb->longitude_deg, 1e-12);
  }
}

// ─── Drift ──────────────────────────────────────────────────────────────

TEST(GpsDisturbance, T6_DriftAccumulatesLinearly) {
  GpsDisturbance d;
  DriftConfig dc;
  dc.start_at_s = 0.0;
  dc.end_at_s = 100.0;
  dc.east_rate_m_per_s = 0.1;       // 10cm/s east
  d.withDrift(dc);

  const auto s10 = d.apply(baseFix(), 10.0);   // 1.0m east accumulated
  const auto s20 = d.apply(baseFix(), 20.0);   // 2.0m east accumulated
  ASSERT_TRUE(s10 && s20);
  const double delta10 = s10->longitude_deg - baseFix().longitude_deg;
  const double delta20 = s20->longitude_deg - baseFix().longitude_deg;
  // delta20 should be ~2× delta10.
  EXPECT_NEAR(delta20, delta10 * 2.0, std::abs(delta10) * 0.01);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
