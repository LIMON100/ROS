// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — FireAuthorizationNode (PATCHED 2026-05-13).

#include "san_fire_authorization/fire_authorization_node.hpp"

#include <chrono>
#include <stdexcept>

#include <combat_robot_msgs/msg/fire_authorization_request.hpp>
#include <combat_robot_msgs/msg/fire_authorization_response.hpp>

namespace san_fire_authorization
{

using FireReq = combat_robot_msgs::msg::FireAuthorizationRequest;
using FireResp = combat_robot_msgs::msg::FireAuthorizationResponse;
using OpState = combat_robot_msgs::msg::OperationState;

FireAuthorizationNode::FireAuthorizationNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("fire_authorization_node", opts)
{
  declareParameters();
  loadParametersAndConstructModules();

  const auto qos_p1 = rclcpp::QoS(10).reliable();

  req_sub_ = create_subscription<FireReq>(
    "/swarm/fire/authorization_request",
    qos_p1,
    std::bind(
      &FireAuthorizationNode::onRequest, this,
      std::placeholders::_1));

  resp_pub_ = create_publisher<FireResp>(
    "/swarm/fire/authorization_response", qos_p1);

  op_state_sub_ = create_subscription<OpState>(
    "/swarm/operation_state",
    rclcpp::QoS(1).best_effort(),
    std::bind(
      &FireAuthorizationNode::onOperationState, this,
      std::placeholders::_1));

  tick_timer_ = create_wall_timer(
    std::chrono::milliseconds(100),
    std::bind(&FireAuthorizationNode::onTwoKeyTick, this));

  RCLCPP_INFO(
    get_logger(),
    "FireAuthorizationNode UP: secret=%s audit=%s op_state_max_age=%ums",
    secret_path_.c_str(), audit_log_path_.c_str(), op_state_max_age_ms_);
}

void FireAuthorizationNode::declareParameters()
{
  declare_parameter<std::string>(
    "secret_path", "/etc/san/mesh_secret.bin");
  declare_parameter<std::string>(
    "audit_log_path", "/var/log/san/fire_audit.log");
  declare_parameter<int>("rotation_bytes", 10 * 1024 * 1024);
  // PATCH 2026-05-13: OperationState freshness.
  declare_parameter<int>("op_state_max_age_ms", 5000);
}

void FireAuthorizationNode::loadParametersAndConstructModules()
{
  secret_path_ = get_parameter("secret_path").as_string();
  audit_log_path_ = get_parameter("audit_log_path").as_string();
  const int rotation_bytes =
    static_cast<int>(get_parameter("rotation_bytes").as_int());
  op_state_max_age_ms_ = static_cast<uint32_t>(
    get_parameter("op_state_max_age_ms").as_int());

  hmac_ = std::make_unique<HmacAuthenticator>(secret_path_);
  key_sm_ = std::make_unique<TwoKeyStateMachine>();
  audit_ = std::make_unique<AuditLogger>(
    audit_log_path_,
    static_cast<std::size_t>(rotation_bytes));
}

// ─── callbacks ──────────────────────────────────────────────────────────

void FireAuthorizationNode::onRequest(const FireReq::SharedPtr msg)
{
  if (!msg) {return;}
  last_request_id_ = msg->request_id;
  evaluateAndRespond(*msg);
}

void FireAuthorizationNode::onOperationState(const OpState::SharedPtr msg)
{
  if (!msg) {return;}
  std::lock_guard<std::mutex> lock(op_mu_);
  op_in_limp_mode_ = msg->in_limp_mode;
  op_n_alive_ = msg->n_alive_robots;
  // PATCH 2026-05-13 (corrected 2026-05-24): OperationState.msg has
  // no hub_term / leader_term fields — only hub_robot_id /
  // leader_robot_id (IDs, not terms). The intent of recording term
  // for forensic context predates the msg schema; until OperationState
  // adds those fields, leave the audit slots at 0.
  //
  // Audit A11 (P3): when a future DCN extends OperationState.msg with
  // term fields, restore population here. Tracked via the placeholder
  // and the v1.5.4 cross-audit report.
  op_hub_term_ = 0;
  op_leader_term_ = 0;
  op_state_last_ms_ = msg->timestamp_ms;
}

void FireAuthorizationNode::onTwoKeyTick()
{
  const auto now_ms = static_cast<uint64_t>(
    now().nanoseconds() / 1'000'000);
  const auto r = key_sm_->tick(now_ms);
  if (r == TwoKeyResult::DeniedTimeout) {
    RCLCPP_WARN(
      get_logger(),
      "TwoKey timeout (no KEY2 within %ld ms) — re-arm required",
      static_cast<long>(kTwoKeyTimeoutMs));
  }
}

// ─── core gate ──────────────────────────────────────────────────────────

void FireAuthorizationNode::evaluateAndRespond(const FireReq & req)
{
  const auto now_ms = static_cast<uint64_t>(
    now().nanoseconds() / 1'000'000);

  // Snapshot OperationState under lock (PATCH 2026-05-13).
  bool op_in_limp;
  uint32_t op_hub_term, op_leader_term, op_n_alive;
  uint64_t op_state_last_ms;
  {
    std::lock_guard<std::mutex> lock(op_mu_);
    op_in_limp = op_in_limp_mode_;
    op_hub_term = op_hub_term_;
    op_leader_term = op_leader_term_;
    op_n_alive = op_n_alive_;
    op_state_last_ms = op_state_last_ms_;
  }

  // PATCH 2026-05-13: staleness guard — if OperationState is older than
  // op_state_max_age_ms, assume in_limp_mode for conservative tagging.
  bool op_state_stale = false;
  if (op_state_last_ms > 0) {
    const uint64_t age_ms =
      (now_ms > op_state_last_ms) ? (now_ms - op_state_last_ms) : 0ULL;
    if (age_ms > op_state_max_age_ms_) {
      op_state_stale = true;
      op_in_limp = true;       // conservative
    }
  }

  FireResp resp;
  resp.header.stamp = now();
  resp.request_id = req.request_id;
  resp.sequence = req.sequence;
  resp.response_timestamp_ms = now_ms;
  resp.granted = false;
  resp.reason = FireResp::REASON_DENIED_OTHER;
  resp.reason_detail = "";
  resp.limp_mode_fire = false;
  resp.audit_log_uuid = "";

  AuditEntry ae;
  ae.timestamp_ms = now_ms;
  ae.request_id = req.request_id;
  ae.operator_id = req.operator_id;
  ae.target_lat_e7 = req.target_lat_e7;
  ae.target_lon_e7 = req.target_lon_e7;
  ae.target_alt_mm = req.target_alt_mm;
  ae.hub_term = op_hub_term;             // ★ now populated
  ae.leader_term = op_leader_term;       // ★ now populated
  ae.n_alive_robots = op_n_alive;
  ae.limp_mode_fire = false;

  // PATCH 2026-05-13: audit-fail → DENY (was: log error + grant anyway).
  // DCN-2026-001 D-004 mandates "모든 발사 인가는 영구 감사 의무" —
  // without a durable audit record, no fire shall be authorized.
  auto finalize = [&](bool granted_decision, uint8_t reason_code,
      const std::string & detail) {
      ae.granted = granted_decision;
      ae.reason = reasonCodeToString(reason_code);
      ae.reason_detail = detail;
      ae.limp_mode_fire = (granted_decision && op_in_limp);

      if (op_state_stale) {
        ae.reason_detail += " [op_state stale]";
      }

      const auto emit_r = audit_->emit(ae);
      if (emit_r.ok) {
        resp.audit_log_uuid = emit_r.uuid;
        resp.granted = granted_decision;
        resp.reason = reason_code;
        resp.reason_detail = ae.reason_detail;
        resp.limp_mode_fire = ae.limp_mode_fire;
      } else {
        // ★ PATCH: audit failed → force DENY regardless of decision.
        RCLCPP_ERROR(
          get_logger(),
          "Audit emit FAILED (dropped=%zu) — DENYING fire authorization "
          "per DCN-2026-001 D-004 (no audit, no fire)",
          audit_->droppedCount());
        resp.granted = false;
        resp.reason = FireResp::REASON_DENIED_OTHER;
        resp.reason_detail = "audit log unavailable";
        resp.limp_mode_fire = false;
        resp.audit_log_uuid = "";
      }
      resp_pub_->publish(resp);
    };

  // ─ 1) HMAC verify ───────────────────────────────────────────────
  AuthMessage am;
  am.request_id = req.request_id;
  am.sequence = req.sequence;
  am.operator_id = req.operator_id;
  am.nonce = req.nonce;
  am.request_timestamp_ms = req.request_timestamp_ms;
  am.command_type = req.command_type;
  am.target_lat_e7 = req.target_lat_e7;
  am.target_lon_e7 = req.target_lon_e7;
  am.target_alt_mm = req.target_alt_mm;

  const AuthResult hr = hmac_->verify(am, req.hmac_signature, now_ms);
  if (hr != AuthResult::Granted) {
    finalize(
      false, hmacResultToReason(hr),
      "HMAC validation failed");
    return;
  }

  // ─ 2) Two-key state machine ─────────────────────────────────────
  TwoKeyResult kr = TwoKeyResult::Idle;
  switch (req.command_type) {
    case FireReq::TWO_KEY_KEY1_TARGET_TAP:
      kr = key_sm_->onKey1(req.request_id, now_ms);
      break;
    case FireReq::TWO_KEY_KEY2_CONFIRM:
      kr = key_sm_->onKey2(req.request_id, now_ms);
      break;
    case FireReq::TWO_KEY_CANCEL:
      kr = key_sm_->onCancel();
      break;
    default:
      finalize(
        false, FireResp::REASON_DENIED_OTHER,
        "unknown command_type");
      return;
  }

  if (kr == TwoKeyResult::Armed) {
    finalize(
      false, FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE,
      "ARMED — awaiting KEY2");
    return;
  }
  if (kr == TwoKeyResult::Cancelled) {
    finalize(
      false, FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE,
      "CANCELLED by operator");
    return;
  }
  if (kr == TwoKeyResult::DeniedIncomplete) {
    finalize(
      false, FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE,
      "KEY2 without prior KEY1 or request_id mismatch");
    return;
  }
  if (kr == TwoKeyResult::DeniedTimeout) {
    finalize(
      false, FireResp::REASON_DENIED_TWO_KEY_TIMEOUT,
      "KEY2 arrived after 5000 ms timeout");
    return;
  }

  // ─ 3) Limp Mode tagging (does NOT deny) ─────────────────────────
  if (op_in_limp) {
    RCLCPP_WARN(
      get_logger(),
      "Granting fire in Limp Mode (operator=%s, request_id=%u)%s",
      req.operator_id.c_str(), req.request_id,
      op_state_stale ? " [op_state stale]" : "");
  }

  // ─ 4) Granted ───────────────────────────────────────────────────
  finalize(
    true, FireResp::REASON_GRANTED,
    op_in_limp ? "GRANTED in Limp Mode" : "GRANTED");
}

uint8_t FireAuthorizationNode::hmacResultToReason(AuthResult r)
{
  switch (r) {
    case AuthResult::Granted:               return FireResp::REASON_GRANTED;
    case AuthResult::DeniedHmacFail:        return FireResp::REASON_DENIED_HMAC_FAIL;
    case AuthResult::DeniedNonceReuse:      return FireResp::REASON_DENIED_NONCE_REUSE;
    case AuthResult::DeniedTimestampDrift:  return FireResp::REASON_DENIED_TIMESTAMP_DRIFT;
    case AuthResult::DeniedInternal:        return FireResp::REASON_DENIED_OTHER;
  }
  return FireResp::REASON_DENIED_OTHER;
}

uint8_t FireAuthorizationNode::twoKeyResultToReason(TwoKeyResult r)
{
  switch (r) {
    case TwoKeyResult::Granted:           return FireResp::REASON_GRANTED;
    case TwoKeyResult::DeniedIncomplete:  return FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE;
    case TwoKeyResult::DeniedTimeout:     return FireResp::REASON_DENIED_TWO_KEY_TIMEOUT;
    case TwoKeyResult::Idle:
    case TwoKeyResult::Armed:
    case TwoKeyResult::Cancelled:
      return FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE;
  }
  return FireResp::REASON_DENIED_OTHER;
}

std::string FireAuthorizationNode::reasonCodeToString(uint8_t code)
{
  switch (code) {
    case FireResp::REASON_GRANTED:                  return "GRANTED";
    case FireResp::REASON_DENIED_HMAC_FAIL:         return "HMAC_FAIL";
    case FireResp::REASON_DENIED_NONCE_REUSE:       return "NONCE_REUSE";
    case FireResp::REASON_DENIED_TIMESTAMP_DRIFT:   return "TIMESTAMP_DRIFT";
    case FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE: return "TWO_KEY_INCOMPLETE";
    case FireResp::REASON_DENIED_TWO_KEY_TIMEOUT:   return "TWO_KEY_TIMEOUT";
    case FireResp::REASON_DENIED_AUTONOMOUS_ENGAGEMENT: return "AUTONOMOUS_ENGAGEMENT";
    case FireResp::REASON_DENIED_OTHER:             return "OTHER";
    default:                                         return "UNKNOWN";
  }
}

}  // namespace san_fire_authorization
