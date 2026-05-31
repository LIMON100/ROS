"""SAN v1.5 — Waypoint sender operator tool.

Adapted from skyhunter_nav_tools::waypoint_sender (Limon code).
Sends a sequence of waypoints to the current Leader's Nav2 stack
via the NavigateToPose action AND publishes the high-level
combat_robot_msgs/WaypointCommand for telemetry / command-echo.

On Leader change (LeaderRoleAnnouncement role=LEADER_PROMOTED), the
node switches the action target namespace (e.g. "/SH_02") and
resends the current waypoint so the new Leader picks up where the
previous one stopped.

Usage:
    ros2 run san_operator_tools waypoint_sender
    ros2 run san_operator_tools waypoint_sender --ros-args \\
        -p waypoint_file:=/path/to/route.yaml
"""
import time
from typing import List, Tuple

import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient
from std_msgs.msg import Header
from geometry_msgs.msg import Point

from nav2_msgs.action import NavigateToPose
from combat_robot_msgs.msg import (
    LeaderRoleAnnouncement,
    WaypointCommand,
    EmergencyStop,
)


# Default mission waypoints (XY in meters from origin)
DEFAULT_WAYPOINTS: List[Tuple[float, float]] = [
    (20.0,   0.0),    # 20m straight ahead
    (35.0, -20.0),    # 35m ahead, 20m right
    (45.0, -35.0),    # diagonal further
    (60.0, -20.0),    # 60m, recovery right
]


