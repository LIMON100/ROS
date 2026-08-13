#include <limits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "combat_robot_nav2/swarm_convoy_geometry.hpp"

using combat_robot_nav2::alongTrack;
using combat_robot_nav2::detourFarAlongTrack;
using Poly = std::vector<std::pair<double, double>>;

static const double kNone = -std::numeric_limits<double>::max();

TEST(AlongTrack, ProjectsWorldPointToMetres)
{
  Poly ref;
  for (int i = 0; i <= 10; ++i) { ref.push_back({i * 10.0, 0.0}); }   // x: 0..100
  // Along-track = x-coordinate; lateral offset does not change it.
  EXPECT_NEAR(alongTrack({55.0, 3.0}, ref), 55.0, 1e-6);
  EXPECT_NEAR(alongTrack({55.0, -8.0}, ref), 55.0, 1e-6);
  EXPECT_NEAR(alongTrack({0.0, 0.0}, ref), 0.0, 1e-6);
  // A point BEFORE the path start projects to a negative value (not 0) — this is
  // the property that makes it safe as a gating measure.
  EXPECT_NEAR(alongTrack({-5.0, 0.0}, ref), -5.0, 1e-6);
}

TEST(AlongTrack, DegenerateRefIsZero)
{
  EXPECT_DOUBLE_EQ(alongTrack({1.0, 1.0}, Poly{}), 0.0);
  EXPECT_DOUBLE_EQ(alongTrack({1.0, 1.0}, Poly{{0.0, 0.0}}), 0.0);
}

TEST(DetourFarAlongTrack, NoDeviationReturnsNone)
{
  Poly ref, trace;
  for (int i = 0; i <= 10; ++i) {
    ref.push_back({i * 10.0, 0.0});
    trace.push_back({i * 10.0, 0.0});
  }
  EXPECT_DOUBLE_EQ(detourFarAlongTrack(trace, ref, 1.0), kNone);
}

TEST(DetourFarAlongTrack, ReturnsFarEdgeMetres)
{
  Poly ref, trace;
  for (int i = 0; i <= 10; ++i) { ref.push_back({i * 10.0, 0.0}); }
  trace = ref;
  trace.push_back({40.0, -7.0});    // deviating breadcrumbs at along-track 40 and 60
  trace.push_back({60.0, -7.0});
  // Far edge = the further deviating point → along-track 60.
  EXPECT_NEAR(detourFarAlongTrack(trace, ref, 1.0), 60.0, 1e-6);
}

TEST(DetourFarAlongTrack, ThresholdIgnoresSmallJitter)
{
  Poly ref, trace;
  for (int i = 0; i <= 10; ++i) {
    ref.push_back({i * 10.0, 0.0});
    trace.push_back({i * 10.0, (i % 2) ? 0.3 : -0.3});   // ±0.3 m < 1.0 m thresh
  }
  EXPECT_DOUBLE_EQ(detourFarAlongTrack(trace, ref, 1.0), kNone);
}

TEST(DetourFarAlongTrack, EmptyInputsSafe)
{
  Poly empty;
  Poly ref{{0.0, 0.0}, {10.0, 0.0}};
  EXPECT_DOUBLE_EQ(detourFarAlongTrack(empty, ref, 1.0), kNone);
  EXPECT_DOUBLE_EQ(detourFarAlongTrack(ref, empty, 1.0), kNone);
}
