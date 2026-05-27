// SAN v1.5 Phase 2-E Turn 2 — Stub Go2 SDK.
//
// Used when unitree_sdk2 is NOT found by CMake (CI, dev laptop).
// Provides a Go2SdkInterface implementation that:
//   - init() : succeeds (so the Node can construct) but flags
//              isHealthy() == false until a real SDK appears.
//   - register*() : stores callbacks but never invokes them.
//   - send*() : returns false (downstream sees "SDK unhealthy").
//
// This means a colcon build on a dev machine without unitree_sdk2:
//   - compiles
//   - links the stub
//   - the Node starts up but logs "UNHEALTHY" every second
//   - no actual locomotion happens
//
// On a real robot, the build system finds unitree_sdk2 and links
// real_go2_sdk.cpp instead (Turn 2.5 follow-up — currently TODO).

#include "san_unitree_driver/go2_sdk_interface.hpp"

#include <iostream>

namespace san_unitree_driver {

namespace {

class StubGo2Sdk : public Go2SdkInterface {
public:
  void init(const std::string& interface_name) override {
    std::cerr << "[san_unitree_driver][STUB] init(\""
              << interface_name
              << "\") — real unitree_sdk2 not linked; this is a "
              << "build-only stub. Real Go2 communication will NOT work.\n";
    initialized_ = true;
  }

  void registerLidarCallback(LidarCallback cb) override   { lidar_cb_  = std::move(cb); }
  void registerImuCallback(ImuCallback cb) override       { imu_cb_    = std::move(cb); }
  void registerCameraCallback(CameraCallback cb) override { camera_cb_ = std::move(cb); }
  void registerStateCallback(StateCallback cb) override   { state_cb_  = std::move(cb); }

  bool sendCmdVel(const CmdVel& /*cmd*/) override     { return false; }
  bool sendGoalPose(const GoalPose& /*goal*/) override { return false; }

  bool isHealthy() const override { return false; }   // never healthy

private:
  bool initialized_ = false;
  LidarCallback   lidar_cb_;
  ImuCallback     imu_cb_;
  CameraCallback  camera_cb_;
  StateCallback   state_cb_;
};

}  // namespace

std::unique_ptr<Go2SdkInterface> makeRealGo2Sdk() {
  // When the real SDK wrapper is wired up (Turn 2.5), this factory
  // will return a RealGo2Sdk instance. For now (stub-only build),
  // we return the stub and let isHealthy()==false alert operators.
  return std::make_unique<StubGo2Sdk>();
}

}  // namespace san_unitree_driver