class WaypointSender(Node):
    """Publishes WaypointCommand AND drives Nav2 via NavigateToPose;
    re-publishes/resends on Leader change."""

    def __init__(self):
        super().__init__("waypoint_sender")
        self.declare_parameter("cruise_speed_mps", 1.5)
        self.declare_parameter("loop_when_done", False)
        self.declare_parameter("publish_interval_s", 1.0)
        self.declare_parameter("auto_publish_on_leader_change", True)
        # Action namespace prefix. Empty = global "/navigate_to_pose"
        # (robot_id=1 Leader runs in global ns per squadron.launch.py).
        # For follower goals use e.g. "/robot_2".
        self.declare_parameter("nav2_namespace", "")
        # Goal frame (must match Nav2 global_frame; default "map")
        self.declare_parameter("goal_frame", "map")
        # Distance at which we consider a goal "reached" (m). Used
        # only by the resilient-resend logic if the action result is
        # ever lost. Nav2's own xy_goal_tolerance governs completion.
        self.declare_parameter("startup_delay_s", 5.0)

        self.cruise_speed = float(
            self.get_parameter("cruise_speed_mps").value)
        self.loop_done = bool(
            self.get_parameter("loop_when_done").value)
        self.publish_interval = float(
            self.get_parameter("publish_interval_s").value)
        self.auto_on_change = bool(
            self.get_parameter("auto_publish_on_leader_change").value)
        self.nav2_ns = str(
            self.get_parameter("nav2_namespace").value)
        self.goal_frame = str(
            self.get_parameter("goal_frame").value)
        startup_delay = float(
            self.get_parameter("startup_delay_s").value)

        self.waypoints = DEFAULT_WAYPOINTS
        self.current_leader_id = 0
        self.command_id_counter = 0

        # Nav2 ActionClient state
        self._action_client: ActionClient | None = None
        self._current_wp_index = 0
        self._goal_in_flight = False
        self._current_goal_handle = None
        self._connect_to_nav2()

        # Subscribe to leader announcement so we follow failovers
        self.create_subscription(
            LeaderRoleAnnouncement,
            "/swarm/leader/role_announce",
            self._on_leader, 10)

        # Publish waypoint commands + emergency halt
        self._wp_pub = self.create_publisher(
            WaypointCommand, "/swarm/waypoint_command", 10)
        self._halt_pub = self.create_publisher(
            EmergencyStop, "/swarm/emergency_stop", 10)

        # Publish at low rate (operator may re-trigger by changing
        # waypoints; auto-republish on Leader change).
        self.create_timer(self.publish_interval, self._publish_tick)

        # Delayed start so Nav2 lifecycle has time to come up; if no
        # LeaderRoleAnnouncement arrives within startup_delay we still
        # start sending goals (single-robot dev / bench).
        self._mission_started = False
        self._start_timer = self.create_timer(
            startup_delay, self._auto_start_mission)

        self.get_logger().info(
            f"WaypointSender UP — {len(self.waypoints)} waypoints, "
            f"cruise={self.cruise_speed} m/s, nav2_ns='{self.nav2_ns}', "
            f"will auto-start in {startup_delay:.1f}s if no leader announce")

    # ─── Nav2 action client ────────────────────────────────────────────

    def _action_topic(self) -> str:
        if self.nav2_ns:
            return f"{self.nav2_ns}/navigate_to_pose"
        return "/navigate_to_pose"

    def _connect_to_nav2(self) -> None:
        topic = self._action_topic()
        self.get_logger().info(
            f"Connecting to Nav2 action server: {topic}")
        self._action_client = ActionClient(
            self, NavigateToPose, topic)

    def _auto_start_mission(self) -> None:
        """If startup_delay elapses without LeaderRoleAnnouncement,
        kick off the mission anyway (single-robot dev mode)."""
        self._start_timer.cancel()
        if not self._mission_started:
            self.get_logger().info(
                "No leader announce in startup window — "
                "auto-starting mission")
            self._mission_started = True
            self._send_next_goal()

    def _send_next_goal(self) -> None:
        if self._goal_in_flight:
            return
        if self._current_wp_index >= len(self.waypoints):
            if self.loop_done:
                self.get_logger().info(
                    "Mission complete — looping")
                self._current_wp_index = 0
            else:
                self.get_logger().info(
                    "============================================="
                )
                self.get_logger().info(
                    "MISSION COMPLETE — all waypoints reached"
                )
                self.get_logger().info(
                    "============================================="
                )
                return

        x, y = self.waypoints[self._current_wp_index]
        if not self._action_client.server_is_ready():
            self.get_logger().warn(
                f"Nav2 action server not ready at "
                f"{self._action_topic()} — retrying in 1 s")
            self.create_timer(1.0, self._retry_send_once)
            return

        goal = NavigateToPose.Goal()
        goal.pose.header.frame_id = self.goal_frame
        goal.pose.header.stamp = self.get_clock().now().to_msg()
        goal.pose.pose.position.x = float(x)
        goal.pose.pose.position.y = float(y)
        goal.pose.pose.orientation.w = 1.0

        self.get_logger().info(
            f"Sending WP {self._current_wp_index + 1}/"
            f"{len(self.waypoints)}: x={x} y={y}")
        self._goal_in_flight = True
        future = self._action_client.send_goal_async(goal)
        future.add_done_callback(self._on_goal_response)

    def _retry_send_once(self) -> None:
        # one-shot retry helper — cancel its timer and try again
        self._send_next_goal()

    def _on_goal_response(self, future) -> None:
        handle = future.result()
        if not handle.accepted:
            self.get_logger().error(
                "Nav2 rejected goal — will retry in 2 s")
            self._goal_in_flight = False
            self.create_timer(2.0, self._send_next_goal)
            return
        self._current_goal_handle = handle
        result_future = handle.get_result_async()
        result_future.add_done_callback(self._on_goal_result)

    def _on_goal_result(self, future) -> None:
        self._goal_in_flight = False
        self._current_goal_handle = None
        result = future.result()
        status = result.status
        # GoalStatus.STATUS_SUCCEEDED == 4
        if status == 4:
            self.get_logger().info(
                f"WP {self._current_wp_index + 1} reached")
            self._current_wp_index += 1
            self._send_next_goal()
        else:
            self.get_logger().warn(
                f"WP {self._current_wp_index + 1} ended with "
                f"status={status} — retrying in 2 s")
            self.create_timer(2.0, self._send_next_goal)

    # ─── Leader follow + telemetry ─────────────────────────────────────

    def _on_leader(self, msg: LeaderRoleAnnouncement):
        """Track current Leader. On LEADER_PROMOTED, switch Nav2
        target namespace (if needed) and resend current waypoint."""
        try:
            new_leader = int(msg.robot_id)
        except AttributeError:
            return

        # Only react to PROMOTED (skip CANDIDATE / NORMAL / DEMOTED)
        if msg.role != LeaderRoleAnnouncement.LEADER_PROMOTED:
            # Still record the leader id so the WaypointCommand telemetry
            # has a valid target, but don't reroute Nav2.
            self.current_leader_id = new_leader
            return

        if new_leader != self.current_leader_id:
            self.get_logger().warn(
                f"Leader change detected: "
                f"{self.current_leader_id} → {new_leader}")
            self.current_leader_id = new_leader

            # Switch nav2 namespace if leader id != 1
            # Convention: leader=1 → "" (global); else → "/robot_<id>"
            new_ns = "" if new_leader == 1 else f"/robot_{new_leader}"
            if new_ns != self.nav2_ns:
                self.nav2_ns = new_ns
                self._goal_in_flight = False
                self._current_goal_handle = None
                self._connect_to_nav2()

            if self.auto_on_change:
                self._publish_waypoints()
                self._mission_started = True
                self._send_next_goal()
        else:
            self.current_leader_id = new_leader
            if not self._mission_started:
                self._mission_started = True
                self._send_next_goal()

    def _publish_tick(self):
        """Periodic publish (low rate). Operator stays in control;
        this just keeps the current waypoints visible to Leader."""
        if self.current_leader_id == 0:
            return
        self._publish_waypoints()

    def _publish_waypoints(self):
        msg = WaypointCommand()
        self.command_id_counter += 1
        msg.header = Header(frame_id="world")
        msg.command_id = self.command_id_counter
        msg.sequence = self.command_id_counter
        msg.target_robot_id = self.current_leader_id
        msg.cruise_speed_mps = self.cruise_speed
        msg.loop_when_done = self.loop_done
        msg.timestamp_ms = int(time.time() * 1000)
        msg.waypoints = []
        for x, y in self.waypoints:
            p = Point()
            p.x = float(x)
            p.y = float(y)
            p.z = 0.0
            msg.waypoints.append(p)
        self._wp_pub.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = WaypointSender()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
