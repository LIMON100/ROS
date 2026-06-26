// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 PHASE 8 - BatteryMonitor unit test.
//
// Verifies the 3rd-tier (battery-max follower) selection logic in
// isolation: ranking, tie-breaking by lower robot_id, exclusion of
// Leader/Hub/Deputy, and the failure detection helpers.

#include <gtest/gtest.h>

#include "san_role_management/battery_monitor.hpp"
#include "san_role_management/role_types.hpp"

using namespace san_role_management;

namespace
{

BatterySnapshot makeSnap(
  uint32_t id, float pct, bool sbc1 = true,
  bool sbc2 = true)
{
  BatterySnapshot s;
  s.robot_id = id;
  s.battery_percent = pct;
  s.sbc1_healthy = sbc1;
  s.sbc2_healthy = sbc2;
  return s;
}

}  // namespace

TEST(BatteryMonitor, EmptyReturnsZero) {
  BatteryMonitor mon;
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, 10.0f), 0u);
}

TEST(BatteryMonitor, PicksHighestBatteryFollower) {
  BatteryMonitor mon;
  mon.update(makeSnap(4, 60.f));
  mon.update(makeSnap(5, 90.f));
  mon.update(makeSnap(6, 45.f));
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);
}

TEST(BatteryMonitor, ExcludesLeaderHubDeputy) {
  BatteryMonitor mon;
  mon.update(makeSnap(1, 99.f));      // Leader - excluded
  mon.update(makeSnap(2, 95.f));      // Hub    - excluded
  mon.update(makeSnap(3, 92.f));      // Deputy - excluded
  mon.update(makeSnap(4, 40.f));
  mon.update(makeSnap(5, 80.f));
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);
}

TEST(BatteryMonitor, TieResolvedByLowerRobotId) {
  BatteryMonitor mon;
  mon.update(makeSnap(7, 75.f));
  mon.update(makeSnap(4, 75.f));        // tie with 7, lower id wins
  mon.update(makeSnap(8, 75.f));
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 4u);
}

TEST(BatteryMonitor, BelowMinimumBatteryIsSkipped) {
  BatteryMonitor mon;
  mon.update(makeSnap(4, 5.f));         // below 10% floor
  mon.update(makeSnap(5, 8.f));         // below 10% floor
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 0u);
}

TEST(BatteryMonitor, UnhealthySbcSkipped) {
  BatteryMonitor mon;
  mon.update(makeSnap(4, 90.f, /*sbc1=*/ false, /*sbc2=*/ true));
  mon.update(makeSnap(5, 50.f, /*sbc1=*/ true, /*sbc2=*/ true));
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);
}

TEST(BatteryMonitor, HubFailedDetection) {
  BatteryMonitor mon;
  EXPECT_TRUE(mon.isHubFailed(2));      // unseen = failed
  mon.update(makeSnap(2, 50.f, true, true));
  EXPECT_FALSE(mon.isHubFailed(2));
  mon.update(makeSnap(2, 50.f, false, false));
  EXPECT_TRUE(mon.isHubFailed(2));
  mon.update(makeSnap(2, 50.f, true, false));     // one SBC OK
  EXPECT_FALSE(mon.isHubFailed(2));
}

TEST(BatteryMonitor, DeputyFailedDetection) {
  BatteryMonitor mon;
  EXPECT_TRUE(mon.isDeputyFailed(3));
  mon.update(makeSnap(3, 50.f, true, true));
  EXPECT_FALSE(mon.isDeputyFailed(3));
}

// ─── Phase-7 audit P1 — boundary/oscillation invariants ─────────────
//
// The earlier suite covered selection correctness but not the
// dynamic / boundary cases that drive real-field oscillation:
//
//   B1  Battery at exact minimum is INCLUDED (boundary inclusivity).
//       The selection rule is ">=" per BelowMinimumBatteryIsSkipped's
//       inverse — pin both sides explicitly to guard against the
//       "exact threshold" off-by-one (production failure mode:
//       borderline robot toggles between selected and rejected on
//       every 0.01% drift, triggering succession churn).
//
//   B2  Dynamic battery drop — same robot updated with progressively
//       lower battery; selection result changes accordingly. Pins
//       the invariant that update() REPLACES rather than accumulates
//       (no stale-snapshot retention).
//
//   B3  Selection consistency under interleaved updates — robot A
//       update, robot B update, robot A update again with lower
//       battery; result reflects the LATEST snapshot for each robot
//       (per-robot last-write-wins).

TEST(BatteryMonitor, B1_BoundaryAtExactMinimumIncluded) {
  BatteryMonitor mon;
  // Floor in MIN_BATTERY_FOLLOWER from BelowMinimumBatteryIsSkipped
  // is 10% (per the BatterySelection helper); test the exact value
  // is treated as INCLUDED (selection uses >= bound).
  mon.update(makeSnap(4, MIN_BATTERY_FOLLOWER));     // exactly at min
  mon.update(makeSnap(5, 5.f));                       // below floor
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 4u)
    << "Boundary inclusivity broken: a follower at EXACTLY "
    << "MIN_BATTERY_FOLLOWER (" << MIN_BATTERY_FOLLOWER
    << "%) must be selectable. Off-by-one here causes oscillation "
    "of Leader succession when borderline battery drifts by "
    "0.01% across the threshold.";
}

TEST(BatteryMonitor, B2_DynamicBatteryDropChangesSelection) {
  BatteryMonitor mon;
  // Initial: robot 5 has highest battery — should be selected.
  mon.update(makeSnap(4, 50.f));
  mon.update(makeSnap(5, 90.f));
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);

  // Robot 5's battery drops below 4's — selection must switch.
  // update() must REPLACE the previous snapshot (not accumulate or
  // average); otherwise the selection would be stuck on stale data.
  mon.update(makeSnap(5, 30.f));
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 4u)
    << "Robot 5 dropped from 90% to 30%; selection should switch "
    "to robot 4 (50%). If still 5, update() is retaining "
    "stale data — would freeze Leader on a depleted robot.";

  // Drop below threshold entirely — no candidate available.
  // Both robots 4 and 5 must drop below MIN_BATTERY_FOLLOWER; robot
  // 5 was at 30% from the previous step and would otherwise remain
  // selectable.
  mon.update(makeSnap(4, 5.f));
  mon.update(makeSnap(5, 8.f));
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 0u)
    << "All followers now below MIN_BATTERY_FOLLOWER; selection "
    "must return 0 (no eligible candidate) — caller routes to "
    "Limp Mode at this point.";
}

TEST(BatteryMonitor, B3_InterleavedUpdatesLastWriteWinsPerRobot) {
  BatteryMonitor mon;
  // Sequence: r4=50%, r5=80%, r4=20%
  // Expected: r5 selected (its 80% > r4's 20% — r4's latest wins)
  mon.update(makeSnap(4, 50.f));
  mon.update(makeSnap(5, 80.f));
  mon.update(makeSnap(4, 20.f));     // r4 update REPLACES r4's 50%

  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);

  // Verify r4 was updated, not added — change r4 again to LOWER
  // than r5 and confirm r5 still wins.
  mon.update(makeSnap(4, 70.f));     // now > MIN but still < 80
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 5u);

  // Push r4 above r5 — r4 should win now.
  mon.update(makeSnap(4, 95.f));
  EXPECT_EQ(mon.pickMaxBatteryFollower(1, 2, 3, MIN_BATTERY_FOLLOWER), 4u);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
