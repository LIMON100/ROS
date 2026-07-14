// SAN v1.5 — Sector allocator + Pan-Tilt controller standalone tests.
//
// Coverage:
//   A1   8-robot Recon: 1 leader + 1 hub + 6 followers
//   A2   Leader gets fixed ±30°
//   A3   Hub gets rear 180° (wraps around ±180)
//   A4   6 followers evenly split front 240°
//   A5   Recon vs Defence vs Assault sector widths differ
//   A6   Gap fill — 1 follower dies → remaining recompute
//   A7   Threat focus — 3 nearest followers re-pointed
//   A8   Empty input → empty output
//
//   S1   Sweep dps formula matches SDD §8.4 example (90°, 60° HFOV, 6s)
//   S2   Sweep dps clamped to 30°/s default and 15°/s night
//   S3   Sweep bounces within sector bounds
//   T1   Track converges to target — final error well below KPP 0.05°
//   T2   Track dead-zone prevents jitter
//   E1   Engage with non-zero rate leads target
//   F1   Fixed mode holds position

#include "san_surveillance/sector_allocator.hpp"
#include "san_surveillance/pan_tilt_controller.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace san_surveillance {
namespace {

// ─── Sector Allocator ──────────────────────────────────────────────────

TEST(SectorAllocator, A1_EightRobotReconBalanced) {
  AllocatorInput in;
  in.robots = {
      {1, RobotRole::Leader,   true},
      {2, RobotRole::Hub,      true},
      {3, RobotRole::Follower, true},
      {4, RobotRole::Follower, true},
      {5, RobotRole::Follower, true},
      {6, RobotRole::Follower, true},
      {7, RobotRole::Follower, true},
      {8, RobotRole::Follower, true},
  };
  in.mode = SurveillanceMode::Recon;
  auto out = allocateSectors(in);
  ASSERT_EQ(out.size(), 8u);
}

TEST(SectorAllocator, A2_LeaderGetsFixedFrontBand) {
  AllocatorInput in;
  in.robots = {
      {1, RobotRole::Leader, true},
      {2, RobotRole::Follower, true},
      {3, RobotRole::Follower, true},
  };
  auto out = allocateSectors(in);
  // First entry is the leader
  EXPECT_EQ(out[0].robot_id, 1u);
  EXPECT_FLOAT_EQ(out[0].sector_start_deg, -30.0f);
  EXPECT_FLOAT_EQ(out[0].sector_end_deg,   +30.0f);
  // Leader mode should be FIXED (no pan-tilt)
  EXPECT_EQ(out[0].mode_hint, 2u);  // MODE_FIXED
}

TEST(SectorAllocator, A3_HubRearWrapsAroundPi) {
  AllocatorInput in;
  in.robots = {
      {1, RobotRole::Leader, true},
      {2, RobotRole::Hub,    true},
  };
  auto out = allocateSectors(in);
  // Find hub entry
  bool found = false;
  for (const auto& s : out) {
    if (s.robot_id == 2) {
      found = true;
      // Hub sector is [90, 180] U [-180, -90] — start > end means wrap
      EXPECT_TRUE(s.wrapsAround());
      EXPECT_FLOAT_EQ(s.sector_start_deg,  +90.0f);
      EXPECT_FLOAT_EQ(s.sector_end_deg,    -90.0f);
      EXPECT_NEAR(s.widthDeg(), 180.0f, 1e-3);
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(SectorAllocator, A4_SixFollowersSplitFront240Recon) {
  AllocatorInput in;
  in.robots = {
      {1, RobotRole::Leader, true},
      {2, RobotRole::Hub,    true},
  };
  for (uint32_t i = 3; i <= 8; ++i) {
    in.robots.push_back({i, RobotRole::Follower, true});
  }
  in.mode = SurveillanceMode::Recon;
  auto out = allocateSectors(in);

  // Each follower should have width = 240/6 = 40°
  int follower_count = 0;
  for (const auto& s : out) {
    if (s.robot_id >= 3 && s.robot_id <= 8) {
      EXPECT_NEAR(s.widthDeg(), 40.0f, 1e-3)
          << "robot " << s.robot_id << " width=" << s.widthDeg();
      ++follower_count;
    }
  }
  EXPECT_EQ(follower_count, 6);
}

TEST(SectorAllocator, A5_ReconVsDefenceVsAssaultWidths) {
  auto make_robots = []() {
    AllocatorInput in;
    in.robots.push_back({1, RobotRole::Leader, true});
    for (uint32_t i = 2; i <= 5; ++i) {
      in.robots.push_back({i, RobotRole::Follower, true});
    }
    return in;
  };

  auto recon = make_robots();   recon.mode   = SurveillanceMode::Recon;
  auto defc  = make_robots();   defc.mode    = SurveillanceMode::Defence;
  auto assault = make_robots(); assault.mode = SurveillanceMode::Assault;

  auto r_out = allocateSectors(recon);
  auto d_out = allocateSectors(defc);
  auto a_out = allocateSectors(assault);

  // Width per follower (find any non-leader)
  auto follower_width = [](const std::vector<SectorAssignment>& v) {
    for (const auto& s : v) {
      if (s.robot_id >= 2) return s.widthDeg();
    }
    return 0.0f;
  };
  float r = follower_width(r_out);
  float d = follower_width(d_out);
  float a = follower_width(a_out);
  // Recon 240/4 = 60, Defence = 45, Assault narrow 18
  EXPECT_NEAR(r, 60.0f, 1e-3);
  EXPECT_NEAR(d, 45.0f, 1e-3);
  EXPECT_NEAR(a, 18.0f, 1e-3);
  EXPECT_GT(r, d);     // Recon wider than Defence
  EXPECT_GT(d, a);     // Defence wider than Assault
}

TEST(SectorAllocator, A6_GapFillOnFollowerLoss) {
  AllocatorInput in;
  in.robots = {
      {1, RobotRole::Leader, true},
      {2, RobotRole::Hub,    true},
      {3, RobotRole::Follower, true},
      {4, RobotRole::Follower, true},
      {5, RobotRole::Follower, false},   // DEAD
      {6, RobotRole::Follower, true},
  };
  auto out = allocateSectors(in);
  // Dead robot should not appear
  for (const auto& s : out) {
    EXPECT_NE(s.robot_id, 5u);
  }
  // 3 alive followers should each cover 240/3 = 80°
  for (const auto& s : out) {
    if (s.robot_id >= 3 && s.robot_id <= 6) {
      EXPECT_NEAR(s.widthDeg(), 80.0f, 1e-3)
          << "robot " << s.robot_id;
    }
  }
}

TEST(SectorAllocator, A7_ThreatFocusReorientsThreeFollowers) {
  AllocatorInput in;
  in.robots = {{1, RobotRole::Leader, true}, {2, RobotRole::Hub, true}};
  for (uint32_t i = 3; i <= 8; ++i) {
    in.robots.push_back({i, RobotRole::Follower, true});
  }
  in.mode = SurveillanceMode::Recon;
  in.threat_bearing_deg = -90.0f;  // threat on left flank
  auto out = allocateSectors(in);

  int n_focus = 0;
  for (const auto& s : out) {
    if (s.priority == 1) {        // PRIORITY_THREAT_FOCUS
      ++n_focus;
      // Track mode should be selected
      EXPECT_EQ(s.mode_hint, 1u);  // MODE_TRACK
      // Centre should be near -90°
      const float centre = s.wrapsAround()
          ? -180.0f + (s.sector_end_deg + (360.0f - s.sector_start_deg)) * 0.5f
          : (s.sector_start_deg + s.sector_end_deg) * 0.5f;
      EXPECT_NEAR(centre, -90.0f, 1e-3);
    }
  }
  // SDD §8.6.1: 2-3 followers; we used 3
  EXPECT_EQ(n_focus, 3);
}

TEST(SectorAllocator, A8_EmptyInputProducesEmptyOutput) {
  AllocatorInput in;
  auto out = allocateSectors(in);
  EXPECT_TRUE(out.empty());
}

// ─── Pan-Tilt Controller ───────────────────────────────────────────────

TEST(PanTilt, S1_SweepDpsMatchesSDDExample) {
  // SDD §8.4 example: 90° sector, 60° HFOV, 6s → ω = 10°/s
  PanTiltConfig cfg;
  cfg.hfov_deg       = 60.0f;
  cfg.sweep_period_s = 6.0f;
  cfg.sweep_max_dps  = 100.0f;   // unsaturated
  float omega = computeSweepDps(cfg, 90.0f, false);
  EXPECT_NEAR(omega, 10.0f, 1e-3);

  // Second example: 180° sector, 60° HFOV, 8s → 30°/s
  cfg.sweep_period_s = 8.0f;
  omega = computeSweepDps(cfg, 180.0f, false);
  EXPECT_NEAR(omega, 30.0f, 1e-3);
}

TEST(PanTilt, S2_SweepDpsClampedNightVsDay) {
  PanTiltConfig cfg;
  cfg.hfov_deg = 60.0f;
  cfg.sweep_period_s  = 6.0f;
  cfg.sweep_max_dps   = 30.0f;
  cfg.sweep_night_dps = 15.0f;
  // 360° sector — would be (360-60)*2/6 = 100°/s, clamp to 30/15
  EXPECT_FLOAT_EQ(computeSweepDps(cfg, 360.0f, false), 30.0f);
  EXPECT_FLOAT_EQ(computeSweepDps(cfg, 360.0f, true),  15.0f);
  // Smaller-than-HFOV sector → 0
  EXPECT_FLOAT_EQ(computeSweepDps(cfg, 50.0f, false), 0.0f);
}

TEST(PanTilt, S3_SweepBouncesWithinBounds) {
  PanTiltConfig cfg;
  PanTiltState  st;
  st.mode = PanTiltMode::Sweep;
  st.pan_deg = -55.0f;
  st.sweep_going_right = true;

  SweepParams p;
  p.centre_deg = -55.0f;
  p.sector_width_deg = 50.0f;  // sector [-80, -30]
  p.night_mode = false;

  // Run 4 seconds at dt=0.1s and verify position stays in [-80, -30]
  for (int i = 0; i < 40; ++i) {
    stepController(cfg, st, 0.1f, p);
    EXPECT_GE(st.pan_deg, -80.0f - 0.5f);   // tiny tolerance for rounding
    EXPECT_LE(st.pan_deg, -30.0f + 0.5f);
  }
}

TEST(PanTilt, T1_TrackConvergesBelowKPP) {
  PanTiltConfig cfg;
  cfg.track_kp_pan      = 1.5f;
  cfg.track_kp_tilt     = 1.5f;
  cfg.track_dead_zone   = 0.02f;   // 0.02° < KPP 0.05°
  cfg.track_max_dps     = 60.0f;

  PanTiltState st;
  st.mode      = PanTiltMode::Track;
  st.pan_deg   = 0.0f;
  st.tilt_deg  = 0.0f;

  TrackTarget t{ /*bearing=*/ 5.0f, /*elevation=*/ 2.0f };

  // Run 4 seconds at 50 Hz (dt=0.02s) → controller should converge
  for (int i = 0; i < 200; ++i) {
    stepController(cfg, st, 0.02f, std::nullopt, t, std::nullopt);
  }

  // Final residual should be ≤ 0.05° (KPP)
  EXPECT_LE(st.last_track_error_deg, 0.05f)
      << "Track residual " << st.last_track_error_deg
      << " > KPP 0.05°";
  // Pan/Tilt should be close to target
  EXPECT_NEAR(st.pan_deg,  5.0f, 0.05f);
  EXPECT_NEAR(st.tilt_deg, 2.0f, 0.05f);
}

TEST(PanTilt, T2_TrackDeadZonePreventsJitter) {
  PanTiltConfig cfg;
  cfg.track_dead_zone = 0.05f;     // 0.05° dead zone
  PanTiltState st;
  st.mode = PanTiltMode::Track;
  st.pan_deg  = 5.0f;
  st.tilt_deg = 2.0f;
  // Target inside dead zone
  TrackTarget t{5.02f, 2.01f};
  auto out = stepController(cfg, st, 0.02f, std::nullopt, t, std::nullopt);
  // Should report zero speed and not move
  EXPECT_FLOAT_EQ(out.speed_dps, 0.0f);
  EXPECT_FLOAT_EQ(st.pan_deg,  5.0f);
  EXPECT_FLOAT_EQ(st.tilt_deg, 2.0f);
}

TEST(PanTilt, E1_EngageLeadsTarget) {
  PanTiltConfig cfg;
  cfg.track_dead_zone = 0.001f;    // very tight to see lead effect
  PanTiltState st;
  st.mode     = PanTiltMode::Engage;
  st.pan_deg  = 0.0f;
  st.tilt_deg = 0.0f;

  EngageTarget e;
  e.bearing_deg = 10.0f;
  e.elevation_deg = 0.0f;
  e.bearing_rate_dps   = 5.0f;     // target moving 5°/s right
  e.elevation_rate_dps = 0.0f;

  // 1 step of 0.5s — lead = 10 + 5*0.5 = 12.5
  // After convergence at this lead, pan should be near 12.5
  for (int i = 0; i < 100; ++i) {
    stepController(cfg, st, 0.02f, std::nullopt, std::nullopt, e);
  }
  // Pan should track approximately to a lead-compensated steady state
  EXPECT_GT(st.pan_deg, 10.5f) << "Engage didn't lead target";
}

TEST(PanTilt, F1_FixedHoldsPosition) {
  PanTiltConfig cfg;
  PanTiltState st;
  st.mode = PanTiltMode::Fixed;
  st.pan_deg = 45.0f;
  st.tilt_deg = 5.0f;
  for (int i = 0; i < 100; ++i) {
    stepController(cfg, st, 0.02f);
  }
  EXPECT_FLOAT_EQ(st.pan_deg, 45.0f);
  EXPECT_FLOAT_EQ(st.tilt_deg, 5.0f);
}

// ─── Geometry helpers ─────────────────────────────────────────────────

TEST(Geometry, NormalizeAngleWrapsCorrectly) {
  EXPECT_FLOAT_EQ(normalizeAngle(0.0f),    0.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(180.0f),  180.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(-180.0f), -180.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(190.0f), -170.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(-190.0f),  170.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(360.0f),    0.0f);
}

TEST(Geometry, AngularDifferenceShortestPath) {
  EXPECT_FLOAT_EQ(angularDifference(   0.0f,    0.0f), 0.0f);
  EXPECT_FLOAT_EQ(angularDifference( 170.0f, -170.0f), 20.0f);  // wraps
  EXPECT_FLOAT_EQ(angularDifference(  90.0f,  -90.0f), 180.0f);
  EXPECT_FLOAT_EQ(angularDifference(  45.0f,   45.0f), 0.0f);
}

}  // namespace
}  // namespace san_surveillance
