// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — FireAuthorizationNode (PATCHED 2026-05-13).
//
// DCN-2026-001 D-004 (Limp Mode 발사 정책 Option A):
//   * 모든 FireAuthorizationRequest 는 HMAC + Two-key + (Limp Mode 검사)
//     순서로 평가되고, 결과는 audit 로그 + ROS topic 양쪽으로 기록.
//   * Limp Mode 진입 시에도 발사 권한 유지 (audit 만 limp_mode_fire=true).
//
// PATCH 2026-05-13 (Fire deep-dive review):
//   * Audit-fail → fire DENY. The previous code logged the audit
//     failure and still published granted=true. That violated DCN
//     D-004's "모든 발사 인가는 영구 감사 의무". Now a failed audit
//     emit() causes a REASON_DENIED_OTHER response with detail
//     "audit log unavailable" and no fire is authorized.
//   * OperationState staleness guard. The previous code happily used
//     a 60-s-stale OperationState; now if the last heartbeat is older
//     than `op_state_max_age_ms` (default 5000 ms) the gate treats
//     in_limp_mode as TRUE (conservative) for tagging — does not deny
//     fire because the FireAuthorizationRequest itself is HMAC+Two-key
//     authenticated, but the audit reflects the staleness.
//   * hub_term / leader_term now actually populated from the
//     OperationState message (was always 0 in audit entries).
//
// 권원:
//   * SAN-SDD-SWARM-001 v1.5 §5.7.2.1
//   * SAN-OPS-SOP-001   v1.5 §7.11
//   * SAN-IDS-CMD-001   v1.5 §3.5

#ifndef SAN_FIRE_AUTHORIZATION__FIRE_AUTHORIZATION_NODE_HPP_
#define SAN_FIRE_AUTHORIZATION__FIRE_AUTHORIZATION_NODE_HPP_

#include <memory>
#include <mutex>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/fire_authorization_request.hpp>
#include <combat_robot_msgs/msg/fire_authorization_response.hpp>
#include <combat_robot_msgs/msg/operation_state.hpp>

#include "san_fire_authorization/audit_logger.hpp"
#include "san_fire_authorization/hmac_authenticator.hpp"
#include "san_fire_authorization/two_key_state_machine.hpp"

namespace san_fire_authorization
{

class FireAuthorizationNode : public rclcpp::Node
{
public:
  explicit FireAuthorizationNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

private:
  void declareParameters();
  void loadParametersAndConstructModules();

  void onRequest(
    const combat_robot_msgs::msg::FireAuthorizationRequest::SharedPtr msg);
  void onOperationState(
    const combat_robot_msgs::msg::OperationState::SharedPtr msg);
  void onTwoKeyTick();

  void evaluateAndRespond(
    const combat_robot_msgs::msg::FireAuthorizationRequest & req);

  static uint8_t hmacResultToReason(AuthResult r);
  static uint8_t twoKeyResultToReason(TwoKeyResult r);
  static std::string reasonCodeToString(uint8_t code);

  std::unique_ptr<HmacAuthenticator> hmac_;
  std::unique_ptr<TwoKeyStateMachine> key_sm_;
  std::unique_ptr<AuditLogger> audit_;

  rclcpp::Subscription<combat_robot_msgs::msg::FireAuthorizationRequest>::SharedPtr
    req_sub_;
  rclcpp::Subscription<combat_robot_msgs::msg::OperationState>::SharedPtr
    op_state_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FireAuthorizationResponse>::SharedPtr
    resp_pub_;
  rclcpp::TimerBase::SharedPtr tick_timer_;

  // ─── OperationState snapshot ──────────────────────────────────────
  // PATCH 2026-05-13: protected by op_mu_ so onRequest gets a
  // coherent view even if onOperationState runs on a different
  // callback group (MultiThreadedExecutor).
  mutable std::mutex op_mu_;
  bool op_in_limp_mode_ = false;
  uint32_t op_hub_term_ = 0;                // ★ now actually populated
  uint32_t op_leader_term_ = 0;             // ★ now actually populated
  uint32_t op_n_alive_ = 0;
  uint64_t op_state_last_ms_ = 0;

  uint32_t last_request_id_ = 0;

  // Parameters.
  std::string secret_path_;
  std::string audit_log_path_;
  uint32_t op_state_max_age_ms_ = 5000;     // ★ PATCH 2026-05-13
};

}  // namespace san_fire_authorization

#endif  // SAN_FIRE_AUTHORIZATION__FIRE_AUTHORIZATION_NODE_HPP_
