// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 9 — End-to-end integration scenarios for the
// Phase 2-D fire authorization gate.
//
// These tests exercise all three modules (HmacAuthenticator,
// TwoKeyStateMachine, AuditLogger) together against the same
// scenarios the production FireAuthorizationNode handles. They are
// runnable WITHOUT rclcpp / ROS 2, so they validate the gate logic
// itself rather than the ROS plumbing. The ROS-level fixture lives
// in test_fire_authorization_node.cpp (requires colcon test).
//
// Scenario coverage (DCN-2026-001 D-004 fully exercised):
//   S1  Normal fire (KEY1 + KEY2 within timeout, HMAC OK)
//   S2  HMAC tampered signature → DENIED, audit recorded
//   S3  Stray KEY2 without KEY1 → DENIED_INCOMPLETE, audit recorded
//   S4  KEY1 then KEY2 6s later → DENIED_TIMEOUT, audit recorded
//   S5  Replay (nonce reuse) → DENIED_NONCE_REUSE, audit recorded
//   S6  TST v1.5 S18-5: Limp Mode entry → fire → limp_mode_fire=true
//       audit + audit chain validates end-to-end
//
// After all scenarios the audit log file should:
//   - Validate via verifyChain end-to-end (no tampering)
//   - Have exactly N entries (one per scenario)
//   - Have at least one entry with limp_mode_fire == true (S6)

