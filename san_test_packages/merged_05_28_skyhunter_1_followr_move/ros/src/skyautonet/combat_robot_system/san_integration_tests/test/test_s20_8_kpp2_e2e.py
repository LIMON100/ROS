# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

# SAN-TST-S20-8 — KPP-2 End-to-End Timing
#
# Verifies KPP-2: 회피 반응 ≤ 300ms.
#
# Critical path measured END-TO-END:
#   cost_map_update publish  →  /cmd_vel evasion command publish
#
# Test procedure:
#   1. Launch reroute_node + tier_node for robot_id=3
#   2. Publish (and keep publishing) a robot_status at origin
#   3. Publish a follower_target with target_pose_pred_1s in front
#   4. Publish a CostMapUpdate with a lethal cell on the predicted path
#      → record t_map (publication timestamp)
#   5. Wait for /cmd_vel emission with angular ≠ 0 (evasion response)
#      → record t_cmd (subscription timestamp)
#   6. Assert: (t_cmd - t_map) ≤ 300 ms
#
# This is the PDR evidence test for KPP-2.

import time
import unittest

import launch
import launch.actions
import launch_ros.actions
import launch_testing.actions
import launch_testing.markers
import pytest
import rclpy
from rclpy.node import Node

from geometry_msgs.msg import Pose2D, Twist
from std_msgs.msg import Bool, Header
from combat_robot_msgs.msg import (
    CostMapUpdate, FollowerTargetMessage, RobotStatus,
)

KPP2_DEADLINE_MS = 300.0
ROBOT_ID = 3

# Test grid geometry — 100×100 cells × 0.1m = 10m × 10m
GRID_W = 100
GRID_H = 100
GRID_RES = 0.1
ORIGIN_X = 0.0
ORIGIN_Y = 0.0


def _make_free_grid():
    """All-free cost grid (raw uint8 — decodeCostGrid raw path)."""
    return [0] * (GRID_W * GRID_H)


def _paint_rect(grid, x_lo, y_lo, x_hi, y_hi, cost):
    """Mark grid cells in world-coord rectangle to given cost."""
    gx_lo = int((x_lo - ORIGIN_X) / GRID_RES)
    gy_lo = int((y_lo - ORIGIN_Y) / GRID_RES)
    gx_hi = int((x_hi - ORIGIN_X) / GRID_RES)
    gy_hi = int((y_hi - ORIGIN_Y) / GRID_RES)
    for gy in range(max(0, gy_lo), min(GRID_H, gy_hi + 1)):
        for gx in range(max(0, gx_lo), min(GRID_W, gx_hi + 1)):
            grid[gy * GRID_W + gx] = cost


@pytest.mark.launch_test
@launch_testing.markers.keep_alive
def generate_test_description():
    tier = launch_ros.actions.Node(
        package="san_follower_tier",
        executable="tier_node",
        name="tier_node",
        namespace=f"/robot_{ROBOT_ID}",
        parameters=[{
            "robot_id":       ROBOT_ID,
            "tick_period_ms": 50,           # accelerated tick
            "base_distance_d0_m": 3.0,
        }],
        output="screen",
    )
    reroute = launch_ros.actions.Node(
        package="san_reroute_planner",
        executable="reroute_node",
        name="reroute_node",
        namespace=f"/robot_{ROBOT_ID}",
        parameters=[{
            "robot_id":                  ROBOT_ID,
            "tick_period_ms":            50,
            "evasion_linear_speed_mps": 1.0,
            "evasion_angular_max_rps":  1.5,
        }],
        output="screen",
    )
    return launch.LaunchDescription([
        tier, reroute,
        launch.actions.TimerAction(
            period=2.0,
            actions=[launch_testing.actions.ReadyToTest()],
        ),
    ]), {"tier": tier, "reroute": reroute}


