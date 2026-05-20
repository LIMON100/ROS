"""SAN v1.5 Phase 2-E Turn 9-10 — Mission rclpy node.

⭐ FIRST rclpy NODE — establishes the Tier 2 Python pattern per
DCN-2026-002 D-007. Subsequent rclpy nodes (Perception Turn 11-12,
BLE Turn 13) follow this template.

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
"""
from __future__ import annotations

import json
import math
from typing import Optional

import rclpy

# combat_robot_msgs import — must come from generated message package
from combat_robot_msgs.msg import (
    EmergencyStop,
    HubRoleAnnouncement,
    ManualOverrideCommand,
    RobotStatus,
    SwarmHealthSummary,
    TierStatusChange,
    LeaderState,
)
from geometry_msgs.msg import PoseStamped, Twist
from rclpy.node import Node
from rclpy.qos import (
    QoSDurabilityPolicy,
    QoSHistoryPolicy,
    QoSProfile,
    QoSReliabilityPolicy,
)
from std_msgs.msg import String

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
        # self._combat_sub = self.create_subscription(
        #         LeaderState,
        #         "/perception/combat_state",
        #         self._on_combat_state, reliable_qos)

        self._tick_hz = float(
            self.get_parameter("tick_hz").value)
        self._min_battery = float(
            self.get_parameter("min_battery_percent").value)
        self._frame_id = str(
            self.get_parameter("frame_id").value)
        if self._tick_hz <= 0.0 or self._tick_hz > 100.0:
            raise ValueError(
                f"MissionNode: tick_hz out of range: {self._tick_hz}")

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

        self._combat_sub = self.create_subscription(
            LeaderState,
            "/perception/combat_state",
            self._on_combat_state, reliable_qos)



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

        # ─── Timer ─────────────────────────────────────────────────
        self._timer = self.create_timer(
            1.0 / self._tick_hz, self._on_tick)

        self.get_logger().info(
            f"MissionNode UP: tick={self._tick_hz} Hz "
            f"min_battery={self._min_battery}% "
            f"mode={self._ctx.mode.current.value}")

    # ─── Subscribers ───────────────────────────────────────────────

    def _on_combat_state(self, msg: LeaderState) -> None:
        if not self._using_fallback:
            return
        if msg.target_locked and msg.swarm_state >= 3:
            self._ctx.priority.combat_active = True
            self._ctx.priority.combat_target_xy = (msg.target_pos.x, msg.target_pos.y)
        else:
            self._ctx.priority.combat_active = False

    def _on_pose(self, msg: PoseStamped) -> None:
        # Phase 7 fix: pose_xy and yaw_rad are two related fields; a
        # BT tick reading them between the two writes would observe
        # pose from msg N with yaw from msg N+1. Hold ctx.lock so the
        # snapshot is coherent.
        q = msg.pose.orientation
        yaw = math.atan2(
            2.0 * (q.w * q.z + q.x * q.y),
            1.0 - 2.0 * (q.y * q.y + q.z * q.z),
        )
        with self._ctx.lock:
            self._ctx.pose_xy = (msg.pose.position.x, msg.pose.position.y)
            self._ctx.yaw_rad = yaw

    def _on_status(self, msg: RobotStatus) -> None:
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
        OVERRIDE_RELEASE."""
        if not self._using_fallback:
            return
        # Only apply if scope targets this robot
        # NOTE: this node doesn't know its own robot_id by default;
        # the parameter "robot_role" tells us our role only. For the
        # PDR-prep wiring we honour SCOPE_ALL_ROBOTS and SCOPE_LEADER_ONLY
        # leniently; production hardening would also check robot_id.
        if msg.scope == EmergencyStop.SCOPE_ALL_ROBOTS:
            apply = True
        elif msg.scope == EmergencyStop.SCOPE_LEADER_ONLY:
            apply = (self._ctx.priority.robot_role == "leader")
        else:
            # SCOPE_SINGLE_ROBOT — conservative: always apply
            # (production would compare msg.target_robot_id to self)
            apply = True
        if apply:
            self._ctx.priority.emergency_active = True
            self.get_logger().warn(
                f"EMERGENCY STOP activated "
                f"(reason='{msg.reason}' op='{msg.operator_id}')")

    def _on_manual_override(self, msg: ManualOverrideCommand) -> None:
        """P1 driver. override_type controls semantics:
            CMD_VEL → manual mode + forward cmd_vel
            HALT    → stop (effectively P0-like)
            RETURN  → trigger RTH path (health_critical proxy)
            RELEASE → exit manual + release emergency
        """
        if not self._using_fallback:
            return

        ot = msg.override_type
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
            # Operator yields control back to autonomy + clears
            # any pending emergency.
            self._ctx.priority.manual_mode_active = False
            self._ctx.priority.manual_cmd_vel = None
            self._ctx.priority.emergency_release_armed = True
            self._ctx.priority.health_critical = False

    def _on_health(self, msg: SwarmHealthSummary) -> None:
        """P2 driver. Critical when either SBC has failed."""
        if not self._using_fallback:
            return
        self._ctx.priority.health_critical = bool(
            msg.slam_sbc_failed or msg.comm_sbc_failed)

    def _on_tier_status(self, msg: TierStatusChange) -> None:
        """Informational — feed current tier into BT context for
        ApplyTierAwareMotionLimit action."""
        if not self._using_fallback:
            return
        self._ctx.priority.current_tier = int(msg.current_tier)

    def _on_hub_role(self, msg: HubRoleAnnouncement) -> None:
        """P3 driver. If this robot is the current Hub (msg.role ==
        HUB_NORMAL or HUB_PROMOTED AND msg.robot_id matches our role
        param), set is_hub. Hub takeover available if all 3 capability
        flags are true."""
        if not self._using_fallback:
            return
        # Track if we're the announced active Hub
        is_announced_hub = (
            msg.role in (HubRoleAnnouncement.HUB_NORMAL,
                          HubRoleAnnouncement.HUB_PROMOTED))
        # Conservative: our parameter robot_role authoritative for which
        # robot we are; if robot_role=="hub", we are the Hub.
        # The msg gives us hand-off readiness from the network.
        self._ctx.priority.is_hub = (
            self._ctx.priority.robot_role == "hub")
        self._ctx.priority.hub_takeover_available = (
            is_announced_hub and
            bool(msg.lte_active) and
            bool(msg.slam_aggregation_active) and
            bool(msg.video_relay_active))

    # ─── Tick ──────────────────────────────────────────────────────

    def _on_tick(self) -> None:
        import time # <-- ADD
        import math # <-- ADD
        
        # --- INJECTED ANTI-STUCK DETECTION (Leader only) ---
        if self._using_fallback and self._ctx.priority.robot_role == "leader" and not self._ctx.priority.stuck_active and not self._ctx.is_halt_mode:
            if self._ctx.pose_xy is not None:  # <-- ADDED SAFETY GUARD
                now = time.time()
                if self._ctx.last_pose_time == 0.0:
                    self._ctx.last_pose_time = now
                    self._ctx.last_pose_xy = self._ctx.pose_xy
                elif now - self._ctx.last_pose_time > 3.0:
                    if self._ctx.last_pose_xy is not None:
                        dist = math.hypot(self._ctx.pose_xy[0] - self._ctx.last_pose_xy[0], self._ctx.pose_xy[1] - self._ctx.last_pose_xy[1])
                        if dist < 0.3: # Stuck!
                            self.get_logger().error("!!! LEADER PHYSICALLY STUCK !!! Triggering BT Recovery.")
                            self._ctx.priority.stuck_active = True
                            self._ctx.priority.stuck_recovery_start = now
                    self._ctx.last_pose_time = now
                    self._ctx.last_pose_xy = self._ctx.pose_xy
        # ---------------------------------------------------

        self._ctx.tick_count += 1
        status = self._tree.tick(self._ctx)

        # Publish goal iff BT produced one this tick
        if self._ctx.goal_xy is not None:
            goal = PoseStamped()
            goal.header.stamp = self.get_clock().now().to_msg()
            goal.header.frame_id = self._frame_id
            goal.pose.position.x = float(self._ctx.goal_xy[0])
            goal.pose.position.y = float(self._ctx.goal_xy[1])
            yaw = self._ctx.goal_yaw_rad or 0.0
            goal.pose.orientation.z = math.sin(yaw / 2.0)
            goal.pose.orientation.w = math.cos(yaw / 2.0)
            self._goal_pub.publish(goal)

        # PDR-7a: For the fallback tree, publish /cmd_vel based on
        # the active priority:
        #   P0 EmergencyStop → zero velocity (stop)
        #   P1 ManualOverride → forward operator's cmd_vel
        # Other priorities/Normal flow rely on dedicated nodes
        # (san_reroute_planner publishes /cmd_vel for T1.5; tier_node
        #  motion-limit applies elsewhere) so mission_node only
        # passes through P0/P1 commands.
        if self._using_fallback:
            cmd_to_send = None
            if self._ctx.priority.emergency_active:
                cmd_to_send = Twist()       # all-zero (stop)
            elif self._ctx.priority.stuck_active and self._ctx.priority.manual_cmd_vel is not None: # <-- ADDED THIS BLOCK
                lin, ang = self._ctx.priority.manual_cmd_vel
                cmd_to_send = Twist()
                cmd_to_send.linear.x = float(lin)
                cmd_to_send.angular.z = float(ang)
            elif (self._ctx.priority.manual_mode_active and
                    self._ctx.priority.manual_cmd_vel is not None):
                lin, ang = self._ctx.priority.manual_cmd_vel
                cmd_to_send = Twist()
                cmd_to_send.linear.x = float(lin)
                cmd_to_send.angular.z = float(ang)
            if cmd_to_send is not None:
                self._cmd_vel_pub.publish(cmd_to_send)

        # Periodic state broadcast (JSON for tooling simplicity;
        # binary message could replace this in a future turn)
        if self._ctx.tick_count % int(self._tick_hz) == 0:
            state_msg = String()
            state_msg.data = json.dumps({
                "tick": self._ctx.tick_count,
                "bt_status": status.name,
                "mode": self._ctx.mode.current.value,
                "is_leader": self._ctx.is_leader,
                "battery": round(self._ctx.battery_percent, 1),
                "limp_mode": self._ctx.in_limp_mode,
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
