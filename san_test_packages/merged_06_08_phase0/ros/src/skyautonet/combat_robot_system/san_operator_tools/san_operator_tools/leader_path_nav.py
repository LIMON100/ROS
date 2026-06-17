# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Leader waypoint navigator + dashed path visualization.

Lightweight, Nav2-free leader driver for the leader/follower Gazebo demo
(leader_follower_demo.launch.py). Reads the leader's /odom and drives
/cmd_vel through a list of XY waypoints with a simple turn-then-go P
controller, then holds at the final goal.

It also publishes a visualization_msgs/MarkerArray on /leader/waypoint_path
that RViz renders as a DASHED line from the leader's current position
through the remaining waypoints to the goal, plus a sphere at each waypoint
and a distinct sphere at the goal. The dashed effect is built with a
LINE_LIST marker (alternating draw/skip segments) since RViz has no native
dash style.

This is a sim/demo tool (Tier 3, rclpy) — the production leader path comes
from the Nav2 stack driven by WaypointCommand. Frames: the leader spawns at
the world origin so its odom frame is identity to "map"; waypoints and
markers are therefore expressed directly in the world frame.

Usage:
    ros2 run san_operator_tools leader_path_nav
    ros2 run san_operator_tools leader_path_nav --ros-args \\
        -p "waypoints:=[6.0, 0.0, 12.0, 4.0, 20.0, 4.0, 28.0, 0.0]" \\
        -p cruise_speed_mps:=1.2