class KppProbe(Node):
    """Probe — publishes inputs, records /cmd_vel arrival time."""
    def __init__(self):
        super().__init__("kpp2_probe")
        ns = f"/robot_{ROBOT_ID}"
        # Inputs (publish toward the nodes' input topics)
        self.status_pub = self.create_publisher(
            RobotStatus, f"{ns}/reroute_node/robot_status", 10)
        self.target_pub = self.create_publisher(
            FollowerTargetMessage,
            "/swarm/formation/follower_target", 20)
        self.cost_pub = self.create_publisher(
            CostMapUpdate, f"{ns}/reroute_node/cost_map_update", 5)

        # Outputs (observe)
        self.cmd_history = []   # [(monotonic_s, linear_x, angular_z)]
        self.obs_history = []   # [(monotonic_s, obstacle_bool)]
        self.create_subscription(
            Twist, f"{ns}/reroute_node/cmd_vel", self._on_cmd, 10)
        self.create_subscription(
            Bool, f"{ns}/reroute_node/obstacle_on_path",
            self._on_obs, 5)

        self.t_lethal_publish_s = None   # set when we publish lethal map

    def _on_cmd(self, msg):
        self.cmd_history.append((
            time.monotonic(), msg.linear.x, msg.angular.z))

    def _on_obs(self, msg):
        self.obs_history.append((time.monotonic(), bool(msg.data)))

    def publish_status(self):
        s = RobotStatus()
        s.header = Header()
        s.header.frame_id = "world"
        s.robot_id = ROBOT_ID
        s.pose.position.x = 1.0
        s.pose.position.y = 5.0
        self.status_pub.publish(s)

    def publish_target(self):
        t = FollowerTargetMessage()
        t.header = Header()
        t.header.frame_id = "world"
        t.target_robot_id = ROBOT_ID
        t.formation_epoch = 1
        # 1-second prediction: pose_now and pose_pred_1s along +x
        t.target_pose_now.position.x = 1.0
        t.target_pose_now.position.y = 5.0
        t.target_pose_pred_1s.position.x = 9.0
        t.target_pose_pred_1s.position.y = 5.0
        t.max_speed_mps = 1.0
        t.lead_bias_s = 0.1
        self.target_pub.publish(t)

    def publish_costmap(self, with_lethal: bool):
        """Publish a cost grid. If with_lethal=True, plant a lethal
        rectangle squarely on the predicted path from (1, 5) to (9, 5)."""
        grid = _make_free_grid()
        if with_lethal:
            _paint_rect(grid, 4.8, 4.7, 5.2, 5.3, 254)
        m = CostMapUpdate()
        m.header = Header()
        m.header.frame_id = "world"
        m.sequence = 0
        m.robot_id = str(ROBOT_ID)
        m.cost_grid_png = grid    # raw uint8 (decodeCostGrid raw fallback)
        m.origin = Pose2D(x=ORIGIN_X, y=ORIGIN_Y, theta=0.0)
        m.resolution_m = GRID_RES
        m.width_cells = GRID_W
        m.height_cells = GRID_H
        m.timestamp_ms = int(time.time() * 1000)
        m.computed_at_ms = m.timestamp_ms
        m.lethal_count = sum(1 for c in grid if c == 254)
        m.inflated_count = 0
        self.cost_pub.publish(m)
        if with_lethal:
            self.t_lethal_publish_s = time.monotonic()


class TestKpp2EndToEnd(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.probe = KppProbe()

    @classmethod
    def tearDownClass(cls):
        cls.probe.destroy_node()
        rclpy.shutdown()

    def test_kpp2_evasion_response_under_300ms(self):
        probe = self.probe

        # Phase 1: warm up nodes with status + target + free cost map.
        # Pump at 20 Hz for 1 second so all subscriptions settle.
        t_warm_end = time.monotonic() + 1.0
        while time.monotonic() < t_warm_end:
            probe.publish_status()
            probe.publish_target()
            probe.publish_costmap(with_lethal=False)
            rclpy.spin_once(probe, timeout_sec=0.05)

        # Sanity: no obstacle reported yet
        recent_obs = [b for _, b in probe.obs_history[-5:]]
        self.assertTrue(
            all(not b for b in recent_obs) or len(recent_obs) == 0,
            f"Obstacle reported while map was free: {recent_obs}",
        )

        # Phase 2: inject lethal cell — record timestamp
        probe.publish_costmap(with_lethal=True)
        t_inject_s = probe.t_lethal_publish_s
        self.assertIsNotNone(t_inject_s)

        # Phase 3: wait for /cmd_vel evasion command (angular ≠ 0)
        deadline_s = t_inject_s + (KPP2_DEADLINE_MS / 1000.0)
        evasion_cmd = None
        while time.monotonic() < deadline_s + 0.5:
            # Keep pumping inputs so nodes don't lose track
            probe.publish_status()
            probe.publish_target()
            rclpy.spin_once(probe, timeout_sec=0.02)
            # Look for cmd_vel with non-zero angular (evasion turn)
            for t_s, _lin, ang in probe.cmd_history:
                if t_s > t_inject_s and abs(ang) > 1e-3:
                    evasion_cmd = (t_s, ang)
                    break
            if evasion_cmd is not None:
                break

        self.assertIsNotNone(
            evasion_cmd,
            "No /cmd_vel evasion command received within deadline",
        )
        t_cmd_s, ang = evasion_cmd
        elapsed_ms = (t_cmd_s - t_inject_s) * 1000.0

        print(f"\n[KPP-2 E2E] cost_map → /cmd_vel evasion = "
              f"{elapsed_ms:.1f} ms (budget {KPP2_DEADLINE_MS:.0f} ms)")
        print(f"[KPP-2 E2E] evasion angular = {ang:+.3f} rad/s")

        self.assertLessEqual(
            elapsed_ms, KPP2_DEADLINE_MS,
            f"KPP-2 violated: {elapsed_ms:.1f} ms > "
            f"{KPP2_DEADLINE_MS:.0f} ms",
        )
