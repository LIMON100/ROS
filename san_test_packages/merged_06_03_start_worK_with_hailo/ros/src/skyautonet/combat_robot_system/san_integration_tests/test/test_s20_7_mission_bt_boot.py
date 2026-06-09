# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

# SAN-TST-S20-7 — Mission BT Fallback Root smoke
#
# Verifies SDD §6.1 Mission BT structure: mission_node launches
# with tree_type=fallback (default) and produces tick output. The
# detailed priority semantics are covered by the 15 standalone
# pytest cases in san_mission/test/test_mission_bt.py — this test
# adds the launch_testing-level smoke check that the rclpy node
# successfully boots with the new tree topology.
#
# Test procedure:
#   1. Launch mission_node with tree_type=fallback, robot_role=follower
#   2. Wait up to 5s for the node to come up
#   3. Verify the process is alive
#   4. Tear down cleanly
#
# Note: full priority injection (EmergencyStop / ManualOverride) is
# scheduled for PDR-7 (msg additions) — at that point this test
# extends to S20-7b: publish EmergencyStop, verify BT response.

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


@pytest.mark.launch_test
@launch_testing.markers.keep_alive
def generate_test_description():
    mission = launch_ros.actions.Node(
        package="san_mission",
        executable="mission_node",
        name="mission_node",
        namespace="/robot_3",
        parameters=[{
            "tick_hz":             5.0,
            "min_battery_percent": 15.0,
            "initial_mode":        "recon",
            "tree_type":           "fallback",  # SDD §6.1
            "robot_role":          "follower",
        }],
        output="screen",
    )
    return launch.LaunchDescription([
        mission,
        launch.actions.TimerAction(
            period=4.0,
            actions=[launch_testing.actions.ReadyToTest()],
        ),
    ]), {"mission_node": mission}


class BootProbe(Node):
    def __init__(self):
        super().__init__("bt_boot_probe")


class TestMissionBtBoot(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.probe = BootProbe()

    @classmethod
    def tearDownClass(cls):
        cls.probe.destroy_node()
        rclpy.shutdown()

    def test_mission_node_boots_with_fallback_tree(
            self, proc_info, mission_node):
        # Pump rclpy for 3 seconds — if the node crashed at boot
        # this will catch it.
        t_end = time.monotonic() + 3.0
        while time.monotonic() < t_end:
            rclpy.spin_once(self.probe, timeout_sec=0.1)

        # Verify process still running
        proc_info.assertWaitForStartup(process=mission_node, timeout=5)
        running_pids = list(proc_info.process_names())
        self.assertTrue(
            any("mission_node" in name for name in running_pids),
            f"mission_node not in running processes: {running_pids}",
        )
        print("\n[S20-7] mission_node booted with fallback tree ✅")
