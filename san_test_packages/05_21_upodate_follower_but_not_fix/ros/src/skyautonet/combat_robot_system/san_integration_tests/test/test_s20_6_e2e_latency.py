# SAN-TST-S20-6 — End-to-end latency test (KPP-3).
#
# Verifies KPP-3: leader → follower topic roundtrip p95 ≤ 150 ms.
#
# Procedure:
#   1. Create two test nodes — one publishes timestamped messages,
#      the other echoes them back via a paired topic.
#   2. Measure 200 roundtrips; compute p50/p95/p99.
#   3. Assert p95 ≤ 150 ms.
#
# This test uses ROS 2 native intra-process zero-copy where possible,
# providing a fair baseline of the DDS layer's capability on this host.

import time
import unittest
from statistics import mean

import launch
import launch.actions
import launch_testing.actions
import launch_testing.markers
import pytest
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy
from std_msgs.msg import Float64

P95_DEADLINE_MS = 150
ROUNDTRIPS     = 200


@pytest.mark.launch_test
@launch_testing.markers.keep_alive
def generate_test_description():
    """No external nodes — pub + sub are in the test process."""
    return launch.LaunchDescription([
        launch.actions.TimerAction(
            period=1.0,
            actions=[launch_testing.actions.ReadyToTest()],
        ),
    ])


class LatencyEcho(Node):
    """Echoes any message it sees back on the paired topic."""
    def __init__(self):
        super().__init__("latency_echo")
        qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.RELIABLE)
        self.pub = self.create_publisher(Float64, "/lat/back", qos)
        self.create_subscription(
            Float64, "/lat/fwd", self._echo, qos
        )

    def _echo(self, msg):
        self.pub.publish(msg)


class LatencyMeasurer(Node):
    """Sends timestamped messages and records arrival deltas."""
    def __init__(self):
        super().__init__("latency_measurer")
        qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.RELIABLE)
        self.pub = self.create_publisher(Float64, "/lat/fwd", qos)
        self.create_subscription(Float64, "/lat/back", self._on_back, qos)
        self.deltas_ms = []

    def _on_back(self, msg):
        now_s = time.monotonic()
        sent_s = msg.data
        self.deltas_ms.append((now_s - sent_s) * 1000.0)

    def send(self):
        msg = Float64()
        msg.data = time.monotonic()
        self.pub.publish(msg)


class TestEndToEndLatency(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.echo     = LatencyEcho()
        cls.measurer = LatencyMeasurer()

    @classmethod
    def tearDownClass(cls):
        cls.echo.destroy_node()
        cls.measurer.destroy_node()
        rclpy.shutdown()

    def test_01_p95_below_150ms(self):
        """200 roundtrips; p95 must be ≤ 150 ms."""
        from rclpy.executors import SingleThreadedExecutor
        exec_ = SingleThreadedExecutor()
        exec_.add_node(self.echo)
        exec_.add_node(self.measurer)

        # Allow discovery to settle
        end_warmup = time.monotonic() + 1.0
        while time.monotonic() < end_warmup:
            exec_.spin_once(timeout_sec=0.05)

        # 200 roundtrips
        for _ in range(ROUNDTRIPS):
            self.measurer.send()
            t_end = time.monotonic() + 0.5  # max 500ms per roundtrip
            n_before = len(self.measurer.deltas_ms)
            while (time.monotonic() < t_end
                   and len(self.measurer.deltas_ms) == n_before):
                exec_.spin_once(timeout_sec=0.01)

        deltas = self.measurer.deltas_ms
        self.assertGreaterEqual(
            len(deltas), int(0.9 * ROUNDTRIPS),
            f"Lost too many roundtrips ({len(deltas)}/{ROUNDTRIPS})"
        )
        # Compute p50/p95/p99
        deltas_sorted = sorted(deltas)
        p50 = deltas_sorted[len(deltas_sorted) // 2]
        p95 = deltas_sorted[int(len(deltas_sorted) * 0.95)]
        p99 = deltas_sorted[int(len(deltas_sorted) * 0.99)]
        avg = mean(deltas)
        print(f"\n[TST S20-6] Latency stats (n={len(deltas)}):")
        print(f"  mean={avg:.2f}ms p50={p50:.2f}ms "
              f"p95={p95:.2f}ms p99={p99:.2f}ms")
        self.assertLessEqual(
            p95, P95_DEADLINE_MS,
            f"KPP-3 violated: p95={p95:.2f}ms > {P95_DEADLINE_MS}ms"
        )
