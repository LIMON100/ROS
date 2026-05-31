// SAN v1.5 Phase 2-E Turn 5 — External payload IMU driver.
//
// Replaces adapters/payload_sensors.py::ExternalImuAdapter per
// DCN-2026-002 D-007 (Tier 1 C++). Publishes sensor_msgs/Imu on
// ~/imu at the receiver's native rate (typically 100-400 Hz).
//
// Two operating modes:
//   * Real serial — opens device, framing via BinaryFrameParser,
//                   payload decode is model-specific (TODO Turn 5.5)
//   * Stub        — emits static-platform noise model (matches Python
//                   adapter's stub for consumer regression).
//
// The serial backend is the same SerialInterface concept as
// san_rtk_gnss but kept private here for now; Turn 14 may consolidate
// into a shared san_common_hw package.

#ifndef SAN_IMU_DRIVER__IMU_DRIVER_NODE_HPP_
#define SAN_IMU_DRIVER__IMU_DRIVER_NODE_HPP_

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>

namespace san_imu_driver {

/// Minimal serial backend abstraction (local to this package).
class ImuSerialInterface {
public:
  virtual ~ImuSerialInterface() = default;
  virtual bool open(const std::string& device, int baud) = 0;
  virtual void close() = 0;
  /// Read up to `max_bytes` with `timeout`; returns what's available.
  virtual std::vector<uint8_t> read(
      size_t max_bytes,
      std::chrono::milliseconds timeout) = 0;
  virtual bool isOpen() const = 0;
};
std::unique_ptr<ImuSerialInterface> makeRealImuSerial();

class ImuDriverNode : public rclcpp::Node {
public:
  explicit ImuDriverNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());
  ImuDriverNode(
      const rclcpp::NodeOptions& opts,
      std::unique_ptr<ImuSerialInterface> serial);
  ~ImuDriverNode() override;

private:
  void declareParameters();
  void loadParameters();
  void readerLoop();
  void publishStubSample();
  void publishFromPayload(const std::vector<uint8_t>& payload);
  void onHealthTick();

  // Members
  std::unique_ptr<ImuSerialInterface> serial_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::TimerBase::SharedPtr                         health_timer_;
  rclcpp::TimerBase::SharedPtr                         stub_timer_;

  // Params
  std::string device_;
  int         baud_;
  std::string frame_id_;
  double      stub_rate_hz_;
  bool        stub_on_no_serial_;
  // Frame parser config (defaults match a generic Xsens-like preamble)
  uint8_t     sync_byte_0_;
  bool        checksum_xor_;

  // Reader thread
  std::thread       reader_thread_;
  std::atomic<bool> running_{false};
  bool              stub_mode_ = false;

  // Stats
  std::atomic<uint32_t> frame_count_{0};
  std::atomic<uint32_t> drop_count_{0};
};

}  // namespace san_imu_driver

#endif  // SAN_IMU_DRIVER__IMU_DRIVER_NODE_HPP_
