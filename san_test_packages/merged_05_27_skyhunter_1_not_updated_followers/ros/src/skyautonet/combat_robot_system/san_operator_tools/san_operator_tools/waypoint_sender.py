# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Waypoint sender operator tool.

Adapted from skyhunter_nav_tools::waypoint_sender (Limon code).
Sends a sequence of waypoints to the current Leader via the
combat_robot_msgs/WaypointCommand interface. Also listens for
Leader changes via LeaderRoleAnnouncement so the mission survives
Leader failover (D-005).

Usage:
    ros2 run san_operator_tools waypoint_sender
    ros2 run san_operator_tools waypoint_sender --ros-args \\
        -p waypoint_file:=/path/to/route.yaml
"""
import time
from typing import List, Tuple

import rclpy
from rclpy.node import Node
from std_msgs.msg import Header
from geometry_msgs.msg import Point

from combat_robot_msgs.msg import (
    LeaderRoleAnnouncement,
    WaypointCommand,
    EmergencyStop,
)

from san_operator_tools.command_auth import CommandAuthGate


# Default mission waypoints (XY in meters from origin)
DEFAULT_WAYPOINTS: List[Tuple[float, float]] = [
    (20.0,   0.0),    # 20m straight ahead
    (35.0, -20.0),    # 35m ahead, 20m right
    (45.0, -35.0),    # diagonal further
    (60.0, -20.0),    # 60m, recovery right
]


class WaypointSender(Node):
    """Publishes WaypointCommand to current Leader; re-publishes on
    Leader change to ensure the new Leader gets the mission."""

    def __init__(self):
        super().__init__("waypoint_sender")
        self.declare_parameter("cruise_speed_mps", 1.5)
        self.declare_parameter("loop_when_done", False)
        self.declare_parameter("publish_interval_s", 1.0)
        self.declare_parameter("auto_publish_on_leader_change", True)

        self.cruise_speed = float(
            self.get_parameter("cruise_speed_mps").value)
        self.loop_done = bool(
            self.get_parameter("loop_when_done").value)
        self.publish_interval = float(
            self.get_parameter("publish_interval_s").value)
        self.auto_on_change = bool(
            self.get_parameter("auto_publish_on_leader_change").value)

        self.waypoints = DEFAULT_WAYPOINTS
        self.current_leader_id = 0
        self.command_id_counter = 0

        # Phase 0 PR-D: command authorization gate. Refuses to publish
        # without operator_id when production_mode=true.
        self._auth = CommandAuthGate(self, "WaypointCommand")

        # Subscribe to leader announcement so we follow failovers
        self.create_subscription(
            LeaderRoleAnnouncement,
            "/swarm/leader_role_announcement",
            self._on_leader, 10)

        # Publish waypoint commands + emergency halt
        self._wp_pub = self.create_publisher(
            WaypointCommand, "/swarm/waypoint_command", 10)
        self._halt_pub = self.create_publisher(
            EmergencyStop, "/swarm/emergency_stop", 10)

        # Publish at low rate (operator may re-trigger by changing
        # waypoints; auto-republish on Leader change).
        self.create_timer(self.publish_interval, self._publish_tick)

        self.get_logger().info(
            f"WaypointSender UP — {len(self.waypoints)} waypoints, "
            f"cruise={self.cruise_speed} m/s")

    def _on_leader(self, msg: LeaderRoleAnnouncement):
        """Track current Leader. On change, re-publish waypoints."""
        try:
            new_leader = int(msg.leader_robot_id)
        except AttributeError:
            new_leader = int(msg.robot_id)
        if new_leader != self.current_leader_id:
            self.get_logger().warn(
                f"Leader change detected: "
                f"{self.current_leader_id} → {new_leader}")
            self.current_leader_id = new_leader
            if self.auto_on_change:
                self._publish_waypoints()

    def _publish_tick(self):
        """Periodic publish (low rate). Operator stays in control;
        this just keeps the current waypoints visible to Leader."""
        if self.current_leader_id == 0:
            return
        self._publish_waypoints()

    def _publish_waypoints(self):
        summary = (f"waypoints={len(self.waypoints)} "
                   f"cruise={self.cruise_speed}m/s "
                   f"loop={self.loop_done}")
        if not self._auth.check_and_log(summary,
                                          target_id=self.current_leader_id):
            return  # refused — production_mode + empty operator_id

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
