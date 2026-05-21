// SAN v1.5 PHASE 9 — FireAuthorizationNode: ROS 2 gate combining all
// three Phase 2-D modules (HMAC + Two-key + Audit).
//
// DCN-2026-001 D-004 (Limp Mode 발사 정책 Option A):
//   * 모든 FireAuthorizationRequest 는 HMAC + Two-key + (Limp Mode 검사)
//     순서로 평가되고, 결과는 audit 로그 + ROS topic 양쪽으로 기록.
//   * Limp Mode 진입 시에도 발사 권한 유지 (audit 만 limp_mode_fire=true 태깅).
//
// Subscriptions:
//   /swarm/fire/authorization_request  (FireAuthorizationRequest)
//   /swarm/operation_state             (OperationState)
//
// Publications:
//   /swarm/fire/authorization_response (FireAuthorizationResponse)
//
// Timers:
//   10 Hz — Two-key state machine timeout detection.
//
// 권원:
//   * SAN-SDD-SWARM-001 v1.5 §5.7.2.1
//   * SAN-OPS-SOP-001   v1.5 §7.11
//   * SAN-IDS-CMD-001   v1.5 §3.5

#ifndef SAN_FIRE_AUTHORIZATION__FIRE_AUTHORIZATION_NODE_HPP_
#define SAN_FIRE_AUTHORIZATION__FIRE_AUTHORIZATION_NODE_HPP_

#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/fire_authorization_request.hpp>
#include <combat_robot_msgs/msg/fire_authorization_response.hpp>
#include <combat_robot_msgs/msg/operation_state.hpp>

#include "san_fire_authorization/audit_logger.hpp"
#include "san_fire_authorization/hmac_authenticator.hpp"
#include "san_fire_authorization/two_key_state_machine.hpp"

namespace san_fire_authorization {

class FireAuthorizationNode : public rclcpp::Node {
public:
  /// Construct with NodeOptions. Parameters are declared via the
  /// ROS 2 parameter system and read at startup; see
  /// config/fire_authorization.yaml for the defaults.
  explicit FireAuthorizationNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());

private:
  // ─── parameter loading ────────────────────────────────────────────
  void declareParameters();
  void loadParametersAndConstructModules();

  // ─── ROS callbacks ────────────────────────────────────────────────
  void onRequest(
      const combat_robot_msgs::msg::FireAuthorizationRequest::SharedPtr msg);
  void onOperationState(
      const combat_robot_msgs::msg::OperationState::SharedPtr msg);
  void onTwoKeyTick();

  // ─── core gate logic ──────────────────────────────────────────────
  /// Run HMAC + Two-key + Limp-Mode policy against `req`, publish a
  /// response, and write an audit entry. Returns nothing — every
  /// decision is durable via the audit log.
  void evaluateAndRespond(
      const combat_robot_msgs::msg::FireAuthorizationRequest& req);

  /// Translate internal results to FireAuthorizationResponse.REASON_*.
  static uint8_t hmacResultToReason(AuthResult r);
  static uint8_t twoKeyResultToReason(TwoKeyResult r);

  /// Translate REASON_* code to short text for audit `reason` field.
  static std::string reasonCodeToString(uint8_t code);

  // ─── members ──────────────────────────────────────────────────────
  std::unique_ptr<HmacAuthenticator>    hmac_;
  std::unique_ptr<TwoKeyStateMachine>   key_sm_;
  std::unique_ptr<AuditLogger>          audit_;

  rclcpp::Subscription<combat_robot_msgs::msg::FireAuthorizationRequest>::SharedPtr
      req_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::OperationState>::SharedPtr
      op_state_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FireAuthorizationResponse>::SharedPtr
      resp_pub_;
  rclcpp::TimerBase::SharedPtr tick_timer_;

  // Mirrored operation state — read by request callback, written by
  // op_state callback. Single-threaded executor by default; if user
  // configures MultiThreadedExecutor a callback-group lock is needed
  // (TODO Turn 5 hardening).
  bool      op_in_limp_mode_  = false;
  uint32_t  op_hub_term_      = 0;
  uint32_t  op_leader_term_   = 0;
  uint32_t  op_n_alive_       = 0;
  uint64_t  op_state_last_ms_ = 0;

  // Last request id seen — for diagnostics.
  uint32_t  last_request_id_  = 0;

  // Parameters.
  std::string secret_path_;
  std::string audit_log_path_;
};

}  // namespace san_fire_authorization

#endif  // SAN_FIRE_AUTHORIZATION__FIRE_AUTHORIZATION_NODE_HPP_
