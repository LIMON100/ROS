// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-021 rosbridge_mock_server.
//
// Synthetic publishers for the topics the Aban Android App subscribes
// to, per docs/external/Aban_Android_rosbridge_schema_v2.md.
//
// Schema-actual mapping (vs spec draft)
// -------------------------------------
// The optionA spec referenced two message names that do not exist in
// combat_robot_msgs as written:
//
//   spec name      actual name             notes
//   -----------    --------------------    ----------------------------
//   Heartbeat      HeartBeat               CamelCase difference; same
//                                          payload concept but actual
//                                          schema uses robot_id + role
//                                          (uint8 enum), not sbc_id +
//                                          healthy. Mock uses ACTUAL.
//   FireEvent      FireResult              No FireEvent type exists.
//                                          FireResult covers the same
//                                          UI concern (post-fire
//                                          notification). Mock uses
//                                          FireResult and the
//                                          /gun_trigger/simulated_fire_result
//                                          topic name.
//
// Topics whose production backends are not yet implemented (gate_demo
// — DCN-2026-016; MC retransmit — DCN-2026-019) are published by this
// mock regardless, so the Aban app can develop UI for the full schema.
// The spec doc marks these as "v1.5.3 future" so Aban knows what to
// expect in production.

#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/fire_result.hpp>
#include <combat_robot_msgs/msg/heart_beat.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <std_msgs/msg/string.hpp>

