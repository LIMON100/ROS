# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

# SAN-TST-S20-2 — Leader failover test.
#
# Verifies KPP-4: Leader 강제 종료 후 새 leader_promote ≤ 10 s.
#
# IMPORTANT — Leader-bootstrap semantics:
#   The CONFIGURED leader (robot_id == leader_robot_id, default 1) does
#   NOT broadcast LEADER_PROMOTED on startup. By design, it leads
#   implicitly; only failover triggers a broadcast (see
#   leader_role_manager.cpp watchdogTick — `if (is_leader_) return`).
#   Followers wait for a heartbeat from the leader, and on timeout the
#   highest-priority candidate self-promotes and broadcasts.
#
#   So this test:
#     - Hardcodes the initial leader to robot 1 (matches squadron config).
#     - Force-kills robot_1's role_management process.
#     - With no leader heartbeat received, robots 2/3/4 enter the
#       grace window and the Deputy (robot 2) self-promotes.
#     - The test waits for the FIRST LeaderRoleAnnouncement with
#       role == LEADER_PROMOTED and asserts dt(kill → announce) ≤ 10 s.
#
#   The previous version asserted "some robot must self-broadcast within
#   5 s of startup" — that contradicted production semantics and never
#   triggered because no robot publishes status to bootstrap heartbeat
#   discovery in this minimal test fixture. SOP-CI-001 §3 unmasked it.
#
# Run standalone:
#   launch_test test_s20_2_leader_failover.py

import os
import signal
import time
import unittest

import launch
import launch.actions
import launch_ros.actions
import launch_testing.actions
import launch_testing.markers
import pytest
import rclpy
from combat_robot_msgs.msg import LeaderRoleAnnouncement, RobotStatus
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

KPP4_DEADLINE_S = 10.0

# Topic / msg corrected from /swarm/leader_id (UInt8) — that topic
# was never produced by any node. The real signal is the
# /swarm/leader/role_announce LeaderRoleAnnouncement broadcast, which
# leader_role_manager.cpp:76 publishes with TRANSIENT_LOCAL QoS.
# A robot is "the leader" when its announcement carries role ==
# LEADER_PROMOTED. SOP-CI-001 §3 unmasked the original topic-name
# mismatch.


def _make_role_manager(robot_id):
    """Construct one role_management_node for the given robot_id.
    (Package executable is `role_management_node`; see
    san_role_management/CMakeLists.txt:34. squadron.launch.py uses the
    same name. The earlier `role_manager_node` here was a typo that
    only surfaced once SOP-CI-001 §3 stopped masking real failures.)"""
    return launch_ros.actions.Node(
        package="san_role_management",
        executable="role_management_node",
        name=f"role_manager_{robot_id}",
        namespace=f"/robot_{robot_id}",
        parameters=[{"robot_id": robot_id}],
        output="screen",
    )


@pytest.mark.launch_test
@launch_testing.markers.keep_alive
def generate_test_description():
    """Launch 4 role-manager nodes; leader emerges via election."""
    nodes = [_make_role_manager(rid) for rid in (1, 2, 3, 4)]
    return launch.LaunchDescription(
        nodes + [
            launch.actions.TimerAction(
                period=3.0,
                actions=[launch_testing.actions.ReadyToTest()],
            ),
        ]
    ), {f"role_manager_{rid}": nodes[rid - 1] for rid in (1, 2, 3, 4)}


