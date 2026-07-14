#include "san_l5_regression/scenario_runner.hpp"

#include "san_l5_regression/topic_watcher.hpp"

namespace san_l5_regression {

using LeaderRoleAnn = combat_robot_msgs::msg::LeaderRoleAnnouncement;
using HubRoleAnn    = combat_robot_msgs::msg::HubRoleAnnouncement;

// Mirror the SuccessionPriority enum values from san_role_management.
// We do not link against that package to keep the regression target
// runnable even when role_management is disabled.
namespace prio {
constexpr uint8_t DEPUTY      = 1;
constexpr uint8_t HUB         = 2;
constexpr uint8_t BATTERY_MAX = 3;
}  // namespace prio

ScenarioRunner::ScenarioRunner(const RunnerConfig& cfg)
    : rclcpp::Node("san_l5_regression_runner"),
      cfg_(cfg)
{
    injector_ = std::make_unique<FailureInjector>(this);

    rclcpp::QoS qos(20);
    qos.reliable();
    leader_watcher_ = std::make_unique<TopicWatcher<LeaderRoleAnn>>(
        this, "/swarm/leader/role_announce", qos);
    hub_watcher_ = std::make_unique<TopicWatcher<HubRoleAnn>>(
        this, "/swarm/hub/role_announce", qos);
    limp_alert_watcher_ = std::make_unique<TopicWatcher<std_msgs::msg::String>>(
        this, "/swarm/limp_mode/alert", qos);

    heartbeat_timer_ = create_wall_timer(
        std::chrono::milliseconds(cfg_.heartbeat_period_ms),
        [this] { heartbeatTick(); });
}

void ScenarioRunner::heartbeatTick() {
    injector_->publishAll();
}

void ScenarioRunner::seedEightRobotBaseline() {
    // robot_id 1..8 with realistic batteries; S3 marked Deputy.
    for (uint32_t id = 1; id <= 8; ++id) {
        RobotHealth h;
        h.sbc1_healthy = true;
        h.sbc2_healthy = (id == cfg_.hub_robot_id || id == cfg_.deputy_robot_id);
        h.battery_percent = 80.0f - static_cast<float>(id);     // 79..72
        h.is_deputy_ugv = (id == cfg_.deputy_robot_id);
        h.is_leader_role_active = (id == 1);
        injector_->setHealth(id, h);
    }
}

void ScenarioRunner::resetBaseline() {
    injector_->reset();
    leader_watcher_->reset();
    hub_watcher_->reset();
    limp_alert_watcher_->reset();
    seedEightRobotBaseline();
    injector_->publishAll();
}

bool ScenarioRunner::isLeaderPromotedBy(const LeaderRoleAnn& m,
                                        uint32_t robot_id, uint8_t priority)
{
    return m.role == LeaderRoleAnn::LEADER_PROMOTED
        && m.robot_id == robot_id
        && m.succession_priority == priority;
}

bool ScenarioRunner::isLeaderPromotedAnyPriority(const LeaderRoleAnn& m,
                                                  uint8_t priority)
{
    return m.role == LeaderRoleAnn::LEADER_PROMOTED
        && m.succession_priority == priority;
}

bool ScenarioRunner::isHubPromotedWithFullTakeover(const HubRoleAnn& m,
                                                    uint32_t robot_id)
{
    return m.role == HubRoleAnn::HUB_PROMOTED
        && m.robot_id == robot_id
        && m.lte_active
        && m.slam_aggregation_active
        && m.video_relay_active;
}

bool ScenarioRunner::isLimpModeAlert(const std_msgs::msg::String& m,
                                      const std::string& needle)
{
    return m.data.find(needle) != std::string::npos;
}

// ───────────────────────────── S18-1 ─────────────────────────────
// Leader (S1) 작동 불능 → Deputy (S3) 1순위 승계, ≤ 5 s
ScenarioReport ScenarioRunner::runS18_1_LeaderToDeputy() {
    ScenarioReport r;
    r.id = "S18-1";
    r.description = "Leader → Deputy 승계 (≤ 5 s)";
    r.deadline_ms = cfg_.s18_1_deadline_ms;

    resetBaseline();
    // S1 (Leader) 불능: stop publishing entirely so the role manager's
    // heartbeat watchdog (200ms × 7 = 1400ms) fires.
    injector_->removeRobot(1);

    const uint32_t deputy = cfg_.deputy_robot_id;
    auto elapsed = leader_watcher_->waitFor(
        [deputy](const LeaderRoleAnn& m) {
            return isLeaderPromotedBy(m, deputy, prio::DEPUTY);
        },
        std::chrono::milliseconds(r.deadline_ms));

    if (elapsed) {
        r.recordPass(*elapsed);
        r.attributes["promoted_robot_id"] = std::to_string(deputy);
        r.attributes["succession_priority"] = "DEPUTY";
    } else {
        r.recordTimeout();
    }
    return r;
}

// ───────────────────────────── S18-2 ─────────────────────────────
// Leader + Deputy 모두 불능 → Hub (S2) 2순위 승계, ≤ 8 s
ScenarioReport ScenarioRunner::runS18_2_LeaderDeputyToHub() {
    ScenarioReport r;
    r.id = "S18-2";
    r.description = "Leader + Deputy → Hub 승계 (≤ 8 s)";
    r.deadline_ms = cfg_.s18_2_deadline_ms;

    resetBaseline();
    // S1 (Leader) and S3 (Deputy) both go dark.
    injector_->removeRobot(1);
    injector_->removeRobot(cfg_.deputy_robot_id);

    const uint32_t hub = cfg_.hub_robot_id;
    auto elapsed = leader_watcher_->waitFor(
        [hub](const LeaderRoleAnn& m) {
            return isLeaderPromotedBy(m, hub, prio::HUB);
        },
        std::chrono::milliseconds(r.deadline_ms));

    if (elapsed) {
        r.recordPass(*elapsed);
        r.attributes["promoted_robot_id"] = std::to_string(hub);
        r.attributes["succession_priority"] = "HUB";
    } else {
        r.recordTimeout();
    }
    return r;
}

// ───────────────────────────── S18-3 ─────────────────────────────
// Hub (S2) 불능 → Deputy 가 LTE + SLAM + Video 모두 인수, ≤ 7 s
ScenarioReport ScenarioRunner::runS18_3_HubToDeputyTakeover() {
    ScenarioReport r;
    r.id = "S18-3";
    r.description = "Hub → Deputy 인수 (LTE + SLAM + Video, ≤ 7 s)";
    r.deadline_ms = cfg_.s18_3_deadline_ms;

    resetBaseline();
    injector_->removeRobot(cfg_.hub_robot_id);

    const uint32_t deputy = cfg_.deputy_robot_id;
    auto elapsed = hub_watcher_->waitFor(
        [deputy](const HubRoleAnn& m) {
            return isHubPromotedWithFullTakeover(m, deputy);
        },
        std::chrono::milliseconds(r.deadline_ms));

    if (elapsed) {
        r.recordPass(*elapsed);
        r.attributes["promoted_robot_id"] = std::to_string(deputy);
        r.attributes["lte_active"] = "true";
        r.attributes["slam_aggregation_active"] = "true";
        r.attributes["video_relay_active"] = "true";
    } else {
        r.recordTimeout();
    }
    return r;
}

// ───────────────────────────── S18-4 ─────────────────────────────
// Leader + Deputy + Hub 모두 불능 → 배터리 최대 follower 승계, ≤ 10 s
ScenarioReport ScenarioRunner::runS18_4_ThreeFailedBatteryFollower() {
    ScenarioReport r;
    r.id = "S18-4";
    r.description = "3 대 불능 → 배터리 최대 follower 승계 (≤ 10 s)";
    r.deadline_ms = cfg_.s18_4_deadline_ms;

    resetBaseline();
    // Knock out Leader, Hub, Deputy.
    injector_->removeRobot(1);
    injector_->removeRobot(cfg_.hub_robot_id);
    injector_->removeRobot(cfg_.deputy_robot_id);
    // Make S5 the clear battery winner among followers (4..8).
    injector_->setBattery(4, 60.0f);
    injector_->setBattery(5, 90.0f);
    injector_->setBattery(6, 45.0f);
    injector_->setBattery(7, 55.0f);
    injector_->setBattery(8, 30.0f);

    auto elapsed = leader_watcher_->waitFor(
        [](const LeaderRoleAnn& m) {
            return isLeaderPromotedAnyPriority(m, prio::BATTERY_MAX);
        },
        std::chrono::milliseconds(r.deadline_ms));

    if (elapsed) {
        r.recordPass(*elapsed);
        if (auto latest = leader_watcher_->latest()) {
            r.attributes["promoted_robot_id"] =
                std::to_string(latest->robot_id);
            r.attributes["succession_priority"] = "BATTERY_MAX";
        }
    } else {
        r.recordTimeout();
    }
    return r;
}

// ───────────────────────────── S18-5 ─────────────────────────────
// Hub + Deputy 둘 다 불능 → Limp Mode 진입 alert 발행, ≤ 8 s
ScenarioReport ScenarioRunner::runS18_5_HubDeputyBothDownLimpEnter() {
    ScenarioReport r;
    r.id = "S18-5";
    r.description = "Hub + Deputy 모두 불능 → Limp Mode 진입 (≤ 8 s)";
    r.deadline_ms = cfg_.s18_5_deadline_ms;

    resetBaseline();
    injector_->removeRobot(cfg_.hub_robot_id);
    injector_->removeRobot(cfg_.deputy_robot_id);

    auto elapsed = limp_alert_watcher_->waitFor(
        [](const std_msgs::msg::String& m) {
            return isLimpModeAlert(m, "LIMP_MODE_ACTIVE");
        },
        std::chrono::milliseconds(r.deadline_ms));

    if (elapsed) {
        r.recordPass(*elapsed);
        r.attributes["alert_text"] =
            limp_alert_watcher_->latest()
                ? limp_alert_watcher_->latest()->data : "";
    } else {
        r.recordTimeout();
    }
    return r;
}

// ───────────────────────────── S18-6 ─────────────────────────────
// Deputy 복구 → Limp Mode 이탈 alert, ≤ 5 s
ScenarioReport ScenarioRunner::runS18_6_DeputyRecoversLimpExit() {
    ScenarioReport r;
    r.id = "S18-6";
    r.description = "Deputy 복구 → Limp Mode 이탈 (≤ 5 s)";
    r.deadline_ms = cfg_.s18_6_deadline_ms;

    // Start in Limp Mode (both Hub and Deputy down).
    resetBaseline();
    injector_->removeRobot(cfg_.hub_robot_id);
    injector_->removeRobot(cfg_.deputy_robot_id);
    // Allow the LimpModeManager to latch.
    limp_alert_watcher_->waitFor(
        [](const std_msgs::msg::String& m) {
            return isLimpModeAlert(m, "LIMP_MODE_ACTIVE");
        },
        std::chrono::milliseconds(cfg_.s18_5_deadline_ms));
    limp_alert_watcher_->reset();

    // Deputy comes back online.
    RobotHealth restored;
    restored.sbc1_healthy = true;
    restored.sbc2_healthy = true;
    restored.battery_percent = 75.0f;
    restored.is_deputy_ugv = true;
    injector_->restoreRobot(cfg_.deputy_robot_id, restored);

    auto elapsed = limp_alert_watcher_->waitFor(
        [](const std_msgs::msg::String& m) {
            return isLimpModeAlert(m, "LIMP_MODE_EXITED");
        },
        std::chrono::milliseconds(r.deadline_ms));

    if (elapsed) {
        r.recordPass(*elapsed);
        r.attributes["alert_text"] =
            limp_alert_watcher_->latest()
                ? limp_alert_watcher_->latest()->data : "";
    } else {
        r.recordTimeout();
    }
    return r;
}

ScenarioReportWriter ScenarioRunner::runAll() {
    ScenarioReportWriter w;
    w.add(runS18_1_LeaderToDeputy());
    w.add(runS18_2_LeaderDeputyToHub());
    w.add(runS18_3_HubToDeputyTakeover());
    w.add(runS18_4_ThreeFailedBatteryFollower());
    w.add(runS18_5_HubDeputyBothDownLimpEnter());
    w.add(runS18_6_DeputyRecoversLimpExit());
    return w;
}

}  // namespace san_l5_regression
