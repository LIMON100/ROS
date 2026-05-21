# SAN-TST-S20-7b — Mission BT Fallback Priority Injection
#
# Enhanced version of S20-7 — actually exercises the SDD §6.1
# Fallback root by injecting prioritised messages and observing
# the BT's response on /cmd_vel.
#
# This test was made possible by PDR-7a (mission_node ROS wiring
# of PriorityState fields). Before that wiring, S20-7 was a pure
# boot smoke test; now we can validate END-TO-END BT priority
# semantics in a real rclpy context.
#
# Test sequence:
#   1. Launch mission_node with tree_type=fallback, robot_role=follower
#   2. Verify baseline: no priority active → normal flow
#   3. Inject EmergencyStop (SCOPE_ALL_ROBOTS) → expect /cmd_vel zero
#   4. Inject ManualOverrideCommand RELEASE → expect emergency cleared
#   5. Inject ManualOverrideCommand CMD_VEL → expect /cmd_vel forwarded
#   6. Inject ManualOverrideCommand RELEASE → expect manual cleared

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

from geometry_msgs.msg import Twist
from std_msgs.msg import Header
from combat_robot_msgs.msg import (
    EmergencyStop, ManualOverrideCommand,
)


@pytest.mark.launch_test
@launch_testing.markers.keep_alive
def generate_test_description():
    mission = launch_ros.actions.Node(
        package="san_mission",
        executable="mission_node",
        name="mission_node",
        namespace="/robot_3",
        parameters=[{
            "tick_hz":             10.0,        # faster for test
            "min_battery_percent": 15.0,
            "initial_mode":        "recon",
            "tree_type":           "fallback",
            "robot_role":          "follower",
        }],
        output="screen",
    )
    return launch.LaunchDescription([
        mission,
        launch.actions.TimerAction(
            period=3.0,
            actions=[launch_testing.actions.ReadyToTest()],
        ),
    ]), {"mission_node": mission}


class PriorityProbe(Node):
    """Injects priority messages and observes /cmd_vel."""
    def __init__(self):
        super().__init__("priority_probe")
        ns = "/robot_3"
        self.estop_pub = self.create_publisher(
            EmergencyStop, f"{ns}/emergency_stop", 10)
        self.manual_pub = self.create_publisher(
            ManualOverrideCommand,
            f"{ns}/manual_override", 10)
        self.cmd_history = []   # [(monotonic_s, lin, ang)]
        self.create_subscription(
            Twist, f"{ns}/mission_node/cmd_vel",
            self._on_cmd, 10)

    def _on_cmd(self, msg):
        self.cmd_history.append((
            time.monotonic(),
            float(msg.linear.x),
            float(msg.angular.z),
        ))

    def inject_emergency(self):
        m = EmergencyStop()
        m.header = Header(frame_id="world")
        m.scope = EmergencyStop.SCOPE_ALL_ROBOTS
        m.reason = "test_injection"
        m.operator_id = "test_probe"
        m.timestamp_ms = int(time.time() * 1000)
        self.estop_pub.publish(m)

    def inject_manual_cmd_vel(self, lin=0.5, ang=0.1):
        m = ManualOverrideCommand()
        m.header = Header(frame_id="world")
        m.target_robot_id = 3
        m.override_type = ManualOverrideCommand.OVERRIDE_CMD_VEL
        m.cmd_vel.linear.x = lin
        m.cmd_vel.angular.z = ang
        m.max_duration_sec = 5
        m.operator_id = "test_probe"
        m.timestamp_ms = int(time.time() * 1000)
        self.manual_pub.publish(m)

    def inject_release(self):
        m = ManualOverrideCommand()
        m.header = Header(frame_id="world")
        m.target_robot_id = 3
        m.override_type = ManualOverrideCommand.OVERRIDE_RELEASE
        m.operator_id = "test_probe"
        m.timestamp_ms = int(time.time() * 1000)
        self.manual_pub.publish(m)


class TestBtPriorityInjection(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.probe = PriorityProbe()

    @classmethod
    def tearDownClass(cls):
        cls.probe.destroy_node()
        rclpy.shutdown()

    def _spin_for(self, duration_s: float):
        end = time.monotonic() + duration_s
        while time.monotonic() < end:
            rclpy.spin_once(self.probe, timeout_sec=0.05)

    def _last_cmd_within(self, window_s=1.0):
        cutoff = time.monotonic() - window_s
        recent = [
            (t, l, a) for t, l, a in self.probe.cmd_history if t > cutoff
        ]
        return recent[-1] if recent else None

    def test_priority_emergency_then_manual_then_release(self):
        # Settle phase — wait for node boot completion
        self._spin_for(1.0)

        # Phase 1: Baseline. The follower role's NormalMissionFlow
        # should not emit /cmd_vel (mission_node only emits when
        # P0 or P1 is active). So no /cmd_vel should arrive.
        baseline_n = len(self.probe.cmd_history)
        self._spin_for(0.5)
        new_during_baseline = len(self.probe.cmd_history) - baseline_n
        print(f"\n[S20-7b] baseline /cmd_vel count over 0.5s = "
              f"{new_during_baseline}")
        # Allow up to 1 spurious message — followers should mostly
        # be quiet on /cmd_vel from mission_node (other nodes emit it).
        self.assertLessEqual(
            new_during_baseline, 1,
            "mission_node emitted unexpected /cmd_vel in baseline",
        )

        # Phase 2: Inject EmergencyStop → expect /cmd_vel = (0, 0)
        self.probe.inject_emergency()
        self._spin_for(0.8)
        last = self._last_cmd_within(window_s=0.7)
        self.assertIsNotNone(
            last, "No /cmd_vel after EmergencyStop injection",
        )
        _, lin, ang = last
        print(f"[S20-7b] after EmergencyStop: cmd_vel=({lin:.2f}, {ang:.2f})")
        self.assertEqual(lin, 0.0, "Emergency cmd_vel.linear not zero")
        self.assertEqual(ang, 0.0, "Emergency cmd_vel.angular not zero")

        # Phase 3: ManualOverride RELEASE clears emergency (and
        # arms BT's WaitForRelease)
        self.probe.inject_release()
        self._spin_for(0.5)

        # Phase 4: Inject CMD_VEL manual override → expect forwarded
        self.probe.inject_manual_cmd_vel(lin=0.75, ang=0.25)
        self._spin_for(0.5)
        last = self._last_cmd_within(window_s=0.4)
        self.assertIsNotNone(
            last, "No /cmd_vel after manual_override CMD_VEL",
        )
        _, lin, ang = last
        print(f"[S20-7b] after ManualOverride CMD_VEL: "
              f"cmd_vel=({lin:.2f}, {ang:.2f})")
        self.assertAlmostEqual(lin, 0.75, delta=0.01,
            msg="Manual cmd_vel.linear not forwarded")
        self.assertAlmostEqual(ang, 0.25, delta=0.01,
            msg="Manual cmd_vel.angular not forwarded")

        # Phase 5: Release → mission_node stops emitting
        self.probe.inject_release()
        self._spin_for(0.5)
        n_before_quiet = len(self.probe.cmd_history)
        self._spin_for(0.5)
        new_quiet = len(self.probe.cmd_history) - n_before_quiet
        print(f"[S20-7b] after RELEASE: /cmd_vel count over 0.5s = "
              f"{new_quiet}")
        self.assertLessEqual(
            new_quiet, 1,
            "mission_node still emitting after RELEASE",
        )

        print("\n[S20-7b] ✅ BT priority injection sequence PASS")
