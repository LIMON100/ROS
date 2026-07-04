// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 1 - cost_map_node.
//
// Subscribes to san_lidar ground/obstacle clouds, runs the four
// layers, composes the master grid, and publishes CostMapUpdate at
// 10 Hz (1 Hz refresh of the underlying layers). Tracks the per-cycle
// latency so we can emit a warning when the v1.3 KPP (p99 ≤ 5 s) is
// at risk.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <std_msgs/msg/string.hpp>

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <combat_robot_msgs/msg/cost_map_update.hpp>

#include "san_costmap/cost_map_publisher.hpp"

namespace san_costmap
{

class CostMapNode : public rclcpp::Node
{
public:
  CostMapNode();
  explicit CostMapNode(const rclcpp::NodeOptions & options);

  // Test accessors.
  int width() const {return publisher_.width();}
  int height() const {return publisher_.height();}
  double lastLatencySec() const {return last_latency_s_.load();}
  uint32_t consecutiveLethalCount() const
  {
    return consecutive_lethal_;
  }

  // Test entry point: drive one full cycle synchronously.
  combat_robot_msgs::msg::CostMapUpdate buildOneShotForTest(
    const pcl::PointCloud<pcl::PointXYZI>::Ptr & obstacle_cloud,
    const pcl::PointCloud<pcl::PointXYZI>::Ptr & ground_cloud);

private:
  int width_ = DEFAULT_GRID_CELLS;
  int height_ = DEFAULT_GRID_CELLS;
  double resolution_m_ = DEFAULT_RESOLUTION_M;
  double origin_x_ = 0.0;
  double origin_y_ = 0.0;
  double kpp_latency_target_s_ = 5.0;

  std::string robot_id_str_ = "0";

  pcl::PointCloud<pcl::PointXYZI>::Ptr latest_obstacle_;
  pcl::PointCloud<pcl::PointXYZI>::Ptr latest_ground_;

  // Raw last-message cache. PCL conversion is deferred to runUpdate()
  // so we don't pay 3-5 ms per sensor callback (~10-20 Hz) when the
  // update cycle is 2 Hz — most converted clouds were discarded.
  //
  // cloud_mutex_ protects the swap. Sensor callbacks and the update
  // timer run on different executor threads under MTE, so reading /
  // assigning the SharedPtr without a lock is a race. The slow
  // fromROSMsg in runUpdate() copies the SharedPtr out under the
  // lock then converts WITHOUT the lock held, so callbacks aren't
  // blocked on conversion latency. (DCN-2026-005 D-014.)
  std::mutex cloud_mutex_;
  sensor_msgs::msg::PointCloud2::SharedPtr latest_obstacle_msg_;
  sensor_msgs::msg::PointCloud2::SharedPtr latest_ground_msg_;

  CostMapPublisher publisher_;
  uint32_t sequence_ = 0;
  uint32_t consecutive_lethal_ = 0;
  std::atomic<double> last_latency_s_;

  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr
    obstacle_sub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr
    ground_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::CostMapUpdate>::SharedPtr
    cost_map_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr
    operator_alert_pub_;

  rclcpp::TimerBase::SharedPtr update_timer_;
  rclcpp::TimerBase::SharedPtr publish_timer_;

  void declareParameters();
  void readParameters();
  void wireInterfaces();

  void onObstacleCloud(sensor_msgs::msg::PointCloud2::SharedPtr msg);
  void onGroundCloud(sensor_msgs::msg::PointCloud2::SharedPtr msg);
  void runUpdate();
  void publishCached();
  void recordLatency(double seconds);
  void maybePublishOperatorAlert();
};

}  // namespace san_costmap
