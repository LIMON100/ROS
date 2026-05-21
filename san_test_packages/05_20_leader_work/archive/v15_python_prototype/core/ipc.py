"""
Inter-process communication.

Two channels:
  • mp.Queue: small messages (pose, status, commands, refs)
  • SharedMemory pool: large data (point clouds, encoded video)

Topic naming follows ROS 2 conventions for easy migration.
"""
from __future__ import annotations

import multiprocessing as mp
import queue as q
from dataclasses import dataclass


@dataclass
class TopicQueues:
    # ─ Sensor refs (point cloud / image data lives in SHM) ─
    lidar_ref: mp.Queue           # LidarScanRef
    imu: mp.Queue                 # ImuData (Go2 internal)
    imu_external: mp.Queue        # ImuData (separate IMU on RK3588 — higher quality)
    camera_ref: mp.Queue          # CameraFrameRef (Go2 front cam)
    imx678_ref: mp.Queue          # CameraFrameRef (Sony 4K on payload)
    # ─ Camera 3-way fan-out (per AIRYS pattern §1) ─
    # One V4L2 capture → three independent ref queues so AI / Stream / Display
    # threads stay independent. Backpressure on any one consumer doesn't
    # affect the other two. Streaming and display are optional consumers
    # (only present when WiFi is up / dev display attached).
    camera_ai_ref: mp.Queue       # CameraFrameRef → PerceptionProcess
    camera_stream_ref: mp.Queue   # CameraFrameRef → StreamingProcess (UDP/SRT)
    camera_display_ref: mp.Queue  # CameraFrameRef → DisplayProcess (dev only)
    camera_subscribers: mp.Queue  # {"action":"add"|"remove","consumer":"stream"|"display"}
    thermal_ref: mp.Queue         # ThermalFrameRef
    lrf: mp.Queue                 # LrfReading
    rtk: mp.Queue                 # RtkFix
    lte_status: mp.Queue          # LteStatus — cellular link health
    # ─ NTRIP / RTK split (SDD §3.2 lists NtripClient as its own process) ─
    rtcm_corrections: mp.Queue    # bytes — RTCM3 frame, NtripClient → RtkGnss serial
    gga_latest: mp.Queue          # str   — latest $GxGGA, RtkGnss → NtripClient (VRS)

    # ─ State ─
    pose: mp.Queue                # Pose6D from SLAM/Localization
    twist: mp.Queue               # Twist6D
    robot_status: mp.Queue        # RobotStatus
    localization_status: mp.Queue # LocalizationStatus — which source is active

    # ─ Map ─
    fused_tile: mp.Queue
    cumulative_update: mp.Queue   # for time-aware accumulation
    shared_map_out: mp.Queue      # SharedMapChunk — leader broadcasts to swarm
    shared_map_in: mp.Queue       # SharedMapChunk — follower receives from leader

    # ─ Mission/Control ─
    goal_pose: mp.Queue           # GoalPose → Go2 (Nav2 standard)
    cmd_vel: mp.Queue             # CmdVel fallback
    mission_state: mp.Queue       # mission state changes (for monitoring)

    # ─ Perception output ─
    anomaly: mp.Queue             # AnomalyEvent

    # ─ Safety ─
    safety_event: mp.Queue        # SafetyEvent (E1..E5)

    # ─ Control plane (BLE / WiFi / Streaming orchestration) ─
    ble_command: mp.Queue         # CMD opcode from app (uplink)
    ble_settings: mp.Queue        # SETTINGS write from app
    ble_phase: mp.Queue           # phase int → BLE STATE notification
    ble_creds: mp.Queue           # WIFI_CRED dict → BLE notification
    ble_errors: mp.Queue          # error_code int → BLE ERROR notification
    wifi_request: mp.Queue        # {"action":"up"|"down"}
    wifi_progress: mp.Queue       # {"pct": int, "phase": str}
    stream_request: mp.Queue      # {"action":"start"|"stop", ...}
    stream_status: mp.Queue       # {"playing": bool, "frames_pushed":..., ...}
    app_rpc: mp.Queue             # JSON-RPC from app via WS (method/params)
    ws_phase: mp.Queue            # phase byte → WsTelemetry (shadow of ble_phase)
    # ─ WS broadcast (Patrol-Server-deferred mode, SDD Rev.A.5) ─
    # When comm.enable_patrol_server is false, AnomalyEvent + heartbeat
    # snapshots are redirected to these queues for ws:// fan-out to the
    # operator app, instead of being POSTed to a remote server.
    ws_anomaly: mp.Queue          # AnomalyEvent → ws "type":"anomaly"
    ws_heartbeat: mp.Queue        # dict snapshot → ws "type":"heartbeat"
    # ─ Swarm topics (SDD Rev.A.5 §6.7, §7.3) ─
    sw_breadcrumb: mp.Queue       # BreadcrumbPoint published by leader
    sw_follower_target: mp.Queue  # FollowerTargetMessage (1 s lookahead, 10 Hz)
    sw_follower_map: mp.Queue     # MapTile from each follower's local SLAM
    sw_shared_map: mp.Queue       # Hub-fused MapTile broadcast to all robots
    sw_tier: mp.Queue             # TierEvent (T0/T1.5/T1..T4 transitions)
    sw_election_state: mp.Queue   # Modified Raft elected_leader_id transitions
    sw_sector_assign: mp.Queue    # SectorAssign — leader → robot surveillance sector (P1)
    pantilt_command: mp.Queue     # PanTiltCommand — robot-internal pan-tilt head cmd
    hub_aggregated_map: mp.Queue  # AggregatedMap — Hub UGV SBC#1 → swarm (P2, 30–60s)
    slam_local_delta: mp.Queue    # SLAMLocalDelta — follower → Hub (P2, replaces v1.0 SLAMDelta)
    tablet_video_request: mp.Queue   # VideoRequest — Android APP → robot (P1, RELIABLE)
    tablet_video_response: mp.Queue  # VideoResponse — robot → Android APP (P1, RELIABLE)
    hub_threat_alert: mp.Queue    # ThreatAlert — Hub dual-SBC peer-failure / degraded-mode events
    hub_swarm_health_summary: mp.Queue  # SwarmHealthSummary — Hub dual-SBC rollup, transitions only
    hub_mesh_status: mp.Queue     # MeshStatus — OpenWrt batctl + mwan3 rollup (P1, ~5s)
    emergency_alert: mp.Queue     # EmergencyAlert — cliff / rollover / collision interrupts
    swarm_health: mp.Queue        # LeaderRollbackChecker → WsTelemetry health snapshot
    # ─ Audit / permission control plane (SDD Rev.A.6 §9.7) ─
    # `audit_event` is the system-wide audit bus. Any process publishes dicts
    # shaped per core.audit_log.publish_audit(); a thread in main drains and
    # writes through a single AuditLogger so the hash chain is consistent.
    # `auth_state` carries PIN-auth transitions from BleControlProcess so
    # downstream gates (Mission, Geofence override, BatteryMonitor dev_override)
    # can observe whether the operator is currently authenticated.
    audit_event: mp.Queue         # dict {category, event, actor, params, ...}
    auth_state: mp.Queue          # dict {authenticated: bool, ts_mono: float, reason: str}
    # SAN v1.3 §11: 1 Hz heartbeat carrying deployment_mode, robot_id mapping,
    # deputy_chain, alive-follower count. Consumed by WS telemetry / audit /
    # debug dashboard so the active policy is observable without re-parsing
    # the yaml overlay tree.
    operation_state: mp.Queue     # OperationState
    # SAN v1.3 §6.4: 1 Hz composited 4-layer cost map (obstacle +
    # traversability + inflation + static). Consumed by Mission (Tier 1.5
    # reroute), WS telemetry (operator overlay), recordings.
    cost_map_update: mp.Queue     # CostMapUpdate
    # SAN v1.3 §6.4: operator-visible alert bus. Distinct from emergency_alert
    # (which triggers immediate halt) — operator_alert raises a screen
    # notification asking a human to make a call (e.g. avoidance failed 3×
    # in a row, please intervene).
    operator_alert: mp.Queue      # OperatorAlert


