#include <cstdint>

#include <gtest/gtest.h>

#include "combat_robot_nav2/early_column.hpp"

using combat_robot_nav2::EarlyColumnLatch;
using combat_robot_nav2::effectiveFormationMode;
using combat_robot_nav2::kEarlyColumnMode;

// ---- effectiveFormationMode -------------------------------------------------

TEST(EffectiveFormationMode, OverrideForcesColumn)
{
  // While the leader's obstacle-pass override is active, every robot assumes the
  // COLUMN mode regardless of what the FSM/operator commanded.
  EXPECT_EQ(effectiveFormationMode(true, 3 /*DIAMOND mode*/), kEarlyColumnMode);
  EXPECT_EQ(effectiveFormationMode(true, 0 /*ABREAST*/), kEarlyColumnMode);
  EXPECT_EQ(effectiveFormationMode(true, 2 /*WEDGE*/), kEarlyColumnMode);
}

TEST(EffectiveFormationMode, InactivePassesThroughFsmMode)
{
  EXPECT_EQ(effectiveFormationMode(false, 3), 3);
  EXPECT_EQ(effectiveFormationMode(false, 0), 0);
  EXPECT_EQ(effectiveFormationMode(false, kEarlyColumnMode), kEarlyColumnMode);
}

// ---- EarlyColumnLatch -------------------------------------------------------

TEST(EarlyColumnLatch, EngagesAfterPersistTicks)
{
  EarlyColumnLatch latch;
  // Obstacle at 12 m, lookahead 15 m, needs 3 consecutive ticks.
  EXPECT_FALSE(latch.update(12.0, 15.0, 3, false));   // streak 1
  EXPECT_FALSE(latch.update(12.0, 15.0, 3, false));   // streak 2
  EXPECT_TRUE(latch.update(12.0, 15.0, 3, false));    // streak 3 → engage
  EXPECT_TRUE(latch.active());
}

TEST(EarlyColumnLatch, OutOfRangeResetsStreak)
{
  EarlyColumnLatch latch;
  EXPECT_FALSE(latch.update(12.0, 15.0, 3, false));   // streak 1
  EXPECT_FALSE(latch.update(20.0, 15.0, 3, false));   // too far → streak 0
  EXPECT_FALSE(latch.update(12.0, 15.0, 3, false));   // streak 1 again
  EXPECT_FALSE(latch.update(12.0, 15.0, 3, false));   // streak 2
  EXPECT_TRUE(latch.update(12.0, 15.0, 3, false));    // streak 3 → engage
}

TEST(EarlyColumnLatch, NonPositiveDistIsNoObstacle)
{
  EarlyColumnLatch latch;
  EXPECT_FALSE(latch.update(-1.0, 15.0, 3, false));
  EXPECT_FALSE(latch.update(0.0, 15.0, 3, false));
  EXPECT_FALSE(latch.update(-1.0, 15.0, 3, false));
  EXPECT_FALSE(latch.active());
}

TEST(EarlyColumnLatch, StaysActiveUntilLeaderClear)
{
  EarlyColumnLatch latch;
  latch.update(10.0, 15.0, 1, false);   // engage immediately (persist 1)
  ASSERT_TRUE(latch.active());
  // Obstacle vanishes from view but leader has NOT yet passed it → stay COLUMN
  // (followers still threading behind the leader).
  EXPECT_TRUE(latch.update(-1.0, 15.0, 1, false));
  EXPECT_TRUE(latch.update(-1.0, 15.0, 1, false));
  // Leader clears the obstacle by margin → release.
  EXPECT_FALSE(latch.update(-1.0, 15.0, 1, true));
  EXPECT_FALSE(latch.active());
}

TEST(EarlyColumnLatch, ReengagesOnNextObstacleAfterClear)
{
  EarlyColumnLatch latch;
  latch.update(10.0, 15.0, 1, false);   // engage
  latch.update(-1.0, 15.0, 1, true);    // release
  ASSERT_FALSE(latch.active());
  // A new obstacle appears later → engages again.
  EXPECT_TRUE(latch.update(9.0, 15.0, 1, false));
  EXPECT_TRUE(latch.active());
}

TEST(EarlyColumnLatch, ResetClearsState)
{
  EarlyColumnLatch latch;
  latch.update(10.0, 15.0, 1, false);
  ASSERT_TRUE(latch.active());
  latch.reset();
  EXPECT_FALSE(latch.active());
}
