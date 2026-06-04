// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_lidar/ground_segmenter.hpp"

#include <cmath>

#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/passthrough.h>

namespace san_lidar
{

GroundSegmenter::GroundSegmenter(const GroundSegmenterParams & params)
: params_(params)
{}

GroundSegmenterResult GroundSegmenter::segment(
  const pcl::PointCloud<pcl::PointXYZI>::ConstPtr & cloud) const
{
  GroundSegmenterResult r;
  r.ground_points = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  r.obstacle_points = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();

  if (cloud == nullptr || cloud->empty()) {
    // [DCN-2026-006 EXT D-017] Explicit fail reason.
    r.fail_reason = GroundSegmenterFailReason::EMPTY_INPUT;
    ++fail_count_;
    return r;
  }

  // ─── Pre-filter z band ─────────────────────────────────────────
  auto banded = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  {
    pcl::PassThrough<pcl::PointXYZI> pass;
    pass.setInputCloud(cloud);
    pass.setFilterFieldName("z");
    pass.setFilterLimits(
      params_.prefilter_z_min_m,
      params_.prefilter_z_max_m);
    pass.filter(*banded);
  }
  if (banded->empty()) {
    // [DCN-2026-006 EXT D-017] Prefilter dropped everything -
    // sensor likely facing the ceiling or mount offset wrong.
    r.fail_reason = GroundSegmenterFailReason::EMPTY_AFTER_BAND;
    ++fail_count_;
    return r;
  }

  // ─── RANSAC plane fit ──────────────────────────────────────────
  pcl::SACSegmentation<pcl::PointXYZI> seg;
  seg.setModelType(pcl::SACMODEL_PLANE);
  seg.setMethodType(pcl::SAC_RANSAC);
  seg.setMaxIterations(params_.ransac_max_iterations);
  seg.setDistanceThreshold(params_.ransac_distance_threshold_m);
  seg.setOptimizeCoefficients(params_.optimize_coefficients);

  auto coefficients = std::make_shared<pcl::ModelCoefficients>();
  auto inliers = std::make_shared<pcl::PointIndices>();
  seg.setInputCloud(banded);
  seg.segment(*inliers, *coefficients);

  if (inliers->indices.empty()) {
    // [DCN-2026-006 EXT D-017] No plane fit at all.
    *r.obstacle_points = *banded;
    r.fail_reason = GroundSegmenterFailReason::RANSAC_NO_INLIERS;
    ++fail_count_;
    return r;
  }
  if (coefficients->values.size() < 4) {
    // [DCN-2026-006 EXT D-017] Inliers found but plane parameters
    // are degenerate - treat as fail and route everything to the
    // obstacle channel for safety.
    *r.obstacle_points = *banded;
    r.fail_reason = GroundSegmenterFailReason::RANSAC_DEGENERATE;
    ++fail_count_;
    return r;
  }

  pcl::ExtractIndices<pcl::PointXYZI> extract;
  extract.setInputCloud(banded);
  extract.setIndices(inliers);
  extract.setNegative(false);
  extract.filter(*r.ground_points);
  extract.setNegative(true);
  extract.filter(*r.obstacle_points);

  r.ground_normal = Eigen::Vector3f(
    coefficients->values[0],
    coefficients->values[1],
    coefficients->values[2]);

  // [Sanitizer-hardening] RANSAC can return (0,0,0) coefficients on
  // pathological inputs (collinear inliers, all-zero points). Eigen's
  // normalize() then yields NaN components, which propagate through
  // std::abs / std::min / std::max (NaN passes operator< guards) into
  // std::acos and out via slope_pub_ / traversability cost arithmetic.
  // Reject the degenerate case before normalize() rather than after.
  if (!r.ground_normal.allFinite() ||
    r.ground_normal.squaredNorm() < 1e-6f)
  {
    *r.obstacle_points = *banded;
    r.ground_points->clear();
    r.fail_reason = GroundSegmenterFailReason::RANSAC_DEGENERATE;
    ++fail_count_;
    return r;
  }
  r.ground_normal.normalize();

  // Slope = angle between ground normal and world Z.
  const float zcos = std::abs(r.ground_normal.z());
  const float clamped = std::min(1.0f, std::max(-1.0f, zcos));
  r.slope_deg = std::acos(clamped) * 180.0f / static_cast<float>(M_PI);
  r.valid = true;
  ++success_count_;
  return r;
}

}  // namespace san_lidar
