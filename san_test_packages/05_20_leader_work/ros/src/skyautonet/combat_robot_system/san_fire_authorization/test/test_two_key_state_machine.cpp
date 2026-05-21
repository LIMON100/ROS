// SAN v1.5 PHASE 9 — TwoKeyStateMachine unit tests.
//
// Coverage:
//   Core 5 transitions:
//     T1  Idle → onKey1 → Armed
//     T2  Armed → onKey2 (within timeout, matching id) → Granted (→ Idle)
//     T3  Armed → onCancel → Cancelled (→ Idle)
//     T4  Idle → onKey2 → DeniedIncomplete (stray KEY2)
//     T5  Armed → onKey2 (after kTwoKeyTimeoutMs) → DeniedTimeout (→ Idle)
//   Timeout via tick():
//     T6  Armed → tick(after timeout) → DeniedTimeout (→ Idle)
//   Edge cases:
//     E1  Armed → onKey1 (re-arm, new request_id) → Armed (target changed)
//     E2  Armed → onKey2 (mismatching request_id) → DeniedIncomplete (still Armed)
//     E3  After Granted → onKey2 (stale) → DeniedIncomplete
//     E4  Multiple ticks within timeout → all Armed
//     E5  Cancel from Idle → Idle (no-op)
//     E6  Clock running backwards → DeniedTimeout (defensive)

#include "san_fire_authorization/two_key_state_machine.hpp"

#include <gtest/gtest.h>

namespace san_fire_authorization {
namespace {

constexpr uint32_t kReqA   = 100;
constexpr uint32_t kReqB   = 200;
constexpr uint64_t kT0     = 1'700'000'000'000ULL;

// ─── Core 5 transitions ─────────────────────────────────────────────────

TEST(TwoKeyStateMachineTest, T1_IdleToKey1ArmedOnKey1) {
  TwoKeyStateMachine sm;
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);

  const auto r = sm.onKey1(kReqA, kT0);
  EXPECT_EQ(r, TwoKeyResult::Armed);
  EXPECT_EQ(sm.state(), TwoKeyState::Key1Armed);
  EXPECT_EQ(sm.key1RequestId(), kReqA);
  EXPECT_EQ(sm.key1TimestampMs(), kT0);
}

TEST(TwoKeyStateMachineTest, T2_ArmedToGrantedOnKey2WithinTimeout) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);

  // KEY2 arrives 2000 ms later — well within 5000 ms window.
  const auto r = sm.onKey2(kReqA, kT0 + 2000);
  EXPECT_EQ(r, TwoKeyResult::Granted);
  // Must auto-reset so the next shot requires a fresh re-arm.
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);
  EXPECT_EQ(sm.key1RequestId(), 0u);
  EXPECT_EQ(sm.key1TimestampMs(), 0u);
}

TEST(TwoKeyStateMachineTest, T3_ArmedToCancelledOnCancel) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);

  const auto r = sm.onCancel();
  EXPECT_EQ(r, TwoKeyResult::Cancelled);
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);
  EXPECT_EQ(sm.key1RequestId(), 0u);
}

TEST(TwoKeyStateMachineTest, T4_IdleStrayKey2DeniedIncomplete) {
  TwoKeyStateMachine sm;
  // No prior KEY1 — KEY2 arrives out of nowhere.
  const auto r = sm.onKey2(kReqA, kT0);
  EXPECT_EQ(r, TwoKeyResult::DeniedIncomplete);
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);
}

TEST(TwoKeyStateMachineTest, T5_ArmedKey2AfterTimeoutDenied) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);

  // KEY2 arrives 5001 ms later — one ms past the window.
  const auto r = sm.onKey2(kReqA, kT0 + kTwoKeyTimeoutMs + 1);
  EXPECT_EQ(r, TwoKeyResult::DeniedTimeout);
  EXPECT_EQ(sm.state(), TwoKeyState::Idle)
      << "Timeout must reset to Idle so operator must re-arm";
}

// ─── Timeout via tick() ─────────────────────────────────────────────────

TEST(TwoKeyStateMachineTest, T6_ArmedTickAfterTimeoutDenied) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);

  // No KEY2 arrives. Periodic tick at +6000 ms catches the timeout.
  const auto r = sm.tick(kT0 + kTwoKeyTimeoutMs + 1000);
  EXPECT_EQ(r, TwoKeyResult::DeniedTimeout);
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);
}

// ─── Edge cases ─────────────────────────────────────────────────────────

