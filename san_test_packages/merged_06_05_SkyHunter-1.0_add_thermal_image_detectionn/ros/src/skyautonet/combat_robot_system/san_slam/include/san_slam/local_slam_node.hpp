// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 3 - per-robot local SLAM wrapper.
//
// Subscribes to slam_toolbox's /map (nav_msgs/OccupancyGrid),
// maintains the previous snapshot, computes the per-cell delta, and
// publishes SLAMLocalDelta at 1 Hz with PNG encoding. Coverage window
// timestamps let the Hub aggregator order out-of-sequence deltas.

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <combat_robot_msgs/msg/slam_local_delta.hpp>

namespace san_slam
{

class LocalSlamNode : public rclcpp::Node
{
public:
  LocalSlamNode();
  explicit LocalSlamNode(const rclcpp::NodeOptions & options);

  // Test accessors.
  std::size_t snapshotSize() const
  {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return current_snapshot_.size();
  }
  uint32_t publishedCount() const {return published_count_;}

  // Test entry points.
  void injectMapForTest(const nav_msgs::msg::OccupancyGrid & map);
  combat_robot_msgs::msg::SLAMLocalDelta buildDeltaForTest();

private:
  // robot_id is declared as integer to match the squadron-wide
  // convention used by 10 other nodes (combat_robot_operation_system,
  // leader_role_manager, hub_role_manager, tier_node, ...) and the
  // squadron.yaml override (`robot_id: 0`). The previous string
  // declaration caused InvalidParameterTypeException at launch time
  // (TST S20-1 → SOP-CI-001 §3).
  int robot_id_ = 0;
  std::string map_topic_ = "/map";
  std::string delta_topic_;
  double publish_period_sec_ = 1.0;

  int width_ = 0;
  int height_ = 0;
  float resolution_m_ = 0.05f;
  geometry_msgs::msg::Pose2D origin_;

  mutable std::mutex snapshot_mutex_;
  std::vector<int8_t> previous_snapshot_;
  std::vector<int8_t> current_snapshot_;

  uint64_t coverage_start_ms_ = 0;
  uint64_t coverage_end_ms_ = 0;
  uint32_t published_count_ = 0;

  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr map_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::SLAMLocalDelta>::SharedPtr
    delta_pub_;
  rclcpp::TimerBase::SharedPtr publish_timer_;

  void declareParameters();
  void readParameters();
  void wireInterfaces();

  void onMap(nav_msgs::msg::OccupancyGrid::SharedPtr msg);
  void publishDelta();
  combat_robot_msgs::msg::SLAMLocalDelta buildMessage();

  uint64_t nowMs() const;
};

}  // namespace san_slam
