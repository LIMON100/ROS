# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
Delay Injection Test
Package: san_comm_sim/test/
Target: All link delays in 5-20ms range across 100+ samples

Usage:
    python3 test_delay.py
    python3 test_delay.py --samples 200 --timeout 30
"""

import argparse
import statistics
import sys
import time

import rclpy
from rclpy.node import Node
from san_comm_msgs.msg import MeshMetrics


class DelayCollector(Node):

    def __init__(self, target_samples: int, timeout_s: float):
        super().__init__('delay_collector')
        self.target_samples = target_samples
        self.timeout_s      = timeout_s
        self.samples        = []
        self.create_subscription(MeshMetrics, '/mesh_metrics', self._cb, 10)
        self.get_logger().info(f'Collecting {target_samples} delay samples...')

    def _cb(self, msg: MeshMetrics):
        for link in msg.links:
            if link.connected:
                self.samples.append(link.delay_ms)

    def run(self):
        deadline = time.monotonic() + self.timeout_s

        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if len(self.samples) >= self.target_samples:
                break

        if not self.samples:
            self.get_logger().error('No samples collected — is wifi6_mesh_sim running?')
            sys.exit(1)

        if len(self.samples) < self.target_samples:
            self.get_logger().warn(
                f'Timeout reached. Collected {len(self.samples)} / {self.target_samples} samples.'
            )

        self._evaluate()

    def _evaluate(self):
        s      = self.samples[:self.target_samples]
        mn     = min(s)
        mx     = max(s)
        mean   = statistics.mean(s)
        stdev  = statistics.stdev(s) if len(s) > 1 else 0.0
        in_range  = [v for v in s if 5.0 <= v <= 20.0]
        pass_rate = len(in_range) / len(s) * 100

        print('\n' + '=' * 50)
        print('Delay Injection Test Results')
        print('=' * 50)
        print(f'  Samples collected : {len(s)}')
        print(f'  Min delay         : {mn:.2f} ms')
        print(f'  Max delay         : {mx:.2f} ms')
        print(f'  Mean delay        : {mean:.2f} ms')
        print(f'  Std deviation     : {stdev:.2f} ms')
        print(f'  In range [5-20ms] : {len(in_range)} / {len(s)} ({pass_rate:.1f}%)')
        print('-' * 50)

        if pass_rate >= 95.0:
            print('RESULT: PASS ✅')
        else:
            print(f'RESULT: FAIL ❌  ({pass_rate:.1f}% in range, need ≥95%)')
            print('=' * 50 + '\n')
            sys.exit(1)
        print('=' * 50 + '\n')


def main():
    parser = argparse.ArgumentParser(description='Delay Injection Test')
    parser.add_argument('--samples', type=int,   default=100,  help='Number of samples (default: 100)')
    parser.add_argument('--timeout', type=float, default=60.0, help='Timeout in seconds (default: 60)')
    args = parser.parse_args()

    rclpy.init()
    node = DelayCollector(args.samples, args.timeout)
    try:
        node.run()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()