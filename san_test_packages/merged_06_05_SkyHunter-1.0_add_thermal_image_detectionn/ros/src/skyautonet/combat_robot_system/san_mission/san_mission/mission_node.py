# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E Turn 9-10 — Mission rclpy node.

⭐ FIRST rclpy NODE — establishes the Tier 2 Python pattern per
DCN-2026-002 D-007. Subsequent rclpy nodes (Perception Turn 11-12 and
subsequent Tier 2 nodes per DCN-2026-002 D-007) follow this template.

Replaces mission/mission_process.py from the legacy Python prototype.
Subscribes:
  * /robot_N/<this>/pose            (geometry_msgs/PoseStamped)
  * /robot_N/<this>/robot_status    (combat_robot_msgs/RobotStatus)
Publishes:
  * ~/goal_pose                     (geometry_msgs/PoseStamped)
  * ~/mission_state                 (std_msgs/String — JSON)

Behavior tree ticked at 5 Hz (per Python prototype). Tree is built
from san_mission.mission_context.build_patrol_tree(); operators can
swap trees via parameters in future iterations.

PATCH 2026-05-13 (san_mission deep-dive review):
  * C2 — tick_hz must be ≥ 1.0 (was ≤ 100; 0.5 caused
    ZeroDivisionError on every tick from `tick_count % int(tick_hz)`).
  * C3 — goal_xy is cleared at the start of each tick AND only
    published when the BT returns non-FAILURE this tick.
  * C4 — every subscription callback acquires ctx.lock so multi-field
    priority updates are atomic.
  * C6 — emergency release is a 2-step handshake: subscription clears
    emergency_active, BT just observes. BT no longer mutates the
    field the subscription owns.
  * C7 — EmergencyStop.SCOPE_SINGLE_ROBOT now compares msg.target_robot_id
    against the new robot_id parameter. The old "always apply" was
    a real safety problem (single-robot stop stopped the swarm).
  * M12 — tick_hz upper bound dropped from 100 to 50 (BT runs as
    plain Python; 50 Hz is already pushing it).
