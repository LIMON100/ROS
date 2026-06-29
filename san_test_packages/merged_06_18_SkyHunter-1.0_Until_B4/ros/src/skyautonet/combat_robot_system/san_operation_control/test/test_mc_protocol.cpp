// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-019] MC ACK protocol tests — 8 cases.
//
// Pure-logic via the McProtocolNode::evaluateAndUpdateForTest +
// McSenderNode::stampForTest / ackForTest seams. No rclcpp::spin
// required; each TEST_F drives the validation pipeline directly.

#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/mc_ack.hpp>
#include <combat_robot_msgs/msg/mc_message.hpp>

#include "san_operation_control/mc_protocol_node.hpp"
#include "san_operation_control/mc_sender_node.hpp"

namespace san_operation_control
{
namespace
{

using MCMsg = combat_robot_msgs::msg::MCMessage;
using MCAck = combat_robot_msgs::msg::MCAck;
using namespace std::chrono_literals;

class McProtocolFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
    proto_ = std::make_shared<McProtocolNode>();
    sender_ = std::make_shared<McSenderNode>();
  }

  MCMsg makeMessage(
    uint32_t seq, const std::string & cmd,
    const std::string & payload)
  {
    MCMsg m;
    m.seq = seq;
    m.command = cmd;
    m.payload = payload;
    m.crc32_checksum =
      McProtocolNode::computeCrc32(cmd + payload);
    return m;
  }

  std::shared_ptr<McProtocolNode> proto_;
  std::shared_ptr<McSenderNode> sender_;
};

// ─── T1: valid message → OK ──────────────────────────────────────────────
TEST_F(McProtocolFixture, T1_ValidMessageAckedOK) {
  auto msg = makeMessage(1, "START", "{}");
  auto ack = proto_->evaluateAndUpdateForTest(msg);
  EXPECT_EQ(ack.outcome, "OK");
  EXPECT_EQ(ack.seq, 1u);
  EXPECT_EQ(proto_->highestAckedForTest(), 1u);
}

// ─── T2: bad checksum → INVALID_CHECKSUM ─────────────────────────────────
TEST_F(McProtocolFixture, T2_BadChecksumRejected) {
  auto msg = makeMessage(2, "STOP", "{}");
  msg.crc32_checksum = 0xDEADBEEF;
  auto ack = proto_->evaluateAndUpdateForTest(msg);
  EXPECT_EQ(ack.outcome, "INVALID_CHECKSUM");
  EXPECT_EQ(proto_->highestAckedForTest(), 0u)
    << "rejected message must NOT advance the window";
}

// ─── T3: same seq twice → DUPLICATE ──────────────────────────────────────
TEST_F(McProtocolFixture, T3_DuplicateSeqDropped) {
  auto msg = makeMessage(5, "PAUSE", "{}");
  auto ack1 = proto_->evaluateAndUpdateForTest(msg);
  EXPECT_EQ(ack1.outcome, "OK");
  auto ack2 = proto_->evaluateAndUpdateForTest(msg);
  EXPECT_EQ(ack2.outcome, "DUPLICATE");
}

// ─── T4: seq far behind window → OUT_OF_ORDER ────────────────────────────
TEST_F(McProtocolFixture, T4_OutOfOrderOldSeqDropped) {
  // Push window forward.
  auto head = makeMessage(100, "START", "p");
  proto_->evaluateAndUpdateForTest(head);

  // Way old seq (>WINDOW_SIZE behind head)
  auto stale = makeMessage(50, "STOP", "p");
  auto ack = proto_->evaluateAndUpdateForTest(stale);
  EXPECT_EQ(ack.outcome, "OUT_OF_ORDER");
}

// ─── T5: backward seq within window is DUPLICATE (audit B1 corrected)
//
// Pre-audit T5 expected "OK" for any backward seq inside the window,
// which was the source of the B1 replay vulnerability. The forward-
// step protocol guarantee (sender's next_seq_++ is monotonic) means
// any backward seq must be either DUPLICATE (within window) or
// OUT_OF_ORDER (outside).
TEST_F(McProtocolFixture, T5_SlidingWindowBackwardIsDuplicate) {
  auto head = makeMessage(100, "START", "p");
  proto_->evaluateAndUpdateForTest(head);

  // Within window — seq 90 is 10 behind head (< WINDOW_SIZE=16).
  // Sender protocol: this can only be a retransmit of an already-
  // acked message. Receiver must respond DUPLICATE.
  auto in_window = makeMessage(90, "RESUME", "p2");
  auto ack = proto_->evaluateAndUpdateForTest(in_window);
  EXPECT_EQ(ack.outcome, "DUPLICATE")
    << "backward seq within WINDOW_SIZE must be DUPLICATE — sender "
    "is strictly forward-stepping, so any backward seq is a retry";
}

// ─── T6: sender pending until OK ack ─────────────────────────────────────
// Verifies the stamp+pending pipeline: a stamped command stays
// pending until ackForTest("OK") clears it. OUT_OF_ORDER also leaves
// it pending (receiver's window may catch a future retransmit).
// (Audit B7 P2 corrected: INVALID_CHECKSUM is now an immediate-drop
//  — moved to T6b.)
TEST_F(McProtocolFixture, T6_SenderPendingUntilAck) {
  MCMsg raw; raw.command = "LOAD_PATH"; raw.payload = "{\"wps\":[]}";
  auto stamped = sender_->stampForTest(raw);
  EXPECT_GT(stamped.seq, 0u);
  EXPECT_EQ(sender_->pendingCount(), 1u);

  // OUT_OF_ORDER leaves it pending — retransmit may catch up later.
  sender_->ackForTest(stamped.seq, "OUT_OF_ORDER");
  EXPECT_EQ(sender_->pendingCount(), 1u);

  // OK clears.
  sender_->ackForTest(stamped.seq, "OK");
  EXPECT_EQ(sender_->pendingCount(), 0u);
}

