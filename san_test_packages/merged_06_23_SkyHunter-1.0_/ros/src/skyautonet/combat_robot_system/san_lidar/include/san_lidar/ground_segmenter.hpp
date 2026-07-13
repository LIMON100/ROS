// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 PHASE 7 - RANSAC ground-plane segmentation.
//
// Inputs the raw Robosense E1 cloud, returns ground / obstacle splits
// plus the ground-normal vector for the traversability layer. PCL
// SACSegmentation drives the plane fit; the threshold (5 cm) matches
// the grid resolution so a single missed inlier does not create a
// false obstacle column.
//
// DCN-2026-006 EXT (v1.5.2):
//   D-017 RANSAC fail alarm    : the result now carries a
//                                 machine-readable failure reason
//                                 + cumulative fail counter so the
//                                 driver can publish DiagnosticArray
//                                 and a ThreatAlert instead of
//                                 silently degrading.

#pragma once

#include <Eigen/Core>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

namespace san_lidar
{

struct GroundSegmenterParams
{
  int ransac_max_iterations = 1000;
  float ransac_distance_threshold_m = 0.05f;     // 5 cm
  bool optimize_coefficients = true;
  // Filter ground points outside this z band first - keeps the
  // RANSAC fit from anchoring to a ceiling beam if the mast is too
  // close to the ground.
  float prefilter_z_min_m = -0.5f;
  float prefilter_z_max_m = 3.0f;
};

// [DCN-2026-006 EXT D-017] Machine-readable failure reasons. The
// driver maps these onto DiagnosticArray + ThreatAlert.threat_type so
// the operator can see *why* the lidar pipeline degraded.
enum class GroundSegmenterFailReason : uint8_t
{
  NONE              = 0,
  EMPTY_INPUT       = 1,      // input cloud was null or empty
  EMPTY_AFTER_BAND  = 2,      // prefilter dropped every point
  RANSAC_NO_INLIERS = 3,      // SAC returned 0 inliers
  RANSAC_DEGENERATE = 4,      // coefficients < 4 -> plane invalid
};

inline const char * toString(GroundSegmenterFailReason r)
{
  switch (r) {
    case GroundSegmenterFailReason::NONE:              return "none";
    case GroundSegmenterFailReason::EMPTY_INPUT:       return "empty_input";
    case GroundSegmenterFailReason::EMPTY_AFTER_BAND:  return "empty_after_band";
    case GroundSegmenterFailReason::RANSAC_NO_INLIERS: return "ransac_no_inliers";
    case GroundSegmenterFailReason::RANSAC_DEGENERATE: return "ransac_degenerate";
  }
  return "unknown";
}

struct GroundSegmenterResult
{
  pcl::PointCloud<pcl::PointXYZI>::Ptr ground_points;
  pcl::PointCloud<pcl::PointXYZI>::Ptr obstacle_points;
  Eigen::Vector3f ground_normal{0.0f, 0.0f, 1.0f};
  float slope_deg = 0.0f;       // ground plane vs world Z (degrees)
  bool valid = false;           // false when RANSAC failed

  // [DCN-2026-006 EXT D-017] populated whenever valid==false.
  GroundSegmenterFailReason fail_reason = GroundSegmenterFailReason::NONE;
};

class GroundSegmenter
{
public:
  explicit GroundSegmenter(
    const GroundSegmenterParams & params = GroundSegmenterParams{});

  void setParams(const GroundSegmenterParams & params)
  {
    params_ = params;
  }

  // Run RANSAC on the input cloud. The result holds new pcl clouds,
  // so the caller may release the input after this returns.
  GroundSegmenterResult segment(
    const pcl::PointCloud<pcl::PointXYZI>::ConstPtr & cloud) const;

  // [DCN-2026-006 EXT D-017] Cumulative failure counter. Driver
  // reads this to publish DiagnosticStatus periodically. Atomic
  // because segment() is const and may be called from multiple
  // worker threads under intra-process composition.
  uint64_t failCount() const {return fail_count_.load();}
  uint64_t successCount() const {return success_count_.load();}

private:
  GroundSegmenterParams params_;
  mutable std::atomic<uint64_t> fail_count_{0};
  mutable std::atomic<uint64_t> success_count_{0};
};

}  // namespace san_lidar
