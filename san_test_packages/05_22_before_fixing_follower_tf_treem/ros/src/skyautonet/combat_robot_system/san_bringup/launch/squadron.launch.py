# SAN v1.5 Phase 2-E Turn 1 — Squadron-level launch composition.
#
# Replaces main.py multiprocessing bootstrap per DCN-2026-002 D-007
# (3-Tier toolchain) + D-008 (IPC unification on ROS 2).
#
# Usage (manual, dev / smoke test):
#   ros2 launch san_bringup squadron.launch.py \
#       robot_id:=1 robot_role:=leader deployment_mode:=production
#
# Usage (production via systemd):
#   sudo systemctl start san-squadron
#
# Arguments:
#   robot_id            Integer 1-8 (1=Leader Go2, 2=Hub UGV,
#                                    3=Deputy UGV, 4-8=Followers)
#   robot_role          leader | hub | deputy | follower
#   deployment_mode     production | training | bench | dev
#   hub_features        true|false (auto-true when robot_role==hub)
#   include_perception  true|false (Phase 2-E Turn 11+ wraps with rclpy)
#   include_camera      true|false (Phase 2-E Turn 6+ swaps to imx678/thermal)
#   include_regression  true|false (auto-true for bench/dev modes)
#
# Coverage (Phase 2-E Turn 1):
#   11 always-on Tier 1 C++ nodes from existing packages + 3 conditional.
#   HW adapter nodes (Turns 2-7), Mission/Perception (Turns 9-12), BLE
#   (Turn 13) will join here as each is ported to rclcpp/rclpy.
#
# Refs:
#   * DCN-2026-002 — D-007 Amendment, D-008 IPC unification
#   * ADR-006 — IPC Unification Strategy
#   * SDD-SWARM v1.5 §10.1.1 — standard toolchain
#   * OPS-SOP v1.5 §3.1 — deployment modes

from ament_index_python import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, IncludeLaunchDescription, LogInfo
from launch.conditions import IfCondition
from launch.substitutions import (
    LaunchConfiguration,
    PathJoinSubstitution,
    PythonExpression,
)
from launch_ros.actions import Node, PushRosNamespace
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare

import os
from launch.launch_description_sources import PythonLaunchDescriptionSource

def _arg(name, default, description):
    """Compact DeclareLaunchArgument wrapper."""
    return DeclareLaunchArgument(
        name, default_value=str(default), description=description
    )


