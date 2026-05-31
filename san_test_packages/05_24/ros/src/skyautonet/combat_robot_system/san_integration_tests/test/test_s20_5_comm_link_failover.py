# SAN-TST-S20-5 — Comm Link failover test (WiFi6 ↔ LTE).
#
# Verifies the hysteresis state machine in san_comm_link::LinkHealthMonitor
# (N_FAIL consecutive failures → switch to LTE; N_OK successes → revert).
# Unit-level state machine coverage lives in 12 standalone gtests
# (K1-K12); this integration test validates the deployment shape and
# inbound LTE-status wiring so an end-to-end regression catches them.
#
# Strict-mode design (vs. the previous soft-fallback):
#   - The exact /robot_1/comm_link_node/status topic MUST appear.
#   - A simulated LTE-registered + PDP-active LteModemStatus is
#     injected; the comm_link_node MUST then publish at least one
#     CommLinkStatus with lte_ok=True. Proves the LTE subscription
#     is wired, not just that the publisher exists.
#
# The WiFi6 probe targets a real public host (1.1.1.1:443). On CI
# runners without network egress that probe will fail every tick,
# which deterministically drives the state machine toward LTE
# inside `consec_fail_to_downgrade` ticks — the test waits long
# enough to see the switch, but does not REQUIRE active_link == LTE
# (since runners with full egress may keep WiFi6).

import time
import unittest

import launch
import launch.actions
import launch_ros.actions
import launch_testing.actions
import launch_testing.markers
import pytest
import rclpy
from combat_robot_msgs.msg import CommLinkStatus, LteModemStatus
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy


STATUS_TOPIC      = "/robot_1/comm_link_node/status"
LTE_TOPIC         = "/lte/modem_status"
TOPIC_DISCOVERY_S = 8.0
LTE_PROPAGATE_S   = 6.0     # 1 Hz tick → up to a few ticks to absorb


@pytest.mark.launch_test
@launch_testing.markers.keep_alive
def generate_test_description():
    return launch.LaunchDescription([
        launch_ros.actions.Node(
            package="san_comm_link",
            executable="comm_link_node",
            name="comm_link_node",
            namespace="/robot_1",
            output="screen",
        ),
        launch.actions.TimerAction(
            period=3.0,
            actions=[launch_testing.actions.ReadyToTest()],
        ),
    ])


class CommLinkProbe(Node):
    def __init__(self):
        super().__init__("comm_link_probe")
        self.statuses = []  # list of CommLinkStatus

        status_qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self.create_subscription(
            CommLinkStatus, STATUS_TOPIC,
            self._on_status, status_qos
        )
        lte_qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.BEST_EFFORT,
        )
        self.lte_pub = self.create_publisher(
            LteModemStatus, LTE_TOPIC, lte_qos
        )

    def _on_status(self, msg):
        self.statuses.append(msg)

    def push_lte_up(self):
        m = LteModemStatus()
        m.registered = LteModemStatus.REG_HOME
        m.pdp_active = True
        self.lte_pub.publish(m)


class TestCommLinkFailover(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.probe = CommLinkProbe()

    @classmethod
    def tearDownClass(cls):
        cls.probe.destroy_node()
        rclpy.shutdown()

    def _wait_for_topic(self, name, timeout_s):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rclpy.spin_once(self.probe, timeout_sec=0.2)
            topics = dict(self.probe.get_topic_names_and_types())
            if name in topics:
                return True
        return False

    def test_01_status_topic_present(self):
        """comm_link_node MUST publish on /robot_1/comm_link_node/status."""
        self.assertTrue(
            self._wait_for_topic(STATUS_TOPIC, TOPIC_DISCOVERY_S),
            f"{STATUS_TOPIC} not in graph after {TOPIC_DISCOVERY_S}s — "
            "comm_link_node failed to come up"
        )

    def test_02_first_status_published(self):
        """At least one CommLinkStatus must arrive within a few ticks."""
        deadline = time.monotonic() + TOPIC_DISCOVERY_S
        while time.monotonic() < deadline:
            rclpy.spin_once(self.probe, timeout_sec=0.2)
            if self.probe.statuses:
                break
        self.assertTrue(
            self.probe.statuses,
            f"No CommLinkStatus received within {TOPIC_DISCOVERY_S}s"
        )

    def test_03_lte_status_subscription_observed(self):
        """Push a synthetic 'LTE up' message; comm_link_node should
        absorb it on /lte/modem_status and surface lte_ok=True in a
        subsequent CommLinkStatus tick. Proves the inbound LTE
        subscription is wired."""
        # Push a few LTE up messages — BEST_EFFORT means we want
        # multiple shots; the 1 Hz tick on the node side absorbs one
        # within the next second.
        snapshot_count = len(self.probe.statuses)
        for _ in range(3):
            self.probe.push_lte_up()
            rclpy.spin_once(self.probe, timeout_sec=0.1)
            time.sleep(0.2)

        deadline = time.monotonic() + LTE_PROPAGATE_S
        saw_lte_ok = False
        while time.monotonic() < deadline:
            rclpy.spin_once(self.probe, timeout_sec=0.2)
            new = self.probe.statuses[snapshot_count:]
            if any(s.lte_ok for s in new):
                saw_lte_ok = True
                break

        self.assertTrue(
            saw_lte_ok,
            f"Injected LTE-up never appeared in CommLinkStatus.lte_ok "
            f"within {LTE_PROPAGATE_S}s; received "
            f"{len(self.probe.statuses) - snapshot_count} new status msgs"
        )
        # active_link may stay WIFI6 if the runner has network access
        # to the probe target (1.1.1.1:443). We just log it for triage.
        last = self.probe.statuses[-1]
        print(f"\n[TST S20-5] last status: active_link={last.active_link} "
              f"wifi6_ok={last.wifi6_ok} lte_ok={last.lte_ok} "
              f"switches={last.switch_count}")
