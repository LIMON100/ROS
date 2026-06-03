// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — Two-key arming state machine implementation.
// See two_key_state_machine.hpp for the API contract.

#include "san_fire_authorization/two_key_state_machine.hpp"

namespace san_fire_authorization
{

// ─── Event handlers ─────────────────────────────────────────────────────

TwoKeyResult TwoKeyStateMachine::onKey1(
  uint32_t request_id,
  uint64_t now_ms)
{
  std::lock_guard<std::mutex> lock(mutex_);
  // KEY1 always (re-)arms. Operator may change target by tapping a
  // different point — that's a fresh request_id with a fresh clock.
  state_ = TwoKeyState::Key1Armed;
  key1_request_id_ = request_id;
  key1_timestamp_ms_ = now_ms;
  return TwoKeyResult::Armed;
}

TwoKeyResult TwoKeyStateMachine::onKey2(
  uint32_t request_id,
  uint64_t now_ms)
{
  std::lock_guard<std::mutex> lock(mutex_);

  // Idle or other-armed → incomplete sequence.
  if (state_ != TwoKeyState::Key1Armed) {
    return TwoKeyResult::DeniedIncomplete;
  }
  // KEY2 must match the armed KEY1's request_id. Mismatching IDs
  // means this KEY2 belongs to a different (stale or future) fire
  // intent — treat as incomplete.
  if (request_id != key1_request_id_) {
    return TwoKeyResult::DeniedIncomplete;
  }

  // Timeout check. Use signed arithmetic so a backwards clock
  // (now_ms < key1_timestamp_ms_) is also caught.
  const int64_t elapsed_ms = static_cast<int64_t>(now_ms) -
    static_cast<int64_t>(key1_timestamp_ms_);
  if (elapsed_ms < 0 || elapsed_ms > kTwoKeyTimeoutMs) {
    resetLocked();
    return TwoKeyResult::DeniedTimeout;
  }

  // Granted — reset so the next fire intent must re-arm.
  resetLocked();
  return TwoKeyResult::Granted;
}

TwoKeyResult TwoKeyStateMachine::onCancel()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == TwoKeyState::Idle) {
    return TwoKeyResult::Idle;
  }
  resetLocked();
  return TwoKeyResult::Cancelled;
}

TwoKeyResult TwoKeyStateMachine::tick(uint64_t now_ms)
{
  std::lock_guard<std::mutex> lock(mutex_);

  if (state_ != TwoKeyState::Key1Armed) {
    return TwoKeyResult::Idle;
  }
  const int64_t elapsed_ms = static_cast<int64_t>(now_ms) -
    static_cast<int64_t>(key1_timestamp_ms_);
  if (elapsed_ms > kTwoKeyTimeoutMs) {
    resetLocked();
    return TwoKeyResult::DeniedTimeout;
  }
  return TwoKeyResult::Armed;
}

// ─── Diagnostics ────────────────────────────────────────────────────────

TwoKeyState TwoKeyStateMachine::state() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

uint32_t TwoKeyStateMachine::key1RequestId() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return key1_request_id_;
}

uint64_t TwoKeyStateMachine::key1TimestampMs() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return key1_timestamp_ms_;
}

// ─── Internal ───────────────────────────────────────────────────────────

void TwoKeyStateMachine::resetLocked()
{
  state_ = TwoKeyState::Idle;
  key1_request_id_ = 0;
  key1_timestamp_ms_ = 0;
}

}  // namespace san_fire_authorization