"""
from __future__ import annotations

import json
import math
from typing import Optional

import rclpy

# combat_robot_msgs import — must come from generated message package
from combat_robot_msgs.action import ReturnToHome
from combat_robot_msgs.msg import (
    EmergencyStop,
    HubRoleAnnouncement,
    ManualOverrideCommand,
    OperatorHeartbeat,
    RobotStatus,
    SwarmHealthSummary,
    TierStatusChange,
)
from geometry_msgs.msg import PoseStamped, Twist
from rclpy.action import ActionClient
from rclpy.node import Node
from rclpy.qos import (
    QoSDurabilityPolicy,
    QoSHistoryPolicy,
    QoSProfile,
    QoSReliabilityPolicy,
)
from std_msgs.msg import String

from san_mission.behavior_tree import Status
from san_mission.mission_context import MissionContext, build_patrol_tree
from san_mission.mission_bt import (
    ExtendedMissionContext, build_mission_tree,
)
from san_mission.operational_modes import OperationalMode


class MissionNode(Node):
    def __init__(self):
        super().__init__("mission_node")

        # ─── Parameters ────────────────────────────────────────────
        self.declare_parameter("tick_hz",                5.0)
        self.declare_parameter("min_battery_percent",   15.0)
        self.declare_parameter("initial_mode",          "recon")
        self.declare_parameter("frame_id",              "map")
        # PDR-4: select tree topology. "fallback" = SDD §6.1 Fallback
        # root with 5 priority subtrees; "patrol" = legacy patrol tree.
        self.declare_parameter("tree_type",        "fallback")
        self.declare_parameter("robot_role",       "follower")
        # PATCH 2026-05-13 (C7): this node's robot_id so scope checks
        # on EmergencyStop / ManualOverride actually work for
        # SCOPE_SINGLE_ROBOT.
        self.declare_parameter("robot_id",         0)
        # IDS §3.8 operator deadman. Timeout default 30 s (NOT the msg
        # doc's 3 s): the App reconnect backoff tops out at 30 s, so a
        # shorter window would false-trip OPERATOR_LOST on every reconnect
        # (SAN-WIFI-OPSPEC-001 §3 D-WIFI-002).
        self.declare_parameter("operator_deadman_enabled",        True)
        self.declare_parameter("operator_heartbeat_timeout_sec", 30.0)

        self._tick_hz = float(
            self.get_parameter("tick_hz").value)
        self._min_battery = float(
            self.get_parameter("min_battery_percent").value)
        self._frame_id = str(
            self.get_parameter("frame_id").value)
        # ★ PATCH 2026-05-13 (C2, M12): tick_hz must be ≥ 1.0
        # (the periodic state broadcast `tick_count % int(tick_hz)`
        # divides by zero otherwise), and bounded above for sanity.
        if self._tick_hz < 1.0 or self._tick_hz > 50.0:
            raise ValueError(
                f"MissionNode: tick_hz out of range [1.0, 50.0]: "
                f"{self._tick_hz}")
        self._robot_id = int(self.get_parameter("robot_id").value)

        # IDS §3.8 operator deadman state.
        self._operator_deadman_enabled = bool(
            self.get_parameter("operator_deadman_enabled").value)
        self._operator_timeout_ms = float(
            self.get_parameter("operator_heartbeat_timeout_sec").value) * 1000.0
        self._last_operator_hb_ms = 0
        self._operator_seen = False   # arm only after the first heartbeat

        tree_type = str(self.get_parameter("tree_type").value)
        robot_role = str(self.get_parameter("robot_role").value)

        # ─── State ─────────────────────────────────────────────────
        if tree_type == "fallback":
            # PDR-4: SDD §6.1 Fallback root
            self._ctx = ExtendedMissionContext()
            self._ctx.priority.robot_role = robot_role
            self._tree = build_mission_tree()
            self.get_logger().info(
                f"MissionNode: tree=fallback (SDD §6.1) role={robot_role}"
            )
        else:
            # Legacy patrol tree (kept for backward compatibility)
            self._ctx = MissionContext()
            self._tree = build_patrol_tree(
                min_battery_percent=self._min_battery)
            self.get_logger().info(
                "MissionNode: tree=patrol (legacy)"
            )

        try:
            initial_mode = OperationalMode(
                str(self.get_parameter("initial_mode").value))
            ok, _ = self._ctx.mode.request_mode(initial_mode)
            if not ok:
                # DEV_TEST without PIN — fall back to RECON
                self._ctx.mode.request_mode(OperationalMode.RECON)
        except ValueError:
            self._ctx.mode.request_mode(OperationalMode.RECON)

        # ─── QoS profiles (canonical Tier 2 rclpy) ─────────────────
        sensor_qos = QoSProfile(
            history=QoSHistoryPolicy.KEEP_LAST, depth=5,
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        reliable_qos = QoSProfile(
            history=QoSHistoryPolicy.KEEP_LAST, depth=10,
            reliability=QoSReliabilityPolicy.RELIABLE,
            durability=QoSDurabilityPolicy.VOLATILE,
        )

        # ─── Subscribers ───────────────────────────────────────────
        self._pose_sub = self.create_subscription(
            PoseStamped, "pose", self._on_pose, sensor_qos)
        self._status_sub = self.create_subscription(
            RobotStatus, "robot_status", self._on_status, reliable_qos)

        # PDR-7a: subscriptions wiring the ExtendedMissionContext's
        # PriorityState fields. Without these, the BT Fallback subtrees
        # (P0-P3) can never fire — they remain in 'never triggered'
        # state forever.
        self._using_fallback = (tree_type == "fallback")
        if self._using_fallback:
            self._estop_sub = self.create_subscription(
                EmergencyStop,
                "emergency_stop",
                self._on_emergency_stop, reliable_qos)
            self._manual_sub = self.create_subscription(
                ManualOverrideCommand,
                "manual_override",
                self._on_manual_override, reliable_qos)
            self._health_sub = self.create_subscription(
                SwarmHealthSummary,
                "/swarm/health_summary",
                self._on_health, reliable_qos)
            self._tier_sub = self.create_subscription(
                TierStatusChange,
                "tier_status_change",
                self._on_tier_status, reliable_qos)
            self._hub_role_sub = self.create_subscription(
                HubRoleAnnouncement,
                "/swarm/hub_role_announcement",
                self._on_hub_role, reliable_qos)
            # IDS §3.8 operator deadman (BEST_EFFORT, like the App's 1 Hz
            # heartbeat). Absence of this message past the timeout = lost.
            self._operator_hb_sub = self.create_subscription(
                OperatorHeartbeat,
                "/operator/heartbeat",
                self._on_operator_heartbeat, sensor_qos)

        # ─── Publishers ────────────────────────────────────────────
        self._goal_pub = self.create_publisher(
            PoseStamped, "~/goal_pose", reliable_qos)
        self._mission_state_pub = self.create_publisher(
            String, "~/mission_state", reliable_qos)

        # PDR-7a: manual cmd_vel passthrough + emergency stop output.
        # P1 ManualOverride forwards operator's cmd_vel; P0 EmergencyStop
        # publishes a zero Twist.
        if self._using_fallback:
            self._cmd_vel_pub = self.create_publisher(
                Twist, "~/cmd_vel", reliable_qos)

        # ─── DCN-2026-017 + ADR-008: /rth action client ───────────
        # Tier 2 Python BT delegates Return-to-Home to the Tier 1 C++
        # action server (san_rth). The BT leaf calls the trigger via
        # ctx.priority.rth_send_callable; result callbacks populate
        # rth_last_result so the next tick can resolve.
        if self._using_fallback:
            self._rth_client = ActionClient(self, ReturnToHome, "/rth")
            self._rth_goal_handle = None
            self._ctx.priority.rth_send_callable = self._trigger_rth

        # ─── Timer ─────────────────────────────────────────────────
        self._timer = self.create_timer(
            1.0 / self._tick_hz, self._on_tick)

        self.get_logger().info(
            f"MissionNode UP: tick={self._tick_hz} Hz "
            f"min_battery={self._min_battery}% "
            f"mode={self._ctx.mode.current.value}")

    # ─── Subscribers (PATCH 2026-05-13 C4: all under ctx.lock) ─────

    def _on_pose(self, msg: PoseStamped) -> None:
        # R-13 + Phase 7: pose_xy and yaw_rad are two related fields; a
        # BT tick reading them between the two writes would observe pose
        # from msg N with yaw from msg N+1. Compute yaw outside the lock
        # to keep lock scope small, then write both fields atomically.
        q = msg.pose.orientation
        yaw = math.atan2(
            2.0 * (q.w * q.z + q.x * q.y),
            1.0 - 2.0 * (q.y * q.y + q.z * q.z),
        )
        with self._ctx.lock:
            self._ctx.pose_xy = (msg.pose.position.x, msg.pose.position.y)
            self._ctx.yaw_rad = yaw

    def _on_status(self, msg: RobotStatus) -> None:
        with self._ctx.lock:
            self._ctx.battery_percent = float(msg.battery_percent)
            self._ctx.in_limp_mode    = bool(msg.in_limp_mode)
            self._ctx.slam_healthy    = bool(msg.slam_healthy)
            self._ctx.is_leader       = bool(msg.is_leader_role_active)

    # ─── PDR-7a: PriorityState callbacks ───────────────────────────
    # Each populates the ExtendedMissionContext.priority fields so the
    # SDD §6.1 Fallback root subtrees can fire as designed.

    def _on_emergency_stop(self, msg: EmergencyStop) -> None:
        """P0 driver. EmergencyStop msg targets us (scope) → activate
        emergency mode. Release comes via ManualOverrideCommand
        OVERRIDE_RELEASE.

        PATCH 2026-05-13 (C7): SCOPE_SINGLE_ROBOT now compares
        msg.target_robot_id to our robot_id parameter. Previously
        it applied unconditionally — a single-robot stop stopped
        the entire swarm.
        """
        if not self._using_fallback:
            return
        if msg.scope == EmergencyStop.SCOPE_ALL_ROBOTS:
            apply = True
        elif msg.scope == EmergencyStop.SCOPE_LEADER_ONLY:
            apply = (self._ctx.priority.robot_role == "leader")
        elif msg.scope == EmergencyStop.SCOPE_SINGLE_ROBOT:
            # ★ PATCH 2026-05-13 (C7): explicit robot_id match.
            apply = (self._robot_id != 0
                     and int(msg.target_robot_id) == self._robot_id)
        else:
            apply = False
        if apply:
            with self._ctx.lock:
                self._ctx.priority.emergency_active = True
                # Clear any pending release; this is a fresh emergency.
                self._ctx.priority.emergency_release_armed = False
            self.get_logger().warn(
                f"EMERGENCY STOP activated "
                f"(reason='{msg.reason}' op='{msg.operator_id}' "
                f"scope={msg.scope})")

    def _on_manual_override(self, msg: ManualOverrideCommand) -> None:
        """P1 driver. override_type controls semantics:
            CMD_VEL → manual mode + forward cmd_vel
            HALT    → stop (effectively P0-like)
            RETURN  → trigger RTH path (health_critical proxy)
            RELEASE → exit manual + clear emergency (atomic handshake)

        PATCH 2026-05-13 (C4): all field updates atomic under ctx.lock.
        PATCH 2026-05-13 (C6): OVERRIDE_RELEASE clears emergency_active
        AND arms emergency_release_armed atomically. The BT side
        (_wait_for_release) is now a pure observer; only the subscription
        writes to these fields.
        """
        if not self._using_fallback:
            return

        ot = msg.override_type
        # R-13 (C4/C6): all writes inside one ctx.lock so the BT never
        # observes a half-updated state. Includes the DCN-2026-017
        # OVERRIDE_RELEASE → RTH-state reset from main (lock-wrapped
        # for the same reason).
        with self._ctx.lock:
            if ot == ManualOverrideCommand.OVERRIDE_CMD_VEL:
                self._ctx.priority.manual_mode_active = True
                self._ctx.priority.manual_cmd_vel = (
                    float(msg.cmd_vel.linear.x),
                    float(msg.cmd_vel.angular.z),
                )
            elif ot == ManualOverrideCommand.OVERRIDE_HALT:
                self._ctx.priority.manual_mode_active = True
                self._ctx.priority.manual_cmd_vel = (0.0, 0.0)
            elif ot == ManualOverrideCommand.OVERRIDE_RETURN:
                # Treat as health-critical RTH (P2)
                self._ctx.priority.health_critical = True
                self._ctx.priority.rth_destination_known = True
                self._ctx.priority.manual_mode_active = False
                self._ctx.priority.manual_cmd_vel = None
            elif ot == ManualOverrideCommand.OVERRIDE_RELEASE:
                # R-13 (C6) atomic handshake + main DCN-2026-017 RTH reset.
                # Clear manual + emergency + RTH state together so a
                # future degraded episode (or RETURN) can re-arm cleanly.
                self._ctx.priority.manual_mode_active = False
                self._ctx.priority.manual_cmd_vel = None
                self._ctx.priority.health_critical = False
                self._ctx.priority.emergency_active = False
                self._ctx.priority.emergency_release_armed = True
                self._ctx.priority.rth_in_flight = False
                self._ctx.priority.rth_last_result = None

    def _on_health(self, msg: SwarmHealthSummary) -> None:
        """P2 driver. Critical when either SBC has failed."""
        if not self._using_fallback:
            return
        new_critical = bool(msg.slam_sbc_failed or msg.comm_sbc_failed)
        # R-13 (C4) lock + DCN-2026-017 falling-edge RTH reset: on
        # critical → recovered transition, clear stale RTH state so a
        # future degraded episode can re-arm /rth cleanly. All under
        # one lock so the BT observes the transition atomically.
        with self._ctx.lock:
            if self._ctx.priority.health_critical and not new_critical:
                self._ctx.priority.rth_in_flight = False
                self._ctx.priority.rth_last_result = None
            self._ctx.priority.health_critical = new_critical

    def _now_ms(self) -> int:
        return int(self.get_clock().now().nanoseconds // 1_000_000)

    def _on_operator_heartbeat(self, msg: OperatorHeartbeat) -> None:
        """IDS §3.8 deadman feed. Record arrival; the per-tick check
        flips operator_lost when this goes stale past the timeout."""
        if not self._using_fallback:
            return
        with self._ctx.lock:
            self._last_operator_hb_ms = self._now_ms()
            self._operator_seen = True

    def _evaluate_operator_deadman(self) -> None:
        """OperatorHeartbeat deadman (IDS §3.8). Call holding self._ctx.lock.

        Arms only after the first heartbeat (a robot with no operator yet
        connected is not "lost"). On the lost rising edge: route to P2
        RTH-or-stand via priority.operator_lost, AND drop any stale manual
        override so a vanished operator's last manual command cannot shadow
        the failsafe. On the recovered edge: clear RTH state so a future
        loss re-arms /rth cleanly (mirrors _on_health's falling edge).
        """
        if not (self._using_fallback and self._operator_deadman_enabled):
            return
        if not self._operator_seen:
            return
        age_ms = self._now_ms() - self._last_operator_hb_ms
        lost = age_ms > self._operator_timeout_ms
        if lost == self._ctx.priority.operator_lost:
            return
        if lost:
            self._ctx.priority.manual_mode_active = False
            self._ctx.priority.manual_cmd_vel = None
            self.get_logger().warn(
                f"OPERATOR_LOST: no heartbeat for {age_ms / 1000.0:.1f}s "
                f"(> {self._operator_timeout_ms / 1000.0:.0f}s) — engaging "
                f"P2 return-to-home / stand failsafe")
        else:
            self._ctx.priority.rth_in_flight = False
            self._ctx.priority.rth_last_result = None
            self.get_logger().info("OPERATOR_RECOVERED: heartbeat resumed")
        self._ctx.priority.operator_lost = lost

    def _on_tier_status(self, msg: TierStatusChange) -> None:
        """Informational — feed current tier into BT context for
        ApplyTierAwareMotionLimit action."""
        if not self._using_fallback:
            return
        with self._ctx.lock:
            self._ctx.priority.current_tier = int(msg.current_tier)

    def _on_hub_role(self, msg: HubRoleAnnouncement) -> None:
        """P3 driver. PATCH 2026-05-13 (C4): all updates under lock."""
        if not self._using_fallback:
            return
        is_announced_hub = (
            msg.role in (HubRoleAnnouncement.HUB_NORMAL,
                          HubRoleAnnouncement.HUB_PROMOTED))
        with self._ctx.lock:
            self._ctx.priority.is_hub = (
                self._ctx.priority.robot_role == "hub")
            self._ctx.priority.hub_takeover_available = (
                is_announced_hub and
                bool(msg.lte_active) and
                bool(msg.slam_aggregation_active) and
                bool(msg.video_relay_active))

    # ─── DCN-2026-017: /rth action delegation ──────────────────────

    def _trigger_rth(self, reset_home: bool) -> None:
        """Send a goal to the /rth action server (san_rth).

        Called by mission_bt._return_to_home_or_stand via the
        ctx.priority.rth_send_callable hook. Non-blocking: the result
        propagates back to ctx.priority.rth_last_result through
        _on_rth_response → _on_rth_result callbacks, so the next BT
        tick resolves to SUCCESS/FAILURE.
        """
        if not self._rth_client.server_is_ready():
            # Server not up yet — surface as TIMEOUT so BT can fall
            # back to stand on next tick rather than spin RUNNING.
            self.get_logger().warn(
                "/rth action server not ready; reporting TIMEOUT")
            with self._ctx.lock:    # P1-4 fix
                self._ctx.priority.rth_in_flight = False
                self._ctx.priority.rth_last_result = "TIMEOUT"
            return

        goal = ReturnToHome.Goal()
        goal.reset_home_pose = bool(reset_home)
        future = self._rth_client.send_goal_async(goal)
        future.add_done_callback(self._on_rth_response)
        self.get_logger().info(
            f"RTH goal dispatched (reset_home={reset_home})")

    def _on_rth_response(self, future) -> None:
        """ActionClient send_goal_async response callback.

        P1-4 fix: all priority writes under ctx.lock so the BT tick
        (which reads rth_in_flight + rth_last_result in the P2 leaf)
        observes a consistent state. rclpy executor threads may
        interleave with the timer callback under MultiThreadedExecutor
        (and the lock is also defensive under SingleThreadedExecutor —
        cheap insurance).
        """
        try:
            goal_handle = future.result()
        except Exception as exc:                # noqa: BLE001
            self.get_logger().error(f"RTH goal exception: {exc}")
            with self._ctx.lock:    # P1-4 fix
                self._ctx.priority.rth_in_flight = False
                self._ctx.priority.rth_last_result = "CANCELLED"
            return

        if not goal_handle.accepted:
            self.get_logger().warn("RTH goal rejected by san_rth")
            with self._ctx.lock:    # P1-4 fix
                self._ctx.priority.rth_in_flight = False
                self._ctx.priority.rth_last_result = "CANCELLED"
            return

        self._rth_goal_handle = goal_handle
        result_future = goal_handle.get_result_async()
        result_future.add_done_callback(self._on_rth_result)

    def _on_rth_result(self, future) -> None:
        """ActionClient get_result_async callback.

        P1-4 fix: priority writes under ctx.lock — same rationale as
        _on_rth_response above.
        """
        try:
            result = future.result().result
        except Exception as exc:                # noqa: BLE001
            self.get_logger().error(f"RTH result exception: {exc}")
            with self._ctx.lock:    # P1-4 fix
                self._ctx.priority.rth_in_flight = False
                self._ctx.priority.rth_last_result = "CANCELLED"
            return

        reason = str(result.termination_reason) or (
            "OK" if result.success else "TIMEOUT")
        with self._ctx.lock:        # P1-4 fix
            self._ctx.priority.rth_in_flight = False
            self._ctx.priority.rth_last_result = reason
        self.get_logger().info(
            f"RTH completed: success={result.success} "
            f"reason={reason} dist={result.final_distance_m:.2f}m "
            f"yaw_err={result.final_yaw_error_rad:.3f}rad")

    # ─── Tick ──────────────────────────────────────────────────────

    def _on_tick(self) -> None:
        """One BT tick.

        PATCH 2026-05-13 (C3): goal_xy/goal_yaw_rad are cleared BEFORE
        the BT runs, and the goal is published only if (a) the BT
        set it this tick (goal_xy is not None) AND (b) the BT didn't
        return FAILURE. Previously, a stale goal from a previous
        successful tick was re-published every tick regardless.

        PATCH 2026-05-13 (C4): callbacks acquire ctx.lock; the BT
        tick reads under the same lock so it never sees torn state.
        """
        # ★ PATCH 2026-05-13 (C3): clear goal before tick.
        with self._ctx.lock:
            self._ctx.goal_xy      = None
            self._ctx.goal_yaw_rad = None
            self._ctx.tick_count  += 1
            # IDS §3.8 operator deadman: flip operator_lost (→ P2 RTH)
            # before the BT reads priority state this tick.
            self._evaluate_operator_deadman()
            status = self._tree.tick(self._ctx)
            # Snapshot what we need to publish (release lock before I/O).
            snapshot_goal_xy      = self._ctx.goal_xy
            snapshot_goal_yaw     = self._ctx.goal_yaw_rad
            snapshot_emergency    = (
                self._ctx.priority.emergency_active
                if self._using_fallback else False)
            snapshot_manual_active = (
                self._ctx.priority.manual_mode_active
                if self._using_fallback else False)
            snapshot_manual_cmd   = (
                self._ctx.priority.manual_cmd_vel
                if self._using_fallback else None)
            snapshot_tick         = self._ctx.tick_count
            snapshot_mode         = self._ctx.mode.current.value
            snapshot_is_leader    = self._ctx.is_leader
            snapshot_battery      = self._ctx.battery_percent
            snapshot_limp         = self._ctx.in_limp_mode

        # ★ PATCH 2026-05-13 (C3): publish goal only on non-FAILURE
        # AND when BT actually set goal_xy this tick.
        if status != Status.FAILURE and snapshot_goal_xy is not None:
            goal = PoseStamped()
            goal.header.stamp = self.get_clock().now().to_msg()
            goal.header.frame_id = self._frame_id
            goal.pose.position.x = float(snapshot_goal_xy[0])
            goal.pose.position.y = float(snapshot_goal_xy[1])
            yaw = snapshot_goal_yaw or 0.0
            goal.pose.orientation.z = math.sin(yaw / 2.0)
            goal.pose.orientation.w = math.cos(yaw / 2.0)
            self._goal_pub.publish(goal)

        # cmd_vel passthrough for P0/P1 (snapshots already taken).
        if self._using_fallback:
            cmd_to_send = None
            if snapshot_emergency:
                cmd_to_send = Twist()       # all-zero (stop)
            elif (snapshot_manual_active and
                    snapshot_manual_cmd is not None):
                lin, ang = snapshot_manual_cmd
                cmd_to_send = Twist()
                cmd_to_send.linear.x = float(lin)
                cmd_to_send.angular.z = float(ang)
            if cmd_to_send is not None:
                self._cmd_vel_pub.publish(cmd_to_send)

        # ★ PATCH 2026-05-13 (C2): tick_hz validated ≥ 1.0 so
        # int(tick_hz) ≥ 1 → no division by zero.
        if snapshot_tick % int(self._tick_hz) == 0:
            state_msg = String()
            state_msg.data = json.dumps({
                "tick": snapshot_tick,
                "bt_status": status.name,
                "mode": snapshot_mode,
                "is_leader": snapshot_is_leader,
                "battery": round(snapshot_battery, 1),
                "limp_mode": snapshot_limp,
            })
            self._mission_state_pub.publish(state_msg)


def main(args=None):
    rclpy.init(args=args)
    node: Optional[MissionNode] = None
    try:
        node = MissionNode()
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    except Exception as e:
        rclpy.logging.get_logger("mission_main").fatal(
            f"MissionNode aborted: {e}")
        raise
    finally:
        if node is not None:
            node.destroy_node()
        # rclpy.shutdown() can fire on a context that the SIGINT
        # handler already shut down — raises
        # "RCLError: rcl_shutdown already called". Guard the call so
        # the process exits cleanly (returncode 0 / SIGTERM=-15)
        # instead of dirty=1, which trips
        # test_s20_1.test_03_clean_shutdown.
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
