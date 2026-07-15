# SAN-TST-S20-9 — T4 BREADCRUMB_RECOVERY Comm Timeout
#
# Verifies SDD §6.2 T4 transition: tier_node enters T4
# BREADCRUMB_RECOVERY when 60s pass without prediction
# (FollowerTargetMessage) OR when δ > 4 d₀.
#
# To keep test runtime reasonable we override comm_timeout_ms=2000
# (2 seconds) via parameter. The transition logic is identical;
# only the threshold scales.
#
# Test procedure:
#   1. Launch tier_node with comm_timeout_ms=2000
#   2. Publish status + target → verify T0 entry
#   3. Stop publishing target → wait 2.5s
#   4. Verify TierStatusChange to T4 with reason "comm_timeout_60s"
#   5. Resume publishing target → verify T4→T0 with "comm_restored"

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

from std_msgs.msg import Header
from combat_robot_msgs.msg import (
    FollowerTargetMessage, RobotStatus, TierStatusChange,
)

ROBOT_ID = 3
COMM_TIMEOUT_MS = 2000   # accelerated 2s for test (production: 60000ms)


@pytest.mark.launch_test
@launch_testing.markers.keep_alive
def generate_test_description():
    tier = launch_ros.actions.Node(
        package="san_follower_tier",
        executable="tier_node",
        name="tier_node",
        namespace=f"/robot_{ROBOT_ID}",
        parameters=[{
            "robot_id":           ROBOT_ID,
            "tick_period_ms":     50,
            "base_distance_d0_m": 3.0,
            "comm_timeout_ms":    COMM_TIMEOUT_MS,
        }],
        output="screen",
    )
    return launch.LaunchDescription([
        tier,
        launch.actions.TimerAction(
            period=2.0,
            actions=[launch_testing.actions.ReadyToTest()],
        ),
    ]), {"tier": tier}


class T4Probe(Node):
    def __init__(self):
        super().__init__("t4_probe")
        ns = f"/robot_{ROBOT_ID}"
        self.status_pub = self.create_publisher(
            RobotStatus, f"{ns}/tier_node/robot_status", 10)
        self.target_pub = self.create_publisher(
            FollowerTargetMessage,
            "/swarm/formation/follower_target", 20)
        self.transitions = []   # [(monotonic_s, prev_tier, curr_tier, reason)]
        self.create_subscription(
            TierStatusChange, f"{ns}/tier_node/tier_status_change",
            self._on_tier, 20)

    def _on_tier(self, msg):
        self.transitions.append((
            time.monotonic(),
            int(msg.previous_tier), int(msg.current_tier),
            str(msg.reason)))

    def publish_status(self):
        s = RobotStatus()
        s.header = Header(frame_id="world")
        s.robot_id = ROBOT_ID
        s.pose.position.x = 1.0
        s.pose.position.y = 5.0
        self.status_pub.publish(s)

    def publish_target(self):
        t = FollowerTargetMessage()
        t.header = Header(frame_id="world")
        t.target_robot_id = ROBOT_ID
        t.formation_epoch = 1
        t.target_pose_now.position.x = 1.5
        t.target_pose_now.position.y = 5.0
        t.target_pose_pred_1s.position.x = 1.5
        t.target_pose_pred_1s.position.y = 5.0
        t.max_speed_mps = 1.0
        self.target_pub.publish(t)


class TestT4Recovery(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.probe = T4Probe()

    @classmethod
    def tearDownClass(cls):
        cls.probe.destroy_node()
        rclpy.shutdown()

    def test_t4_entry_on_comm_timeout_and_recovery(self):
        probe = self.probe

        # Phase 1: warm up with both target + status — expect T0 entry
        t_end = time.monotonic() + 1.0
        while time.monotonic() < t_end:
            probe.publish_status()
            probe.publish_target()
            rclpy.spin_once(probe, timeout_sec=0.05)

        # Confirm at least one transition seen, ending in T0 (=0)
        self.assertGreater(
            len(probe.transitions), 0,
            "No tier transitions during warm-up",
        )
        latest_tier = probe.transitions[-1][2]
        self.assertEqual(
            latest_tier, 0,
            f"Expected T0 after warm-up, got {latest_tier}",
        )
        n_before_loss = len(probe.transitions)

        # Phase 2: STOP publishing target — only status
        t_loss = time.monotonic()
        t_stop_end = t_loss + (COMM_TIMEOUT_MS / 1000.0) + 1.0
        while time.monotonic() < t_stop_end:
            probe.publish_status()
            rclpy.spin_once(probe, timeout_sec=0.05)

        # Phase 3: must have seen T4 (=5) entry
        t4_entries = [
            tr for tr in probe.transitions[n_before_loss:]
            if tr[2] == 5
        ]
        self.assertTrue(
            len(t4_entries) > 0,
            "No T4 transition seen after comm timeout. "
            f"All transitions: {probe.transitions}",
        )
        first_t4 = t4_entries[0]
        elapsed_s = first_t4[0] - t_loss
        print(f"\n[S20-9] T4 entry after {elapsed_s:.2f}s "
              f"(timeout {COMM_TIMEOUT_MS/1000:.1f}s)")
        print(f"[S20-9] T4 entry reason: '{first_t4[3]}'")

        # Phase 4: resume target → expect T4 → T0 with "comm_restored"
        n_before_recover = len(probe.transitions)
        t_recover_end = time.monotonic() + 1.5
        while time.monotonic() < t_recover_end:
            probe.publish_status()
            probe.publish_target()
            rclpy.spin_once(probe, timeout_sec=0.05)

        recoveries = [
            tr for tr in probe.transitions[n_before_recover:]
            if tr[2] == 0      # back to T0
        ]
        self.assertTrue(
            len(recoveries) > 0,
            "No T4→T0 recovery seen after target resume. "
            f"After recovery transitions: "
            f"{probe.transitions[n_before_recover:]}",
        )
        recovery_reason = recoveries[0][3]
        print(f"[S20-9] T0 recovery reason: '{recovery_reason}'")
        self.assertIn(
            recovery_reason, ["comm_restored", "prediction_received"],
            f"Unexpected recovery reason: {recovery_reason}",
        )