# Per-topic maxsize defaults. publish() drops the oldest message on
# overflow (sliding-window real-time semantics), so a larger maxsize on
# a high-rate sensor topic just lets a slow consumer catch up further
# back in time before samples start being dropped — the publisher is
# never blocked. Sized for the rates documented in README §2.
TOPIC_MAXSIZE = {
    # High-rate sensors: ~50 = 0.25 s window at 200 Hz, 0.5 s at 100 Hz
    "imu":              50,
    "imu_external":     50,
    "lidar_ref":        30,
    # State at ~100 Hz pose / ~50 Hz twist
    "pose":             30,
    "twist":            30,
    "localization_status": 30,
    # Camera fan-out — bursty, multiple consumers
    "camera_ref":       20,
    "imx678_ref":       20,
    "camera_ai_ref":    20,
    "camera_stream_ref": 20,
    "camera_display_ref": 20,
    "thermal_ref":      20,
    # NTRIP — RTCM frames arrive in bursts when corrections refresh; GGA
    # is once per ~10 s. Small queues since both are low-rate.
    "rtcm_corrections": 32,
    "gga_latest":       4,
    # Audit bus — low-rate but bursty (a mission start emits several in
    # quick succession). Oversize so a transient writer-thread stall
    # doesn't drop security-relevant events to the publish() ring eviction.
    "audit_event":      64,
    # Sector assignment — leader broadcasts one msg per follower on each
    # 10s/event tick. Sized to hold a full swarm (≤32 robots) burst before
    # any consumer wakes.
    "sw_sector_assign": 32,
    # Pan-tilt command — emitted on each sector update + on threat track.
    # A small queue is plenty: the consumer is the gimbal driver, and the
    # latest command always supersedes older ones (drop-oldest is fine).
    "pantilt_command": 8,
    # Aggregated map — low rate (30–60 s) but messages can be tens of KB
    # of PNG. Small queue: a follower that misses one will get the next
    # within the broadcast period anyway.
    "hub_aggregated_map": 4,
    # SLAM local-delta — same cadence as the Hub aggregated map but
    # received from N followers, so the Hub queue needs N slots.
    "slam_local_delta": 16,
    # Tablet video request — user-driven, event-rate. A small queue is
    # plenty; rapid tap-spamming the start/stop button just slides the
    # oldest pending request out.
    "tablet_video_request": 8,
    # Video response — event-driven on streaming-state transitions only.
    "tablet_video_response": 4,
    # Threat alerts — bursty on a single SBC outage (one alert per peer
    # gone silent) but otherwise rare.
    "hub_threat_alert": 8,
    # SBC failure rollup — one publish per transition (timeout or
    # recovery), so the queue stays small. Sized to absorb a burst if
    # both SBCs flap simultaneously.
    "hub_swarm_health_summary": 8,
    # Mesh status — one publish every ~5 s from the Hub's MeshMonitor.
    # 8 slots = 40 s of history if the consumer is briefly stalled.
    "hub_mesh_status": 8,
    # Emergency alerts — extremely rare (cliff/rollover/collision). A
    # tiny queue is right: an unconsumed alert is a bug, not capacity.
    "emergency_alert": 4,
    # OperationState heartbeat — exactly 1 Hz publish from main(). A small
    # queue is right because each new heartbeat supersedes the previous.
    "operation_state": 4,
    # Cost map — 1 Hz; each new map supersedes the previous.
    "cost_map_update": 4,
    # Operator alerts — event-rate (avoidance failure, …). Small queue is
    # right since an unconsumed alert is a bug, not capacity.
    "operator_alert": 8,
}
DEFAULT_MAXSIZE = 20