class LeaderProbe(Node):
    """Subscriber that records LEADER_PROMOTED announcements + a status
    publisher that simulates the missing operation_control_node side of
    the squadron.

    Two bootstrap requirements (both must be satisfied or no follower
    can self-promote — see leader_role_manager.cpp):

      1) /swarm/robot_status from robot_id == leader_robot_id (=1)
         updates each follower's `last_leader_heartbeat_`. Without it,
         watchdogTick short-circuits on `!has_value()`.

      2) /swarm/robot_status from robot_id == self.robot_id_ updates
         each follower's own snapshot in battery_monitor_. Without it,
         determineMyPriority returns LIMP_MODE because
         `battery_monitor_.get(robot_id_).robot_id == 0`.

    So the probe publishes one status per robot at 10 Hz, with the
    fields that the priority logic reads (battery_percent ≥ min,
    sbc1_healthy, sbc2_healthy, is_deputy_ugv flag set for r=3).

    On stop_leader_heartbeat() the probe stops publishing for robot 1
    only — followers continue to see their own status, but the leader
    silence triggers the failover watchdog within ~1.4 s and the
    Deputy (priority 1) self-promotes."""

    LEADER_ROBOT_ID = 1
    HUB_ROBOT_ID    = 2
    DEPUTY_ROBOT_ID = 3
    ALL_ROBOTS      = (1, 2, 3, 4)

    def __init__(self):
        super().__init__("leader_probe")
        self.history = []   # [(timestamp_s, leader_id)]
        ann_qos = QoSProfile(
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self.create_subscription(
            LeaderRoleAnnouncement, "/swarm/leader/role_announce",
            self._on_announce, ann_qos,
        )

        self.status_pub = self.create_publisher(
            RobotStatus, "/swarm/robot_status", 10)
        # Which robots are still "alive" (publishing heartbeats).
        self._alive = set(self.ALL_ROBOTS)
        # 10 Hz — well below the 1400 ms LEADER_HEARTBEAT_TIMEOUT_MS.
        self._heartbeat_timer = self.create_timer(0.1, self._tick)

    def _on_announce(self, msg):
        if msg.role == LeaderRoleAnnouncement.LEADER_PROMOTED:
            self.history.append((time.monotonic(), msg.robot_id))

    def _make_status(self, robot_id: int) -> RobotStatus:
        s = RobotStatus()
        s.robot_id = robot_id
        s.is_deputy_ugv = (robot_id == self.DEPUTY_ROBOT_ID)
        # battery_percent + sbc1/sbc2 must be present for
        # determineMyPriority to clear DEPUTY / HUB / BATTERY_MAX gates.
        s.battery_percent = 80.0
        s.sbc1_healthy = True
        s.sbc2_healthy = True
        s.timestamp_ms = int(time.time() * 1000)
        return s

    def _tick(self):
        for rid in tuple(self._alive):
            self.status_pub.publish(self._make_status(rid))

    def stop_leader_heartbeat(self):
        """Simulate robot 1's death — stop its status broadcast only."""
        self._alive.discard(self.LEADER_ROBOT_ID)


class TestLeaderFailover(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.probe = LeaderProbe()

    @classmethod
    def tearDownClass(cls):
        cls.probe.destroy_node()
        rclpy.shutdown()

    def test_01_initial_leader_is_robot_1_by_config(self):
        """Initial leader is the CONFIGURED leader (robot_id=1).
        Spin briefly to bootstrap heartbeat reception; no LEADER_PROMOTED
        announcement should arrive while robot 1 is "alive" (the probe
        keeps publishing its status) — robot 1 leads implicitly."""
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            rclpy.spin_once(self.probe, timeout_sec=0.1)
        self.assertEqual(
            self.probe.history, [],
            "No LEADER_PROMOTED announcement expected pre-failover; "
            f"got {self.probe.history}"
        )

    def test_02_kill_leader_triggers_reelection_within_10s(
        self, role_manager_1, role_manager_2,
        role_manager_3, role_manager_4
    ):
        """Force-kill the configured leader (robot 1); a new leader must
        broadcast LEADER_PROMOTED within KPP4_DEADLINE_S."""
        initial_leader = 1   # squadron config: robot_id=1 is leader
        node_map = {
            1: role_manager_1, 2: role_manager_2,
            3: role_manager_3, 4: role_manager_4,
        }
        leader_proc = node_map[initial_leader]

        t_kill = time.monotonic()
        # Two-step simulation: (1) stop publishing robot_1's heartbeat
        # so followers see leader-silence and start the timeout count,
        # (2) SIGKILL robot_1's role_management process for realism.
        # Step (1) is what actually triggers re-election in this fixture
        # (the probe is the only source of /swarm/robot_status); step
        # (2) just makes the scenario faithful to a HW failure.
        self.probe.stop_leader_heartbeat()
        os.kill(leader_proc.process_details["pid"], signal.SIGKILL)

        # Spin until /swarm/leader_id changes or we hit the deadline.
        deadline = t_kill + KPP4_DEADLINE_S
        new_leader_id = None
        new_leader_t  = None
        while time.monotonic() < deadline:
            rclpy.spin_once(self.probe, timeout_sec=0.2)
            for ts, lid in self.probe.history:
                if ts > t_kill and lid != initial_leader:
                    new_leader_id = lid
                    new_leader_t  = ts
                    break
            if new_leader_id is not None:
                break

        self.assertIsNotNone(
            new_leader_id,
            f"No new leader within {KPP4_DEADLINE_S} s "
            f"(initial leader was {initial_leader})"
        )
        elapsed = new_leader_t - t_kill
        self.assertLessEqual(
            elapsed, KPP4_DEADLINE_S,
            f"Re-election took {elapsed:.2f} s "
            f"(KPP-4 limit {KPP4_DEADLINE_S} s)"
        )
        print(f"\n[TST S20-2] Re-election: {elapsed:.2f}s "
              f"(was {initial_leader} → now {new_leader_id})")
