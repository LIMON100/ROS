// SAN v1.3 PHASE 7 - combat robot operation system control node.
//
// Owns:
//   * deployment_mode (yaml-driven, drives DEMO + watchdog policy)
//   * DemoSequencer (DEMO + LAB_TEST modes)
//   * SensorWatchdog (yaml override honored only in DEVELOPMENT)
//   * CommandEcho (every observed SwarmRobotCommand updates last_id)
//
// Subscribes to all 8 SwarmRobotCommand variants and echoes the
// most recent command_id on RobotStatus + SwarmHealthSummary.

#pragma once

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/formation_command.hpp>
#include <combat_robot_msgs/msg/mission_state_command.hpp>
#include <combat_robot_msgs/msg/fire_authorization.hpp>
#include <combat_robot_msgs/msg/jamming_command.hpp>
#include <combat_robot_msgs/msg/waypoint_command.hpp>
#include <combat_robot_msgs/msg/manual_override_command.hpp>
#include <combat_robot_msgs/msg/emergency_stop.hpp>
#include <combat_robot_msgs/msg/video_stream_request.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <combat_robot_msgs/msg/swarm_health_summary.hpp>
// [DCN-2026-011 D-033] Dual-SBC peer heartbeat (own + peer).
#include <std_msgs/msg/header.hpp>

#include <memory>
#include <optional>
#include <string>

#include "san_operation_control/command_echo.hpp"
#include "san_operation_control/demo_sequencer.hpp"
#include "san_operation_control/deployment_mode.hpp"
#include "san_operation_control/sensor_watchdog.hpp"

namespace san_operation_control {

class OperationControlNode : public rclcpp::Node {
public:
    OperationControlNode();
    explicit OperationControlNode(const rclcpp::NodeOptions& options);

    // Test accessors.
    DeploymentMode deploymentMode() const { return mode_; }
    DemoSequencer& demoSequencer()        { return demo_; }
    SensorWatchdog& watchdog()            { return watchdog_; }
    CommandEcho&   commandEcho()          { return echo_; }
    bool isWatchdogEnabled() const { return watchdog_.isEnabled(); }
    uint32_t lastReceivedCommandId() const { return echo_.lastId(); }
    // [DCN-2026-011 D-032] Test seams — read the cached resolved sbc_id
    // and drive the resolution helper with a synthetic path. The helper
    // is read-only (no member mutation) so exposing it as `const` is
    // safe; tests use it with a tmp file under temp_directory_path().
    uint8_t sbcIdForTest() const { return sbc_id_; }
    uint8_t resolveSbcIdForTest(const std::string& path) const {
        return resolveSbcId(path);
    }

    // [DCN-2026-011 D-033] Test entry points for the dual-SBC heartbeat
    // path. publishStatusForTest() exercises publishStatus() and returns
    // the published RobotStatus so the test can assert the sbc{1,2}_healthy
    // fields directly. injectPeerHeartbeatForTest() drops a synthetic
    // peer Header into the last_peer_heartbeat_ slot so STALE / FRESH
    // branches can be tested without an executor.
    combat_robot_msgs::msg::RobotStatus publishStatusForTest();
    void injectPeerHeartbeatForTest(const rclcpp::Time& stamp);

    // Inject a sensor update for tests (skips the actual subscription).
    void noteSensorForTest(const std::string& name, uint64_t now_ms) {
        watchdog_.update(name, now_ms);
    }

private:
    DeploymentMode  mode_ = DeploymentMode::PRODUCTION;
    DemoSequencer   demo_;
    SensorWatchdog  watchdog_;
    CommandEcho     echo_;
    int robot_id_ = 1;
    // [DCN-2026-011 D-032] Dual-SBC slot ID. Resolved from a
    // 3-tier priority: launch parameter (-1 = unset) → file
    // /etc/skyautonet/sbc_id → fallback 0. Only Hub UGV uses 1/2;
    // every other robot ends up as 0 ("N/A").
    uint8_t sbc_id_ = 0;

