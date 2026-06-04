// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 1 - obstacle layer with v1.3 UGV thresholds.
//
// Plain C++ class (not Nav2 plugin-bound) so unit tests can drive it
// without a layered_costmap_2d::Costmap2DROS lifecycle. A thin Nav2
// adapter is provided in src/obstacle_layer_v13.cpp via the
// nav2_costmap_2d::Layer interface; the algorithm core lives here.

#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cstdint>
#include <cstddef>
#include <vector>

#include "san_costmap/cost_constants.hpp"

namespace san_costmap
{

class ObstacleLayerV13
{
public:
  ObstacleLayerV13(
    int width = DEFAULT_GRID_CELLS,
    int height = DEFAULT_GRID_CELLS,
    float resolution_m = DEFAULT_RESOLUTION_M);

  void setGeometry(
    int width, int height, float resolution_m,
    float origin_x = 0.0f, float origin_y = 0.0f);

  // Replace the obstacle cloud used by updateBounds().
  void setInputCloud(
    pcl::PointCloud<pcl::PointXYZI>::ConstPtr cloud)
  {
    input_ = std::move(cloud);
  }

  // Rebuild the local cost grid from the latest input cloud.
  void updateBounds();

  // Read-only accessors for the master cost-map publisher + tests.
  uint8_t cost(int gx, int gy) const;
  const std::vector<uint8_t> & grid() const {return grid_;}
  int width()  const {return width_;}
  int height() const {return height_;}
  float resolution_m() const {return resolution_m_;}

  // Threshold overrides (kept default to v1.3 spec, but the tests
  // need to mutate them for boundary cases).
  void setLethalHeightM(float m) {lethal_height_m_ = m;}
  void setInflatedHeightM(float m) {inflated_height_m_ = m;}

private:
  int width_;
  int height_;
  float resolution_m_;
  float origin_x_ = 0.0f;
  float origin_y_ = 0.0f;
  float lethal_height_m_ = OBSTACLE_LETHAL_HEIGHT_M;
  float inflated_height_m_ = OBSTACLE_INFLATED_HEIGHT_M;

  pcl::PointCloud<pcl::PointXYZI>::ConstPtr input_;
  std::vector<uint8_t> grid_;
};

}  // namespace san_costmap