def generate_launch_description():
    # ─── Launch arguments ─────────────────────────────────────────────
    robot_id           = LaunchConfiguration("robot_id")
    robot_role         = LaunchConfiguration("robot_role")
    deployment_mode    = LaunchConfiguration("deployment_mode")
    hub_features       = LaunchConfiguration("hub_features")
    include_regression = LaunchConfiguration("include_regression")
    # [DCN-2026-011 D-032] sbc_id (-1 = auto-resolve via
    # /etc/skyautonet/sbc_id; 0 = N/A; 1 = primary Hub SBC; 2 = secondary).
    # Only OperationControlNode consumes the parameter — non-Hub robots
    # leave the default -1 → file → 0. The robot profile wrappers
    # (hub_sbc1.launch.py / hub_sbc2.launch.py — DCN-2026-011 D-034)
    # set this explicitly so provisioning errors can't shift the slot.
    sbc_id             = LaunchConfiguration("sbc_id")

    # ─── Per-package config files ─────────────────────────────────────
    fire_auth_config = PathJoinSubstitution([
        FindPackageShare("san_fire_authorization"),
        "config", "fire_authorization.yaml",
    ])
    squadron_config = PathJoinSubstitution([
        FindPackageShare("san_bringup"),
        "config", "squadron.yaml",
    ])

    # ─── Common node parameters ───────────────────────────────────────
    # Every Tier 1 node receives robot_id + deployment_mode injected so
    # it can self-configure namespaces, log levels, and feature flags.
    common_params = [
        squadron_config,
        # LaunchConfiguration values are strings by default; node
        # declarations for robot_id are `int`, so ros2launch raises
        # InvalidParameterTypeException ("setting it to {integer} is
        # not allowed" — local_slam_node / cost_map_node terminate()).
        # Use ParameterValue with value_type=int to coerce.
        {"robot_id": ParameterValue(robot_id, value_type=int)},
        {"deployment_mode": deployment_mode},
        # [DCN-2026-011 D-032] sbc_id is consumed only by
        # operation_control_node; other Tier 1 nodes silently ignore
        # the override (parameter not declared).
        {"sbc_id": ParameterValue(sbc_id, value_type=int)},
    ]

    # ─── Tier 1 ALWAYS-ON nodes (Phase 1 + 2-D delivered packages) ────
    # Executable names match the actual add_executable() targets in each
    # package's CMakeLists.txt — verified against main on 2026-05-12.
    always_on = GroupAction(
        actions=[
            LogInfo(msg=["[san_bringup] starting Tier 1 always-on nodes"]),

            # Role management — Leader/Hub/Deputy/Limp-Mode succession
            # SDD-SWARM v1.5 §5.6
            Node(
                package="san_role_management",
                executable="role_management_node",
                name="role_management_node",
                output="screen",
                parameters=common_params,
            ),

            # LTE redundancy — role announce + link quality
            # SDD-SWARM v1.5 §5.5
            # NOTE: lte_node is the role manager; lte_link_quality_node is
            #       its sidecar publisher; mwan3_init is a Type=oneshot
            #       systemd ExecStartPre, not launched here.
            Node(
                package="san_lte_redundancy",
                executable="lte_node",
                name="lte_node",
                output="screen",
                parameters=common_params,
            ),
            Node(
                package="san_lte_redundancy",
                executable="lte_link_quality_node",
                name="lte_link_quality_node",
                output="screen",
                parameters=common_params,
            ),

            # Phase 2-E Turn 3 — AT-command modem driver
            # (DCN-2026-002 D-008, replaces adapters/lte_modem.py)
            Node(
                package="san_lte_redundancy",
                executable="lte_modem_node",
                name="lte_modem_node",
                output="screen",
                parameters=common_params,
            ),

            # Phase 2-E Turn 4 — RTK GNSS receiver (u-blox F9P)
            # /rtcm_corrections and /gga_latest are global so the
            # NtripClient + RtkGnss pair can talk without per-robot
            # namespace prefixes.
            Node(
                package="san_rtk_gnss",
                executable="rtk_gnss_node",
                name="rtk_gnss_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_rtk_gnss"),
                        "config", "rtk_gnss.yaml",
                    ]),
                ] + common_params,
                remappings=[
                    ("~/rtcm_corrections", "/rtcm_corrections"),
                    ("~/gga_latest", "/gga_latest"),
                ],
            ),

            # Phase 2-E Turn 4 — NTRIP caster client (RTCM uplink)
            Node(
                package="san_ntrip_client",
                executable="ntrip_client_node",
                name="ntrip_client_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_ntrip_client"),
                        "config", "ntrip_client.yaml",
                    ]),
                ] + common_params,
                remappings=[
                    ("~/rtcm_corrections", "/rtcm_corrections"),
                    ("~/gga_latest", "/gga_latest"),
                ],
            ),

            # Phase 2-E Turn 5 — External payload IMU driver
            # (DCN-2026-002 D-008, replaces adapters/payload_sensors.py::ExternalImuAdapter)
            Node(
                package="san_imu_driver",
                executable="imu_driver_node",
                name="imu_driver_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_imu_driver"),
                        "config", "imu_driver.yaml",
                    ]),
                ] + common_params,
            ),

            # Phase 2-E Turn 6 — IMX678 4K H.265 camera
            # (DCN-2026-002 D-008, ShmPool → intra-process zero-copy)
            Node(
                package="san_cameras",
                executable="imx678_camera_node",
                name="imx678_camera_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_cameras"),
                        "config", "imx678.yaml",
                    ]),
                ] + common_params,
            ),

            # Phase 2-E Turn 6 — Thermal camera (640x512 mono16)
            Node(
                package="san_cameras",
                executable="thermal_camera_node",
                name="thermal_camera_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_cameras"),
                        "config", "thermal.yaml",
                    ]),
                ] + common_params,
            ),

            # Phase 2-E Turn 7 — Single-point Laser Range Finder
            # (DCN-2026-002 D-008, replaces adapters/payload_sensors.py::LrfAdapter)
            Node(
                package="san_lidar",
                executable="lrf_node",
                name="lrf_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_lidar"),
                        "config", "lrf.yaml",
                    ]),
                ] + common_params,
            ),

            # Phase 2-E Turn 8 — Telemetry uplink with WiFi6↔LTE failover
            # (DCN-2026-002 D-008, Tier 2 — replaces comm/comm_process.py)
            Node(
                package="san_comm",
                executable="comm_uplink_node",
                name="comm_uplink_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_comm"),
                        "config", "comm_uplink.yaml",
                    ]),
                ] + common_params,
                remappings=[
                    # Pull LTE modem status from san_lte_redundancy (Turn 3)
                    ("/robot_lte_status", "lte_modem_node/modem_status"),
                ],
            ),

            # Phase 2-E Tier 2 — mesh link health monitor
            Node(
                package="san_comm_link",
                executable="comm_link_node",
                name="comm_link_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_comm_link"),
                        "config", "comm_link.yaml",
                    ]),
                ] + common_params,
            ),

            # Phase 2-E Turn 9-10 — Mission orchestrator (Tier 2 rclpy)
            # ⭐ FIRST rclpy node — DCN-2026-002 D-007 hybrid policy
            # Replaces mission/mission_process.py
            Node(
                package="san_mission",
                executable="mission_node",
                name="mission_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_mission"),
                        "config", "mission.yaml",
                    ]),
                ] + common_params,
                # remappings=[("~/cmd_vel", "cmd_vel")],
                remappings=[("~/cmd_vel", "mission_node/cmd_vel")]
            ),

            # ─────────────────────────────────────────────────────────
            # [v1.5.1.A F-2 fix] san_perception::perception_node DISABLED.
            #
            # Rationale (DCN-2026-005):
            #   * v1.5.1 D-003 (commit cdbb5e5) introduced the full C++
            #     human_detector with RKNN/YOLOv5 backend. This is the
            #     canonical detection pipeline going forward.
            #   * san_perception (Python, Turn 11-12) was a legacy Tier-3
            #     rclpy node that also published to ~/detections. With
            #     both active, mission_node and threat_aggregator faced
            #     two DetectionArray streams on different namespaces
            #     (/perception_node/detections vs /human_detector_node/
            #     detections) — ambiguous deduplication responsibility.
            #   * Active hot-path consumers (mission_node, threat_
            #     aggregator) are expected to subscribe to
            #     /human_detector_node/detections explicitly. See
            #     DCN-2026-005 §3.2 for the topic remap policy.
            #   * san_perception package is retained in the tree for
            #     possible future revival (e.g. RGB/thermal fusion node
            #     decoupled from detection); only its launch entry is
            #     removed here.
            #
            # Original block preserved below as a comment for archeology.
            # Re-enable by deleting the leading '#' on each line.
            #
            # Node(
            #     package="san_perception",
            #     executable="perception_node",
            #     name="perception_node",
            #     output="screen",
            #     parameters=[
            #         PathJoinSubstitution([
            #             FindPackageShare("san_perception"),
            #             "config", "perception.yaml",
            #         ]),
            #     ] + common_params,
            #     remappings=[
            #         ("camera_compressed",
            #          "/imx678_camera_node/image_compressed"),
            #         ("thermal_image",
            #          "/thermal_camera_node/image"),
            #         ("pose", "/pose"),
            #     ],
            # ),

            # [v1.5.2 DCN-2026-008 v2 — D-WIFI-001] BLE Control removed.
            # PM policy 2026-05-13: Android App ↔ Leader communication
            # uses Wi-Fi (rosbridge_server) only. The 7-phase BLE FSM
            # (BOOT → BLE_ADV → BLE_CONN → WIFI_BRINGUP → WIFI_READY
            # → STREAMING) and the san_ble_control package are deleted.
            # See `docs/external_spec/SAN-WIFI-OPSPEC-001_Android_App_Spec.md`
            # for the operator-link contract.

            # [DCN-2026-008 v2 — D-WIFI-002] Android App operator link.
            #
            # rosbridge_server exposes ROS 2 topics + services over a
            # WebSocket on port 9090 so the Galaxy Tab S9 (or any
            # roslibjs / roslib4j client) can publish operator commands
            # and subscribe to fleet status.
            #
            # Topic exposure is restricted via topics_glob so only the
            # operator / swarm / per-robot status topics are reachable;
            # services_glob is empty to keep service calls private.
            #
            # Connection handling (Android App side):
            #   * `reconnect_on_close: true` + exponential backoff
            #     (1s → 2s → 4s → 8s → 16s → 30s).
            #   * `session_id` from OperatorHeartbeat is preserved across
            #     reconnect so robot-side state can be re-keyed.
            #
            # When the OPERATOR_LOST consumer is wired up (currently
            # Phase 7 deferred — OperatorHeartbeat.msg exists but no
            # rclpy subscription does), its grace window MUST be set to
            # >= 30 s to swallow the App-side reconnect burst. See
            # SAN-WIFI-OPSPEC-001 §3 for the contract.
            Node(
                package="rosbridge_server",
                executable="rosbridge_websocket",
                name="rosbridge_websocket",
                output="screen",
                parameters=[{
                    "port": 9090,
                    "address": "",                  # bind to all ifs
                    "max_message_size": 10000000,   # 10 MB
                    "fragment_timeout": 600,
                    "delay_between_messages": 0.0,
                    "topics_glob": "[/operator/*, /swarm/*, /robot_*]",
                    "services_glob": "[]",          # no service exposure
                }],
            ),

            # Local SLAM
            Node(
                package="san_slam",
                executable="local_slam_node",
                name="local_slam_node",
                output="screen",
                parameters=common_params,
            ),

            # Cost map (4-layer per cost_map_4layer.md)
            Node(
                package="san_costmap",
                executable="cost_map_node",
                name="cost_map_node",
                output="screen",
                parameters=common_params,
            ),

            # Swarm coordinator — sector assign, formation, deputy slot
            Node(
                package="swarm_coordinator",
                executable="swarm_coordinator",
                name="swarm_coordinator_node",
                output="screen",
                parameters=common_params,
            ),

            Node(
                package="swarm_coordinator",
                executable="swarm_monitor_node",
                name="swarm_monitor_node",
                output="screen",
                parameters=common_params,
            ),

            # Fire authorization gate (Phase 2-D — D-004 100% 정합)
            # SDD-SWARM v1.5 §5.7.2.1
            Node(
                package="san_fire_authorization",
                executable="fire_authorization_node",
                name="fire_authorization_node",
                output="screen",
                parameters=[fire_auth_config] + common_params,
            ),

            # Top-level operation system — heartbeat + state aggregation
            # IDS v1.5 §3 (operation_state 1 Hz heartbeat)
            Node(
                package="combat_robot_operation_system",
                executable="combat_robot_operation_system",
                name="operation_system_node",
                output="screen",
                parameters=common_params,
            ),

            # Operation control — operator commands routing
            Node(
                package="san_operation_control",
                executable="operation_control_node",
                name="operation_control_node",
                output="screen",
                parameters=common_params,
            ),

            # LiDAR driver (Robosense E1)
            # NOTE: Phase 2-E Turn 7 will wrap this with a san_lidar
            #       rclcpp::Node fixing topic naming; for Turn 1 we keep
            #       the existing driver binary.
            Node(
                package="san_lidar",
                executable="robosense_e1_driver",
                name="lidar_node",
                output="screen",
                parameters=common_params, 
            ),

            # Video sender (LTE/Mesh uplink)
            Node(
                package="san_video_sender",
                executable="video_sender_node",
                name="video_sender_node",
                output="screen",
                parameters=common_params,
            ),

            # ────────────────────────────────────────────────────────
            # v1.5.1 (DCN-2026-003 D-003 + I-15 fix) — H.265 HW decoder
            # ────────────────────────────────────────────────────────
            # imx678_camera_node publishes H.265-encoded CompressedImage
            # at 30 fps. cv::imdecode (used by the v1.5 human_detector
            # patch) only handles still-image formats — see I-15 review.
            # This decoder uses GStreamer + mppvideodec (RK3588 MPP VPU)
            # to decode in hardware and republish raw sensor_msgs/Image
            # which human_detector and any other raw-frame consumer
            # (future spatial-fusion perception, recording, snapshot
            # OSD) can ingest directly.
            #
            # Backend selection is automatic: prefer_hw=true picks
            # mppvideodec when available (board) and falls back to
            # avdec_h265 (CI host with gstreamer1.0-libav).
            Node(
                package="san_video_decoder",
                executable="video_decoder_node",
                name="video_decoder_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_video_decoder"),
                        "config", "video_decoder.yaml",
                    ]),
                    {"compressed_topic":
                        "/imx678_camera_node/image_compressed"},
                    {"decoded_topic":
                        "/imx678_camera_node/image_decoded"},
                    {"prefer_hw": True},
                ] + common_params,
            ),

            # ────────────────────────────────────────────────────────
            # Object detection (v1.5.1 — DCN-2026-003 D-003 + I-15 fix)
            # ────────────────────────────────────────────────────────
            # The C++ human_detector owns the full camera → NPU →
            # DetectionArray pipeline (Airys V6.13.5-derived YOLOv5
            # post-process on RK3588). In v1.5.1 the input source is
            # the san_video_decoder output (raw BGR8 sensor_msgs/Image),
            # NOT the camera's H.265 CompressedImage — see I-15 fix.
            Node(
                package="human_detector",
                executable="human_detector_node",
                name="human_detector_node",
                output="screen",
                parameters=[
                    {"image_mode": "raw"},          # v1.5.1 I-15 fix
                    {"camera_topic":
                        "/imx678_camera_node/image_compressed"},
                    {"decoded_topic":
                        "/imx678_camera_node/image_decoded"},
                    {"detections_topic": "~/detections"},
                    {"max_inference_hz": 15},
                    {"drop_when_busy": True},
                    {"inference_backend": "rk3588"},
                ] + common_params,
            ),
        ]
    )

    # ─── Hub-only conditional nodes ───────────────────────────────────
    # hub_features = 'auto' (default) → True iff robot_role == 'hub'.
    # hub_features = 'true' / 'false'   → explicit override.
    # The PythonExpression evaluates at the IfCondition site so both
    # `robot_role` and `hub_features` LaunchConfigurations are
    # already resolved — avoids the earlier fragility of a
    # PythonExpression default-value that referenced another
    # LaunchConfiguration (raised "invalid syntax (<string>, line 1)").
    hub_only = GroupAction(
        condition=IfCondition(PythonExpression([
            "('", robot_role, "' == 'hub') if '", hub_features,
            "' == 'auto' else '", hub_features, "'.lower() == 'true'",
        ])),
        actions=[
            LogInfo(msg=["[san_bringup] starting hub-only nodes"]),

            # Hub SLAM aggregation (3 s period per ADR-002)
            Node(
                package="san_hub_slam",
                executable="hub_slam_node",
                name="hub_slam_node",
                output="screen",
                parameters=common_params,
            ),

            # Hub communication relay (GStreamer SRT)
            Node(
                package="san_hub_comm",
                executable="gstreamer_relay_node",
                name="hub_comm_relay_node",
                output="screen",
                parameters=common_params,
            ),

            # Phase 2-E Tier 2 — fleet orchestration (hub_orchestrator)
            Node(
                package="san_hub_orchestrator",
                executable="hub_orchestrator_node",
                name="hub_orchestrator_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_hub_orchestrator"),
                        "config", "hub_orchestrator.yaml",
                    ]),
                ] + common_params,
            ),

            # Phase 2-E Turn 8 (extended) — Threat alert aggregator
            # Dedups + escalates per-robot threat reports, publishes
            # hub-level stream for operator UI / degraded-mode routing.
            Node(
                package="san_hub_orchestrator",
                executable="threat_aggregator_node",
                name="threat_aggregator_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_hub_orchestrator"),
                        "config", "threat_aggregator.yaml",
                    ]),
                ] + common_params,
            ),

            # PDR-prep — SDD §7: 9 Formations + Hungarian slot assignment.
            # Publishes SlotAssignment, FollowerTargetMessage (P0 10 Hz),
            # FormationStatus (1 Hz). Hub-side authority. Owns KPP-1
            # (alignment ≤ 2m) and contributes to KPP-3/KPP-5.
            Node(
                package="san_formation",
                executable="formation_node",
                name="formation_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_formation"),
                        "config", "formation.yaml",
                    ]),
                ] + common_params,
            ),

            # PDR-prep — SDD §8: 360° Surveillance Sector + Pan-Tilt control.
            # Publishes SurveillanceSectorAssignment (per-robot, 10s + event)
            # and initial PanTiltCommand entry. Hub-side authority. Owns
            # KPP 팬틸트 추적 ≤ 0.05° (Track mode 정확도 검증).
            Node(
                package="san_surveillance",
                executable="surveillance_node",
                name="surveillance_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_surveillance"),
                        "config", "surveillance.yaml",
                    ]),
                ] + common_params,
            ),
        ]
    )

    # ─── Leader-only nodes (Unitree Go2 quadruped is Leader S1) ───────
    # Phase 2-E Turn 2: san_unitree_driver added.
    leader_only = GroupAction(
        condition=IfCondition(PythonExpression(
            ["'", robot_role, "' == 'leader'"])),
        actions=[
            LogInfo(msg=["[san_bringup] starting leader-only nodes "
                         "(Unitree Go2)"]),
            Node(
                package="san_unitree_driver",
                executable="unitree_go2_node",
                name="unitree_go2_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_unitree_driver"),
                        "config", "unitree_go2.yaml",
                    ]),
                ] + common_params,
            ),

            Node(
                package="san_formation",
                executable="tactical_leader_node",
                name="tactical_leader_node",
                output="screen",
                parameters=common_params,
                remappings=[
                    ('odom', '/odom'),
                    ('plan', '/plan'),
                    ('leader_state', '/leader_state'),
                    ('/swarm/waypoint_command', '/swarm/cmd/waypoint') 
                ]
            ),

            Node(
                package='human_detector',
                executable='swarm_lidar_filter',
                name='swarm_lidar_filter',
                output='screen',
                parameters=[{'stealth_radius': 1.0}],
                remappings=[
                    ('scan/points',         '/scan/points'),
                    ('scan/points_filtered','/scan/points_filtered'),
                    ('scan/ground_points',  '/scan/ground_points'),
                ],
            ),

        ]
    )

    # ─── Follower-only nodes (Tier FSM — KPP-2 measurement) ───────────
    # PDR-2: san_follower_tier provides the 6-state FSM (T0-T4) that
    # determines a follower's tracking behaviour. Required on every
    # follower for KPP-2 ≤ 300ms response verification.
    # PDR-5: san_reroute_planner adds T1.5 cost-map-based evasion
    # (the actual planner that emits /cmd_vel when obstacle detected).
    follower_only = GroupAction(
        condition=IfCondition(PythonExpression(
            ["'", robot_role, "' == 'follower'"])),
        actions=[
            LogInfo(msg=["[san_bringup] starting follower-only nodes "
                         "(Tier FSM + Reroute Planner)"]),
            Node(
                package="san_follower_tier",
                executable="tier_node",
                name="tier_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_follower_tier"),
                        "config", "tier.yaml",
                    ]),
                    # LaunchConfiguration values are strings by default; node
        # declarations for robot_id are `int`, so ros2launch raises
        # InvalidParameterTypeException ("setting it to {integer} is
        # not allowed" — local_slam_node / cost_map_node terminate()).
        # Use ParameterValue with value_type=int to coerce.
        {"robot_id": ParameterValue(robot_id, value_type=int)},
                ] + common_params,
                remappings=[
                    ("/swarm/formation/follower_target", "/swarm/formation/follower_target"),
                    ("/leader_state", "/leader_state"), # ADD THIS: Force look at global leader
                ],
            ),
            # PDR-5: Tier 1.5 AUTO_REROUTE planner — KPP-2 ≤ 300ms E2E.
            # Subscribes cost map + follower target, publishes
            # obstacle_on_path (→ tier_node) and /cmd_vel (when T1.5).
            Node(
                package="san_reroute_planner",
                executable="reroute_node",
                name="reroute_node",
                output="screen",
                parameters=[
                    PathJoinSubstitution([
                        FindPackageShare("san_reroute_planner"),
                        "config", "reroute.yaml",
                    ]),
                    # LaunchConfiguration values are strings by default; node
        # declarations for robot_id are `int`, so ros2launch raises
        # InvalidParameterTypeException ("setting it to {integer} is
        # not allowed" — local_slam_node / cost_map_node terminate()).
        # Use ParameterValue with value_type=int to coerce.
        {"robot_id": ParameterValue(robot_id, value_type=int)},
                ] + common_params,
                remappings=[
                    ("~/cmd_vel", "cmd_vel"),
                    ("/leader_state", "/leader_state"), # ADD THIS: Force look at global leader
                ],
            ),
        ]
    )

    # ─── Test-mode regression (deployment_mode in {bench, dev}) ───────
    # include_regression = 'auto' (default) → True iff deployment_mode
    # in {bench, dev}. Explicit override accepts 'true'/'false'.
    # Same inline-PythonExpression pattern as hub_only above.
    regression_only = GroupAction(
        condition=IfCondition(PythonExpression([
            "('", deployment_mode, "' in ['bench', 'dev']) if '",
            include_regression, "' == 'auto' else '",
            include_regression, "'.lower() == 'true'",
        ])),
        actions=[
            LogInfo(msg=["[san_bringup] starting regression node (test mode)"]),
            Node(
                package="san_l5_regression",
                executable="regression_main",
                name="l5_regression_node",
                output="screen",
                parameters=common_params,
            ),
        ]
    )

    # ─── Phase 2-E Turn 2-13 placeholders (filled by future turns) ────
    # Future packages joining this launch as they're ported:
    #   Turn 2: san_unitree_driver (Unitree Go2)
    #   Turn 3: lte_modem_node inside san_lte_redundancy
    #   Turn 4: san_rtk_gnss, san_ntrip_client
    #   Turn 5: san_imu_driver, san_slam_bridge
    #   Turn 6: san_camera_imx678, san_camera_thermal
    #   Turn 7: refined san_lidar wrapper
    #   Turn 8: san_mesh_comm, san_hub_ugv_adapter
    #   Turn 9-10:  san_mission (rclpy)
    #   Turn 11-12: san_perception (rclpy)
    #   Turn 13:    san_ble_control (rclpy) — removed in DCN-2026-008 v2

    # Block A: If robot_id is NOT 1, add a namespace (e.g. /robot_2)
    # namespaced_nodes = GroupAction(
    #     condition=IfCondition(PythonExpression(["'", robot_id, "' != '1'"])),
    #     actions=[
    #         PushRosNamespace(PythonExpression(["'robot_' + str('", robot_id, "')"])),
    #         always_on,
    #         hub_only,
    #         leader_only,
    #         follower_only,
    #         regression_only,
    #     ]
    # )

    # # Block B: If robot_id IS 1 (The Leader), launch everything GLOBALLY (no namespace)
    # global_nodes = GroupAction(
    #     condition=IfCondition(PythonExpression(["'", robot_id, "' == '1'"])),
    #     actions=[
    #         always_on,
    #         hub_only,
    #         leader_only,
    #         follower_only,
    #         regression_only,
    #     ]
    # )

    nav_share = get_package_share_directory("san_nav2")
    loc_share = get_package_share_directory("san_localization")

    namespaced_nodes = GroupAction(
        condition=IfCondition(PythonExpression(["'", robot_id, "' != '1'"])),
        actions=[
            PushRosNamespace(PythonExpression(["'robot_' + str('", robot_id, "')"])),
            always_on, hub_only, leader_only, follower_only, regression_only,
            
            # --- ADDED NAV2 AND LOCALIZATION FOR FOLLOWERS ---
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(os.path.join(loc_share, "launch", "localization.launch.py")),
                launch_arguments={
                    "use_sim_time": "true",
                    "namespace": PythonExpression(["'robot_' + str('", robot_id, "')"]),
                }.items(),
            ),



            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(os.path.join(nav_share, "launch", "nav2.launch.py")),
                launch_arguments={
                    "use_sim_time": "true",
                    "autostart": "true",
                    "namespace": PythonExpression(["'robot_' + str('", robot_id, "')"]),
                    "params_file": os.path.join(nav_share, "config", "nav2_follower_params.yaml"),
                }.items(),
            )

        ]
    )

    global_nodes = GroupAction(
        condition=IfCondition(PythonExpression(["'", robot_id, "' == '1'"])),
        actions=[
            always_on, hub_only, leader_only, follower_only, regression_only,

            # Node(
            #     package='tf2_ros',
            #     executable='static_transform_publisher',
            #     name='map_to_world',
            #     arguments=['0', '0', '0', '0', '0', '0', 'map', 'world']
            # ),
            
            # --- ADDED NAV2 AND LOCALIZATION FOR LEADER ---
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(os.path.join(loc_share, "launch", "localization.launch.py")),
                launch_arguments={"use_sim_time": "true", "namespace": ""}.items(),
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(os.path.join(nav_share, "launch", "nav2.launch.py")),
                launch_arguments={"use_sim_time": "true", "autostart": "true", "namespace": ""}.items(),
            )
        ]
    )
    # --- END OF MODIFICATION ---

    return LaunchDescription([
        _arg("robot_id", "1",
             "Robot ID in the squadron (1=Leader Go2, 2=Hub UGV, "
             "3=Deputy UGV, 4-8=Followers)"),
        _arg("robot_role", "leader",
             "leader|hub|deputy|follower — drives feature flags"),
        _arg("deployment_mode", "production",
             "production|training|bench|dev — OPS-SOP v1.5 §3.1"),
        _arg("hub_features", "auto",
             "Enable hub-only nodes ('auto' = robot_role=='hub'; true|false override)"),
        _arg("include_regression", "auto",
             "Include L5 regression node ('auto' = deployment_mode in {bench,dev})"),
        _arg("sbc_id", "-1",
             "Dual-SBC slot ID (-1=auto-detect via /etc/skyautonet/sbc_id, "
             "0=N/A, 1=primary, 2=secondary) — DCN-2026-011 D-032"),

        LogInfo(msg=[
            "[san_bringup] robot_id=", robot_id,
            " role=", robot_role,
            " mode=", deployment_mode,
        ]),

        # Add the two conditional blocks here
        namespaced_nodes,
        global_nodes
    ])