    using FormationCmd     = combat_robot_msgs::msg::FormationCommand;
    using MissionStateCmd  = combat_robot_msgs::msg::MissionStateCommand;
    using FireAuth         = combat_robot_msgs::msg::FireAuthorization;
    using JammingCmd       = combat_robot_msgs::msg::JammingCommand;
    using WaypointCmd      = combat_robot_msgs::msg::WaypointCommand;
    using ManualOverride   = combat_robot_msgs::msg::ManualOverrideCommand;
    using EStop            = combat_robot_msgs::msg::EmergencyStop;
    using VideoReq         = combat_robot_msgs::msg::VideoStreamRequest;
    using RobotStatus      = combat_robot_msgs::msg::RobotStatus;
    using SwarmHealth      = combat_robot_msgs::msg::SwarmHealthSummary;

    rclcpp::Subscription<FormationCmd>::SharedPtr     sub_formation_;
    rclcpp::Subscription<MissionStateCmd>::SharedPtr  sub_mission_;
    rclcpp::Subscription<FireAuth>::SharedPtr         sub_fire_;
    rclcpp::Subscription<JammingCmd>::SharedPtr       sub_jam_;
    rclcpp::Subscription<WaypointCmd>::SharedPtr      sub_waypoint_;
    rclcpp::Subscription<ManualOverride>::SharedPtr   sub_override_;
    rclcpp::Subscription<EStop>::SharedPtr            sub_estop_;
    rclcpp::Subscription<VideoReq>::SharedPtr         sub_video_;

    rclcpp::Publisher<RobotStatus>::SharedPtr         pub_status_;
    rclcpp::Publisher<SwarmHealth>::SharedPtr         pub_health_;

    rclcpp::TimerBase::SharedPtr tick_timer_;

    // [DCN-2026-011 D-033] Dual-SBC mutual heartbeat. Each SBC publishes
    // a tiny Header (stamp = proof of life, frame_id = "sbcN") at 5 Hz
    // on /hub_internal/sbcN/heartbeat, and subscribes to the other SBC's
    // counterpart. publishStatus() reports the peer healthy when its
    // last heartbeat is fresher than kPeerStaleSec; otherwise the
    // peer slot is reported false so HubHealthMonitor can transition
    // out of NORMAL into CASE_A / CASE_B / BOTH_DOWN.
    rclcpp::Publisher<std_msgs::msg::Header>::SharedPtr     sbc_heartbeat_pub_;
    rclcpp::Subscription<std_msgs::msg::Header>::SharedPtr  peer_heartbeat_sub_;
    rclcpp::TimerBase::SharedPtr                            sbc_heartbeat_timer_;
    std::optional<rclcpp::Time>                             last_peer_heartbeat_;
    static constexpr double kPeerStaleSec = 3.0;
    // [Sanitizer-hardening] MutuallyExclusive callback group binding
    // peer_heartbeat_sub_ + tick_timer_. The sub writes
    // last_peer_heartbeat_ from one thread under MTE; the tick timer
    // calls peerSbcHealthy() which reads it. The MEC group serializes
    // the two so neither std::optional's engaged-bit nor the 64-bit
    // rclcpp::Time inside can be observed mid-update.
    rclcpp::CallbackGroup::SharedPtr                        sbc_cb_group_;

    void declareParameters();
    void readParameters();
    void wireInterfaces();
    // [DCN-2026-011 D-032] Compute sbc_id from launch param + file fallback.
    // Exposed via the public test accessor sbcIdForTest() so the unit
    // test can drive the resolution logic with a temp file path.
    uint8_t resolveSbcId(const std::string& path) const;

    // [DCN-2026-011 D-033] Dual-SBC peer heartbeat helpers.
    void publishSbcHeartbeat();
    void onPeerHeartbeat(const std_msgs::msg::Header::SharedPtr msg);
    bool peerSbcHealthy() const;

    // [DCN-2026-011 D-033] Pure-logic message builder — used by both
    // the production publishStatus() and the test-only
    // publishStatusForTest() entry point so the assertions can drive
    // the sbc{1,2}_healthy logic without spinning a subscriber.
    combat_robot_msgs::msg::RobotStatus buildStatusMessage();

    void onTick();
    void demoPhaseTransition(DemoPhase phase);

    // Each variant's callback funnels through noteCommand() so the
    // echo logic stays in one place. The template lets us reuse the
    // body for any message carrying a `command_id` field.
    template <typename T>
    void noteCommand(const std::shared_ptr<T> msg);

    void publishStatus();
    void publishHealth();

    uint64_t nowMs() const;
};

}  // namespace san_operation_control
