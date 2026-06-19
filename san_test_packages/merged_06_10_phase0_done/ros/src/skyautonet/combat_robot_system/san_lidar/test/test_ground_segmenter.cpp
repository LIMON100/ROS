// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 1 - GroundSegmenter unit test.
//
// Builds a synthetic point cloud (flat ground plate + an elevated
// obstacle column) and verifies RANSAC splits them correctly.

#include <gtest/gtest.h>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include "san_lidar/ground_segmenter.hpp"

namespace
{

pcl::PointCloud<pcl::PointXYZI>::Ptr makeFlatGround(
  float extent_m = 5.0f, float resolution = 0.05f)
{
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  for (float x = -extent_m; x <= extent_m; x += resolution) {
    for (float y = -extent_m; y <= extent_m; y += resolution) {
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

void addBlock(
  const pcl::PointCloud<pcl::PointXYZI>::Ptr & cloud,
  float cx, float cy, float w, float h, float top)
{
  for (float x = cx - w / 2; x <= cx + w / 2; x += 0.02f) {
    for (float y = cy - h / 2; y <= cy + h / 2; y += 0.02f) {
      for (float z = 0.05f; z <= top; z += 0.02f) {
        pcl::PointXYZI p;
        p.x = x; p.y = y; p.z = z; p.intensity = 200.0f;
        cloud->points.push_back(p);
      }
    }
  }
  cloud->width = cloud->points.size();
}

}  // namespace

TEST(GroundSegmenter, FlatGroundIsClassifiedAsGround) {
  auto cloud = makeFlatGround();
  san_lidar::GroundSegmenter seg;
  auto r = seg.segment(cloud);
  EXPECT_TRUE(r.valid);
  EXPECT_GT(r.ground_points->size(), cloud->size() / 2)
    << "majority of points should land in the ground bin";
  EXPECT_LT(r.obstacle_points->size(), cloud->size() / 20)
    << "obstacle bin should be tiny for a flat plate";
  EXPECT_LT(r.slope_deg, 5.0f);
}

TEST(GroundSegmenter, ElevatedBlockBecomesObstacle) {
  auto cloud = makeFlatGround();
  addBlock(
    cloud, /*cx=*/ 1.0f, /*cy=*/ 0.0f,
    /*w=*/ 0.4f, /*h=*/ 0.4f, /*top=*/ 0.5f);
  san_lidar::GroundSegmenter seg;
  auto r = seg.segment(cloud);
  EXPECT_TRUE(r.valid);
  EXPECT_GT(r.obstacle_points->size(), 100u)
    << "block should contribute obstacle points";
  // Slope should remain near horizontal despite the block.
  EXPECT_LT(r.slope_deg, 5.0f);
}

TEST(GroundSegmenter, SlopedGroundReportsAngle) {
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  const float slope_rad = 20.0f * static_cast<float>(M_PI) / 180.0f;
  const float tan_s = std::tan(slope_rad);
  for (float x = -3.0f; x <= 3.0f; x += 0.05f) {
    for (float y = -3.0f; y <= 3.0f; y += 0.05f) {
      pcl::PointXYZI p;
      p.x = x; p.y = y;
      p.z = x * tan_s;        // slope along +x
      p.intensity = 1.0f;
      cloud->points.push_back(p);
    }
  }
  cloud->width = cloud->points.size();
  cloud->height = 1;
  san_lidar::GroundSegmenter seg;
  auto r = seg.segment(cloud);
  EXPECT_TRUE(r.valid);
  EXPECT_NEAR(r.slope_deg, 20.0f, 2.0f)
    << "RANSAC plane fit should recover the synthetic slope";
}

TEST(GroundSegmenter, EmptyInputIsGracefullyHandled) {
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  san_lidar::GroundSegmenter seg;
  auto r = seg.segment(cloud);
  EXPECT_FALSE(r.valid);
  EXPECT_TRUE(r.ground_points->empty());
  EXPECT_TRUE(r.obstacle_points->empty());
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
