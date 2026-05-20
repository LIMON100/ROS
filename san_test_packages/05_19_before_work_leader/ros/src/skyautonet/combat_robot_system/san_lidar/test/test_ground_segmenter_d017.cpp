// SAN v1.5.2 - DCN-2026-006 EXT D-017 regression tests.
//
// Verifies that GroundSegmenter populates the fail_reason field and
// increments the cumulative fail/success counters so the driver can
// publish DiagnosticArray + ThreatAlert instead of silently failing.

#include <gtest/gtest.h>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include "san_lidar/ground_segmenter.hpp"

using san_lidar::GroundSegmenter;
using san_lidar::GroundSegmenterParams;
using san_lidar::GroundSegmenterFailReason;

namespace {

pcl::PointCloud<pcl::PointXYZI>::Ptr makeFlatGround() {
    auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    for (float x = -2.0f; x <= 2.0f; x += 0.05f) {
        for (float y = -2.0f; y <= 2.0f; y += 0.05f) {
            pcl::PointXYZI p;
            p.x = x; p.y = y; p.z = 0.0f; p.intensity = 1.0f;
            cloud->points.push_back(p);
        }
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    cloud->is_dense = true;
    return cloud;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr makeCeilingOnly() {
    // All points above the prefilter z_max - so prefilter removes
    // everything and RANSAC has nothing to fit.
    auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    for (float x = -1.0f; x <= 1.0f; x += 0.05f) {
        pcl::PointXYZI p;
        p.x = x; p.y = 0.0f; p.z = 5.0f; p.intensity = 1.0f;
        cloud->points.push_back(p);
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    cloud->is_dense = true;
    return cloud;
}

}  // namespace

// ─── Empty / null input ──────────────────────────────────────────
TEST(GroundSegmenter_D017, NullCloud_ReportsEmptyInput) {
    GroundSegmenter seg;
    pcl::PointCloud<pcl::PointXYZI>::ConstPtr null_cloud;
    const auto r = seg.segment(null_cloud);
    EXPECT_FALSE(r.valid);
    EXPECT_EQ(r.fail_reason, GroundSegmenterFailReason::EMPTY_INPUT);
    EXPECT_EQ(seg.failCount(), 1U);
    EXPECT_EQ(seg.successCount(), 0U);
}

TEST(GroundSegmenter_D017, EmptyCloud_ReportsEmptyInput) {
    GroundSegmenter seg;
    auto empty = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    const auto r = seg.segment(empty);
    EXPECT_FALSE(r.valid);
    EXPECT_EQ(r.fail_reason, GroundSegmenterFailReason::EMPTY_INPUT);
}

// ─── Prefilter drops everything ──────────────────────────────────
TEST(GroundSegmenter_D017, CeilingOnly_ReportsEmptyAfterBand) {
    GroundSegmenterParams params;
    params.prefilter_z_min_m = -0.5f;
    params.prefilter_z_max_m =  3.0f;     // 5 m points filtered out
    GroundSegmenter seg(params);
    const auto r = seg.segment(makeCeilingOnly());
    EXPECT_FALSE(r.valid);
    EXPECT_EQ(r.fail_reason,
              GroundSegmenterFailReason::EMPTY_AFTER_BAND);
}

// ─── Successful fit ──────────────────────────────────────────────
TEST(GroundSegmenter_D017, FlatGround_ReportsNone_IncrementsSuccess) {
    GroundSegmenter seg;
    const auto r = seg.segment(makeFlatGround());
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.fail_reason, GroundSegmenterFailReason::NONE);
    EXPECT_EQ(seg.failCount(), 0U);
    EXPECT_EQ(seg.successCount(), 1U);
}

// ─── Counter accumulation across multiple calls ───────────────────
TEST(GroundSegmenter_D017, Counters_AccumulateAcrossCalls) {
    GroundSegmenter seg;
    // 2 null, 3 success.
    pcl::PointCloud<pcl::PointXYZI>::ConstPtr null_cloud;
    (void)seg.segment(null_cloud);
    (void)seg.segment(null_cloud);
    (void)seg.segment(makeFlatGround());
    (void)seg.segment(makeFlatGround());
    (void)seg.segment(makeFlatGround());
    EXPECT_EQ(seg.failCount(), 2U);
    EXPECT_EQ(seg.successCount(), 3U);
}

// ─── toString mapping ────────────────────────────────────────────
TEST(GroundSegmenter_D017, ToStringCoversAllReasons) {
    using R = GroundSegmenterFailReason;
    EXPECT_STREQ(toString(R::NONE),              "none");
    EXPECT_STREQ(toString(R::EMPTY_INPUT),       "empty_input");
    EXPECT_STREQ(toString(R::EMPTY_AFTER_BAND),  "empty_after_band");
    EXPECT_STREQ(toString(R::RANSAC_NO_INLIERS), "ransac_no_inliers");
    EXPECT_STREQ(toString(R::RANSAC_DEGENERATE), "ransac_degenerate");
}
