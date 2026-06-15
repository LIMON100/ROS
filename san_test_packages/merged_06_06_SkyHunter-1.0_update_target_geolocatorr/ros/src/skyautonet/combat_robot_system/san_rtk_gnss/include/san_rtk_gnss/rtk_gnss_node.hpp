// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 4 — RtkGnssNode.
//
// Reads NMEA from u-blox F9P via serial, publishes:
//   * ~/fix          — sensor_msgs/NavSatFix  (standard ROS interface)
//   * ~/rtk_status   — combat_robot_msgs/RtkFixStatus (rich detail)
//   * ~/gga_latest   — std_msgs/String (raw GGA for NTRIP VRS uplink)
// Subscribes:
//   * ~/rtcm_corrections — std_msgs/UInt8MultiArray (injects to receiver)
//
// Threading: a worker thread reads serial; the rclcpp executor handles
// the RTCM subscription callback. Both call serial via the same
// SerialInterface, which guarantees thread-safety.

#ifndef SAN_RTK_GNSS__RTK_GNSS_NODE_HPP_
#define SAN_RTK_GNSS__RTK_GNSS_NODE_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/imu.hpp>

#include <combat_robot_msgs/msg/rtk_fix_status.hpp>

#include "san_rtk_gnss/serial_interface.hpp"

namespace san_rtk_gnss
{

class RtkGnssNode : public rclcpp::Node
{
public:
  explicit RtkGnssNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());
  RtkGnssNode(
    const rclcpp::NodeOptions & opts,
    std::unique_ptr<SerialInterface> serial);
  ~RtkGnssNode() override;

private:
  void declareParameters();
  void loadParameters();

  /// Serial reader thread loop.
  void readerLoop();

  /// RTCM correction subscriber callback. Writes the byte array back
  /// to the serial port (thread-safe via SerialInterface).
  void onRtcm(const std_msgs::msg::UInt8MultiArray::SharedPtr msg);

  /// Health timer callback (1 Hz).
  void onHealthTick();

  /// Convert a GGA fix into NavSatFix + RtkFixStatus, publish both.
  void publishFix(const std::string & gga_line);

  /// [DCN-2026-006 EXT D-022] $GxHDT → sensor_msgs/Imu publisher.
  ///
  /// The dual-antenna heading is published as an Imu message because
  /// robot_localization's EKF natively consumes IMU heading via
  /// imu0_config[5] (yaw absolute). Quaternion encodes yaw only;
  /// angular_velocity / linear_acceleration are left zero with their
  /// covariance set to -1 (per robot_localization convention to mark
  /// "no data"). The yaw covariance is set per RTK fix quality:
  ///   FIX  → 0.0017 rad² (≈ 2.4°² — typical dual-antenna 0.1° accuracy)
  ///   FLOAT→ 0.030  rad² (≈ 10°²  — degraded, but still useful)
  ///   else → 1.0    rad² (≈ 57°²  — effectively suppress in EKF)
  void publishHeading(const std::string & hdt_line);

public:
  /// [DCN-2026-006 EXT D-022] Pure-logic helper exposed for unit tests:
  /// build the sensor_msgs/Imu message from a heading angle + fix
  /// quality. No clock, no publisher, no node state — caller supplies
  /// stamp and frame_id. Reused by publishHeading() in production.
  ///
  /// `heading_deg` is the NMEA HDT raw value (degrees clockwise from
  /// True North). The function performs the REP-103 conversion
  /// internally (yaw_rep103 = π/2 - heading_rad).
  static sensor_msgs::msg::Imu buildHeadingMsg(
    double heading_deg,
    uint8_t fix_type,
    const std::string & frame_id,
    const rclcpp::Time & stamp);

  /// [LOC-1 follow-up] Fill NavSatFix.position_covariance from RTK fix
  /// quality. robot_localization's navsat_transform copies this straight
  /// into the EKF's GPS odometry covariance, so the previous behaviour
  /// (all-zero covariance) made the EKF treat GPS as a PERFECT measurement
  /// and snap to it — including in RTK_FLOAT, where position is noisy and
  /// can jump, which yields the erratic driving on Fix→Float transition.
  /// Scaling the covariance by fix quality lets the EKF down-weight FLOAT
  /// automatically (mirrors the dual-antenna heading covariance above):
  ///   RTK_FIX   → σ 0.02 m  (4e-4 m²)
  ///   RTK_FLOAT → σ 0.30 m  (0.09 m²)
  ///   DGPS      → σ 1.0 m
  ///   other     → σ 3.0 m   (autonomous / estimated / dead-reckoning)
  /// Diagonal ENU; sets position_covariance_type = DIAGONAL_KNOWN. Pure,
  /// static, unit-tested.
  static void applyPositionCovariance(
    sensor_msgs::msg::NavSatFix & fix, uint8_t fix_type);

private:
  // ─── Members ───────────────────────────────────────────────────
  std::unique_ptr<SerialInterface> serial_;

  rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr fix_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::RtkFixStatus>::SharedPtr status_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr gga_pub_;
  /// Phase 1 (PR #123): latched stub-status. True when the linked
  /// serial backend is the stub (i.e. write() returns 0 and read()
  /// returns nothing). Consumers can refuse to trust RTK status.
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stub_status_pub_;
  /// [DCN-2026-006 EXT D-022] Dual-antenna heading publisher.
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr heading_pub_;
  rclcpp::Subscription<std_msgs::msg::UInt8MultiArray>::SharedPtr rtcm_sub_;
  rclcpp::TimerBase::SharedPtr health_timer_;

  // Parameters
  // [DCN-2026-006 EXT cleanup §5.1] In-class defaults are defensive —
  // declareParameters/loadParameters fills these before they're used,
  // but the explicit init guards against a future code path that
  // accesses them before loadParameters runs.
  std::string device_{};
  int baud_ = 0;
  std::string frame_id_{};
  bool stub_on_no_serial_ = false;

  // Reader thread + stop flag
  std::thread reader_thread_;
  std::atomic<bool> running_{false};

  // Stats
  std::atomic<uint32_t> nmea_count_{0};
  std::atomic<uint32_t> dropped_count_{0};
  std::atomic<uint32_t> rtcm_inject_count_{0};
  std::atomic<uint32_t> rtcm_inject_drop_count_{0};    // short-write / no-receiver
  std::atomic<uint8_t> last_fix_type_{0};
};

}  // namespace san_rtk_gnss

#endif  // SAN_RTK_GNSS__RTK_GNSS_NODE_HPP_