def make_topic_queues(
    maxsize: int = DEFAULT_MAXSIZE,
    overrides: dict | None = None,
) -> TopicQueues:
    """Construct the TopicQueues bundle.

    `maxsize` is the fallback for any topic not present in TOPIC_MAXSIZE
    (or in `overrides`). High-rate sensor topics get bumped via
    TOPIC_MAXSIZE so a momentarily-stalled consumer doesn't lose 200 Hz
    IMU history within ~0.1 s.
    """
    sizes = dict(TOPIC_MAXSIZE)
    if overrides:
        sizes.update(overrides)

    def Q(name: str) -> mp.Queue:
        return mp.Queue(maxsize=sizes.get(name, maxsize))

    return TopicQueues(
        lidar_ref=Q("lidar_ref"),
        imu=Q("imu"),
        imu_external=Q("imu_external"),
        camera_ref=Q("camera_ref"),
        imx678_ref=Q("imx678_ref"),
        camera_ai_ref=Q("camera_ai_ref"),
        camera_stream_ref=Q("camera_stream_ref"),
        camera_display_ref=Q("camera_display_ref"),
        camera_subscribers=Q("camera_subscribers"),
        thermal_ref=Q("thermal_ref"),
        lrf=Q("lrf"),
        rtk=Q("rtk"),
        lte_status=Q("lte_status"),
        rtcm_corrections=Q("rtcm_corrections"),
        gga_latest=Q("gga_latest"),
        pose=Q("pose"),
        twist=Q("twist"),
        robot_status=Q("robot_status"),
        localization_status=Q("localization_status"),
        fused_tile=Q("fused_tile"),
        cumulative_update=Q("cumulative_update"),
        shared_map_out=Q("shared_map_out"),
        shared_map_in=Q("shared_map_in"),
        goal_pose=Q("goal_pose"),
        cmd_vel=Q("cmd_vel"),
        mission_state=Q("mission_state"),
        anomaly=Q("anomaly"),
        safety_event=Q("safety_event"),
        ble_command=Q("ble_command"),
        ble_settings=Q("ble_settings"),
        ble_phase=Q("ble_phase"),
        ble_creds=Q("ble_creds"),
        ble_errors=Q("ble_errors"),
        wifi_request=Q("wifi_request"),
        wifi_progress=Q("wifi_progress"),
        stream_request=Q("stream_request"),
        stream_status=Q("stream_status"),
        app_rpc=Q("app_rpc"),
        ws_phase=Q("ws_phase"),
        ws_anomaly=Q("ws_anomaly"),
        ws_heartbeat=Q("ws_heartbeat"),
        sw_breadcrumb=Q("sw_breadcrumb"),
        sw_follower_target=Q("sw_follower_target"),
        sw_follower_map=Q("sw_follower_map"),
        sw_shared_map=Q("sw_shared_map"),
        sw_tier=Q("sw_tier"),
        sw_election_state=Q("sw_election_state"),
        sw_sector_assign=Q("sw_sector_assign"),
        pantilt_command=Q("pantilt_command"),
        hub_aggregated_map=Q("hub_aggregated_map"),
        slam_local_delta=Q("slam_local_delta"),
        tablet_video_request=Q("tablet_video_request"),
        tablet_video_response=Q("tablet_video_response"),
        hub_threat_alert=Q("hub_threat_alert"),
        hub_swarm_health_summary=Q("hub_swarm_health_summary"),
        hub_mesh_status=Q("hub_mesh_status"),
        emergency_alert=Q("emergency_alert"),
        swarm_health=Q("swarm_health"),
        audit_event=Q("audit_event"),
        auth_state=Q("auth_state"),
        operation_state=Q("operation_state"),
        cost_map_update=Q("cost_map_update"),
        operator_alert=Q("operator_alert"),
    )


def publish(queue: mp.Queue, msg) -> bool:
    """Real-time semantics: full → drop oldest, push new.

    When schema-check is enabled (debug mode), the message's dataclass
    fields are validated against their annotations. A mismatch raises
    RuntimeError synchronously — the offending step() loop catches it
    and a crash dump is written.
    """
    # Lazy import to avoid circular dep (diag → core, this is core)
    from core.diag import schema_check_enabled, validate_message
    if schema_check_enabled():
        validate_message(msg)
    try:
        queue.put_nowait(msg)
        return True
    except q.Full:
        try:
            _ = queue.get_nowait()
            queue.put_nowait(msg)
        except (q.Empty, q.Full):
            pass
        return False


def consume(queue: mp.Queue, timeout: float = 0.05):
    try:
        return queue.get(timeout=timeout)
    except q.Empty:
        return None


def drain(queue: mp.Queue, max_items: int = 100) -> list:
    """Drain up to max_items messages (for batch consumers)."""
    out = []
    for _ in range(max_items):
        try:
            out.append(queue.get_nowait())
        except q.Empty:
            break
    return out