// ─── T6b (audit B7 P2 regression): INVALID_CHECKSUM ack drops pending
// + does NOT retry (retransmit of identical bytes would just re-fail).
TEST_F(McProtocolFixture, T6b_InvalidChecksumAckImmediateDropNoRetry) {
  MCMsg raw; raw.command = "PAUSE"; raw.payload = "{}";
  auto stamped = sender_->stampForTest(raw);
  EXPECT_EQ(sender_->pendingCount(), 1u);

  sender_->ackForTest(stamped.seq, "INVALID_CHECKSUM");
  EXPECT_EQ(sender_->pendingCount(), 0u)
    << "INVALID_CHECKSUM must drop pending immediately — "
    "retransmit of identical bytes is wasted (audit B7)";
}

// ─── T7: DUPLICATE ack also clears pending (idempotent semantics) ────────
TEST_F(McProtocolFixture, T7_DuplicateAckClearsPending) {
  MCMsg raw; raw.command = "STOP"; raw.payload = "{}";
  auto stamped = sender_->stampForTest(raw);
  EXPECT_EQ(sender_->pendingCount(), 1u);

  sender_->ackForTest(stamped.seq, "DUPLICATE");
  EXPECT_EQ(sender_->pendingCount(), 0u)
    << "DUPLICATE means receiver has it — sender can stop retrying";
}

// ─── T9 (audit B1 P1 regression): replayed seq within window is DUPLICATE
// even after the original record was evicted from received_seqs_.
//
// Pre-fix: classifyOutcome returned "OK" for any seq not currently in
// the set, regardless of whether it had been previously accepted +
// evicted by the WINDOW_SIZE rolling cap. That is a replay-attack
// window.
TEST_F(McProtocolFixture, T9_ReplayedSeqWithinWindowDuplicate) {
  // Step 1 — advance head to 100 with WINDOW_SIZE entries behind it.
  for (uint32_t s = 85; s <= 100; ++s) {
    auto m = makeMessage(s, "STRESS", "x");
    auto ack = proto_->evaluateAndUpdateForTest(m);
    EXPECT_EQ(ack.outcome, "OK") << "seq=" << s;
  }
  EXPECT_EQ(proto_->highestAckedForTest(), 100u);

  // Step 2 — replay seq 90 (originally OK, may or may not be in
  // the set depending on eviction policy). Must be DUPLICATE,
  // never re-OK.
  auto replay = makeMessage(90, "STRESS", "x");
  auto ack = proto_->evaluateAndUpdateForTest(replay);
  EXPECT_EQ(ack.outcome, "DUPLICATE")
    << "seq within window must always be DUPLICATE — replay-vector "
    "was the audit B1 P1 finding";

  // Step 3 — seq 80 (outside window): OUT_OF_ORDER, not OK.
  auto stale = makeMessage(80, "STRESS", "x");
  auto stale_ack = proto_->evaluateAndUpdateForTest(stale);
  EXPECT_EQ(stale_ack.outcome, "OUT_OF_ORDER");

  // Step 4 — seq 101 still works.
  auto next = makeMessage(101, "STRESS", "x");
  EXPECT_EQ(proto_->evaluateAndUpdateForTest(next).outcome, "OK");
}

// ─── T10 (audit B10 P1 smoke): pending_ + onAck under concurrency.
// Drives the lock path; not a TSAN run (CI has separate sanitizer
// job) but exercises the same code that would race pre-fix.
TEST_F(McProtocolFixture, T10_SenderPendingConcurrencySmoke) {
  // Stamp 200 commands, then ack them in interleaved order from a
  // second thread while the main thread keeps stamping. Pre-fix this
  // path (retransmit iterate + onAck erase) would UB under MTE.
  constexpr int N = 200;
  std::vector<uint32_t> seqs;
  seqs.reserve(N);
  for (int i = 0; i < N; ++i) {
    MCMsg raw; raw.command = "STRESS"; raw.payload = "{}";
    seqs.push_back(sender_->stampForTest(raw).seq);
  }
  EXPECT_EQ(sender_->pendingCount(), static_cast<size_t>(N));

  std::thread ack_thread([&]() {
      for (auto s : seqs) {
        sender_->ackForTest(s, "OK");
      }
    });
  // Concurrent stamp while ack thread runs.
  for (int i = 0; i < 50; ++i) {
    MCMsg raw; raw.command = "MORE"; raw.payload = "{}";
    sender_->stampForTest(raw);
  }
  ack_thread.join();

  // After concurrent ops: N stamps OK-acked + 50 stamps still pending
  // (approximately; race is acceptable, no crash is the contract).
  EXPECT_LE(sender_->pendingCount(), static_cast<size_t>(50));
}

// ─── T8: integration — sender → protocol → ack lifecycle ─────────────────
TEST_F(McProtocolFixture, T8_SenderProtocolIntegration) {
  MCMsg raw; raw.command = "START"; raw.payload = "{\"mission\":1}";
  auto stamped = sender_->stampForTest(raw);

  auto ack = proto_->evaluateAndUpdateForTest(stamped);
  EXPECT_EQ(ack.outcome, "OK");
  EXPECT_EQ(ack.seq, stamped.seq);

  sender_->ackForTest(ack.seq, ack.outcome);
  EXPECT_EQ(sender_->pendingCount(), 0u);
}

}  // namespace
}  // namespace san_operation_control

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  const int rc = RUN_ALL_TESTS();
  if (rclcpp::ok()) {rclcpp::shutdown();}
  return rc;
}
