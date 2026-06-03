cmake_minimum_required(VERSION 3.16)
project(combat_robot_msgs)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(action_msgs REQUIRED)

set(msg_files
  "msg/LTERoleAnnouncement.msg"
  "msg/RobotStatus.msg"
  "msg/VideoStreamRequest.msg"
  "msg/VideoStreamHandle.msg"
  # PHASE 7: SwarmRobotCommand variants + swarm rollup
  "msg/FormationCommand.msg"
  "msg/MissionStateCommand.msg"
  "msg/FireAuthorization.msg"
  # PHASE 9 v1.5: Per-trigger fire authorization w/ HMAC + Two-key (DCN-2026-001 D-004)
  "msg/FireAuthorizationRequest.msg"
  "msg/FireAuthorizationResponse.msg"
  "msg/JammingCommand.msg"
  "msg/WaypointCommand.msg"
  "msg/ManualOverrideCommand.msg"
  "msg/EmergencyStop.msg"
  "msg/SwarmHealthSummary.msg"
  # PHASE 8 v1.5: Hub-Deputy 이중화 + 4-tier Leader 승계 (DCN-2026-001 D-005)
  "msg/LeaderRoleAnnouncement.msg"
  "msg/HubRoleAnnouncement.msg"
  # PHASE 0 v1.4: foundation operation-state heartbeat
  "msg/OperationState.msg"
  # PHASE 1: 4-layer local cost map snapshot
  "msg/CostMapUpdate.msg"
  # PHASE 3: SLAM 5s aggregation
  "msg/SLAMLocalDelta.msg"
  "msg/SLAMAggregatedMap.msg"
  # PHASE 5b: LTE auto-rate
  "msg/LteLinkQuality.msg"
  # Phase 2-E Turn 3: LTE modem AT-derived status (DCN-2026-002 D-008)
  "msg/LteModemStatus.msg"
  # Phase 2-E Turn 4: RTK GNSS detailed fix status (DCN-2026-002 D-008)
  "msg/RtkFixStatus.msg"
  # RTK Fix→Float mitigation Layer 3: RTK-quality nav constraints
  "msg/NavConstraints.msg"
  # Phase 2-E Turn 7: single-point laser range finder (DCN-2026-002 D-008)
  "msg/LrfReading.msg"
  # Phase 2-E Turn 8 ext: hub threat aggregation (DCN-2026-002 D-008)
  "msg/ThreatAlert.msg"
  # Phase 2-E Turn 11-12: perception detections (DCN-2026-002 D-008)
  "msg/Detection.msg"
  "msg/DetectionArray.msg"
  # NOTE: BleCommand / BlePhaseStatus removed in DCN-2026-008 v2 (D-WIFI-001).
  # Operator link is WiFi (rosbridge_server) only — see SAN-WIFI-OPSPEC-001.
  # CODE_REVIEW 2026-05-13 — 빌드 차단 해소 (san_comm_link, san_hub_orchestrator)
  "msg/CommLinkStatus.msg"
  "msg/FleetStatus.msg"
  # PDR-prep — IDS-CMD §5.1/§5.2/§5.9/§4.2 (SDD §6.3, §7)
  "msg/SlotAssignment.msg"
  "msg/FollowerTargetMessage.msg"
  "msg/BreadcrumbBroadcast.msg"
  "msg/FormationStatus.msg"
  # PDR-prep — IDS-CMD §5.10/§5.11 (SDD §8 360° Surveillance Sector)
  "msg/SurveillanceSectorAssignment.msg"
  "msg/PanTiltCommand.msg"
  # PDR-prep — IDS-CMD §4.5 (SDD §6.2 5-Tier FSM, KPP-2)
  "msg/TierStatusChange.msg"
  # PDR-7 — IDS-CMD §3.8 / §4.6 / §4.7 / §5.3 / §5.8 (누락 5종 추가)
  # IDS 정합도 80% → 94% 향상
  "msg/OperatorHeartbeat.msg"            # §3.8 운용병사 deadman heartbeat
  "msg/FireResult.msg"                   # §4.6 사격 결과 보고 (D-004 audit)
  "msg/BatteryWarning.msg"               # §4.7 배터리 transition event
  "msg/LeaderElectionVote.msg"           # §5.3 Modified Raft vote (D-005)
  "msg/LeaderElectionResponse.msg"       # §5.3 Modified Raft response
  "msg/HeartBeat.msg"                    # §5.8 inter-robot watchdog
  # DCN-2026-019 — Mission Control wire-level protocol (seq + crc32 +
  # 16-deep sliding window + retransmit-on-ack-timeout). Distinct from
  # MissionStateCommand / ManualOverride which carry mission semantics.
  "msg/MCMessage.msg"
  "msg/MCAck.msg"
)

# DCN-2026-017 — Return-to-Home action interface (consumed by san_rth
# rclcpp_action server and san_mission rclpy action client per ADR-008).
set(action_files
  "action/ReturnToHome.action"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  ${action_files}
  DEPENDENCIES std_msgs geometry_msgs action_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
