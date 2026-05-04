"""
Jammer Service Test
Package: skyhunter_comm/test/
Target: Jam a link → verify metrics show degraded/disconnected (100+ samples)

Usage:
    python3 test_jammer.py
    python3 test_jammer.py --robot-a 1 --robot-b 2 --duration 30 --samples 100
"""

import argparse
import statistics
import sys
import time

import rclpy
from rclpy.node import Node
from skyhunter_msgs.msg import MeshMetrics
from skyhunter_msgs.srv import JamLink


BASELINE_SAMPLES = 20


class JammerTester(Node):

    def __init__(self, robot_a: str, robot_b: str, jam_duration: float,
                 attenuation_db: float, target_samples: int, timeout_s: float):
        super().__init__('jammer_tester')
        self.robot_a        = robot_a
        self.robot_b        = robot_b
        self.jam_duration   = jam_duration
        self.attenuation_db = attenuation_db
        self.target_samples = target_samples
        self.timeout_s      = timeout_s

        self.baseline_samples = []
        self.jammed_samples   = []

        self.create_subscription(MeshMetrics, '/mesh_metrics', self._cb, 10)
        self.jam_client = self.create_client(JamLink, '/jam_link')

        self.get_logger().info(
            f'Jammer test — {robot_a}↔{robot_b}, '
            f'attenuation: {attenuation_db}dB, duration: {jam_duration}s'
        )

    def _get_link(self, msg: MeshMetrics):
        for link in msg.links:
            if (link.robot_a == self.robot_a and link.robot_b == self.robot_b) or \
               (link.robot_a == self.robot_b and link.robot_b == self.robot_a):
                return link
        return None

    def _cb(self, msg: MeshMetrics):
        link = self._get_link(msg)
        if link:
            self.baseline_samples.append({
                'connected': link.connected,
                'rssi':      link.rssi_dbm,
                'loss':      link.packet_loss_pct,
            })

    def _call_jammer(self):
        if not self.jam_client.wait_for_service(timeout_sec=5.0):
            self.get_logger().error('/jam_link service not available — is jammer_service running?')
            sys.exit(1)

        req               = JamLink.Request()
        req.robot_a       = self.robot_a
        req.robot_b       = self.robot_b
        req.attenuation_db = self.attenuation_db
        req.duration_s    = self.jam_duration

        future = self.jam_client.call_async(req)
        deadline = time.monotonic() + 10.0
        while not future.done() and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)

        if not future.done():
            self.get_logger().error('Jammer service call timed out')
            sys.exit(1)

        result = future.result()
        if not result or not result.success:
            self.get_logger().error(
                f'Jammer call failed: {result.message if result else "no response"}'
            )
            sys.exit(1)

        self.get_logger().info(f'Jammer activated: {result.message}')

    def run(self):
        deadline = time.monotonic() + self.timeout_s

        # ── Phase 1: Baseline ─────────────────────────────────────────────────
        self.get_logger().info('Phase 1: Collecting baseline...')
        while len(self.baseline_samples) < BASELINE_SAMPLES:
            if time.monotonic() > deadline:
                self.get_logger().error('Timeout collecting baseline samples')
                sys.exit(1)
            rclpy.spin_once(self, timeout_sec=0.1)

        baseline = self.baseline_samples[:BASELINE_SAMPLES]

        # ── Phase 2: Activate jammer ──────────────────────────────────────────
        self.get_logger().info(f'Phase 2: Activating jammer for {self.jam_duration}s...')
        self._call_jammer()

        # Switch callback to collect jammed samples
        self.baseline_samples = []  # stop collecting baseline
        self.create_subscription(MeshMetrics, '/mesh_metrics', self._jammed_cb, 10)

        while len(self.jammed_samples) < self.target_samples:
            if time.monotonic() > deadline:
                self.get_logger().warn(
                    f'Timeout. Collected {len(self.jammed_samples)} jammed samples.'
                )
                break
            rclpy.spin_once(self, timeout_sec=0.1)

        if not self.jammed_samples:
            self.get_logger().error('No jammed samples collected')
            sys.exit(1)

        self._evaluate(baseline, self.jammed_samples[:self.target_samples])

    def _jammed_cb(self, msg: MeshMetrics):
        link = self._get_link(msg)
        if link:
            self.jammed_samples.append({
                'connected': link.connected,
                'rssi':      link.rssi_dbm,
                'loss':      link.packet_loss_pct,
            })

    def _evaluate(self, b: list, j: list):
        baseline_connected = sum(1 for s in b if s['connected']) / len(b) * 100
        jammed_connected   = sum(1 for s in j if s['connected']) / len(j) * 100
        baseline_loss = statistics.mean([s['loss'] for s in b])
        jammed_loss   = statistics.mean([s['loss'] for s in j])
        baseline_rssi = statistics.mean([s['rssi'] for s in b])
        jammed_rssi   = statistics.mean([s['rssi'] for s in j])

        print('\n' + '=' * 60)
        print('Jammer Service Test Results')
        print('=' * 60)
        print(f'  Link              : {self.robot_a} ↔ {self.robot_b}')
        print(f'  Attenuation       : {self.attenuation_db} dB')
        print(f'  Jam duration      : {self.jam_duration}s')
        print(f'  Baseline samples  : {len(b)}')
        print(f'  Jammed samples    : {len(j)}')
        print()
        print(f'  {"Metric":>20}  {"Baseline":>10}  {"Jammed":>10}')
        print('  ' + '-' * 44)
        print(f'  {"Connected (%)":>20}  {baseline_connected:>10.1f}  {jammed_connected:>10.1f}')
        print(f'  {"Packet Loss (%)":>20}  {baseline_loss:>10.1f}  {jammed_loss:>10.1f}')
        print(f'  {"RSSI (dBm)":>20}  {baseline_rssi:>10.1f}  {jammed_rssi:>10.1f}')
        print()

        degraded = (jammed_connected < 50.0) or (jammed_loss > 50.0)
        print(f'  Link degraded during jam : {"✅ PASS" if degraded else "❌ FAIL"}')
        print('-' * 60)

        if degraded:
            print('RESULT: PASS ✅')
        else:
            print('RESULT: FAIL ❌  (link not sufficiently degraded during jam)')
            print('=' * 60 + '\n')
            sys.exit(1)
        print('=' * 60 + '\n')


def main():
    parser = argparse.ArgumentParser(description='Jammer Service Test')
    parser.add_argument('--robot-a',     default='1',   help='First robot ID (default: 1)')
    parser.add_argument('--robot-b',     default='2',   help='Second robot ID (default: 2)')
    parser.add_argument('--attenuation', type=float, default=100.0,
                        help='Attenuation in dB (default: 100 = disconnect)')
    parser.add_argument('--duration',    type=float, default=30.0,
                        help='Jam duration seconds (default: 30)')
    parser.add_argument('--samples',     type=int,   default=100,
                        help='Jammed samples to collect (default: 100)')
    parser.add_argument('--timeout',     type=float, default=120.0,
                        help='Test timeout seconds (default: 120)')
    args = parser.parse_args()

    rclpy.init()
    node = JammerTester(
        args.robot_a, args.robot_b,
        args.duration, args.attenuation,
        args.samples, args.timeout,
    )
    try:
        node.run()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()