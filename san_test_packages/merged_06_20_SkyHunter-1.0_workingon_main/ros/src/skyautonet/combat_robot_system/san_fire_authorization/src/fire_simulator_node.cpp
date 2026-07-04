// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-018] FireSimulatorNode implementation.

#include "san_fire_authorization/fire_simulator_node.hpp"

#include <limits>

namespace san_fire_authorization
{

FireSimulatorNode::FireSimulatorNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("fire_simulator_node", opts)
{
  declare_parameter<int>("robot_id", 1);
  declare_parameter<double>("alignment_tolerance_deg", 2.0);
  declare_parameter<double>("target_pan_offset_rad", 0.0);
  declare_parameter<double>("target_tilt_offset_rad", 0.0);

  robot_id_ = static_cast<int32_t>(
    get_parameter("robot_id").as_int());
  alignment_tolerance_rad_ =
    get_parameter("alignment_tolerance_deg").as_double() * M_PI / 180.0;
  target_pan_offset_rad_ =
    get_parameter("target_pan_offset_rad").as_double();
  target_tilt_offset_rad_ =
    get_parameter("target_tilt_offset_rad").as_double();

  auth_resp_sub_ = create_subscription<
    combat_robot_msgs::msg::FireAuthorizationResponse>(
    "/swarm/fire/authorization_response",
    rclcpp::QoS(10).reliable(),
    std::bind(
      &FireSimulatorNode::onAuthorizationResponse, this,
      std::placeholders::_1));

  gimbal_sub_ = create_subscription<sensor_msgs::msg::JointState>(
    "/gimbal/pan_tilt_state", 10,
    std::bind(
      &FireSimulatorNode::onGimbalState, this,
      std::placeholders::_1));

  result_pub_ = create_publisher<combat_robot_msgs::msg::FireResult>(
    "/swarm/fire/result", rclcpp::QoS(10).reliable());

  RCLCPP_INFO(
    get_logger(),
    "[DCN-2026-018] FireSimulatorNode ready "
    "(robot_id=%d, tolerance=%.2f°, target_offset=(%.3f, %.3f) rad)",
    robot_id_, alignment_tolerance_rad_ * 180.0 / M_PI,
    target_pan_offset_rad_, target_tilt_offset_rad_);
}

void FireSimulatorNode::onAuthorizationResponse(
  combat_robot_msgs::msg::FireAuthorizationResponse::SharedPtr msg)
{
  if (msg == nullptr) {return;}

  auto fr = evaluateForTest(*msg);

  if (!msg->granted) {
    // Per anti-goals: we still surface the no-authorization case so
    // operator UI gets a visible record, but do NOT pretend to fire.
    fr.result = combat_robot_msgs::msg::FireResult::RESULT_NO_AUTHORIZATION;
    fr.rounds_fired = 0;
  }

  result_pub_->publish(fr);

  RCLCPP_INFO(
    get_logger(),
    "[Simulated fire] result=%u target=%d rounds=%u "
    "auth_chain='%s'",
    fr.result, fr.target_id, fr.rounds_fired,
    fr.authorization_chain.c_str());
}

combat_robot_msgs::msg::FireResult FireSimulatorNode::evaluateForTest(
  const combat_robot_msgs::msg::FireAuthorizationResponse & resp)
{
  // ─── Audit A7 (P2) ─────────────────────────────────────────────
  // Snapshot gimbal + target state under lock so concurrent
  // onGimbalState write doesn't tear a double mid-evaluation. Take
  // copies + release lock before the FireResult build so we don't
  // hold the lock across publish() in production callers.
  double pan, tilt, tgt_pan, tgt_tilt;
  int32_t target_id;
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    pan = current_pan_rad_;
    tilt = current_tilt_rad_;
    tgt_pan = target_pan_offset_rad_;
    tgt_tilt = target_tilt_offset_rad_;
    target_id = active_target_id_;
  }

  // Alignment error = (target_offset) - (current gimbal angle).
  // A perfectly tracked target with the gimbal sitting at the offset
  // gives error == 0 (HIT). A 5° misalignment fails the tolerance.
  const double pan_err = tgt_pan - pan;
  const double tilt_err = tgt_tilt - tilt;

  const bool aligned =
    (std::abs(pan_err) < alignment_tolerance_rad_) &&
    (std::abs(tilt_err) < alignment_tolerance_rad_);

  combat_robot_msgs::msg::FireResult fr;
  fr.header.stamp = now();
  fr.robot_id = static_cast<uint32_t>(robot_id_);
  fr.command_id = resp.request_id;
  fr.sequence = resp.sequence;
  fr.result = resp.granted ?
    (aligned ? combat_robot_msgs::msg::FireResult::RESULT_SUCCESS :
    combat_robot_msgs::msg::FireResult::RESULT_MISS) :
    combat_robot_msgs::msg::FireResult::RESULT_NO_AUTHORIZATION;
  // Audit A10 (P3) note: rounds_fired=1 on MISS is intentional — the
  // sim represents "shot fired, target missed" (operator UI accounting
  // counts a discharged round even on miss). NO_AUTHORIZATION = 0
  // (no round leaves the chamber).
  fr.rounds_fired = resp.granted ? 1u : 0u;
  // Audit A9 (P3): target_id sentinel = UINT32_MAX when no active
  // target — distinguishes from a valid target_id == 0. Operator UI
  // should treat UINT32_MAX as "no track". Pre-fix collapsed both
  // -1 (no track) and 0 (valid id 0) to 0.
  fr.target_id = (target_id >= 0) ?
    static_cast<uint32_t>(target_id) :
    std::numeric_limits<uint32_t>::max();
  fr.distance_to_target_m = 0.0f;
  // Audit A7 — use the snapshot copies, not the live members.
  fr.impact_point_x_m = static_cast<float>(pan);
  fr.impact_point_y_m = static_cast<float>(tilt);
  fr.confidence = aligned ? 1.0f : 0.0f;
  fr.authorization_chain = resp.audit_log_uuid;
  fr.notes = aligned ?
    "Within tolerance — simulated HIT" :
    "Outside tolerance — simulated MISS";
  fr.timestamp_fire_ms = resp.response_timestamp_ms;
  fr.timestamp_report_ms = static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  return fr;
}

void FireSimulatorNode::onGimbalState(sensor_msgs::msg::JointState::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  // Audit A7 — lock the gimbal write so evaluateForTest's snapshot
  // never reads a torn double.
  std::lock_guard<std::mutex> lock(state_mu_);
  if (msg->position.size() >= 1) {current_pan_rad_ = msg->position[0];}
  if (msg->position.size() >= 2) {current_tilt_rad_ = msg->position[1];}
}

}  // namespace san_fire_authorization
