// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-018] FireSimulatorNode — co-located simulator that reacts to
// fire_authorization_node grants and emits combat_robot_msgs/FireResult
// with HIT / MISS based on gimbal alignment vs the active tracked target.
//
// Sibling executable in san_fire_authorization (Tier 1, ADR-008). Stays
// alongside the real gate so the message contract is single-sourced and
// future operator-UI consumers can dev against the simulator before the
// physical weapon harness arrives.
//
// Topics:
//   sub  /swarm/fire/authorization_response (FireAuthorizationResponse)
//   sub  /gimbal/pan_tilt_state             (sensor_msgs/JointState; pos[0]=pan, pos[1]=tilt)
//   pub  /swarm/fire/result                 (FireResult)
//
// Parameters:
//   * robot_id (int, default 1)             — echoed into FireResult.robot_id
//   * alignment_tolerance_deg (double, 2.0) — HIT iff |pan_err| < tol AND |tilt_err| < tol
//   * target_pan_offset_rad (double, 0.0)   — synthetic "target bearing" until
//   * target_tilt_offset_rad (double, 0.0)   tracked_targets topic lands
//
// The simulator NEVER actuates a real device. It is conceptually a test
// double for the weapon controller, but kept in the production package
// so the FireResult schema cannot drift from the authorization gate.

#pragma once

#include <cmath>
#include <cstdint>
#include <mutex>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

#include <combat_robot_msgs/msg/fire_authorization_response.hpp>
#include <combat_robot_msgs/msg/fire_result.hpp>

namespace san_fire_authorization
{

class FireSimulatorNode : public rclcpp::Node
{
public:
  explicit FireSimulatorNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

  // ─── Test seams (no live ROS graph required) ────────────────────
  // Drive the alignment evaluator directly so unit tests can verify
  // HIT / MISS decision + FireResult population without spawning a
  // publisher / subscriber pair.
  //
  // Audit A8 (P3) note: signature is non-const because it touches
  // rclcpp::Node::now() (which is non-const). Logically the call
  // is read-only against this node's state (snapshot under
  // state_mu_); the production onAuthorizationResponse path also
  // calls this method, so naming as `*ForTest` is slightly
  // misleading. Kept for backward compatibility with existing
  // tests; future rename → `evaluate()` with `[[nodiscard]]`.
  combat_robot_msgs::msg::FireResult evaluateForTest(
    const combat_robot_msgs::msg::FireAuthorizationResponse & resp);

  void setGimbalForTest(double pan_rad, double tilt_rad)
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    current_pan_rad_ = pan_rad;
    current_tilt_rad_ = tilt_rad;
  }

  void setTargetForTest(
    double pan_offset_rad,
    double tilt_offset_rad,
    int32_t target_id)
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    target_pan_offset_rad_ = pan_offset_rad;
    target_tilt_offset_rad_ = tilt_offset_rad;
    active_target_id_ = target_id;
  }

  double toleranceRad() const {return alignment_tolerance_rad_;}

private:
  void onAuthorizationResponse(
    combat_robot_msgs::msg::FireAuthorizationResponse::SharedPtr msg);
  void onGimbalState(sensor_msgs::msg::JointState::SharedPtr msg);

  rclcpp::Subscription<combat_robot_msgs::msg::FireAuthorizationResponse>::SharedPtr
    auth_resp_sub_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr gimbal_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FireResult>::SharedPtr result_pub_;

  // ─── Audit A7 (P2) fix ──────────────────────────────────────────
  // Pre-fix: gimbal + target doubles were non-atomic; under MTE the
  // onGimbalState callback writes pan/tilt from one thread while
  // onAuthorizationResponse reads them from another — torn read on
  // a 64-bit double can produce a value not equal to either old or
  // new state, which would mis-classify HIT vs MISS. Mutex protects
  // the full snapshot (pan + tilt + target_offset_*) so evaluateForTest
  // sees a coherent gimbal pose.
  mutable std::mutex state_mu_;

  // Gimbal state (rad) — protected by state_mu_.
  double current_pan_rad_ = 0.0;
  double current_tilt_rad_ = 0.0;

  // Synthetic "target bearing" until a tracked-targets topic lands —
  // protected by state_mu_.
  double target_pan_offset_rad_ = 0.0;
  double target_tilt_offset_rad_ = 0.0;
  int32_t active_target_id_ = -1;

  // Configuration — set once at construction (no lock needed).
  int32_t robot_id_ = 1;
  double alignment_tolerance_rad_ = 2.0 * M_PI / 180.0;      // 2°
};

}  // namespace san_fire_authorization
