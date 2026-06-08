// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — LOC-1 follow-up: NavSatFix position-covariance scaling tests.
//
// applyPositionCovariance() fills NavSatFix.position_covariance from RTK
// fix quality so robot_localization down-weights RTK_FLOAT instead of
// snapping to a noisy/jumpy fix. Coverage (pure helper — no node/serial):
//   P1  RTK_FIX   → tight (2 cm) diagonal, type DIAGONAL_KNOWN
//   P2  RTK_FLOAT → ~225× larger than FIX (30 cm vs 2 cm)
//   P3  monotonic: FIX < FLOAT < DGPS < autonomous
//   P4  diagonal only (off-diagonal stays zero)
//   P5  unknown/no-fix fix_type → autonomous (large) bucket, not zero

#include "san_rtk_gnss/rtk_gnss_node.hpp"
#include "san_rtk_gnss/nmea_parser.hpp"   // FixType enum

#include <gtest/gtest.h>
#include <sensor_msgs/msg/nav_sat_fix.hpp>

namespace san_rtk_gnss
{
namespace
{

double xyVar(uint8_t fix_type)
{
  sensor_msgs::msg::NavSatFix fix;
  RtkGnssNode::applyPositionCovariance(fix, fix_type);
  return fix.position_covariance[0];
}

}  // namespace

TEST(PositionCovariance, P1_RtkFixIsTightAndKnown) {
  sensor_msgs::msg::NavSatFix fix;
  RtkGnssNode::applyPositionCovariance(
    fix, static_cast<uint8_t>(FixType::RtkFix));
  EXPECT_DOUBLE_EQ(fix.position_covariance[0], 0.02 * 0.02);   // E
  EXPECT_DOUBLE_EQ(fix.position_covariance[4], 0.02 * 0.02);   // N
  EXPECT_DOUBLE_EQ(fix.position_covariance[8], 0.05 * 0.05);   // U
  EXPECT_EQ(
    fix.position_covariance_type,
    sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN);
}

TEST(PositionCovariance, P2_FloatIsHeavilyDownWeightedVsFix) {
  const double fix_var = xyVar(static_cast<uint8_t>(FixType::RtkFix));
  const double float_var = xyVar(static_cast<uint8_t>(FixType::RtkFloat));
  // 0.30² / 0.02² = 225×.
  EXPECT_GT(float_var, fix_var * 100.0);
  EXPECT_NEAR(float_var / fix_var, 225.0, 1.0);
}

TEST(PositionCovariance, P3_MonotonicWithDegradingQuality) {
  const double v_fix = xyVar(static_cast<uint8_t>(FixType::RtkFix));
  const double v_float = xyVar(static_cast<uint8_t>(FixType::RtkFloat));
  const double v_dgps = xyVar(static_cast<uint8_t>(FixType::Dgps));
  const double v_auto = xyVar(static_cast<uint8_t>(FixType::Auto2D));
  EXPECT_LT(v_fix, v_float);
  EXPECT_LT(v_float, v_dgps);
  EXPECT_LT(v_dgps, v_auto);
}

TEST(PositionCovariance, P4_DiagonalOnly) {
  sensor_msgs::msg::NavSatFix fix;
  RtkGnssNode::applyPositionCovariance(
    fix, static_cast<uint8_t>(FixType::RtkFloat));
  for (int i = 0; i < 9; ++i) {
    if (i == 0 || i == 4 || i == 8) {continue;}
    EXPECT_DOUBLE_EQ(fix.position_covariance[i], 0.0) << "i=" << i;
  }
}

TEST(PositionCovariance, P5_UnknownFixTypeIsLargeNotZero) {
  // No-fix / estimated / out-of-range → autonomous (large) bucket so the
  // EKF never sees a zero (perfect-trust) covariance.
  const double v_no = xyVar(static_cast<uint8_t>(FixType::No));
  const double v_est = xyVar(static_cast<uint8_t>(FixType::Estimated));
  const double v_oob = xyVar(250);     // out of enum range
  EXPECT_GT(v_no, 1.0);
  EXPECT_GT(v_est, 1.0);
  EXPECT_GT(v_oob, 1.0);
}

}  // namespace san_rtk_gnss
