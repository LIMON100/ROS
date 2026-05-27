// SAN v1.5.2 — DCN-2026-006 EXT D-024 RobotStatus audit tests.
//
// Coverage:
//   T1  Hub-only guard: audit publisher only on robot_role == "hub"
//       (★ regression test for the post-PR-E hotfix — the publisher
//       was previously created on every robot in the always_on group,
//       causing five concurrent publishers on /diagnostics/robot_status_audit).
//   T2  STALE level fires when stale_ms >= 3000.
//   T3  WARN level fires when rate < expected/2.
//   T4  Multiple subscribed robots produce one DiagnosticStatus each.
//   T5  ★ Hub SBC #2 (sbc_id=2) silenced — the dual-SBC standby slot
//       does not publish even though robot_role=='hub' (DCN-2026-011
//       D-032 follow-up; matches SW Operation doc §2 "standby /
//       lifecycle inactive").

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "san_role_management/hub_role_manager.hpp"

using namespace san_role_management;
using diagnostic_msgs::msg::DiagnosticStatus;

namespace {

rclcpp::NodeOptions makeOpts(int robot_id, const std::string& role,
                              int sbc_id = -1) {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"robot_id", robot_id},
        {"robot_role", role},
        {"sbc_id", sbc_id},
        {"hub_robot_id", 2},
        {"deputy_robot_id", 3},
        {"hub_heartbeat_timeout_ms", 200},
        {"watchdog_period_ms", 50},
    });
    return opts;
}

}  // namespace

class RobotStatusAuditTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

// ─── T1: Hub-only publisher guard ─────────────────────────────────────
TEST_F(RobotStatusAuditTest, AuditPublisherOnlyOnHub) {
    auto hub = std::make_shared<HubRoleManager>(makeOpts(2, "hub"));
    EXPECT_TRUE(hub->hasAuditPublisher())
        << "robot_role=='hub' must own the audit publisher";

    auto deputy = std::make_shared<HubRoleManager>(makeOpts(3, "deputy"));
    EXPECT_FALSE(deputy->hasAuditPublisher())
        << "robot_role=='deputy' must NOT publish — would duplicate Hub's stream";

    auto follower = std::make_shared<HubRoleManager>(makeOpts(4, "follower"));
    EXPECT_FALSE(follower->hasAuditPublisher())
        << "robot_role=='follower' must NOT publish";

    auto unset = std::make_shared<HubRoleManager>(makeOpts(5, ""));
    EXPECT_FALSE(unset->hasAuditPublisher())
        << "empty role default must NOT publish (fail-safe)";
}

// ─── T2: STALE level on stale_ms >= 3000 ──────────────────────────────
TEST_F(RobotStatusAuditTest, StaleLevelFiresAfter3Seconds) {
    auto hub = std::make_shared<HubRoleManager>(makeOpts(2, "hub"));
    // A dedicated sandbox map keeps tests independent of the node's
    // private robot_audit_ member; computeAudit is pure-logic over the
    // map (it drains samples_this_window in-place, which is intentional).
    std::unordered_map<uint32_t, HubRoleManager::RobotStatusAudit> sandbox;
    auto& slot = sandbox[4];
    slot.last_received_at    = hub->now() -
        rclcpp::Duration(std::chrono::milliseconds(3500));
    slot.samples_this_window = 5;
    slot.samples_total       = 100;
    slot.last_sbc1_healthy   = true;
    slot.last_sbc2_healthy   = true;

    auto out = HubRoleManager::computeAudit(sandbox, hub->now());
    ASSERT_EQ(out.status.size(), 1u);
    EXPECT_EQ(out.status[0].level, DiagnosticStatus::STALE)
        << "stale_ms=3500 should fire STALE";
    EXPECT_NE(out.status[0].message.find("stale"), std::string::npos);
}

