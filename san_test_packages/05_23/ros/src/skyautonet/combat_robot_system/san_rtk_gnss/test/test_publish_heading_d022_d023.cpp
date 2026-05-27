// SAN v1.5.2 — DCN-2026-006 EXT D-022 + D-023 publish-side tests.
//
// Coverage (via the buildHeadingMsg pure-logic helper — no node, no
// publisher, no clock state):
//   T1  REP-103 quaternion for 0°  (NMEA North  → REP-103 yaw=π/2)
//   T2  REP-103 quaternion for 90° (NMEA East  → REP-103 yaw=0)
//   T3  REP-103 quaternion for 180°/270° (sanity)
//   T4  ★ D-023 covariance scaling: FIX < FLOAT < UNKNOWN
//   T5  angular_velocity / linear_acceleration covariance[0] == -1
//       (robot_localization "no data" convention)
//   T6  frame_id + stamp passthrough

#include "san_rtk_gnss/rtk_gnss_node.hpp"
#include "san_rtk_gnss/nmea_parser.hpp"   // for FixType enum

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <cmath>

namespace san_rtk_gnss {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;

rclcpp::Time someStamp() {
    return rclcpp::Time(123456789, 0);
}

}  // namespace

// ─── T1: NMEA 0° → REP-103 yaw π/2 → q.z=sin(π/4), q.w=cos(π/4) ──────
TEST(PublishHeadingD022, NmeaNorthMapsToRep103NinetyDegYaw) {
    const auto m = RtkGnssNode::buildHeadingMsg(
        /*heading_deg=*/0.0,
        /*fix_type=*/static_cast<uint8_t>(FixType::RtkFix),
        "gnss_link", someStamp());

    EXPECT_NEAR(m.orientation.x, 0.0, kEps);
    EXPECT_NEAR(m.orientation.y, 0.0, kEps);
    EXPECT_NEAR(m.orientation.z, std::sin(kPi / 4.0), kEps);  // ≈ 0.7071
    EXPECT_NEAR(m.orientation.w, std::cos(kPi / 4.0), kEps);  // ≈ 0.7071
}

// ─── T2: NMEA 90° (East) → REP-103 yaw 0 → q.z=0, q.w=1 ─────────────
TEST(PublishHeadingD022, NmeaEastMapsToRep103Zero) {
    const auto m = RtkGnssNode::buildHeadingMsg(
        90.0,
        static_cast<uint8_t>(FixType::RtkFix),
        "gnss_link", someStamp());

    EXPECT_NEAR(m.orientation.z, 0.0, kEps);
    EXPECT_NEAR(m.orientation.w, 1.0, kEps);
}

// ─── T3: 180° (South) and 270° (West) — sanity ──────────────────────
TEST(PublishHeadingD022, SouthAndWestQuaternions) {
    // 180°: REP-103 yaw = π/2 − π = -π/2 → q.z = sin(-π/4) = -0.707
    auto south = RtkGnssNode::buildHeadingMsg(
        180.0, static_cast<uint8_t>(FixType::RtkFix),
        "gnss_link", someStamp());
    EXPECT_NEAR(south.orientation.z, std::sin(-kPi / 4.0), kEps);
    EXPECT_NEAR(south.orientation.w, std::cos(-kPi / 4.0), kEps);

    // 270°: REP-103 yaw = π/2 − 3π/2 = -π → q.z = sin(-π/2) = -1, q.w = 0
    auto west = RtkGnssNode::buildHeadingMsg(
        270.0, static_cast<uint8_t>(FixType::RtkFix),
        "gnss_link", someStamp());
    EXPECT_NEAR(west.orientation.z, std::sin(-kPi / 2.0), kEps);  // -1
    EXPECT_NEAR(west.orientation.w, std::cos(-kPi / 2.0), kEps);  //  0
}

// ─── T4: ★ D-023 covariance scaling by RTK fix quality ──────────────
TEST(PublishHeadingD023, YawCovarianceScalesByFixQuality) {
    // FIX → tightest
    auto m_fix = RtkGnssNode::buildHeadingMsg(
        123.45, static_cast<uint8_t>(FixType::RtkFix),
        "gnss_link", someStamp());
    EXPECT_DOUBLE_EQ(m_fix.orientation_covariance[8], 0.0017);

    // FLOAT → degraded but useful
    auto m_float = RtkGnssNode::buildHeadingMsg(
        123.45, static_cast<uint8_t>(FixType::RtkFloat),
        "gnss_link", someStamp());
    EXPECT_DOUBLE_EQ(m_float.orientation_covariance[8], 0.030);

    // Single-point / no RTK → effectively suppress in EKF (1.0 rad²)
    auto m_unknown = RtkGnssNode::buildHeadingMsg(
        123.45, static_cast<uint8_t>(FixType::No),
        "gnss_link", someStamp());
    EXPECT_DOUBLE_EQ(m_unknown.orientation_covariance[8], 1.0);

    // DGPS / 2D fixes that aren't RTK_FIX/FLOAT also land in the "1.0"
    // bucket — only RTK_FIX and RTK_FLOAT get scaled covariance.
    auto m_dgps = RtkGnssNode::buildHeadingMsg(
        123.45, static_cast<uint8_t>(FixType::Dgps),
        "gnss_link", someStamp());
    EXPECT_DOUBLE_EQ(m_dgps.orientation_covariance[8], 1.0);

    // Strict monotonicity — required by EKF auto-weighting design.
    EXPECT_LT(m_fix.orientation_covariance[8],
              m_float.orientation_covariance[8]);
    EXPECT_LT(m_float.orientation_covariance[8],
              m_unknown.orientation_covariance[8]);

    // Roll / pitch get a fixed "no info" value regardless of fix.
    for (const auto* m : {&m_fix, &m_float, &m_unknown, &m_dgps}) {
        EXPECT_DOUBLE_EQ(m->orientation_covariance[0], 1.0);  // roll
        EXPECT_DOUBLE_EQ(m->orientation_covariance[4], 1.0);  // pitch
    }
}

// ─── T5: angular_velocity / linear_acceleration "no data" sentinel ──
TEST(PublishHeadingD022, NoDataCovarianceSentinels) {
    auto m = RtkGnssNode::buildHeadingMsg(
        45.0, static_cast<uint8_t>(FixType::RtkFix),
        "gnss_link", someStamp());
    EXPECT_DOUBLE_EQ(m.angular_velocity_covariance[0], -1.0)
        << "robot_localization convention: -1 in [0] means 'no data for this axis'";
    EXPECT_DOUBLE_EQ(m.linear_acceleration_covariance[0], -1.0);
}

// ─── T6: frame_id + stamp pass through unchanged ────────────────────
TEST(PublishHeadingD022, FrameIdAndStampPassThrough) {
    const auto stamp = rclcpp::Time(987654321, 42);
    auto m = RtkGnssNode::buildHeadingMsg(
        180.0, static_cast<uint8_t>(FixType::RtkFix),
        "custom_gnss_frame", stamp);
    EXPECT_EQ(m.header.frame_id, "custom_gnss_frame");
    EXPECT_EQ(rclcpp::Time(m.header.stamp).nanoseconds(),
              stamp.nanoseconds());
}

}  // namespace san_rtk_gnss
