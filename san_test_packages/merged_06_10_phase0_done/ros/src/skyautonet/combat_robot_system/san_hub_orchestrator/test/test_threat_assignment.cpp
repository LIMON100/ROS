// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.
#include <gtest/gtest.h>
#include "san_hub_orchestrator/threat_assignment.hpp"

using san_hub_orchestrator::assignWithCap;

TEST(ThreatAssignment, FourRobotsTwoThreatsSplit2plus2)
{
  std::vector<std::vector<float>> cost = {
    {1.0f, 9.0f}, {1.5f, 8.0f},   // robots 0,1 near target 0
    {9.0f, 1.0f}, {8.0f, 1.5f},   // robots 2,3 near target 1
  };
  auto a = assignWithCap(cost);
  ASSERT_EQ(a.size(), 4u);
  EXPECT_EQ(a[0], 0);
  EXPECT_EQ(a[1], 0);
  EXPECT_EQ(a[2], 1);
  EXPECT_EQ(a[3], 1);
}

TEST(ThreatAssignment, CapForcesBalanceWhenAllNearOneTarget)
{
  std::vector<std::vector<float>> cost = {
    {1.0f, 5.0f},
    {1.1f, 5.0f},
    {1.2f, 5.0f},
    {1.3f, 5.0f},
  };
  auto a = assignWithCap(cost);
  int c0 = 0, c1 = 0;
  for (int v : a) {
    (v == 0 ? c0 : c1)++;
  }
  EXPECT_EQ(c0, 2);          // cap=2 forces 2+2 even though all prefer target 0
  EXPECT_EQ(c1, 2);
}