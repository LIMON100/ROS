// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 2 — UnitreeGo2Node rclcpp Node header.
//
// Bridges Go2SdkInterface ↔ ROS 2 topics. See go2_sdk_interface.hpp
// for SDK abstraction rationale.
//
// References:
//   * SDD-SWARM v1.5 §3.1 (HW layout)
//   * IDS v1.5 §5 (topic naming)
//   * IDS v1.5 §7 (QoS profiles)

#ifndef SAN_UNITREE_DRIVER__UNITREE_GO2_NODE_HPP_
#define SAN_UNITREE_DRIVER__UNITREE_GO2_NODE_HPP_

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <std_msgs/msg/bool.hpp>

#include "san_unitree_driver/go2_sdk_interface.hpp"

namespace san_unitree_driver
{

class UnitreeGo2Node : public rclcpp::Node
{
public:
  /// Production constructor — auto-instantiates the real SDK via
  /// makeRealGo2Sdk(). Fails-closed (throws) on SDK init error.
  explicit UnitreeGo2Node(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

  /// Test constructor — caller injects a mock SDK. Public so gtest
  /// can construct without linking unitree_sdk2.
  UnitreeGo2Node(
    const rclcpp::NodeOptions & opts,
    std::unique_ptr<Go2SdkInterface> sdk);

private:
  /// Parameter declaration + reading.
  void declareParameters();
  void loadParameters();

  /// Wire SDK callbacks ↔ ROS publishers and ROS subscribers ↔ SDK.
  void wireSdkAndRos();

  /// SDK → ROS conversion callbacks (run on SDK thread; publish
  /// is thread-safe via rclcpp::Publisher's internal lock).
  void onSdkLidar(const LidarScan & scan);
  void onSdkImu(const ImuData & imu);
  void onSdkCamera(const CameraFrame & frame);
  void onSdkState(const Go2State & state);

  /// ROS → SDK callbacks (run on rclcpp executor thread).
  void onCmdVel(const geometry_msgs::msg::Twist::SharedPtr msg);
  void onGoalPose(const geometry_msgs::msg::PoseStamped::SharedPtr msg);

  /// Periodic health check timer — logs warnings if SDK link drops.
  void onHealthTick();

  /// cmd_vel watchdog tick (10 Hz). If no fresh cmd_vel has arrived
  /// within `cmd_vel_watchdog_ms_` AND the last command was non-zero,
  /// emit a zero-velocity command to the SDK so the robot does not
  /// run away on stale input (e.g. Nav2 crash mid-stride).
  void onCmdVelWatchdog();

  /// Helper: build a stamped header with sensor_msgs frame ids.
  rclcpp::Time toRosTime(uint64_t timestamp_ns) const;

  // ─── Members ───────────────────────────────────────────────────
  std::unique_ptr<Go2SdkInterface> sdk_;

  // Publishers (Go2 → ROS 2)
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr lidar_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr camera_pub_;
  // Go2State doesn't have a generated msg type yet (Turn 2 scope).
  // For now we use sensor_msgs::msg::BatteryState as a placeholder.
  // Full Go2State message will go into combat_robot_msgs in a later turn.
  rclcpp::Publisher<sensor_msgs::msg::BatteryState>::SharedPtr state_pub_raw_;

  // Phase 1 stub-status latched advertisement.
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stub_status_pub_;

  // Subscribers (ROS 2 → Go2)
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr goal_pose_sub_;

  // Health timer (1 Hz)
  rclcpp::TimerBase::SharedPtr health_timer_;
  // cmd_vel watchdog timer (10 Hz) — zeros velocity on stale input.
  rclcpp::TimerBase::SharedPtr cmd_vel_watchdog_timer_;

  // Parameters
  std::string interface_name_;
  std::string lidar_frame_id_;
  std::string imu_frame_id_;
  std::string camera_frame_id_;
  std::string base_frame_id_;
  double max_cmd_vel_linear_mps_;
  double max_cmd_vel_angular_rps_;
  /// Maximum time (ms) since last cmd_vel before the watchdog
  /// auto-zeros velocity. 0 disables the watchdog (NOT recommended).
  int64_t cmd_vel_watchdog_ms_;
  /// Permit running with a stub SDK. Default false (production-safe).
  /// Set true in launch only for bringup/CI when unitree_sdk2 is not
  /// available.
  bool allow_stub_sdk_;

  // Stats (logged by health timer)
  std::size_t lidar_count_ = 0;
  std::size_t imu_count_ = 0;
  std::size_t camera_count_ = 0;
  std::size_t state_count_ = 0;
  std::size_t cmd_vel_count_ = 0;
  std::size_t goal_count_ = 0;
  std::size_t cmd_vel_clamped_ = 0;
  std::atomic<uint32_t> watchdog_zero_count_{0};

  // cmd_vel watchdog state — last command time + last command
  // magnitude. If the last command was already zero we don't need
  // to send another zero (idempotent), so we track "was non-zero".
  std::atomic<int64_t> last_cmd_vel_steady_ms_{0};
  std::atomic<bool> last_cmd_vel_nonzero_{false};
};

}  // namespace san_unitree_driver

#endif  // SAN_UNITREE_DRIVER__UNITREE_GO2_NODE_HPP_
