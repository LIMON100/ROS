# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Waypoint → Nav2 bridge (simulation glue).

Closes the missing link between the operator waypoint command and robot
motion in the Gazebo sim:

    waypoint_sender ──/swarm/waypoint_command──▶ THIS NODE
        ──NavigateThroughPoses action──▶ Nav2 (controller→/cmd_vel)
        ──▶ Gazebo DiffDrive plugin ──▶ robot moves.

It also (optionally) re-publishes a LeaderRoleAnnouncement so the gated
waypoint_sender (which stays silent until current_leader_id != 0)
actually emits its WaypointCommand in a sim that isn't running
san_role_management.

This is a SIM/bench helper. On real hardware the Leader announcement
comes from san_role_management and the on-robot navigation owns the
NavigateThroughPoses client.

Usage:
    ros2 run san_operator_tools waypoint_to_nav2
    ros2 run san_operator_tools waypoint_to_nav2 --ros-args \\
        -p robot_id:=1 -p announce_leader:=true -p goal_frame:=map
"""
import math

import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node
from rclpy.qos import QoSDurabilityPolicy, QoSProfile, QoSReliabilityPolicy

from std_msgs.msg import Header
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateThroughPoses

from combat_robot_msgs.msg import LeaderRoleAnnouncement, WaypointCommand


def _yaw_to_quat(yaw: float):
    """Return (z, w) of a quaternion for a planar yaw (x=y=0)."""
    return math.sin(yaw * 0.5), math.cos(yaw * 0.5)


class WaypointToNav2(Node):
    """Subscribe WaypointCommand, drive Nav2 NavigateThroughPoses."""

    def __init__(self):
        super().__init__("waypoint_to_nav2")

        self.declare_parameter("robot_id", 1)
        self.declare_parameter("goal_frame", "map")
        self.declare_parameter("announce_leader", True)
        self.declare_parameter("announce_period_s", 1.0)
        self.declare_parameter("nav_action", "navigate_through_poses")

        self.robot_id = int(self.get_parameter("robot_id").value)
        self.goal_frame = str(self.get_parameter("goal_frame").value)
        self.announce_leader = bool(self.get_parameter("announce_leader").value)
        announce_period = float(self.get_parameter("announce_period_s").value)
        nav_action = str(self.get_parameter("nav_action").value)

        # Nav2 NavigateThroughPoses action client. Relative name → honours
        # this node's namespace so a per-robot bridge talks to its own Nav2.
        self._nav_client = ActionClient(self, NavigateThroughPoses, nav_action)
        self._goal_handle = None
        self._last_command_id = -1

        # Subscribe to operator waypoint commands.
        self.create_subscription(
            WaypointCommand, "/swarm/waypoint_command", self._on_waypoints, 10)

        # Optionally announce ourselves as Leader so the (gated)
        # waypoint_sender starts publishing. Latched + periodic so late
        # subscribers still get it.
        if self.announce_leader:
            latched = QoSProfile(
                depth=1,
                reliability=QoSReliabilityPolicy.RELIABLE,
                durability=QoSDurabilityPolicy.TRANSIENT_LOCAL)
            self._leader_pub = self.create_publisher(
                LeaderRoleAnnouncement, "/swarm/leader_role_announcement",
                latched)
            self._leader_seq = 0
            self._announce()  # immediate
            self.create_timer(announce_period, self._announce)

        self.get_logger().info(
            f"WaypointToNav2 UP — robot_id={self.robot_id} "
            f"frame={self.goal_frame} action='{nav_action}' "
            f"announce_leader={self.announce_leader}")

    # ── Leader announcement (sim glue) ─────────────────────────────
    def _announce(self):
        msg = LeaderRoleAnnouncement()
        self._leader_seq += 1
        msg.header = Header(frame_id="world")
        msg.sequence = self._leader_seq
        msg.robot_id = self.robot_id
        msg.leader_term = 1
        msg.role = LeaderRoleAnnouncement.LEADER_PROMOTED
        msg.succession_priority = LeaderRoleAnnouncement.PRIORITY_DEPUTY
        msg.battery_percent = 100.0
        msg.reason = "sim_waypoint_bridge"
        self._leader_pub.publish(msg)

    # ── Waypoint command → Nav2 goal ───────────────────────────────
    def _on_waypoints(self, msg: WaypointCommand):
        if msg.target_robot_id not in (0, self.robot_id):
            return  # not addressed to us
        if msg.command_id == self._last_command_id:
            return  # already acting on this command (sender re-publishes)
        if not msg.waypoints:
            self.get_logger().warn("WaypointCommand with no waypoints — ignored")
            return
        # self._last_command_id = msg.command_id

        if not self._nav_client.wait_for_server(timeout_sec=2.0):
            self.get_logger().error(
                "Nav2 NavigateThroughPoses action server unavailable — "
                "is Nav2 (bt_navigator) up and active?")
            return
        self._last_command_id = msg.command_id

        poses = self._build_poses(msg)
        goal = NavigateThroughPoses.Goal()
        goal.poses = poses
        self.get_logger().info(
            f"→ Nav2: {len(poses)} poses (cmd_id={msg.command_id}, "
            f"target={msg.target_robot_id})")

        # Pre-empt any in-flight goal, then send.
        if self._goal_handle is not None:
            self._goal_handle.cancel_goal_async()
            self._goal_handle = None
        self._nav_client.send_goal_async(goal).add_done_callback(self._on_sent)

    def _build_poses(self, msg: WaypointCommand):
        stamp = self.get_clock().now().to_msg()
        pts = msg.waypoints
        poses = []
        for i, p in enumerate(pts):
            ps = PoseStamped()
            ps.header.frame_id = self.goal_frame
            ps.header.stamp = stamp
            ps.pose.position.x = float(p.x)
            ps.pose.position.y = float(p.y)
            ps.pose.position.z = 0.0
            # Heading: face the next waypoint (last keeps previous heading).
            nxt = pts[i + 1] if i + 1 < len(pts) else None
            if nxt is not None:
                yaw = math.atan2(float(nxt.y) - float(p.y),
                                 float(nxt.x) - float(p.x))
            elif i > 0:
                prev = pts[i - 1]
                yaw = math.atan2(float(p.y) - float(prev.y),
                                 float(p.x) - float(prev.x))
            else:
                yaw = 0.0
            qz, qw = _yaw_to_quat(yaw)
            ps.pose.orientation.z = qz
            ps.pose.orientation.w = qw
            poses.append(ps)
        return poses

    def _on_sent(self, future):
        handle = future.result()
        if not handle.accepted:
            self.get_logger().error("Nav2 rejected the goal")
            return
        self._goal_handle = handle
        self.get_logger().info("Nav2 accepted goal")


def main(args=None):
    rclpy.init(args=args)
    node = WaypointToNav2()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
