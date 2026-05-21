// SAN v1.5 PHASE 9 — FireAuthorizationNode implementation.
// See fire_authorization_node.hpp for the API contract.

#include "san_fire_authorization/fire_authorization_node.hpp"

#include <chrono>
#include <stdexcept>

#include <combat_robot_msgs/msg/fire_authorization_request.hpp>
#include <combat_robot_msgs/msg/fire_authorization_response.hpp>

namespace san_fire_authorization {

using FireReq  = combat_robot_msgs::msg::FireAuthorizationRequest;
using FireResp = combat_robot_msgs::msg::FireAuthorizationResponse;
using OpState  = combat_robot_msgs::msg::OperationState;

// ─── ctor ───────────────────────────────────────────────────────────────

FireAuthorizationNode::FireAuthorizationNode(const rclcpp::NodeOptions& opts)
    : rclcpp::Node("fire_authorization_node", opts) {
  declareParameters();
  loadParametersAndConstructModules();

  // P1 RELIABLE for fire — losses are unacceptable. Use depth 10 to
  // smooth bursts from operator finger-jitter.
  const auto qos_p1 = rclcpp::QoS(10).reliable();

  req_sub_ = create_subscription<FireReq>(
      "/swarm/fire/authorization_request",
      qos_p1,
      std::bind(&FireAuthorizationNode::onRequest, this,
                std::placeholders::_1));

  resp_pub_ = create_publisher<FireResp>(
      "/swarm/fire/authorization_response",
      qos_p1);

  // OperationState is a 1 Hz heartbeat — best-effort with depth 1 is
  // sufficient (consumers only care about the latest).
  op_state_sub_ = create_subscription<OpState>(
      "/swarm/operation_state",
      rclcpp::QoS(1).best_effort(),
      std::bind(&FireAuthorizationNode::onOperationState, this,
                std::placeholders::_1));

  // 10 Hz two-key tick for timeout detection without inbound KEY2.
  tick_timer_ = create_wall_timer(
      std::chrono::milliseconds(100),
      std::bind(&FireAuthorizationNode::onTwoKeyTick, this));

  RCLCPP_INFO(get_logger(),
              "FireAuthorizationNode UP: secret=%s, audit=%s",
              secret_path_.c_str(), audit_log_path_.c_str());
}

// ─── parameter loading ──────────────────────────────────────────────────

void FireAuthorizationNode::declareParameters() {
  declare_parameter<std::string>(
      "secret_path", "/etc/san/mesh_secret.bin");
  declare_parameter<std::string>(
      "audit_log_path", "/var/log/san/fire_audit.log");
  // Rotation hint (logrotate.d does the actual rotate; this is for
  // diagnostics only).
  declare_parameter<int>("rotation_bytes", 10 * 1024 * 1024);
}

void FireAuthorizationNode::loadParametersAndConstructModules() {
  secret_path_    = get_parameter("secret_path").as_string();
  audit_log_path_ = get_parameter("audit_log_path").as_string();
  const int rotation_bytes =
      static_cast<int>(get_parameter("rotation_bytes").as_int());

  // Fail-closed: any module ctor failure aborts the node.
  hmac_   = std::make_unique<HmacAuthenticator>(secret_path_);
  key_sm_ = std::make_unique<TwoKeyStateMachine>();
  audit_  = std::make_unique<AuditLogger>(
      audit_log_path_,
      static_cast<std::size_t>(rotation_bytes));
}

// ─── callbacks ──────────────────────────────────────────────────────────

void FireAuthorizationNode::onRequest(const FireReq::SharedPtr msg) {
  if (!msg) {
    return;
  }
  last_request_id_ = msg->request_id;
  evaluateAndRespond(*msg);
}

void FireAuthorizationNode::onOperationState(const OpState::SharedPtr msg) {
  if (!msg) {
    return;
  }
  op_in_limp_mode_  = msg->in_limp_mode;
  // Some heartbeat fields may not be defined in the v1.4 schema;
  // these reads tolerate either presence (zero-defaulted) without
  // crashing.
  op_n_alive_       = msg->n_alive_robots;
  op_state_last_ms_ = msg->timestamp_ms;
}

void FireAuthorizationNode::onTwoKeyTick() {
  const auto now_ms = static_cast<uint64_t>(
      now().nanoseconds() / 1'000'000);
  const auto r = key_sm_->tick(now_ms);
  if (r == TwoKeyResult::DeniedTimeout) {
    RCLCPP_WARN(get_logger(),
                "TwoKey timeout (no KEY2 within %ld ms) — re-arm required",
                static_cast<long>(kTwoKeyTimeoutMs));
    // Note: no audit entry here. The timeout is internal to the gate
    // — no operator-visible decision was made. If the operator later
    // sends KEY2, it will get DENIED_INCOMPLETE which IS audited.
  }
}

// ─── core gate ──────────────────────────────────────────────────────────

void FireAuthorizationNode::evaluateAndRespond(const FireReq& req) {
  const auto now_ms = static_cast<uint64_t>(
      now().nanoseconds() / 1'000'000);

  // Prepare common response + audit scaffolding.
  FireResp resp;
  resp.header.stamp           = now();
  resp.request_id             = req.request_id;
  resp.sequence               = req.sequence;
  resp.response_timestamp_ms  = now_ms;
  resp.granted                = false;  // default deny
  resp.reason                 = FireResp::REASON_DENIED_OTHER;
  resp.reason_detail          = "";
  resp.limp_mode_fire         = false;
  resp.audit_log_uuid         = "";

  AuditEntry ae;
  ae.timestamp_ms     = now_ms;
  ae.request_id       = req.request_id;
  ae.operator_id      = req.operator_id;
  ae.target_lat_e7    = req.target_lat_e7;
  ae.target_lon_e7    = req.target_lon_e7;
  ae.target_alt_mm    = req.target_alt_mm;
  ae.hub_term         = op_hub_term_;
  ae.leader_term      = op_leader_term_;
  ae.n_alive_robots   = op_n_alive_;
  ae.limp_mode_fire   = false;

  // Helper: finalize response + audit + publish + return.
  auto finalize = [&](bool granted, uint8_t reason_code,
                       const std::string& detail) {
    resp.granted       = granted;
    resp.reason        = reason_code;
    resp.reason_detail = detail;
    resp.limp_mode_fire = (granted && op_in_limp_mode_);

    ae.granted        = granted;
    ae.reason         = reasonCodeToString(reason_code);
    ae.reason_detail  = detail;
    ae.limp_mode_fire = resp.limp_mode_fire;

    const auto emit_r = audit_->emit(ae);
    if (emit_r.ok) {
      resp.audit_log_uuid = emit_r.uuid;
    } else {
      RCLCPP_ERROR(get_logger(),
          "Audit log emit FAILED — dropped_count=%zu, response will "
          "carry empty audit_log_uuid (forensic gap)",
          audit_->droppedCount());
    }
    resp_pub_->publish(resp);
  };

  // ─ 1) HMAC verify ───────────────────────────────────────────────
  AuthMessage am;
  am.request_id            = req.request_id;
  am.sequence              = req.sequence;
  am.operator_id           = req.operator_id;
  am.nonce                 = req.nonce;
  am.request_timestamp_ms  = req.request_timestamp_ms;
  am.command_type          = req.command_type;
  am.target_lat_e7         = req.target_lat_e7;
  am.target_lon_e7         = req.target_lon_e7;
  am.target_alt_mm         = req.target_alt_mm;

  const AuthResult hr = hmac_->verify(am, req.hmac_signature, now_ms);
  if (hr != AuthResult::Granted) {
    finalize(false, hmacResultToReason(hr),
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
      finalize(false, FireResp::REASON_DENIED_OTHER,
               "unknown command_type");
      return;
  }

  // KEY1 / CANCEL never grant; they're acks that audit-record + reply
  // with granted=false (no shot fired). The operator UI uses
  // reason_detail = "ARMED" / "CANCELLED" to render state.
  if (kr == TwoKeyResult::Armed) {
    finalize(false, FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE,
             "ARMED — awaiting KEY2");
    return;
  }
  if (kr == TwoKeyResult::Cancelled) {
    finalize(false, FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE,
             "CANCELLED by operator");
    return;
  }
  if (kr == TwoKeyResult::DeniedIncomplete) {
    finalize(false, FireResp::REASON_DENIED_TWO_KEY_INCOMPLETE,
             "KEY2 without prior KEY1 or request_id mismatch");
    return;
  }
  if (kr == TwoKeyResult::DeniedTimeout) {
    finalize(false, FireResp::REASON_DENIED_TWO_KEY_TIMEOUT,
             "KEY2 arrived after 5000 ms timeout");
    return;
  }

  // kr == Granted — fall through to grant.

  // ─ 3) Limp Mode + autonomous engagement guard ───────────────────
  // SDD-SWARM v1.5 §5.7.2.1 Option A: Limp Mode allows operator-
  // initiated fire but DISALLOWS autonomous engagement. The
  // FireAuthorizationRequest is by definition operator-initiated
  // (it carries operator_id + HMAC), so this branch is reserved for
  // future autonomous-fire request types. Logged for completeness.
  if (op_in_limp_mode_) {
    RCLCPP_WARN(get_logger(),
                "Granting fire in Limp Mode (operator=%s, request_id=%u)",
                req.operator_id.c_str(), req.request_id);
  }

  // ─ 4) Granted ───────────────────────────────────────────────────
  finalize(true, FireResp::REASON_GRANTED,
           op_in_limp_mode_ ? "GRANTED in Limp Mode"
                            : "GRANTED");
}

// ─── result → reason code ───────────────────────────────────────────────

uint8_t FireAuthorizationNode::hmacResultToReason(AuthResult r) {
  switch (r) {
    case AuthResult::Granted:               return FireResp::REASON_GRANTED;
    case AuthResult::DeniedHmacFail:        return FireResp::REASON_DENIED_HMAC_FAIL;
    case AuthResult::DeniedNonceReuse:      return FireResp::REASON_DENIED_NONCE_REUSE;
    case AuthResult::DeniedTimestampDrift:  return FireResp::REASON_DENIED_TIMESTAMP_DRIFT;
    case AuthResult::DeniedInternal:        return FireResp::REASON_DENIED_OTHER;
  }
  return FireResp::REASON_DENIED_OTHER;
}

uint8_t FireAuthorizationNode::twoKeyResultToReason(TwoKeyResult r) {
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

std::string FireAuthorizationNode::reasonCodeToString(uint8_t code) {
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
