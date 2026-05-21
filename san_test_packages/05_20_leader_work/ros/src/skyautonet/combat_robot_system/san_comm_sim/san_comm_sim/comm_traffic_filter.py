#!/usr/bin/env python3
"""
comm_traffic_filter.py — SkyHunter Networking Node 4 (ROS 2 entry point)

Thin ROS 2 node.  All relay policy logic lives in models/traffic_policy.py.

Aggregates all 8 robots' topics and relays them based on the active
communication tier reported by swarm_comm_manager.

Tier behaviour:
  WiFi6        → pass-through for all topic types
  LTE          → odom throttled to 2 Hz, camera to 1 fps, cmd_vel pass-through
  LoRa         → all blocked (e-stop only via lora_sim_stub)
  Disconnected → all blocked

Package : san_comm_sim
Inputs  : /comm_state, /SH_NN/odom, /SH_NN/camera/compressed, /SH_NN/cmd_vel
Outputs : /bridge/SH_NN/odom, /bridge/SH_NN/camera/compressed, /bridge/status
"""

import time

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from std_msgs.msg import String
from nav_msgs.msg import Odometry
from sensor_msgs.msg import CompressedImage,Image
from geometry_msgs.msg import Twist

from san_comm_msgs.msg import CommState, MeshLinkStates

from san_comm_sim.models import TrafficPolicy, TIER_NAMES


class CommTrafficFilter(Node):

    def __init__(self) -> None:
        super().__init__("comm_traffic_filter")

        # ── Parameters ───────────────────────────────────────────────────────
        self.declare_parameter("num_robots",8)
        self.declare_parameter("status_rate_hz", 1.0)
        self.declare_parameter("robot.prefix","SH_")

        num_robots   = self.get_parameter("num_robots").value
        status_rate  = self.get_parameter("status_rate_hz").value
        robot_prefix = self.get_parameter("robot.prefix").value
        self._robot_prefix = robot_prefix

        # ── Policy model ─────────────────────────────────────────────────────
        self._policy = TrafficPolicy(num_robots=num_robots)

        # ── State ────────────────────────────────────────────────────────────
        self._current_tier: int = 1   # start assuming WiFi6

        # ── Subscribers — control plane ──────────────────────────────────────
        self.create_subscription(
            CommState, "/comm_state", self._comm_state_cb, 10
        )
        self.create_subscription(
            MeshLinkStates, "/mesh_link_states", self._mesh_link_states_cb, 10
        )

        # ── Per-robot subscribers + bridge publishers ─────────────────────────
        self._bridge_odom_pubs   = {}
        self._bridge_camera_pubs = {}

        for robot_id in range(1, num_robots + 1):
            self._setup_robot_relay(robot_id)

        # ── Bridge status publisher ───────────────────────────────────────────
        self._status_pub = self.create_publisher(String, "/bridge/status", 10)
        self.create_timer(1.0 / status_rate, self._publish_status)

        self.get_logger().info(
            f"comm_traffic_filter ready — "
            f"{num_robots} robots (prefix: {robot_prefix}), initial tier: WiFi6"
        )

    # ── Robot relay setup ─────────────────────────────────────────────────────

    def _setup_robot_relay(self, robot_id: int) -> None:
        """Create subscribers + bridge publishers for one robot."""
        ns = f"{self._robot_prefix}{robot_id:02d}"

        # Subscribers
        self.create_subscription(
            Odometry,
            f"/{ns}/odom",
            lambda msg, r=robot_id: self._odom_cb(msg, r),
            10,
        )
        _camera_qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
        )
        self.create_subscription(
            Image,
            f"/{ns}/rgb_camera/image_raw",
            lambda msg, r=robot_id: self._camera_cb(msg, r),
            _camera_qos,
        )
        self.create_subscription(
            Twist,
            f"/{ns}/cmd_vel",
            lambda msg, r=robot_id: self._cmd_vel_cb(msg, r),
            10,
        )

        # Bridge publishers
        self._bridge_odom_pubs[robot_id] = self.create_publisher(
            Odometry, f"/bridge/{ns}/odom", 10
        )
        self._bridge_camera_pubs[robot_id] = self.create_publisher(
            Image, f"/bridge/{ns}/rgb_camera/image_raw", 10
        )

    # ── Control plane callbacks ───────────────────────────────────────────────

    def _comm_state_cb(self, msg: CommState) -> None:
        if msg.current_tier != self._current_tier:
            self.get_logger().info(
                f"[Bridge] Tier: "
                f"{TIER_NAMES.get(self._current_tier, '?')} → "
                f"{TIER_NAMES.get(msg.current_tier, '?')}"
            )
            self._current_tier = msg.current_tier
            self._policy.set_tier(msg.current_tier)

    def _mesh_link_states_cb(self, msg: MeshLinkStates) -> None:
        # Reserved for future use — e.g. skip relay for disconnected robots
        pass

    # ── Data plane callbacks ──────────────────────────────────────────────────

    def _odom_cb(self, msg: Odometry, robot_id: int) -> None:
        if self._policy.should_relay("odom", robot_id, time.monotonic()):
            self._bridge_odom_pubs[robot_id].publish(msg)

    def _camera_cb(self, msg: Image, robot_id: int) -> None:
        if self._policy.should_relay(
            "camera", robot_id, time.monotonic(), len(msg.data)
        ):
            self._bridge_camera_pubs[robot_id].publish(msg)

    def _cmd_vel_cb(self, msg: Twist, robot_id: int) -> None:
        # cmd_vel is not relayed to a bridge topic — it passes straight through
        # to the robot. Policy check is still applied so drops are counted.
        self._policy.should_relay("cmd_vel", robot_id, time.monotonic())

    # ── Status publisher ──────────────────────────────────────────────────────

    def _publish_status(self) -> None:
        stats = self._policy.get_total_stats()
        tier_name = TIER_NAMES.get(self._current_tier, "UNKNOWN")

        status = (
            f"tier={tier_name} "
            f"sent={stats.sent} "
            f"dropped={stats.dropped} "
            f"drop_rate={stats.drop_rate_pct:.1f}% "
            f"bytes={stats.bytes}"
        )

        msg = String()
        msg.data = status
        self._status_pub.publish(msg)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = CommTrafficFilter()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()