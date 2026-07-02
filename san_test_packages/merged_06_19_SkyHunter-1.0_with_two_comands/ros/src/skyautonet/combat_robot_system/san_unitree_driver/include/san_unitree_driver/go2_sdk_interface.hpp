// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 2 — Unitree Go2 SDK abstraction interface.
//
// Reasons for abstracting the SDK behind an interface:
//   1. Unit testability — gtest can inject a MockGo2Sdk that drives
//      callbacks deterministically. Without this we'd need a live
//      Go2 to run any test.
//   2. Build robustness — the Unitree SDK is system-installed
//      out-of-band. If it's missing (CI, dev laptop), the package
//      still builds with the mock implementation.
//   3. Future-proofing — if Unitree changes their SDK API, only the
//      RealGo2Sdk wrapper changes, not the Node.
//
// This interface is intentionally minimal: it carries only the
// 4 inbound data streams and 2 outbound commands the Node uses.
// All ROS message conversion happens IN THE NODE, not in the SDK
// wrapper — the wrapper deals in plain POD types only.

#ifndef SAN_UNITREE_DRIVER__GO2_SDK_INTERFACE_HPP_
#define SAN_UNITREE_DRIVER__GO2_SDK_INTERFACE_HPP_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace san_unitree_driver
{

// ─── POD data types from the SDK ────────────────────────────────────────
// These mirror the Unitree SDK structures but are SDK-independent so
// the mock and the real wrapper produce identical types.

struct LidarPoint
{
  float x;
  float y;
  float z;
  float intensity;
};

// [Tier1 audit 2026-05-24 P3-2] unitree_go2_node.cpp memcpy's
// std::vector<LidarPoint>::data() straight into a PointCloud2 buffer
// with point_step=16 and fields at [0,4,8,12]. That contract is
// silent unless we lock the struct layout here.
static_assert(
  sizeof(LidarPoint) == 16,
  "LidarPoint must remain 16 bytes for PointCloud2 memcpy");
static_assert(
  offsetof(LidarPoint, x) == 0,
  "LidarPoint::x must be at offset 0");
static_assert(
  offsetof(LidarPoint, y) == 4,
  "LidarPoint::y must be at offset 4");
static_assert(
  offsetof(LidarPoint, z) == 8,
  "LidarPoint::z must be at offset 8");
static_assert(
  offsetof(LidarPoint, intensity) == 12,
  "LidarPoint::intensity must be at offset 12");

struct LidarScan
{
  uint64_t timestamp_ns;
  uint32_t seq;
  std::vector<LidarPoint> points;     // raw float32 ordered list
};

struct ImuData
{
  uint64_t timestamp_ns;
  uint32_t seq;
  // Body frame angular velocity (rad/s)
  double angular_velocity_x;
  double angular_velocity_y;
  double angular_velocity_z;
  // Body frame linear acceleration (m/s²)
  double linear_acceleration_x;
  double linear_acceleration_y;
  double linear_acceleration_z;
  // Orientation quaternion (world frame, w-last)
  double orientation_x;
  double orientation_y;
  double orientation_z;
  double orientation_w;
};

struct CameraFrame
{
  uint64_t timestamp_ns;
  uint32_t seq;
  uint32_t width;
  uint32_t height;
  std::string encoding;   // "rgb8" / "bgr8" / "yuv422" — matches sensor_msgs/Image
  std::vector<uint8_t> data;
};

/// Robot state — battery, motion mode, body pose. Reduced subset of
/// what the Go2 SDK exposes; we publish only what downstream nodes
/// (Mission, Safety, RoleManagement) consume.
struct Go2State
{
  uint64_t timestamp_ns;
  // Battery
  float battery_voltage_v;
  float battery_percent;           // 0..100
  bool battery_charging;
  // Motion
  uint8_t motion_mode;              // 0=idle, 1=stand, 2=walk, 3=trot, ...
  float body_height_m;
  // Pose (world frame, from onboard SLAM)
  double position_x_m;
  double position_y_m;
  double position_z_m;
  double yaw_rad;
  // Faults (bit-mask matches SDK)
  uint32_t fault_bits;
};

/// Locomotion command (body frame).
struct CmdVel
{
  float linear_x_mps;
  float linear_y_mps;
  float angular_z_rps;
};

/// High-level navigation goal (world frame, Go2 internal SLAM).
struct GoalPose
{
  double position_x_m;
  double position_y_m;
  double yaw_rad;
};

/// Callback signatures — kept simple so the mock can invoke them
/// from a test thread without exposing internal SDK types.
using LidarCallback = std::function<void (const LidarScan &)>;
using ImuCallback = std::function<void (const ImuData &)>;
using CameraCallback = std::function<void (const CameraFrame &)>;
using StateCallback = std::function<void (const Go2State &)>;

/// Pure abstract SDK interface. RealGo2Sdk wraps unitree_sdk2;
/// MockGo2Sdk is the in-memory test double.
class Go2SdkInterface
{
public:
  virtual ~Go2SdkInterface() = default;

  /// Initialize the SDK on the given network interface. Throws on
  /// failure (DDS init error, interface not found, etc.).
  /// Idempotent across multiple calls only if `interface_name`
  /// matches the previous call.
  virtual void init(const std::string & interface_name) = 0;

  /// Register inbound callbacks. May be called any time after init().
  /// Replaces any previous registration of the same type.
  virtual void registerLidarCallback(LidarCallback cb)   = 0;
  virtual void registerImuCallback(ImuCallback cb)       = 0;
  virtual void registerCameraCallback(CameraCallback cb) = 0;
  virtual void registerStateCallback(StateCallback cb)   = 0;

  /// Outbound commands. Non-blocking; SDK queues internally.
  /// Returns false if the SDK isn't initialized or the queue is
  /// full.
  virtual bool sendCmdVel(const CmdVel & cmd)         = 0;
  virtual bool sendGoalPose(const GoalPose & goal)    = 0;

  /// Health check — true if init() has completed AND the underlying
  /// DDS link is alive.
  virtual bool isHealthy() const = 0;

  /// True if this implementation is a stub (no real Go2 hardware
  /// communication). The node uses this to refuse-to-start in
  /// production unless `allow_stub_sdk` parameter is true.
  virtual bool isStub() const {return false;}
};

/// Factory function. The "real" implementation is in
/// real_go2_sdk.cpp and is only built when unitree_sdk2 is found by
/// CMake; otherwise this returns a stub that logs and refuses to
/// initialize (which the Node then fails-closed on).
std::unique_ptr<Go2SdkInterface> makeRealGo2Sdk();

}  // namespace san_unitree_driver

#endif  // SAN_UNITREE_DRIVER__GO2_SDK_INTERFACE_HPP_