TEST(TwoKeyStateMachineTest, E1_ArmedReArmWithNewRequestId) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);
  EXPECT_EQ(sm.key1RequestId(), kReqA);

  // Operator changes their mind, taps a new target.
  const auto r = sm.onKey1(kReqB, kT0 + 1000);
  EXPECT_EQ(r, TwoKeyResult::Armed);
  EXPECT_EQ(sm.state(), TwoKeyState::Key1Armed);
  EXPECT_EQ(sm.key1RequestId(), kReqB)
      << "Re-arm must replace the prior request_id";
  EXPECT_EQ(sm.key1TimestampMs(), kT0 + 1000)
      << "Re-arm must reset the timeout window";

  // KEY2 with the OLD request_id is now incomplete.
  EXPECT_EQ(sm.onKey2(kReqA, kT0 + 1500), TwoKeyResult::DeniedIncomplete);
  // State still Armed (waiting for matching KEY2).
  EXPECT_EQ(sm.state(), TwoKeyState::Key1Armed);

  // KEY2 with the NEW request_id grants normally.
  EXPECT_EQ(sm.onKey2(kReqB, kT0 + 1500), TwoKeyResult::Granted);
}

TEST(TwoKeyStateMachineTest, E2_ArmedKey2MismatchingRequestIdDeniedStillArmed) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);

  // KEY2 with a foreign request_id — could be a stale message or
  // delayed packet for a different intent.
  const auto r = sm.onKey2(kReqB, kT0 + 1000);
  EXPECT_EQ(r, TwoKeyResult::DeniedIncomplete);
  // Critically: the original KEY1 must REMAIN ARMED so the operator's
  // legitimate confirm-press still works.
  EXPECT_EQ(sm.state(), TwoKeyState::Key1Armed);
  EXPECT_EQ(sm.key1RequestId(), kReqA);
}

TEST(TwoKeyStateMachineTest, E3_AfterGrantedStaleKey2DeniedIncomplete) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);
  ASSERT_EQ(sm.onKey2(kReqA, kT0 + 1000), TwoKeyResult::Granted);
  // State is now Idle. A duplicate (replay) KEY2 must NOT re-fire.
  EXPECT_EQ(sm.onKey2(kReqA, kT0 + 1100), TwoKeyResult::DeniedIncomplete);
}

TEST(TwoKeyStateMachineTest, E4_MultipleTicksWithinTimeoutAllArmed) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);

  // Simulate 10 Hz tick for 4 seconds — all should report Armed
  // (no timeout, no state change).
  for (int i = 1; i <= 40; ++i) {
    const auto r = sm.tick(kT0 + i * 100);
    ASSERT_EQ(r, TwoKeyResult::Armed)
        << "tick #" << i << " unexpectedly fired " << static_cast<int>(r);
  }
  // Still armed, waiting for KEY2.
  EXPECT_EQ(sm.state(), TwoKeyState::Key1Armed);
  EXPECT_EQ(sm.key1RequestId(), kReqA);

  // KEY2 still grants normally.
  EXPECT_EQ(sm.onKey2(kReqA, kT0 + 4500), TwoKeyResult::Granted);
}

TEST(TwoKeyStateMachineTest, E5_CancelFromIdleIsNoOp) {
  TwoKeyStateMachine sm;
  // State is Idle by default.
  const auto r = sm.onCancel();
  EXPECT_EQ(r, TwoKeyResult::Idle);
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);
}

TEST(TwoKeyStateMachineTest, E6_ClockBackwardsTriggersTimeout) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0 + 10'000), TwoKeyResult::Armed);

  // Gate clock somehow rewinds — e.g. NTP step. The signed-arithmetic
  // guard must treat this as "elapsed_ms < 0" → DeniedTimeout.
  const auto r = sm.onKey2(kReqA, kT0 + 5'000);
  EXPECT_EQ(r, TwoKeyResult::DeniedTimeout);
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);
}

// ─── tick() boundary ────────────────────────────────────────────────────

TEST(TwoKeyStateMachineTest, TickFromIdleReturnsIdle) {
  TwoKeyStateMachine sm;
  EXPECT_EQ(sm.tick(kT0), TwoKeyResult::Idle);
  EXPECT_EQ(sm.tick(kT0 + 999'999), TwoKeyResult::Idle);
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);
}

TEST(TwoKeyStateMachineTest, TickAtExactBoundaryStillArmed) {
  TwoKeyStateMachine sm;
  ASSERT_EQ(sm.onKey1(kReqA, kT0), TwoKeyResult::Armed);

  // tick at +5000 ms exactly — boundary is "elapsed > timeout", so 5000
  // is still in the window.
  const auto r = sm.tick(kT0 + kTwoKeyTimeoutMs);
  EXPECT_EQ(r, TwoKeyResult::Armed);
  EXPECT_EQ(sm.state(), TwoKeyState::Key1Armed);

  // +5001 ms → fires the timeout.
  const auto r2 = sm.tick(kT0 + kTwoKeyTimeoutMs + 1);
  EXPECT_EQ(r2, TwoKeyResult::DeniedTimeout);
  EXPECT_EQ(sm.state(), TwoKeyState::Idle);
}

}  // namespace
}  // namespace san_fire_authorization
