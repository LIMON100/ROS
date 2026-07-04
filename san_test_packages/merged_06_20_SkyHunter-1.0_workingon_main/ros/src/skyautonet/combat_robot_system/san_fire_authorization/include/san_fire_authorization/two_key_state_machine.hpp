// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — Two-key arming state machine.
//
// DCN-2026-001 D-004: 모든 발사 명령은 Two-key arming sequence 강제.
//   KEY1 (target tap)    : 표적 지정 → Armed 상태 진입
//   KEY2 (confirm fire)  : 적색 Confirm 버튼 → 발사 인가
//   timeout              : KEY1 → KEY2 간격 ≤ 5000 ms
//   CANCEL               : 운용자 취소 → Idle 복귀
//
// State machine 자체는 ROS 2 비의존. san_fire_authorization 노드의
// inbound callback 에서 호출 + 단위 시험 (gtest) 으로 검증.
//
// 권원:
//   * SAN-SDD-SWARM-001 v1.5 §5.7.2.1 (Limp Mode 발사 정책 박스)
//   * SAN-OPS-SOP-001   v1.5 §7.11   (Limp Mode FAQ — Option A)
//   * SAN-IDS-CMD-001   v1.5 §3.5    (FireAuthorizationRequest.command_type)
//
// Thread-safety: internal mutex 보호. 다중 callback group 또는
// MultiThreadedExecutor 환경에서도 안전.

#ifndef SAN_FIRE_AUTHORIZATION__TWO_KEY_STATE_MACHINE_HPP_
#define SAN_FIRE_AUTHORIZATION__TWO_KEY_STATE_MACHINE_HPP_

#include <cstdint>
#include <mutex>

namespace san_fire_authorization
{

/// Two-key arming timeout. Operator must press CONFIRM within this
/// window after the target tap, or the gate forces a re-arm.
inline constexpr int64_t kTwoKeyTimeoutMs = 5000;

/// State machine states (internal — exposed for diagnostics + tests).
enum class TwoKeyState : uint8_t
{
  Idle      = 0,    // 무장 안 됨, 발사 불가
  Key1Armed = 1,    // KEY1 수락, KEY2 대기 중
};

/// Event-handler return codes. 1:1 mapped onto FireAuthorizationResponse
/// REASON_* (REASON_GRANTED, REASON_DENIED_TWO_KEY_INCOMPLETE,
/// REASON_DENIED_TWO_KEY_TIMEOUT).
enum class TwoKeyResult : uint8_t
{
  Idle             = 0,   // No-op (current state preserved)
  Armed            = 1,   // KEY1 accepted, waiting for KEY2
  Granted          = 2,   // FIRE granted — gate forwards downstream
  Cancelled        = 3,   // Operator cancelled — return to Idle
  DeniedIncomplete = 4,   // KEY2 without prior KEY1 (or request_id mismatch)
  DeniedTimeout    = 5,   // KEY2 too late (or tick-detected expiry)
};

/// Thread-safe Two-key arming state machine.
///
/// Use case: san_fire_authorization::FireAuthorizationNode subscribes
/// to FireAuthorizationRequest, dispatches by command_type:
///   TWO_KEY_KEY1_TARGET_TAP → onKey1(...)
///   TWO_KEY_KEY2_CONFIRM    → onKey2(...)
///   TWO_KEY_CANCEL          → onCancel(...)
/// and runs tick() periodically (e.g. 10 Hz via rclcpp::Timer) to
/// catch timeouts when no inbound message arrives.
class TwoKeyStateMachine
{
public:
  TwoKeyStateMachine() = default;

  /// Accept a KEY1 (target tap). Always (re-)arms the machine —
  /// operator may change target by tapping a different point. Returns
  /// TwoKeyResult::Armed.
  TwoKeyResult onKey1(uint32_t request_id, uint64_t now_ms);

  /// Accept a KEY2 (confirm fire). Returns:
  ///   - Granted          if state is Key1Armed, request_id matches
  ///                      the armed KEY1, and elapsed ≤ kTwoKeyTimeoutMs.
  ///   - DeniedIncomplete if state is Idle or request_id mismatches.
  ///   - DeniedTimeout    if state is Key1Armed but elapsed >
  ///                      kTwoKeyTimeoutMs.
  /// Always resets to Idle after Granted / DeniedTimeout (caller must
  /// re-arm for next shot).
  TwoKeyResult onKey2(uint32_t request_id, uint64_t now_ms);

  /// Accept a CANCEL. Always returns to Idle. Returns Cancelled if a
  /// KEY1 was armed; Idle (no-op) otherwise. request_id is unused —
  /// cancel is a global "abort" gesture.
  TwoKeyResult onCancel();

  /// Periodic tick — call at ≥ 1 Hz to detect timeout without an
  /// inbound KEY2. If Key1Armed and elapsed > kTwoKeyTimeoutMs, resets
  /// to Idle and returns DeniedTimeout. Otherwise returns the current
  /// state mapped to Armed or Idle.
  TwoKeyResult tick(uint64_t now_ms);

  // ─── Diagnostics ───────────────────────────────────────────────────
  TwoKeyState state() const;
  uint32_t    key1RequestId() const;
  uint64_t    key1TimestampMs() const;

private:
  mutable std::mutex mutex_;
  TwoKeyState state_ = TwoKeyState::Idle;
  uint32_t key1_request_id_ = 0;
  uint64_t key1_timestamp_ms_ = 0;

  /// Reset to Idle. Caller must hold mutex_.
  void resetLocked();
};

}  // namespace san_fire_authorization

#endif  // SAN_FIRE_AUTHORIZATION__TWO_KEY_STATE_MACHINE_HPP_