#include "san_fire_authorization/audit_logger.hpp"
#include "san_fire_authorization/hmac_authenticator.hpp"
#include "san_fire_authorization/two_key_state_machine.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace san_fire_authorization
{
namespace
{

// ─── helpers ────────────────────────────────────────────────────────────

constexpr uint8_t kCmdKey1 = 1;    // FireAuthorizationRequest.TWO_KEY_KEY1_TARGET_TAP
constexpr uint8_t kCmdKey2 = 2;    // FireAuthorizationRequest.TWO_KEY_KEY2_CONFIRM
constexpr uint8_t kCmdCancel = 3;

std::array<uint8_t, kHmacSha256Bytes> testSecret(uint8_t fill = 0x77)
{
  std::array<uint8_t, kHmacSha256Bytes> s{};
  s.fill(fill);
  return s;
}

std::string tempAuditPath(const char * tag)
{
  char tmpl[64];
  std::snprintf(tmpl, sizeof(tmpl), "/tmp/san_audit_int_%s_XXXXXX", tag);
  int fd = ::mkstemp(tmpl);
  if (fd < 0) {return std::string();}
  ::close(fd);
  ::unlink(tmpl);
  return std::string(tmpl);
}

AuthMessage makeAuthMsg(
  uint32_t request_id,
  uint8_t cmd,
  uint64_t nonce,
  uint64_t ts_ms,
  const std::string & operator_id = "op_test")
{
  AuthMessage m;
  m.request_id = request_id;
  m.sequence = 1;
  m.operator_id = operator_id;
  m.nonce = nonce;
  m.request_timestamp_ms = ts_ms;
  m.command_type = cmd;
  m.target_lat_e7 = 374200000;
  m.target_lon_e7 = 1270000000;
  m.target_alt_mm = 250;
  return m;
}

AuditEntry baseAuditEntry(
  const AuthMessage & am,
  uint64_t now_ms,
  uint32_t hub_term = 2,
  uint32_t leader_term = 5,
  uint32_t n_alive = 8)
{
  AuditEntry e;
  e.timestamp_ms = now_ms;
  e.request_id = am.request_id;
  e.operator_id = am.operator_id;
  e.target_lat_e7 = am.target_lat_e7;
  e.target_lon_e7 = am.target_lon_e7;
  e.target_alt_mm = am.target_alt_mm;
  e.hub_term = hub_term;
  e.leader_term = leader_term;
  e.n_alive_robots = n_alive;
  return e;
}

// Replicates FireAuthorizationNode::evaluateAndRespond() decision flow
// in a self-contained form for testing.
struct GateDecision
{
  bool granted = false;
  std::string reason;          // "GRANTED" / "HMAC_FAIL" / ...
  bool limp_mode_fire = false;
  std::string audit_uuid;
};

GateDecision runGate(
  HmacAuthenticator & hmac,
  TwoKeyStateMachine & sm,
  AuditLogger & audit,
  const AuthMessage & am,
  const std::string & hmac_hex,
  uint64_t now_ms,
  bool in_limp_mode)
{
  GateDecision d;
  auto e = baseAuditEntry(am, now_ms);

  // 1) HMAC verify
  const auto hr = hmac.verify(am, hmac_hex, now_ms);
  if (hr != AuthResult::Granted) {
    d.granted = false;
    d.reason = (hr == AuthResult::DeniedHmacFail) ? "HMAC_FAIL" :
      (hr == AuthResult::DeniedNonceReuse) ? "NONCE_REUSE" :
      (hr == AuthResult::DeniedTimestampDrift) ? "TIMESTAMP_DRIFT" :
      "OTHER";
    e.granted = false;
    e.reason = d.reason;
    e.limp_mode_fire = false;
    d.audit_uuid = audit.emit(e).uuid;
    return d;
  }

  // 2) Two-key dispatch
  TwoKeyResult kr;
  switch (am.command_type) {
    case kCmdKey1:    kr = sm.onKey1(am.request_id, now_ms);   break;
    case kCmdKey2:    kr = sm.onKey2(am.request_id, now_ms);   break;
    case kCmdCancel:  kr = sm.onCancel();                       break;
    default:          kr = TwoKeyResult::DeniedIncomplete;      break;
  }

  if (kr == TwoKeyResult::Armed) {
    d.reason = "TWO_KEY_INCOMPLETE";  // ARMED — awaiting KEY2
    e.granted = false;
    e.reason = d.reason;
    e.reason_detail = "ARMED — awaiting KEY2";
    d.audit_uuid = audit.emit(e).uuid;
    return d;
  }
  if (kr == TwoKeyResult::Cancelled) {
    d.reason = "TWO_KEY_INCOMPLETE";
    e.granted = false;
    e.reason = d.reason;
    e.reason_detail = "CANCELLED by operator";
    d.audit_uuid = audit.emit(e).uuid;
    return d;
  }
  if (kr == TwoKeyResult::DeniedIncomplete) {
    d.reason = "TWO_KEY_INCOMPLETE";
    e.granted = false;
    e.reason = d.reason;
    d.audit_uuid = audit.emit(e).uuid;
    return d;
  }
  if (kr == TwoKeyResult::DeniedTimeout) {
    d.reason = "TWO_KEY_TIMEOUT";
    e.granted = false;
    e.reason = d.reason;
    d.audit_uuid = audit.emit(e).uuid;
    return d;
  }
  // kr == Granted

  // 3) Limp Mode flagging
  d.granted = true;
  d.reason = "GRANTED";
  d.limp_mode_fire = in_limp_mode;
  e.granted = true;
  e.reason = "GRANTED";
  e.limp_mode_fire = in_limp_mode;
  e.reason_detail = in_limp_mode ? "GRANTED in Limp Mode" : "GRANTED";
  d.audit_uuid = audit.emit(e).uuid;
  return d;
}

// Helper: count audit entries with a given substring
std::size_t countLinesContaining(
  const std::string & path,
  const std::string & needle)
{
  std::ifstream in(path);
  std::string line;
  std::size_t n = 0;
  while (std::getline(in, line)) {
    if (line.find(needle) != std::string::npos) {++n;}
  }
  return n;
}

// ─── scenarios ─────────────────────────────────────────────────────────

TEST(IntegrationScenarios, S1_NormalFireKey1ThenKey2Granted) {
  const auto audit_path = tempAuditPath("s1");
  ASSERT_FALSE(audit_path.empty());

  HmacAuthenticator hmac(testSecret());
  TwoKeyStateMachine sm;
  AuditLogger audit(audit_path);

  uint64_t now = 1'700'000'000'000ULL;
  const std::string operator_id = "op_alpha";

  // KEY1 — target tap
  {
    auto am = makeAuthMsg(/*req=*/ 100, kCmdKey1, /*nonce=*/ 0xAA01, now, operator_id);
    auto sig = hmac.sign(am);
    auto d = runGate(hmac, sm, audit, am, sig, now, /*limp=*/ false);
    EXPECT_FALSE(d.granted);
    EXPECT_EQ(d.reason, "TWO_KEY_INCOMPLETE");  // ARMED
    EXPECT_FALSE(d.audit_uuid.empty());
  }

  // KEY2 — confirm 2 seconds later
  now += 2000;
  {
    auto am = makeAuthMsg(/*req=*/ 100, kCmdKey2, /*nonce=*/ 0xAA02, now, operator_id);
    auto sig = hmac.sign(am);
    auto d = runGate(hmac, sm, audit, am, sig, now, /*limp=*/ false);
    EXPECT_TRUE(d.granted);
    EXPECT_EQ(d.reason, "GRANTED");
    EXPECT_FALSE(d.limp_mode_fire);
    EXPECT_FALSE(d.audit_uuid.empty());
  }

  // Audit chain must validate.
  const auto vr = AuditLogger::verifyChain(audit_path, kGenesisHash);
  EXPECT_TRUE(vr.valid) << vr.error;
  EXPECT_EQ(vr.last_line_no, 1u);  // 2 entries → 0-based last = 1

  std::remove(audit_path.c_str());
}

TEST(IntegrationScenarios, S2_HmacTamperedSignatureDeniedAndAudited) {
  const auto audit_path = tempAuditPath("s2");
  HmacAuthenticator hmac(testSecret());
  TwoKeyStateMachine sm;
  AuditLogger audit(audit_path);

  const uint64_t now = 1'700'000'001'000ULL;
  auto am = makeAuthMsg(101, kCmdKey1, 0xBB01, now);
  auto sig = hmac.sign(am);
  sig[5] = (sig[5] == 'a') ? 'b' : 'a';   // tamper

  auto d = runGate(hmac, sm, audit, am, sig, now, false);
  EXPECT_FALSE(d.granted);
  EXPECT_EQ(d.reason, "HMAC_FAIL");
  EXPECT_FALSE(d.audit_uuid.empty())
    << "Deny decisions MUST still be audited (D-004 obligation)";

  EXPECT_EQ(countLinesContaining(audit_path, "\"reason\":\"HMAC_FAIL\""), 1u);

  const auto vr = AuditLogger::verifyChain(audit_path, kGenesisHash);
  EXPECT_TRUE(vr.valid) << vr.error;

  std::remove(audit_path.c_str());
}

TEST(IntegrationScenarios, S3_StrayKey2WithoutKey1DeniedAndAudited) {
  const auto audit_path = tempAuditPath("s3");
  HmacAuthenticator hmac(testSecret());
  TwoKeyStateMachine sm;
  AuditLogger audit(audit_path);

  const uint64_t now = 1'700'000'002'000ULL;
  auto am = makeAuthMsg(102, kCmdKey2, 0xCC01, now);   // KEY2 first, no KEY1
  auto sig = hmac.sign(am);

  auto d = runGate(hmac, sm, audit, am, sig, now, false);
  EXPECT_FALSE(d.granted);
  EXPECT_EQ(d.reason, "TWO_KEY_INCOMPLETE");
  EXPECT_FALSE(d.audit_uuid.empty());

  EXPECT_EQ(
    countLinesContaining(audit_path, "\"reason\":\"TWO_KEY_INCOMPLETE\""),
    1u);

  std::remove(audit_path.c_str());
}

TEST(IntegrationScenarios, S4_Key1ThenKey2AfterTimeoutDenied) {
  const auto audit_path = tempAuditPath("s4");
  HmacAuthenticator hmac(testSecret());
  TwoKeyStateMachine sm;
  AuditLogger audit(audit_path);

  uint64_t now = 1'700'000'003'000ULL;

  // KEY1
  {
    auto am = makeAuthMsg(103, kCmdKey1, 0xDD01, now);
    auto sig = hmac.sign(am);
    auto d = runGate(hmac, sm, audit, am, sig, now, false);
    EXPECT_EQ(d.reason, "TWO_KEY_INCOMPLETE");  // ARMED
  }

  // KEY2 — 6 seconds later (past 5s timeout)
  now += 6000;
  {
    auto am = makeAuthMsg(103, kCmdKey2, 0xDD02, now);
    auto sig = hmac.sign(am);
    auto d = runGate(hmac, sm, audit, am, sig, now, false);
    EXPECT_FALSE(d.granted);
    EXPECT_EQ(d.reason, "TWO_KEY_TIMEOUT");
  }

  EXPECT_EQ(
    countLinesContaining(audit_path, "\"reason\":\"TWO_KEY_TIMEOUT\""),
    1u);

  const auto vr = AuditLogger::verifyChain(audit_path, kGenesisHash);
  EXPECT_TRUE(vr.valid) << vr.error;

  std::remove(audit_path.c_str());
}

TEST(IntegrationScenarios, S5_NonceReplayDeniedAndAudited) {
  const auto audit_path = tempAuditPath("s5");
  HmacAuthenticator hmac(testSecret());
  TwoKeyStateMachine sm;
  AuditLogger audit(audit_path);

  uint64_t now = 1'700'000'004'000ULL;

  // First request — granted (well, ARMED since KEY1 alone)
  auto am1 = makeAuthMsg(104, kCmdKey1, /*nonce=*/ 0xEE01, now);
  auto sig1 = hmac.sign(am1);
  auto d1 = runGate(hmac, sm, audit, am1, sig1, now, false);
  EXPECT_EQ(d1.reason, "TWO_KEY_INCOMPLETE");  // ARMED, no fire

  // Replay the SAME request 100ms later — should be rejected for
  // nonce reuse (HMAC layer catches it BEFORE Two-key sees it).
  now += 100;
  auto d2 = runGate(hmac, sm, audit, am1, sig1, now, false);
  EXPECT_FALSE(d2.granted);
  EXPECT_EQ(d2.reason, "NONCE_REUSE");
  EXPECT_FALSE(d2.audit_uuid.empty());

  EXPECT_EQ(
    countLinesContaining(audit_path, "\"reason\":\"NONCE_REUSE\""),
    1u);

  std::remove(audit_path.c_str());
}

// ─── TST v1.5 S18-5: Limp Mode 진입 직후 발사 시나리오 ───────────────────

TEST(IntegrationScenarios, S6_LimpModeFireGrantedAndLimpFlagSetInAudit) {
  // TST v1.5 §11 S18-5 reference scenario: Hub + Deputy both go silent
  // (heartbeat lost) → LimpModeManager publishes in_limp_mode=true on
  // OperationState. Operator subsequently issues a legitimate
  // KEY1+KEY2 fire — must be GRANTED per DCN-2026-001 D-004 Option A,
  // with limp_mode_fire=true tagged in audit log.

  const auto audit_path = tempAuditPath("s6");
  HmacAuthenticator hmac(testSecret());
  TwoKeyStateMachine sm;
  AuditLogger audit(audit_path);

  uint64_t now = 1'700'000'005'000ULL;
  const std::string operator_id = "op_charlie";

  // Pre-condition: OperationState.in_limp_mode = true (set by node
  // when subscribing). We model that as the in_limp_mode flag below.
  const bool in_limp = true;

  // KEY1
  {
    auto am = makeAuthMsg(180, kCmdKey1, 0xF101, now, operator_id);
    auto sig = hmac.sign(am);
    auto d = runGate(hmac, sm, audit, am, sig, now, in_limp);
    EXPECT_EQ(d.reason, "TWO_KEY_INCOMPLETE");  // ARMED
    EXPECT_FALSE(d.granted);
  }

  // KEY2 — 1 second later (well within 5s timeout)
  now += 1000;
  {
    auto am = makeAuthMsg(180, kCmdKey2, 0xF102, now, operator_id);
    auto sig = hmac.sign(am);
    auto d = runGate(hmac, sm, audit, am, sig, now, in_limp);
    EXPECT_TRUE(d.granted)
      << "D-004 Option A: Limp Mode must STILL permit operator fire";
    EXPECT_EQ(d.reason, "GRANTED");
    EXPECT_TRUE(d.limp_mode_fire)
      << "D-004 audit obligation: limp_mode_fire=true on Limp Mode grant";
  }

  // Verify the audit log contains the limp_mode_fire=true entry.
  EXPECT_EQ(countLinesContaining(audit_path, "\"limp_mode_fire\":true"), 1u)
    << "Exactly one entry must carry limp_mode_fire=true (the grant)";
  // The ARMED entry should NOT have limp_mode_fire=true (only the
  // grant itself).
  EXPECT_GE(countLinesContaining(audit_path, "\"limp_mode_fire\":false"), 1u);

  // Chain integrity end-to-end.
  const auto vr = AuditLogger::verifyChain(audit_path, kGenesisHash);
  EXPECT_TRUE(vr.valid) << vr.error;
  EXPECT_EQ(vr.last_line_no, 1u);  // 2 entries

  std::remove(audit_path.c_str());
}

// ─── multi-scenario aggregate: verify chain across the full session ────

TEST(IntegrationScenarios, S7_LongSessionChainIntegrityAcrossManyScenarios) {
  // Mix of granted + denied across 20 fire intents, then verify the
  // single audit file end-to-end. The hash chain must hold through
  // every grant AND every deny.

  const auto audit_path = tempAuditPath("s7");
  HmacAuthenticator hmac(testSecret());
  TwoKeyStateMachine sm;
  AuditLogger audit(audit_path);

  uint64_t now = 1'700'000'010'000ULL;
  uint64_t nonce = 0x10000ULL;
  std::size_t grant_count = 0;
  std::size_t deny_count = 0;

  for (int i = 0; i < 20; ++i) {
    const uint32_t req_id = static_cast<uint32_t>(200 + i);
    const bool limp = (i % 4 == 0);  // every 4th in Limp Mode

    // KEY1
    auto am1 = makeAuthMsg(req_id, kCmdKey1, nonce++, now);
    auto sig1 = hmac.sign(am1);
    auto d1 = runGate(hmac, sm, audit, am1, sig1, now, limp);
    EXPECT_EQ(d1.reason, "TWO_KEY_INCOMPLETE");
    ++deny_count;

    now += 1500;

    // Every 5th iteration: tamper KEY2 sig to drive a deny
    auto am2 = makeAuthMsg(req_id, kCmdKey2, nonce++, now);
    auto sig2 = hmac.sign(am2);
    if (i % 5 == 0) {
      sig2[0] = (sig2[0] == 'a') ? 'b' : 'a';
    }
    auto d2 = runGate(hmac, sm, audit, am2, sig2, now, limp);

    if (d2.granted) {++grant_count;} else {++deny_count;}

    now += 500;
  }

  // Verify chain end-to-end.
  const auto vr = AuditLogger::verifyChain(audit_path, kGenesisHash);
  EXPECT_TRUE(vr.valid) << vr.error;

  // Total audit entries == grant + deny.
  std::ifstream in(audit_path);
  std::string line;
  std::size_t total = 0;
  while (std::getline(in, line)) {if (!line.empty()) {++total;}}
  EXPECT_EQ(total, grant_count + deny_count);
  EXPECT_GT(grant_count, 0u) << "scenario should produce some grants";
  EXPECT_GT(deny_count, 0u) << "scenario should produce some denies";

  // Drop-count must be 0 — fsync should succeed on a normal tmpfs.
  EXPECT_EQ(audit.droppedCount(), 0u);

  std::remove(audit_path.c_str());
}

}  // namespace
}  // namespace san_fire_authorization