// ─── T3: WARN level on rate < expected/2 ──────────────────────────────
TEST_F(RobotStatusAuditTest, WarnLevelFiresOnLowRate) {
    auto hub = std::make_shared<HubRoleManager>(makeOpts(2, "hub"));
    std::unordered_map<uint32_t, HubRoleManager::RobotStatusAudit> sandbox;
    auto& slot = sandbox[5];
    slot.last_received_at    = hub->now();  // fresh
    slot.samples_this_window = 1;            // expected 5 Hz, rate 1 — < expected/2 (=2)
    slot.samples_total       = 12;
    slot.last_sbc1_healthy   = true;
    slot.last_sbc2_healthy   = false;

    auto out = HubRoleManager::computeAudit(sandbox, hub->now(),
                                              /*stale_threshold_ms=*/3000,
                                              /*expected_rate_hz=*/5);
    ASSERT_EQ(out.status.size(), 1u);
    EXPECT_EQ(out.status[0].level, DiagnosticStatus::WARN);
    EXPECT_NE(out.status[0].message.find("rate"), std::string::npos);

    // Rate exactly half (2 Hz @ expected=5) — WARN guard is `rate*2 < expected`,
    // so 2*2 = 4 < 5 still WARN. Confirm.
    slot.samples_this_window = 2;
    auto out2 = HubRoleManager::computeAudit(sandbox, hub->now(), 3000, 5);
    EXPECT_EQ(out2.status[0].level, DiagnosticStatus::WARN);

    // Rate exactly 3 (above half) — OK.
    slot.samples_this_window = 3;
    auto out3 = HubRoleManager::computeAudit(sandbox, hub->now(), 3000, 5);
    EXPECT_EQ(out3.status[0].level, DiagnosticStatus::OK);
}

// ─── T4: Multiple robots → one DiagnosticStatus per robot ─────────────
TEST_F(RobotStatusAuditTest, FourRobotsProduceFourStatuses) {
    auto hub = std::make_shared<HubRoleManager>(makeOpts(2, "hub"));
    std::unordered_map<uint32_t, HubRoleManager::RobotStatusAudit> sandbox;
    for (uint32_t id : {2u, 3u, 4u, 5u}) {
        auto& slot = sandbox[id];
        slot.last_received_at    = hub->now();
        slot.samples_this_window = 5;
        slot.samples_total       = 50;
        slot.last_sbc1_healthy   = true;
        slot.last_sbc2_healthy   = true;
    }
    auto out = HubRoleManager::computeAudit(sandbox, hub->now());
    EXPECT_EQ(out.status.size(), 4u);
    for (auto& ds : out.status) {
        EXPECT_EQ(ds.level, DiagnosticStatus::OK);
        EXPECT_EQ(ds.values.size(), 5u);   // rate_hz/total/stale_ms/sbc1/sbc2
    }
    // computeAudit drains samples_this_window; verify the side effect
    // (matches production cadence — next tick measures only fresh arrivals).
    for (auto& [_, slot] : sandbox) {
        EXPECT_EQ(slot.samples_this_window, 0u);
    }
}

// ─── T5: ★ Hub SBC #2 silenced (dual-SBC standby) ──────────────────────
TEST_F(RobotStatusAuditTest, HubSbc2IsSilenced) {
    auto sbc1 = std::make_shared<HubRoleManager>(
        makeOpts(2, "hub", /*sbc_id=*/1));
    EXPECT_TRUE(sbc1->hasAuditPublisher())
        << "Hub SBC #1 (sbc_id=1) is the active publisher";

    auto sbc2 = std::make_shared<HubRoleManager>(
        makeOpts(2, "hub", /*sbc_id=*/2));
    EXPECT_FALSE(sbc2->hasAuditPublisher())
        << "Hub SBC #2 (sbc_id=2) is standby — must NOT create the "
        << "publisher (matches SW Operation doc §2 'lifecycle inactive')";

    // Backward compat: deployments that haven't set sbc_id (default -1)
    // keep the publisher so existing pre-D-032 systems are unaffected.
    auto legacy = std::make_shared<HubRoleManager>(
        makeOpts(2, "hub", /*sbc_id=*/-1));
    EXPECT_TRUE(legacy->hasAuditPublisher())
        << "default sbc_id=-1 (pre-D-032 deployment) must still publish";

    // sbc_id=0 ("N/A" sentinel from D-032 fallback) — single-SBC Hub
    // image. Also publishes.
    auto single = std::make_shared<HubRoleManager>(
        makeOpts(2, "hub", /*sbc_id=*/0));
    EXPECT_TRUE(single->hasAuditPublisher())
        << "sbc_id=0 (single-SBC Hub) must still publish";
}