#include <chrono>
#include <cmath>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace san_external_mocks
{

class RosbridgeMockServer : public rclcpp::Node
{
public:
  RosbridgeMockServer()
  : Node("rosbridge_mock_server")
  {
    const auto reliable = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
    const auto best_eff = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();

    // ── Audit publishers (1 Hz, Reliable) ──────────────────────
    robot_audit_pub_ =
      create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics/robot_status_audit", reliable);
    slam_audit_pub_ =
      create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics/hub_slam_audit", reliable);
    audit_timer_ = create_wall_timer(
      1s, std::bind(&RosbridgeMockServer::publishAudits, this));

    // ── Threat alerts (event-driven; mock emits 1 every 4 s) ──
    threat_raw_pub_ =
      create_publisher<combat_robot_msgs::msg::ThreatAlert>(
      "/swarm/threat_alert_raw", reliable);
    threat_consensus_pub_ =
      create_publisher<combat_robot_msgs::msg::ThreatAlert>(
      "/swarm/threat_alert_consensus", reliable);
    threat_timer_ = create_wall_timer(
      4s, std::bind(&RosbridgeMockServer::publishThreats, this));

    // ── RTK heading (5 Hz, Best Effort) ─────────────────────────
    heading_pub_ = create_publisher<sensor_msgs::msg::Imu>(
      "/rtk_gnss_node/heading", best_eff);
    heading_timer_ = create_wall_timer(
      200ms,
      std::bind(&RosbridgeMockServer::publishHeading, this));

    // ── Hub SBC heartbeats (1 Hz, Reliable). Schema uses
    //    HeartBeat (CamelCase!) with role enum + sequence. Mock
    //    emits one msg per SBC by setting robot_id = HUB_ROBOT_ID
    //    and rolling the sequence counter. ────────────────────────
    sbc1_pub_ = create_publisher<combat_robot_msgs::msg::HeartBeat>(
      "/hub_internal/sbc1/heartbeat", reliable);
    sbc2_pub_ = create_publisher<combat_robot_msgs::msg::HeartBeat>(
      "/hub_internal/sbc2/heartbeat", reliable);
    heartbeat_timer_ = create_wall_timer(
      1s,
      std::bind(&RosbridgeMockServer::publishHeartbeats, this));

    // ── /swarm/poses (10 Hz, Reliable). Same topic as DCN-2026-013
    //    swarm_monitor_node — when launched in production WITH the
    //    real monitor, the Hub-only gate ensures exactly one
    //    publisher per swarm. In this mock-only environment we ARE
    //    the only publisher. ──────────────────────────────────────
    poses_pub_ = create_publisher<geometry_msgs::msg::PoseArray>(
      "/swarm/poses", reliable);
    poses_timer_ = create_wall_timer(
      100ms,
      std::bind(&RosbridgeMockServer::publishPoses, this));

    // ── /gate1/demo_status (0.2 Hz, Reliable). Backend
    //    (DCN-2026-016 gate_demo_orchestrator) not yet implemented
    //    — mock publishes a synthetic state machine cycle so Aban's
    //    UI demo-timeline view can be developed. ─────────────────
    demo_status_pub_ = create_publisher<std_msgs::msg::String>(
      "/gate1/demo_status", reliable);
    demo_status_timer_ = create_wall_timer(
      5s,
      std::bind(&RosbridgeMockServer::publishDemoStatus, this));

    // ── /gun_trigger/simulated_fire_result (event; 1 every 30 s).
    //    Spec drafted as "FireEvent" — actual schema is FireResult
    //    (no FireEvent type exists). Mock uses FireResult and a
    //    topic name that reflects this. ───────────────────────────
    fire_result_pub_ =
      create_publisher<combat_robot_msgs::msg::FireResult>(
      "/gun_trigger/simulated_fire_result", reliable);
    fire_timer_ = create_wall_timer(
      30s, std::bind(&RosbridgeMockServer::publishFire, this));

    RCLCPP_INFO(
      get_logger(),
      "rosbridge_mock_server: publishing 10 synthetic topics "
      "(v1.5.3 Aban Android schema)");
  }

private:
  // ─── publish callbacks ─────────────────────────────────────────

  void publishAudits()
  {
    diagnostic_msgs::msg::DiagnosticArray msg;
    msg.header.stamp = now();

    diagnostic_msgs::msg::DiagnosticStatus robot_status;
    robot_status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
    robot_status.name = "mock/robot_2";
    robot_status.message = "Hub UGV nominal (mock)";
    robot_status.hardware_id = "robot_2";
    msg.status.push_back(robot_status);
    robot_audit_pub_->publish(msg);

    diagnostic_msgs::msg::DiagnosticStatus slam_status = robot_status;
    slam_status.name = "mock/hub_slam";
    slam_status.message = "SLAM aggregation OK (mock)";
    msg.status.clear();
    msg.status.push_back(slam_status);
    slam_audit_pub_->publish(msg);
  }

  void publishThreats()
  {
    using TA = combat_robot_msgs::msg::ThreatAlert;
    TA raw;
    raw.header.stamp = now();
    raw.severity = TA::SEVERITY_WARNING;
    raw.threat_type = TA::TYPE_DRONE_DETECTED;           // schema field
    raw.source_robot_id = "mock";
    raw.message_ko = "(mock) 적성 UAV 탐지";
    raw.timestamp_ms = static_cast<uint64_t>(
      now().nanoseconds() / 1'000'000ll);
    raw.instance_count = 1;
    threat_raw_pub_->publish(raw);

    TA consensus = raw;
    consensus.severity = TA::SEVERITY_CRITICAL;
    consensus.source_robot_id = "hub";
    consensus.message_ko = "(mock) 확정 위협 — 군집 합의";
    threat_consensus_pub_->publish(consensus);
  }

  void publishHeading()
  {
    sensor_msgs::msg::Imu msg;
    msg.header.stamp = now();
    msg.header.frame_id = "rtk_gnss";

    // Slow simulated yaw rotation, ~3°/s.
    static double yaw = 0.0;
    yaw += 0.05;       // 5 Hz publisher × 0.05 rad ≈ 0.25 rad/s ≈ 14°/s
                       // (intentionally generous so UI animation is visible)
    msg.orientation.z = std::sin(yaw / 2.0);
    msg.orientation.w = std::cos(yaw / 2.0);
    // Synthetic "good RTK" — covariance [8] is the yaw variance.
    msg.orientation_covariance[8] = 0.01;
    heading_pub_->publish(msg);
  }

  void publishHeartbeats()
  {
    using HB = combat_robot_msgs::msg::HeartBeat;

    // DCN-2026-021 P0-3 fix — populate the FULL HeartBeat schema
    // (IDS §5.8). Earlier draft published only robot_id/sequence/role,
    // which left the Aban UI blind to battery/health/operation_mode
    // /tier (the fields it actually renders in the status panel).
    const uint64_t now_ms = static_cast<uint64_t>(
      now().nanoseconds() / 1'000'000ll);

    HB hb1;
    hb1.header.stamp = now();
    hb1.robot_id = 2;                                    // HUB_ROBOT_ID
    hb1.sequence = ++heartbeat_seq_;
    hb1.role = HB::ROLE_HUB;
    hb1.health_status = HB::HEALTH_HEALTHY;
    hb1.battery_percent = 87.5f;                         // synthetic
    hb1.operation_mode = 0;                              // 0=RECON
    hb1.current_tier = 0;                                // T0 (hub baseline)
    hb1.timestamp_ms = now_ms;
    sbc1_pub_->publish(hb1);

    HB hb2 = hb1;
    hb2.header.stamp = now();                            // refresh stamp
    hb2.sequence = ++heartbeat_seq_;                     // distinct seq
    hb2.battery_percent = 86.2f;                         // SBC#2 slightly different
    hb2.timestamp_ms = static_cast<uint64_t>(
      now().nanoseconds() / 1'000'000ll);
    sbc2_pub_->publish(hb2);
  }

  void publishPoses()
  {
    geometry_msgs::msg::PoseArray arr;
    arr.header.stamp = now();
    arr.header.frame_id = "map";

    // 4 synthetic robot positions in line formation @ 2 m spacing.
    for (int i = 0; i < 4; ++i) {
      geometry_msgs::msg::Pose p;
      p.position.x = i * 2.0;
      p.position.y = 0.0;
      p.position.z = 0.0;
      p.orientation.w = 1.0;
      arr.poses.push_back(p);
    }
    poses_pub_->publish(arr);
  }

  void publishDemoStatus()
  {
    static const std::vector<std::string> states = {
      "IDLE", "MOVING_WP1", "SCANNING",
      "MOVING_WP2", "RTH", "IDLE",
    };
    std_msgs::msg::String msg;
    msg.data = states[demo_state_idx_++ % states.size()];
    demo_status_pub_->publish(msg);
  }

  void publishFire()
  {
    using FR = combat_robot_msgs::msg::FireResult;

    // DCN-2026-021 P0-3 fix — populate the FULL FireResult schema
    // (IDS §4.6, 14 fields). Earlier draft published only 9 of 14;
    // missing impact_point + authorization_chain + the two
    // timestamps would crash Aban's audit join (chain ID empty)
    // and render impact point at (0,0).
    const uint64_t now_ms = static_cast<uint64_t>(
      now().nanoseconds() / 1'000'000ll);

    FR msg;
    msg.header.stamp = now();
    msg.robot_id = 2;
    msg.command_id = ++fire_seq_;
    msg.sequence = fire_seq_;
    msg.result = FR::RESULT_SUCCESS;
    msg.rounds_fired = 1;
    msg.target_id = 42;
    msg.distance_to_target_m = 25.0f;
    msg.impact_point_x_m = 24.7f;                       // ~target +/- error
    msg.impact_point_y_m = 0.3f;
    msg.confidence = 0.95f;
    msg.authorization_chain = "mock-chain-" +
      std::to_string(fire_seq_);
    msg.notes = "synthetic mock event";
    msg.timestamp_fire_ms = now_ms - 50;                // ~50 ms ago
    msg.timestamp_report_ms = now_ms;
    fire_result_pub_->publish(msg);
  }

  // ─── publishers ────────────────────────────────────────────────
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr
    robot_audit_pub_, slam_audit_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::ThreatAlert>::SharedPtr
    threat_raw_pub_, threat_consensus_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr heading_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::HeartBeat>::SharedPtr
    sbc1_pub_, sbc2_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr poses_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr demo_status_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::FireResult>::SharedPtr
    fire_result_pub_;

  // ─── timers ────────────────────────────────────────────────────
  rclcpp::TimerBase::SharedPtr audit_timer_, threat_timer_, heading_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_, poses_timer_;
  rclcpp::TimerBase::SharedPtr demo_status_timer_, fire_timer_;

  // ─── state ─────────────────────────────────────────────────────
  uint32_t heartbeat_seq_{0};
  uint32_t fire_seq_{0};
  size_t demo_state_idx_{0};
};

}  // namespace san_external_mocks

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(
    std::make_shared<san_external_mocks::RosbridgeMockServer>());
  rclcpp::shutdown();
  return 0;
}