"""
import math

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data

from geometry_msgs.msg import Point, Twist
from nav_msgs.msg import Odometry
from std_msgs.msg import ColorRGBA
from visualization_msgs.msg import Marker, MarkerArray


# Default mission: gentle S-curve forward from the origin, kept short so the
# whole run stays in view for the demo.
DEFAULT_WAYPOINTS = [6.0, 0.0, 12.0, 4.0, 20.0, 4.0, 28.0, 0.0]


def _yaw_from_quat(q) -> float:
    siny_cosp = 2.0 * (q.w * q.z + q.x * q.y)
    cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny_cosp, cosy_cosp)


def _wrap_pi(a: float) -> float:
    while a > math.pi:
        a -= 2.0 * math.pi
    while a <= -math.pi:
        a += 2.0 * math.pi
    return a


class LeaderPathNav(Node):
    """Drive the leader through waypoints and publish a dashed path marker."""

    def __init__(self):
        super().__init__("leader_path_nav")

        flat = list(self.declare_parameter("waypoints", DEFAULT_WAYPOINTS).value)
        if len(flat) < 2 or len(flat) % 2 != 0:
            self.get_logger().warn(
                "waypoints must be an even-length [x0,y0,x1,y1,...] list; "
                "falling back to default route")
            flat = DEFAULT_WAYPOINTS
        self.waypoints = [(flat[i], flat[i + 1]) for i in range(0, len(flat), 2)]

        self.frame_id = str(self.declare_parameter("goal_frame", "map").value)
        self.cruise = float(self.declare_parameter("cruise_speed_mps", 1.2).value)
        self.kp_lin = float(self.declare_parameter("kp_linear", 0.8).value)
        self.kp_ang = float(self.declare_parameter("kp_angular", 2.0).value)
        self.max_ang = float(self.declare_parameter("max_angular_rps", 1.2).value)
        self.wp_tol = float(self.declare_parameter("waypoint_tolerance_m", 1.0).value)
        self.goal_tol = float(self.declare_parameter("goal_tolerance_m", 0.5).value)

        # Dashed-line styling (metres).
        self.dash_len = float(self.declare_parameter("dash_length_m", 0.6).value)
        self.gap_len = float(self.declare_parameter("dash_gap_m", 0.4).value)
        self.line_w = float(self.declare_parameter("line_width_m", 0.08).value)

        self.have_pose = False
        self.x = 0.0
        self.y = 0.0
        self.yaw = 0.0
        self.wp_idx = 0
        self.reached_goal = False
        self.start_pose = None   # leader's first odom, anchors the drawn route

        self.create_subscription(
            Odometry, "odom", self._on_odom, qos_profile_sensor_data)
        self.cmd_pub = self.create_publisher(Twist, "cmd_vel", 10)
        self.marker_pub = self.create_publisher(
            MarkerArray, "leader/waypoint_path", 10)

        self.create_timer(0.05, self._control_tick)   # 20 Hz drive
        self.create_timer(0.2, self._publish_markers)  # 5 Hz viz

        self.get_logger().info(
            f"LeaderPathNav UP — {len(self.waypoints)} waypoints, "
            f"goal={self.waypoints[-1]}, cruise={self.cruise} m/s, "
            f"frame={self.frame_id}")

    # ── Odometry ─────────────────────────────────────────────────────────
    def _on_odom(self, msg: Odometry):
        self.x = msg.pose.pose.position.x
        self.y = msg.pose.pose.position.y
        self.yaw = _yaw_from_quat(msg.pose.pose.orientation)
        if self.start_pose is None:
            self.start_pose = (self.x, self.y)   # anchor the drawn route
        self.have_pose = True

    # ── Control ──────────────────────────────────────────────────────────
    def _control_tick(self):
        cmd = Twist()
        if not self.have_pose or self.reached_goal:
            self.cmd_pub.publish(cmd)  # hold
            return

        tx, ty = self.waypoints[self.wp_idx]
        dx, dy = tx - self.x, ty - self.y
        dist = math.hypot(dx, dy)

        is_last = self.wp_idx >= len(self.waypoints) - 1
        tol = self.goal_tol if is_last else self.wp_tol
        if dist < tol:
            if is_last:
                self.reached_goal = True
                self.get_logger().info("Leader reached final goal — holding.")
                self.cmd_pub.publish(cmd)
                return
            self.wp_idx += 1
            tx, ty = self.waypoints[self.wp_idx]
            dx, dy = tx - self.x, ty - self.y
            dist = math.hypot(dx, dy)

        desired_yaw = math.atan2(dy, dx)
        yaw_err = _wrap_pi(desired_yaw - self.yaw)

        # Turn (almost) in place at corners so the path is hugged tightly:
        # forward speed goes to 0 once the heading error exceeds ~60°, which
        # stops the wide corner overshoot.
        turn_factor = max(0.0, 1.0 - abs(yaw_err) / (math.pi / 3.0))
        cmd.linear.x = min(self.kp_lin * dist, self.cruise) * turn_factor
        cmd.angular.z = max(-self.max_ang, min(self.max_ang, self.kp_ang * yaw_err))
        self.cmd_pub.publish(cmd)

    # ── Visualization ────────────────────────────────────────────────────
    def _dash_points(self, polyline):
        """Split a polyline into LINE_LIST point pairs (dash / gap)."""
        pts = []
        step = self.dash_len + self.gap_len
        for (ax, ay), (bx, by) in zip(polyline, polyline[1:]):
            seg = math.hypot(bx - ax, by - ay)
            if seg < 1e-6:
                continue
            ux, uy = (bx - ax) / seg, (by - ay) / seg
            d = 0.0
            while d < seg:
                s = d
                e = min(d + self.dash_len, seg)
                pts.append(Point(x=ax + ux * s, y=ay + uy * s, z=0.05))
                pts.append(Point(x=ax + ux * e, y=ay + uy * e, z=0.05))
                d += step
        return pts

    def _publish_markers(self):
        now = self.get_clock().now().to_msg()
        arr = MarkerArray()

        # Full static route: start pose -> every waypoint -> goal. Kept fixed
        # (not "consumed" as the leader drives) so the RViz dashed path matches
        # the route drawn in the Gazebo world exactly.
        polyline = ([self.start_pose] if self.start_pose else []) \
            + list(self.waypoints)

        line = Marker()
        line.header.frame_id = self.frame_id
        line.header.stamp = now
        line.ns = "leader_path"
        line.id = 0
        line.type = Marker.LINE_LIST
        line.action = Marker.ADD
        line.scale.x = self.line_w
        line.color = ColorRGBA(r=0.1, g=1.0, b=0.2, a=1.0)
        line.pose.orientation.w = 1.0
        line.points = self._dash_points(polyline)
        arr.markers.append(line)

        # Waypoint nodes (all but the goal) as small yellow spheres.
        nodes = Marker()
        nodes.header.frame_id = self.frame_id
        nodes.header.stamp = now
        nodes.ns = "leader_path"
        nodes.id = 1
        nodes.type = Marker.SPHERE_LIST
        nodes.action = Marker.ADD
        nodes.scale.x = nodes.scale.y = nodes.scale.z = 0.4
        nodes.color = ColorRGBA(r=1.0, g=0.9, b=0.1, a=0.9)
        nodes.pose.orientation.w = 1.0
        nodes.points = [Point(x=wx, y=wy, z=0.05)
                        for wx, wy in self.waypoints[:-1]]
        arr.markers.append(nodes)

        # Goal as a larger red sphere.
        gx, gy = self.waypoints[-1]
        goal = Marker()
        goal.header.frame_id = self.frame_id
        goal.header.stamp = now
        goal.ns = "leader_path"
        goal.id = 2
        goal.type = Marker.SPHERE
        goal.action = Marker.ADD
        goal.pose.position.x = gx
        goal.pose.position.y = gy
        goal.pose.position.z = 0.1
        goal.pose.orientation.w = 1.0
        goal.scale.x = goal.scale.y = goal.scale.z = 0.7
        goal.color = ColorRGBA(r=1.0, g=0.15, b=0.1, a=0.95)
        arr.markers.append(goal)

        self.marker_pub.publish(arr)


def main(args=None):
    rclpy.init(args=args)
    node = LeaderPathNav()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
