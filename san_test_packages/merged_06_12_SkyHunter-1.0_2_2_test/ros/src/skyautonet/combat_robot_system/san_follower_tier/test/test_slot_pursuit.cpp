#include "san_follower_tier/slot_pursuit.hpp"
#include <gtest/gtest.h>

namespace san_follower_tier
{

TEST(SlotPursuit, ArrivedWithinStopDistanceHolds)
{
  SlotGains g;
  auto c = computeSlotCmd(0.0, 0.0, 0.0, 0.2, 0.0, g);  // 0.2 < stop 0.4
  EXPECT_TRUE(c.arrived);
  EXPECT_DOUBLE_EQ(c.linear, 0.0);
  EXPECT_DOUBLE_EQ(c.angular, 0.0);
}

TEST(SlotPursuit, DrivesForwardWhenAligned)
{
  SlotGains g;
  auto c = computeSlotCmd(0.0, 0.0, 0.0, 5.0, 0.0, g);  // target dead ahead
  EXPECT_GT(c.linear, 0.0);
  EXPECT_NEAR(c.angular, 0.0, 1e-6);
}

TEST(SlotPursuit, TurnsTowardTargetOffBearing)
{
  SlotGains g;
  auto c = computeSlotCmd(0.0, 0.0, 0.0, 0.0, 5.0, g);  // target to the left
  EXPECT_GT(c.angular, 0.0);                            // turn left (+)
}

TEST(SlotPursuit, BigHeadingErrorScalesLinearDown)
{
  SlotGains g;
  auto fwd = computeSlotCmd(0.0, 0.0, 0.0, 5.0, 0.0, g);
  auto side = computeSlotCmd(0.0, 0.0, 0.0, 0.0, 5.0, g);  // 90° off
  EXPECT_LT(side.linear, fwd.linear);                      // turn-in-place priority
}

}  // namespace san_follower_tier