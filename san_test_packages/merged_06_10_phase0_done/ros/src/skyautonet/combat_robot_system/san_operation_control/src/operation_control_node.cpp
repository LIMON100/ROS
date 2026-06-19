// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_operation_control/operation_control_node.hpp"

#include <chrono>
#include <fstream>

using namespace std::chrono_literals;

namespace san_operation_control
{

OperationControlNode::OperationControlNode()
: OperationControlNode(rclcpp::NodeOptions())
{}

OperationControlNode::OperationControlNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("operation_control_node", options)
{
  declareParameters();
  readParameters();
  wireInterfaces();
  RCLCPP_INFO(
    get_logger(),
    "OperationControl started: mode=%s watchdog=%s demo_sequencer=%s",
    toString(mode_).c_str(),
    watchdog_.isEnabled() ? "on" : "off",
    demo_.isEnabled() ? "on" : "off");
}

void OperationControlNode::declareParameters()
{
  declare_parameter<std::string>("deployment_mode", "production");
  declare_parameter<bool>("hw_watchdog_enabled", true);
  declare_parameter<double>("sensor_stale_threshold_sec", 3.0);
  declare_parameter<double>("demo_phase_duration_sec", 10.0);
  declare_parameter<int>("robot_id", 1);
  declare_parameter<double>("tick_period_sec", 1.0);
  // [DCN-2026-011 D-032] Dual-SBC slot. -1 = unset (default); the
  // resolver then falls back to /etc/skyautonet/sbc_id. 0/1/2 are the
  // valid resolved values (0 = N/A, 1 = primary Hub SBC, 2 = secondary).
  declare_parameter<int>("sbc_id", -1);
}

void OperationControlNode::readParameters()
{
  const std::string mode_str =
    get_parameter("deployment_mode").as_string();
  mode_ = fromString(mode_str);
  const bool yaml_watchdog =
    get_parameter("hw_watchdog_enabled").as_bool();

  watchdog_.setStaleThresholdSec(
    get_parameter("sensor_stale_threshold_sec").as_double());
  watchdog_.configure(mode_, yaml_watchdog);

  demo_.setPhaseDurationSec(
    get_parameter("demo_phase_duration_sec").as_double());
  demo_.setPhaseCallback(
    [this](DemoPhase p) {demoPhaseTransition(p);});
  if (demoSequencerEnabled(mode_)) {
    demo_.enableForMode(mode_);
  }

  robot_id_ = get_parameter("robot_id").as_int();

  // [DCN-2026-011 D-032] Resolve the dual-SBC slot. The resolver
  // honours the launch parameter first (so squadron.launch.py and
  // hub_sbc1/2.launch.py wrappers stay authoritative), then a
  // hardware-pinned file at /etc/skyautonet/sbc_id, then defaults to
  // 0 ("N/A"). The log line is a deployment-time sanity check —
  // operations staff use it to confirm provisioning matched intent.
  sbc_id_ = resolveSbcId("/etc/skyautonet/sbc_id");
  RCLCPP_INFO(get_logger(), "sbc_id resolved to %u", sbc_id_);
}

uint8_t OperationControlNode::resolveSbcId(const std::string & path) const
{
  // 1. Launch parameter wins when set to a valid value (0/1/2).
  const int from_param =
    static_cast<int>(get_parameter("sbc_id").as_int());
  if (from_param >= 0 && from_param <= 2) {
    return static_cast<uint8_t>(from_param);
  }

  // 2. /etc/skyautonet/sbc_id — single ASCII digit, optional newline.
  std::ifstream f(path);
  if (f.is_open()) {
    std::string line;
    std::getline(f, line);
    if (!line.empty()) {
      const char c = line[0];
      if (c >= '0' && c <= '2') {
        return static_cast<uint8_t>(c - '0');
      }
    }
  }

  // 3. Fallback — not applicable (non-Hub robots).
  return 0;
}

void OperationControlNode::wireInterfaces()
{
  // [Sanitizer-hardening] Bind the SBC peer heartbeat sub and the
  // tick timer to a single MutuallyExclusive callback group so the
  // heartbeat write into last_peer_heartbeat_ and the read inside
  // buildStatusMessage() cannot race under MultiThreadedExecutor.
  // The command subscriptions stay on the default group — they only
  // touch CommandEcho (already mutex-internal) and never read the
  // heartbeat state.
  sbc_cb_group_ = create_callback_group(
    rclcpp::CallbackGroupType::MutuallyExclusive);
  rclcpp::SubscriptionOptions sbc_sub_opts;
  sbc_sub_opts.callback_group = sbc_cb_group_;

  rclcpp::QoS cmd_qos(10);
  cmd_qos.reliable();

  // Command-echo (audit) subscriptions. formation/waypoint/emergency_stop
  // are aligned to the topics the operator tools actually publish
  // (san_operator_tools: /swarm/formation_command, /swarm/waypoint_command,
  // /swarm/emergency_stop) — previously these listened on /swarm/cmd/* with
  // no publisher, so the operator-command audit trail was silently empty.
  // TODO(IDS): unify the operator-command namespace (the remaining
  // mission_state/fire_authorization/jamming/manual_override topics still
  // have no producer and are pending the canonical IDS convention).
  sub_formation_ = create_subscription<FormationCmd>(
    "/swarm/formation_command", cmd_qos,
    [this](FormationCmd::SharedPtr m) {noteCommand(m);});
  sub_mission_ = create_subscription<MissionStateCmd>(
    "/swarm/cmd/mission_state", cmd_qos,
    [this](MissionStateCmd::SharedPtr m) {noteCommand(m);});
  sub_fire_ = create_subscription<FireAuth>(
    "/swarm/cmd/fire_authorization", cmd_qos,
    [this](FireAuth::SharedPtr m) {noteCommand(m);});
  sub_jam_ = create_subscription<JammingCmd>(
    "/swarm/cmd/jamming", cmd_qos,
    [this](JammingCmd::SharedPtr m) {noteCommand(m);});
  sub_waypoint_ = create_subscription<WaypointCmd>(
    "/swarm/waypoint_command", cmd_qos,
    [this](WaypointCmd::SharedPtr m) {noteCommand(m);});
  sub_override_ = create_subscription<ManualOverride>(
    "/swarm/cmd/manual_override", cmd_qos,
    [this](ManualOverride::SharedPtr m) {noteCommand(m);});
  sub_estop_ = create_subscription<EStop>(
    "/swarm/emergency_stop", cmd_qos,
    [this](EStop::SharedPtr m) {noteCommand(m);});
  sub_video_ = create_subscription<VideoReq>(
    "/video/request", cmd_qos,
    [this](VideoReq::SharedPtr m) {noteCommand(m);});

  // ROS 2 topic-name validator rejects tokens that start with a
  // digit, so "/swarm/<id>/status" with a bare integer (e.g.
  // "/swarm/3/status") throws InvalidTopicNameError at construction
  // time. Prefix with "robot_" to produce a valid identifier token.
  pub_status_ = create_publisher<RobotStatus>(
    "/swarm/robot_" + std::to_string(robot_id_) + "/status",
    rclcpp::QoS(5).reliable());
  pub_health_ = create_publisher<SwarmHealth>(
    "/hub/swarm/health_summary",
    rclcpp::QoS(5).reliable().transient_local());

  const auto period = std::chrono::duration<double>(
    get_parameter("tick_period_sec").as_double());
  tick_timer_ = create_wall_timer(
    std::chrono::duration_cast<std::chrono::nanoseconds>(period),
    std::bind(&OperationControlNode::onTick, this),
    sbc_cb_group_);

  // [DCN-2026-011 D-033] Dual-SBC mutual heartbeat. Only the two
  // Hub SBCs (sbc_id 1 or 2) own a heartbeat pub/sub pair; non-Hub
  // robots (sbc_id 0) skip the wiring entirely so the topic does
  // not appear in their graph.
  if (sbc_id_ == 1 || sbc_id_ == 2) {
    const std::string own_topic =
      "/hub_internal/sbc" + std::to_string(sbc_id_) + "/heartbeat";
    const std::string peer_topic = "/hub_internal/sbc" +
      std::to_string(sbc_id_ == 1 ? 2 : 1) + "/heartbeat";
    rclcpp::QoS hb_qos(rclcpp::KeepLast(5));
    hb_qos.best_effort();
    sbc_heartbeat_pub_ = create_publisher<std_msgs::msg::Header>(
      own_topic, hb_qos);
    peer_heartbeat_sub_ = create_subscription<std_msgs::msg::Header>(
      peer_topic, hb_qos,
      std::bind(
        &OperationControlNode::onPeerHeartbeat, this,
        std::placeholders::_1),
      sbc_sub_opts);
    sbc_heartbeat_timer_ = create_wall_timer(
      std::chrono::milliseconds(200),         // 5 Hz
      std::bind(&OperationControlNode::publishSbcHeartbeat, this),
      sbc_cb_group_);
    RCLCPP_INFO(
      get_logger(),
      "SBC heartbeat wired: pub=%s sub=%s (5 Hz, %.1f s stale)",
      own_topic.c_str(), peer_topic.c_str(), kPeerStaleSec);
  }

  // ─── [DCN-2026-016] Gate-1 demo ROS integration ─────────────────
  // Hosts the operator-facing /gate1/* interface on top of the
  // DemoSequencer this node already owns — no duplicate sequencer
  // ownership. Active in all modes; preconditions are enforced at
  // service-call time.
  gate1_start_srv_ = create_service<std_srvs::srv::Trigger>(
    "/gate1/start_demo",
    std::bind(
      &OperationControlNode::onGate1StartDemo, this,
      std::placeholders::_1, std::placeholders::_2));

  demo_status_pub_ = create_publisher<combat_robot_msgs::msg::OperationState>(
    "/gate1/demo_status", rclcpp::QoS(10).reliable());

  // Separate subscription on `/emergency_stop` (distinct from the
  // existing sub_estop_ on `/swarm/cmd/emergency_stop` which only
  // feeds command_id echo). This one drives the RTH fallback.
  gate1_estop_sub_ = create_subscription<EStop>(
    "/emergency_stop",
    rclcpp::QoS(10).reliable().transient_local(),
    std::bind(
      &OperationControlNode::onGate1EmergencyStop, this,
      std::placeholders::_1));

  demo_status_timer_ = create_wall_timer(
    5s, std::bind(&OperationControlNode::publishDemoStatus, this));

  demo_watchdog_timer_ = create_wall_timer(
    5s, std::bind(&OperationControlNode::demoWatchdogTick, this));

  rth_client_ = rclcpp_action::create_client<
    combat_robot_msgs::action::ReturnToHome>(this, "/rth");

  // Audit A5 — subscribe /swarm/operation_state to learn whether
  // this robot is the elected leader. The Gate-1 EmergencyStop
  // SCOPE_LEADER_ONLY filter reads `is_leader_role_` instead of
  // the wrong `robot_id_ == 1` proxy.
  op_state_sub_for_leader_ = create_subscription<
    combat_robot_msgs::msg::OperationState>(
    "/swarm/operation_state",
    rclcpp::QoS(1).best_effort(),
    [this](combat_robot_msgs::msg::OperationState::SharedPtr msg) {
      if (msg == nullptr) {return;}
      is_leader_role_.store(
        msg->leader_robot_id ==
        static_cast<uint32_t>(robot_id_));
    });

  // DemoSequencer phase callback — readParameters() already wired
  // the in-class demoPhaseTransition; we forward into the Gate-1
  // phase hook from there (see demoPhaseTransition body).

  RCLCPP_INFO(
    get_logger(),
    "DCN-2026-016 Gate-1 ROS integration active "
    "(/gate1/start_demo, /gate1/demo_status, /emergency_stop → /rth)");
}

void OperationControlNode::publishSbcHeartbeat()
{
  if (!sbc_heartbeat_pub_) {return;}
  std_msgs::msg::Header hb;
  hb.stamp = now();
  hb.frame_id = "sbc" + std::to_string(sbc_id_);
  sbc_heartbeat_pub_->publish(hb);
}

void OperationControlNode::onPeerHeartbeat(
  const std_msgs::msg::Header::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  last_peer_heartbeat_ = rclcpp::Time(msg->stamp);
}

bool OperationControlNode::peerSbcHealthy() const
{
  if (!last_peer_heartbeat_.has_value()) {return false;}
  const auto age =
    (now() - *last_peer_heartbeat_).seconds();
  return age < kPeerStaleSec;
}

uint64_t OperationControlNode::nowMs() const
{
  return static_cast<uint64_t>(now().nanoseconds() / 1'000'000ll);
}

template<typename T>
void OperationControlNode::noteCommand(const std::shared_ptr<T> msg)
{
  if (msg == nullptr) {return;}
  echo_.note(msg->command_id, nowMs());
  RCLCPP_DEBUG(
    get_logger(),
    "command_id=%u observed", msg->command_id);
}

// Explicit instantiations - keeps the template definition out of the
// header and avoids surprising one-definition-rule issues.
template void OperationControlNode::noteCommand(FormationCmd::SharedPtr);
template void OperationControlNode::noteCommand(MissionStateCmd::SharedPtr);
template void OperationControlNode::noteCommand(FireAuth::SharedPtr);
template void OperationControlNode::noteCommand(JammingCmd::SharedPtr);
template void OperationControlNode::noteCommand(WaypointCmd::SharedPtr);
template void OperationControlNode::noteCommand(ManualOverride::SharedPtr);
template void OperationControlNode::noteCommand(EStop::SharedPtr);
template void OperationControlNode::noteCommand(VideoReq::SharedPtr);

void OperationControlNode::onTick()
{
  const uint64_t t = nowMs();
  demo_.tick(t);
  publishStatus();
  publishHealth();
}

void OperationControlNode::demoPhaseTransition(DemoPhase phase)
{
  RCLCPP_INFO(
    get_logger(),
    "[DEMO] phase -> %s (mode=%s)",
    demoPhaseName(phase), toString(mode_).c_str());
  // [DCN-2026-016] Forward into the Gate-1 hook so the watchdog
  // can reset its last_phase_advance timestamp and RTB phase can
  // auto-trigger /rth.
  onDemoPhaseTransition(phase);
}

void OperationControlNode::publishStatus()
{
  if (!pub_status_) {return;}
  pub_status_->publish(buildStatusMessage());
}

combat_robot_msgs::msg::RobotStatus
OperationControlNode::publishStatusForTest()
{
  return buildStatusMessage();
}

void OperationControlNode::injectPeerHeartbeatForTest(
  const rclcpp::Time & stamp)
{
  last_peer_heartbeat_ = stamp;
}

combat_robot_msgs::msg::RobotStatus
OperationControlNode::buildStatusMessage()
{
  RobotStatus s;
  s.header.stamp = now();
  s.robot_id = robot_id_;
  s.robot_role = 0;
  s.battery_soc = 1.0f;
  s.locomotion_mode = 0;
  s.tier = 0;
  s.slam_healthy = true;
  s.perception_healthy = true;
  s.comm_healthy = true;
  s.lte_active = false;
  s.is_lte_backup_designated = false;
  // [DCN-2026-011 D-033] Dual-SBC health fields. The own slot is
  // always reported true (we wouldn't be publishing otherwise); the
  // peer slot is reported true only when its heartbeat is fresher
  // than kPeerStaleSec. Non-Hub robots (sbc_id_ == 0) publish both
  // fields false — HubHealthMonitor ignores non-Hub RobotStatus
  // entries, so the sentinel is unambiguous.
  if (sbc_id_ == 1) {
    s.sbc1_healthy = true;
    s.sbc2_healthy = peerSbcHealthy();
  } else if (sbc_id_ == 2) {
    s.sbc1_healthy = peerSbcHealthy();
    s.sbc2_healthy = true;
  } else {
    s.sbc1_healthy = false;
    s.sbc2_healthy = false;
  }
  s.last_received_command_id = echo_.lastId();
  s.last_command_received_ms = echo_.lastMs();
  s.timestamp_ms = nowMs();
  return s;
}

void OperationControlNode::publishHealth()
{
  if (!pub_health_) {return;}
  SwarmHealth h;
  h.header.stamp = now();
  h.last_received_command_id = echo_.lastId();
  h.last_command_received_ms = echo_.lastMs();
  h.timestamp_ms = nowMs();
  pub_health_->publish(h);
}

// ─── [DCN-2026-016] Gate-1 demo ROS integration ─────────────────────────

uint64_t OperationControlNode::steadyNowMs()
{
  return static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

void OperationControlNode::onGate1StartDemo(
  const std_srvs::srv::Trigger::Request::SharedPtr /*req*/,
  std_srvs::srv::Trigger::Response::SharedPtr res)
{
  // Preconditions:
  //   1. deployment_mode in {DEMO, LAB_TEST} (DemoSequencer-enabled)
  //   2. DemoSequencer enableForMode() accepts the mode
  // Production / Bench / Development reject — operator must escalate
  // via the deployment_mode yaml, not via the demo service.
  if (mode_ != DeploymentMode::DEMO && mode_ != DeploymentMode::LAB_TEST) {
    res->success = false;
    res->message = "DemoSequencer not enabled in current mode (" +
      toString(mode_) + ")";
    return;
  }
  const bool ok = demo_.enableForMode(mode_);
  res->success = ok;
  res->message = ok ? "Demo started" :
    "DemoSequencer rejected enableForMode()";
  if (ok) {
    last_phase_advance_ms_.store(steadyNowMs());
  }
}

void OperationControlNode::onGate1EmergencyStop(EStop::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  // EmergencyStop.msg has no `triggered` field — message receipt itself
  // is the trigger. Honour scope: ALL or SINGLE(this robot) or LEADER
  // (audit A5 — was robot_id_ == 1 proxy; now reads is_leader_role_
  // populated by op_state_sub_for_leader_).
  const bool applies =
    (msg->scope == EStop::SCOPE_ALL_ROBOTS) ||
    (msg->scope == EStop::SCOPE_SINGLE_ROBOT &&
    msg->target_robot_id == static_cast<uint32_t>(robot_id_)) ||
    (msg->scope == EStop::SCOPE_LEADER_ONLY && is_leader_role_.load());
  if (!applies) {return;}

  demo_.disable();
  triggerRth("emergency_stop reason='" + msg->reason + "'");
}

void OperationControlNode::publishDemoStatus()
{
  if (!demo_status_pub_) {return;}
  combat_robot_msgs::msg::OperationState st;
  st.header.stamp = now();
  st.deployment_mode = toString(mode_);
  // Operator-screen banner derived from the phase so the UI can show
  // "DEMO/PATROL" instead of just the mode label.
  st.operator_banner = std::string(demoPhaseName(demo_.currentPhase()));
  demo_status_pub_->publish(st);
}

void OperationControlNode::demoWatchdogTick()
{
  // Watchdog is active only when the sequencer has been started
  // (last_phase_advance_ms_ != 0) and is past STANDBY. Resetting to
  // STANDBY (e.g. via emergency-stop disable() path) silently
  // pauses the watchdog without spurious RTH.
  auto last = last_phase_advance_ms_.load();
  if (last == 0) {return;}
  if (demo_.currentPhase() == DemoPhase::STANDBY) {return;}

  const auto now_ms = steadyNowMs();
  if (now_ms <= last || (now_ms - last) <= 60000u) {return;}

  // ─── Audit A3 (P2) fix ─────────────────────────────────────────
  // Pre-fix: load + compare + store(0) was a 3-step RMW; under MTE
  // a concurrent onDemoPhaseTransition could update last_phase_
  // advance_ms_ between our load and store, and we'd zero out the
  // freshly-set timestamp → spurious double-trigger on the next
  // tick. compare_exchange_strong makes the reset atomic vs the
  // value we observed when deciding to trigger.
  if (!last_phase_advance_ms_.compare_exchange_strong(last, 0u)) {
    // Another writer advanced the timestamp between our checks;
    // skip this tick — the new value isn't yet stale.
    return;
  }
  triggerRth("phase_watchdog_60s_no_advance");
}

void OperationControlNode::triggerRth(const std::string & reason)
{
  RCLCPP_WARN(get_logger(), "Gate-1 RTH triggered: %s", reason.c_str());
  if (!rth_client_) {
    RCLCPP_ERROR(
      get_logger(),
      "rth_client_ not initialised — RTH dispatch skipped");
    return;
  }
  // ─── Audit finding A1 (P1) fix ──────────────────────────────────
  // Was: wait_for_action_server(2s) — blocks the calling thread up
  // to 2 s. When the caller is onGate1EmergencyStop (a subscription
  // callback) under SingleThreadedExecutor this freezes all other
  // callbacks (op_state, commands) for the wait window — exactly
  // when responsiveness matters most.
  //
  // Post-fix: non-blocking action_server_is_ready() check. If the
  // server isn't up at dispatch time, log + skip (the watchdog/
  // operator will retry on the next trigger).
  if (!rth_client_->action_server_is_ready()) {
    RCLCPP_ERROR(
      get_logger(),
      "san_rth /rth action server not ready at dispatch — "
      "RTH skipped (non-blocking). Reason: %s", reason.c_str());
    demo_.disable();
    last_phase_advance_ms_.store(0);
    return;
  }
  combat_robot_msgs::action::ReturnToHome::Goal goal;
  goal.reset_home_pose = false;
  rth_client_->async_send_goal(goal);
  demo_.disable();
  last_phase_advance_ms_.store(0);
}

void OperationControlNode::onDemoPhaseTransition(DemoPhase next)
{
  last_phase_advance_ms_.store(steadyNowMs());
  if (next == DemoPhase::RTB) {
    triggerRth("phase_rtb_auto");
  }
}

}  // namespace san_operation_control
