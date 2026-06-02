// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 7 — LrfNode (single-point LRF driver).
//
// Polls a serial-connected single-point Laser Range Finder (Lightware
// LW20, LW-NX, similar) at ~5 Hz and publishes LrfReading on ~/range.
// Replaces adapters/payload_sensors.py::LrfAdapter per DCN-2026-002.
//
// Uses an internal SerialInterface (same pattern as san_imu_driver) so
// tests inject a mock without needing /dev/ttyUSBn.

#ifndef SAN_LIDAR__LRF_NODE_HPP_
#define SAN_LIDAR__LRF_NODE_HPP_

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>

#include <combat_robot_msgs/msg/lrf_reading.hpp>

namespace san_lidar
{

class LrfSerialInterface
{
public:
  virtual ~LrfSerialInterface() = default;
  virtual bool open(const std::string & device, int baud) = 0;
  virtual void close() = 0;
  /// Read one CR/LF-terminated line; empty string on timeout.
  virtual std::string readLine(
    std::chrono::milliseconds timeout) = 0;
  virtual bool isOpen() const = 0;
};
std::unique_ptr<LrfSerialInterface> makeRealLrfSerial();

class LrfNode : public rclcpp::Node
{
public:
  explicit LrfNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());
  LrfNode(
    const rclcpp::NodeOptions & opts,
    std::unique_ptr<LrfSerialInterface> serial);
  ~LrfNode() override;

private:
  void declareParameters();
  void loadParameters();
  void readerLoop();
  void onStubTick();
  void onHealthTick();
  void publishReading(float range_m, float strength, bool valid);

  std::unique_ptr<LrfSerialInterface> serial_;
  rclcpp::Publisher<combat_robot_msgs::msg::LrfReading>::SharedPtr lrf_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stub_status_pub_;
  rclcpp::TimerBase::SharedPtr health_timer_;
  rclcpp::TimerBase::SharedPtr stub_timer_;

  // Params
  std::string device_;
  int baud_;
  std::string frame_id_;
  double rate_hz_;
  float max_range_m_;
  bool stub_on_no_serial_;

  std::thread reader_thread_;
  std::atomic<bool> running_{false};
  bool stub_mode_ = false;

  std::atomic<uint32_t> reading_count_{0};
  std::atomic<uint32_t> dropped_count_{0};
  float last_range_m_ = -1.0f;
  bool last_valid_ = false;
};

}  // namespace san_lidar

#endif  // SAN_LIDAR__LRF_NODE_HPP_
