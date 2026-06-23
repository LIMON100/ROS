// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

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

namespace san_surveillance
{
namespace
{

// ─── Sector Allocator ──────────────────────────────────────────────────

TEST(SectorAllocator, A1_EightRobotReconBalanced) {
  AllocatorInput in;
  in.robots = {
    {1, RobotRole::Leader, true},
    {2, RobotRole::Hub, true},
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
  EXPECT_FLOAT_EQ(out[0].sector_end_deg, +30.0f);
  // Leader mode should be FIXED (no pan-tilt)
  EXPECT_EQ(out[0].mode_hint, 2u);  // MODE_FIXED
}

TEST(SectorAllocator, A3_HubRearWrapsAroundPi) {
  AllocatorInput in;
  in.robots = {
    {1, RobotRole::Leader, true},
    {2, RobotRole::Hub, true},
  };
  auto out = allocateSectors(in);
  // Find hub entry
  bool found = false;
  for (const auto & s : out) {
    if (s.robot_id == 2) {
      found = true;
      // Hub sector is [90, 180] U [-180, -90] — start > end means wrap
      EXPECT_TRUE(s.wrapsAround());
      EXPECT_FLOAT_EQ(s.sector_start_deg, +90.0f);
      EXPECT_FLOAT_EQ(s.sector_end_deg, -90.0f);
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
    {2, RobotRole::Hub, true},
  };
  for (uint32_t i = 3; i <= 8; ++i) {
    in.robots.push_back({i, RobotRole::Follower, true});
  }
  in.mode = SurveillanceMode::Recon;
  auto out = allocateSectors(in);

  // Each follower should have width = 240/6 = 40°
  int follower_count = 0;
  for (const auto & s : out) {
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

  auto recon = make_robots();   recon.mode = SurveillanceMode::Recon;
  auto defc = make_robots();   defc.mode = SurveillanceMode::Defence;
  auto assault = make_robots(); assault.mode = SurveillanceMode::Assault;

  auto r_out = allocateSectors(recon);
  auto d_out = allocateSectors(defc);
  auto a_out = allocateSectors(assault);

  // Width per follower (find any non-leader)
  auto follower_width = [](const std::vector<SectorAssignment> & v) {
      for (const auto & s : v) {
        if (s.robot_id >= 2) {return s.widthDeg();}
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
    {2, RobotRole::Hub, true},
    {3, RobotRole::Follower, true},
    {4, RobotRole::Follower, true},
    {5, RobotRole::Follower, false},     // DEAD
    {6, RobotRole::Follower, true},
  };
  auto out = allocateSectors(in);
  // Dead robot should not appear
  for (const auto & s : out) {
    EXPECT_NE(s.robot_id, 5u);
  }
  // 3 alive followers should each cover 240/3 = 80°
  for (const auto & s : out) {
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
  in.threat_bearings_deg = {-90.0f}; // threat on left flank
  auto out = allocateSectors(in);

  int n_focus = 0;
  for (const auto & s : out) {
    if (s.priority == 1) {        // PRIORITY_THREAT_FOCUS
      ++n_focus;
      // Track mode should be selected
      EXPECT_EQ(s.mode_hint, 1u);  // MODE_TRACK
      // Centre should be near -90°
      const float centre = s.wrapsAround() ?
        -180.0f + (s.sector_end_deg + (360.0f - s.sector_start_deg)) * 0.5f :
        (s.sector_start_deg + s.sector_end_deg) * 0.5f;
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

// ─── Phase-7 audit P1 — coverage continuity invariants ──────────────
//
// The existing A1..A8 tests verify width sums and gap-fill ranks, but
// none asserts that the UNION of all assigned sectors actually covers
// ≥ a target fraction of 360° in either Recon (no threat) or Threat-
// Focus mode. After threat-focus re-points 3 followers onto the
// threat bearing (A7), the OTHER 3 followers + Leader + Hub still
// need to cover the rest of the perimeter without leaving large
// blind gaps. This audit-driven addition pins:
//
//   A9   Recon 8-robot: union coverage ≥ 95% (overlaps allowed)
//   A10  Threat-focus 8-robot: union coverage ≥ 80% (re-point
//        concentrates 3 followers near threat; remaining 5 sectors
//        still cover most of the rest — 80% is the operationally-
//        acceptable bar per SDD §8.6.1 which trades full coverage
//        for threat-side resolution).

namespace
{

// Compute the union coverage of a set of sectors as a fraction of
// 360°. Uses 1° resolution sampling (361 samples) — simple, correct,
// handles wrap-around.
float unionCoverageFraction(
  const std::vector<SectorAssignment> & sectors)
{
  if (sectors.empty()) {return 0.0f;}
  std::vector<bool> covered(361, false);
  for (const auto & s : sectors) {
    // Step degree-by-degree from start, going CCW (i.e. increasing
    // angle), wrapping at +180 → -180. Total degrees swept =
    // widthDeg() (always positive).
    const int width_steps = static_cast<int>(std::round(s.widthDeg()));
    float d = s.sector_start_deg;
    for (int i = 0; i <= width_steps; ++i) {
      // Normalize d into [-180, +180], then map to [0, 360] bucket.
      float n = d;
      while (n > 180.0f) {n -= 360.0f;}
      while (n < -180.0f) {n += 360.0f;}
      const int bucket = static_cast<int>(std::round(n + 180.0f));
      if (bucket >= 0 && bucket < 361) {covered[bucket] = true;}
      d += 1.0f;
    }
  }
  int hit = 0;
  for (int i = 0; i < 360; ++i) {
    if (covered[i]) {
      ++hit;                                            // 0..359
    }
  }
  return static_cast<float>(hit) / 360.0f;
}

}  // namespace

TEST(SectorAllocator, A9_ReconUnionCoverageAtLeast95Percent) {
  AllocatorInput in;
  in.robots = {{1, RobotRole::Leader, true}, {2, RobotRole::Hub, true}};
  for (uint32_t i = 3; i <= 8; ++i) {
    in.robots.push_back({i, RobotRole::Follower, true});
  }
  in.mode = SurveillanceMode::Recon;
  auto out = allocateSectors(in);

  const float frac = unionCoverageFraction(out);
  EXPECT_GE(frac, 0.95f)
    << "Recon 8-robot union coverage = " << (frac * 100.0f)
    << "% — must be ≥ 95% (effectively full 360° with overlap "
    "headroom per SDD §8.1).";
}

TEST(SectorAllocator, A10_ThreatFocusUnionCoverageAtLeast80Percent) {
  AllocatorInput in;
  in.robots = {{1, RobotRole::Leader, true}, {2, RobotRole::Hub, true}};
  for (uint32_t i = 3; i <= 8; ++i) {
    in.robots.push_back({i, RobotRole::Follower, true});
  }
  in.mode = SurveillanceMode::Recon;
  in.threat_bearings_deg = {-90.0f};  // left flank
  auto out = allocateSectors(in);

  const float frac = unionCoverageFraction(out);
  EXPECT_GE(frac, 0.80f)
    << "Threat-focus 8-robot union coverage = " << (frac * 100.0f)
    << "% — must be ≥ 80% per SDD §8.6.1 (3 followers concentrate "
    "on threat, but the remaining sectors must still cover most "
    "of the perimeter — leaving a >20% blind arc on the "
    "non-threat side would be operationally unacceptable).";

  // Additionally pin: at LEAST 3 sectors are assigned PRIORITY_THREAT_FOCUS.
  int n_focus = 0;
  for (const auto & s : out) {
    if (s.priority == 1) {
      ++n_focus;
    }
  }
  EXPECT_EQ(n_focus, 3)
    << "exactly 3 followers should be re-pointed to threat per "
    "SDD §8.6.1 (got " << n_focus << ")";
}

// ─── Pan-Tilt Controller ───────────────────────────────────────────────

TEST(PanTilt, S1_SweepDpsMatchesSDDExample) {
  // SDD §8.4 example: 90° sector, 60° HFOV, 6s → ω = 10°/s
  PanTiltConfig cfg;
  cfg.hfov_deg = 60.0f;
  cfg.sweep_period_s = 6.0f;
  cfg.sweep_max_dps = 100.0f;    // unsaturated
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
  cfg.sweep_period_s = 6.0f;
  cfg.sweep_max_dps = 30.0f;
  cfg.sweep_night_dps = 15.0f;
  // 360° sector — would be (360-60)*2/6 = 100°/s, clamp to 30/15
  EXPECT_FLOAT_EQ(computeSweepDps(cfg, 360.0f, false), 30.0f);
  EXPECT_FLOAT_EQ(computeSweepDps(cfg, 360.0f, true), 15.0f);
  // Smaller-than-HFOV sector → 0
  EXPECT_FLOAT_EQ(computeSweepDps(cfg, 50.0f, false), 0.0f);
}

TEST(PanTilt, S3_SweepBouncesWithinBounds) {
  PanTiltConfig cfg;
  PanTiltState st;
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
  cfg.track_kp_pan = 1.5f;
  cfg.track_kp_tilt = 1.5f;
  cfg.track_dead_zone = 0.02f;     // 0.02° < KPP 0.05°
  cfg.track_max_dps = 60.0f;

  PanTiltState st;
  st.mode = PanTiltMode::Track;
  st.pan_deg = 0.0f;
  st.tilt_deg = 0.0f;

  TrackTarget t{ /*bearing=*/ 5.0f, /*elevation=*/ 2.0f};

  // Run 4 seconds at 50 Hz (dt=0.02s) → controller should converge
  for (int i = 0; i < 200; ++i) {
    stepController(cfg, st, 0.02f, std::nullopt, t, std::nullopt);
  }

  // Final residual should be ≤ 0.05° (KPP)
  EXPECT_LE(st.last_track_error_deg, 0.05f)
    << "Track residual " << st.last_track_error_deg
    << " > KPP 0.05°";
  // Pan/Tilt should be close to target
  EXPECT_NEAR(st.pan_deg, 5.0f, 0.05f);
  EXPECT_NEAR(st.tilt_deg, 2.0f, 0.05f);
}

TEST(PanTilt, T2_TrackDeadZonePreventsJitter) {
  PanTiltConfig cfg;
  cfg.track_dead_zone = 0.05f;     // 0.05° dead zone
  PanTiltState st;
  st.mode = PanTiltMode::Track;
  st.pan_deg = 5.0f;
  st.tilt_deg = 2.0f;
  // Target inside dead zone
  TrackTarget t{5.02f, 2.01f};
  auto out = stepController(cfg, st, 0.02f, std::nullopt, t, std::nullopt);
  // Should report zero speed and not move
  EXPECT_FLOAT_EQ(out.speed_dps, 0.0f);
  EXPECT_FLOAT_EQ(st.pan_deg, 5.0f);
  EXPECT_FLOAT_EQ(st.tilt_deg, 2.0f);
}

TEST(PanTilt, E1_EngageLeadsTarget) {
  PanTiltConfig cfg;
  cfg.track_dead_zone = 0.001f;    // very tight to see lead effect
  PanTiltState st;
  st.mode = PanTiltMode::Engage;
  st.pan_deg = 0.0f;
  st.tilt_deg = 0.0f;

  EngageTarget e;
  e.bearing_deg = 10.0f;
  e.elevation_deg = 0.0f;
  e.bearing_rate_dps = 5.0f;       // target moving 5°/s right
  e.elevation_rate_dps = 0.0f;

  // 1 step of 0.5s — lead = 10 + 5*0.5 = 12.5
  // After convergence at this lead, pan should be near 12.5
  for (int i = 0; i < 100; ++i) {
    stepController(cfg, st, 0.02f, std::nullopt, std::nullopt, e);
  }
  // Pan should track approximately to a lead-compensated steady state
  EXPECT_GT(st.pan_deg, 10.5f) << "Engage didn't lead target";
}

// ─── Phase 0 PR-C: Track mode must honor the configured tilt envelope.
// Pre-patch, stepTrack clamped tilt to [tilt_min-20, tilt_max+50]
// regardless of cfg. For a weapon-mount pan-tilt this allowed the
// muzzle to swing 50° above the configured upper limit (e.g. +60°
// when the documented max is +10°) — over friendly heads.

TEST(PanTilt, PC_TrackTiltClampedToConfiguredEnvelope) {
  PanTiltConfig cfg;
  cfg.tilt_min_deg = -10.0f;
  cfg.tilt_max_deg = 10.0f;
  cfg.track_dead_zone = 0.001f;
  cfg.track_kp_pan = 1.0f;
  cfg.track_kp_tilt = 1.0f;
  cfg.track_max_dps = 120.0f;

  PanTiltState st;
  st.mode = PanTiltMode::Track;
  st.pan_deg = 0.0f;
  st.tilt_deg = 0.0f;

  // Aim WAY above the upper envelope.
  TrackTarget t{0.0f, 60.0f};

  // Run long enough for the controller to drive tilt up. The clamp
  // must keep it at cfg.tilt_max_deg, NOT cfg.tilt_max_deg + 50.
  for (int i = 0; i < 500; ++i) {
    stepController(cfg, st, 0.02f, std::nullopt, t, std::nullopt);
  }

  EXPECT_LE(st.tilt_deg, cfg.tilt_max_deg + 1e-3f)
    << "Track-mode tilt exceeded configured tilt_max_deg "
    << cfg.tilt_max_deg << " (got " << st.tilt_deg << ")";
}

TEST(PanTilt, PC_TrackTiltClampedToLowerEnvelope) {
  PanTiltConfig cfg;
  cfg.tilt_min_deg = -10.0f;
  cfg.tilt_max_deg = 10.0f;
  cfg.track_dead_zone = 0.001f;
  cfg.track_kp_pan = 1.0f;
  cfg.track_kp_tilt = 1.0f;
  cfg.track_max_dps = 120.0f;

  PanTiltState st;
  st.mode = PanTiltMode::Track;
  st.pan_deg = 0.0f;
  st.tilt_deg = 0.0f;

  // Aim WAY below the lower envelope.
  TrackTarget t{0.0f, -60.0f};
  for (int i = 0; i < 500; ++i) {
    stepController(cfg, st, 0.02f, std::nullopt, t, std::nullopt);
  }
  EXPECT_GE(st.tilt_deg, cfg.tilt_min_deg - 1e-3f)
    << "Track-mode tilt undershot configured tilt_min_deg "
    << cfg.tilt_min_deg << " (got " << st.tilt_deg << ")";
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
  EXPECT_FLOAT_EQ(normalizeAngle(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(180.0f), 180.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(-180.0f), -180.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(190.0f), -170.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(-190.0f), 170.0f);
  EXPECT_FLOAT_EQ(normalizeAngle(360.0f), 0.0f);
}

TEST(Geometry, AngularDifferenceShortestPath) {
  EXPECT_FLOAT_EQ(angularDifference(0.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(angularDifference(170.0f, -170.0f), 20.0f);   // wraps
  EXPECT_FLOAT_EQ(angularDifference(90.0f, -90.0f), 180.0f);
  EXPECT_FLOAT_EQ(angularDifference(45.0f, 45.0f), 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════
// PATCH 2026-05-13 — new tests covering deep-dive fixes
// ═══════════════════════════════════════════════════════════════════════

#include "san_surveillance/sector_frame.hpp"

// ─── PS1: World-frame sector anchors to leader yaw ──────────────────────
// Issue C1: previously sectors were always heading-frame, so robots
// driving forward (and rotating) lost absolute 360° coverage.
TEST(PatchFrame, PS1_WorldFrameAnchorsToLeaderYaw) {
  AllocatorInput in;
  in.robots = {
    {1, RobotRole::Leader, true, 90.0f},     // leader yaw = 90° (facing +y)
    {2, RobotRole::Hub, true, 90.0f},
    {3, RobotRole::Follower, true, 90.0f},
    {4, RobotRole::Follower, true, 90.0f},
  };
  in.mode = SurveillanceMode::Recon;
  in.output_frame = SectorFrame::World;
  in.leader_yaw_world_deg = 90.0f;
  auto sectors = allocateSectors(in);
  ASSERT_FALSE(sectors.empty());

  // The leader's sector should be centred on 90° (world), NOT 0°.
  for (const auto & s : sectors) {
    if (s.robot_id == 1) {
      EXPECT_NEAR(s.centreDeg(), 90.0f, 1.0f);
      EXPECT_EQ(s.frame, SectorFrame::World);
      return;
    }
  }
  FAIL() << "Leader sector not found";
}

// ─── PS2: Heading-frame still works (default behavior) ─────────────────
TEST(PatchFrame, PS2_HeadingFrameUnchanged) {
  AllocatorInput in;
  in.robots = {
    {1, RobotRole::Leader, true},
    {2, RobotRole::Hub, true},
    {3, RobotRole::Follower, true},
    {4, RobotRole::Follower, true},
  };
  in.mode = SurveillanceMode::Recon;
  // output_frame defaults to Heading
  auto sectors = allocateSectors(in);
  for (const auto & s : sectors) {
    if (s.robot_id == 1) {
      EXPECT_NEAR(s.centreDeg(), 0.0f, 1.0f);   // front
      EXPECT_EQ(s.frame, SectorFrame::Heading);
      return;
    }
  }
  FAIL() << "Leader sector not found";
}

// ─── PS3: World→Heading transform inverse ──────────────────────────────
TEST(PatchFrame, PS3_WorldHeadingTransformInverse) {
  // If a robot at yaw=45° converts world-bearing 90° to heading, then
  // converts back, we should get 90° again.
  const float robot_yaw = 45.0f;
  const float world = 90.0f;
  const float heading = worldToHeading(world, robot_yaw);
  EXPECT_NEAR(heading, 45.0f, 1e-3f);
  const float back = headingToWorld(heading, robot_yaw);
  EXPECT_NEAR(back, world, 1e-3f);
}

// ─── PS4: DriveClassifier hysteresis ───────────────────────────────────
TEST(PatchFrame, PS4_DriveClassifierHysteresis) {
  DriveClassifier::Config cfg;
  cfg.enter_drive_mps = 0.3f;
  cfg.exit_drive_mps = 0.1f;
  DriveClassifier c(cfg);
  EXPECT_EQ(c.state(), DriveClassifier::State::Patrol);

  // Speed below enter → stays Patrol.
  c.update({0.2f, 0.0f});
  EXPECT_EQ(c.state(), DriveClassifier::State::Patrol);

  // Speed above enter → Drive.
  c.update({0.4f, 0.0f});
  EXPECT_EQ(c.state(), DriveClassifier::State::Drive);

  // Speed between exit and enter → stays Drive (hysteresis).
  c.update({0.2f, 0.0f});
  EXPECT_EQ(c.state(), DriveClassifier::State::Drive);

  // Speed below exit → Patrol.
  c.update({0.05f, 0.0f});
  EXPECT_EQ(c.state(), DriveClassifier::State::Patrol);

  EXPECT_EQ(c.transitionCount(), 2u);
}

// ─── PT1 (★ M4 fix): Track control dt-INDEPENDENT ───────────────────────
// Run the same target through Track at 10Hz and 100Hz; final state
// should be within a small tolerance — proving the gain is no longer
// scaled by dt.
TEST(PatchPanTilt, PT1_TrackDtIndependent) {
  PanTiltConfig cfg;
  cfg.track_kp_pan = 4.0f;     // patched default
  cfg.track_kp_tilt = 4.0f;
  cfg.track_dead_zone = 0.001f;
  TrackTarget tgt{30.0f, 5.0f};

  // Run at 10 Hz for 2 seconds.
  PanTiltState s10;
  s10.mode = PanTiltMode::Track;
  for (int i = 0; i < 20; ++i) {
    stepController(cfg, s10, 0.1f, std::nullopt, tgt, std::nullopt);
  }

  // Run at 100 Hz for 2 seconds.
  PanTiltState s100;
  s100.mode = PanTiltMode::Track;
  for (int i = 0; i < 200; ++i) {
    stepController(cfg, s100, 0.01f, std::nullopt, tgt, std::nullopt);
  }

  // Both should converge close to the target — within a small bound.
  EXPECT_NEAR(s10.pan_deg, tgt.bearing_deg, 0.5f);
  EXPECT_NEAR(s100.pan_deg, tgt.bearing_deg, 0.5f);
  EXPECT_NEAR(s10.tilt_deg, tgt.elevation_deg, 0.5f);
  // ★ The two should agree within 5% — proving dt-independence.
  EXPECT_NEAR(s10.pan_deg, s100.pan_deg, 1.5f);
}

// ─── PT2 (★ M5 fix): Track tilt clamped to cfg envelope ─────────────────
// Aiming far above the configured Track max should clamp at
// track_tilt_max_deg, not silently exceed.
TEST(PatchPanTilt, PT2_TrackTiltClampToEnvelope) {
  PanTiltConfig cfg;
  cfg.track_tilt_max_deg = 60.0f;
  cfg.track_dead_zone = 0.001f;
  TrackTarget tgt{0.0f, 200.0f};   // impossibly high

  PanTiltState s;
  s.mode = PanTiltMode::Track;
  for (int i = 0; i < 500; ++i) {
    stepController(cfg, s, 0.01f, std::nullopt, tgt, std::nullopt);
  }
  EXPECT_LE(s.tilt_deg, cfg.track_tilt_max_deg + 0.1f);
}

// ─── PT3 (★ M6 fix): Sweep boundary residual preserved ──────────────────
// Send a huge dt that crosses the right boundary; the residual motion
// should bounce back, not be silently dropped.
TEST(PatchPanTilt, PT3_SweepBoundaryResidualPreserved) {
  PanTiltConfig cfg;
  cfg.sweep_period_s = 6.0f;
  cfg.sweep_max_dps = 30.0f;
  cfg.hfov_deg = 60.0f;
  SweepParams p{0.0f, 180.0f, false};   // ±90° from centre 0, width 180°
  PanTiltState s;
  s.mode = PanTiltMode::Sweep;
  s.pan_deg = 80.0f;                     // near right boundary (+90)
  s.sweep_going_right = true;

  // Huge dt — 5 s × 30 °/s = 150°. Starting at 80° hits 90° after
  // 10°, then must reflect back to 90 - 140 = -50°.
  stepController(cfg, s, 5.0f, p);
  EXPECT_FALSE(s.sweep_going_right);     // bounced
  EXPECT_NEAR(s.pan_deg, -50.0f, 1.0f);  // residual consumed correctly
}

// ─── PS5: integration — World-frame sector → per-robot heading bearing ──
// When the allocator emits a world-frame sector and the consumer
// (surveillance_node publishAssignments) transforms via the robot's
// yaw, the result should put pan-tilt onto the same WORLD bearing
// regardless of how the robot is rotated.
TEST(PatchFrame, PS5_WorldSectorTransformsToCorrectHeadingPerRobot) {
  // Two followers, both pointing at different world yaws.
  // World-frame sector centred on +120° (some absolute direction).
  // Follower #3 at yaw=0°    → heading-frame target = 120°
  // Follower #4 at yaw=+60°  → heading-frame target = 60°
  // Both look at the same world bearing, just via different body angles.
  AllocatorInput in;
  in.robots = {
    {1, RobotRole::Leader, true, 0.0f},
    {3, RobotRole::Follower, true, 0.0f},
    {4, RobotRole::Follower, true, 60.0f},
  };
  in.mode = SurveillanceMode::Recon;
  in.output_frame = SectorFrame::World;
  in.leader_yaw_world_deg = 0.0f;
  in.threat_bearings_deg = {120.0f};// force focus
  auto sectors = allocateSectors(in);

  bool found_3 = false, found_4 = false;
  for (const auto & s : sectors) {
    if (s.priority != 1 /*THREAT_FOCUS*/) {continue;}
    const float world_centre = s.centreDeg();
    EXPECT_NEAR(world_centre, 120.0f, 1.0f);

    // Apply transform like surveillance_node would.
    if (s.robot_id == 3) {
      const float heading = worldToHeading(world_centre, 0.0f);
      EXPECT_NEAR(heading, 120.0f, 1.0f);
      found_3 = true;
    } else if (s.robot_id == 4) {
      const float heading = worldToHeading(world_centre, 60.0f);
      EXPECT_NEAR(heading, 60.0f, 1.0f);
      found_4 = true;
    }
  }
  EXPECT_TRUE(found_3);
  EXPECT_TRUE(found_4);
}

}  // namespace
}  // namespace san_surveillance
