#!/usr/bin/env python3
"""
test_bridge_stress.py — L11: Bridge stress test (8 robots, CPU/RAM)

Monitors comm_traffic_filter CPU and RAM usage under full 8-robot load
across all communication tiers.

Thresholds:
  CPU  < 15% sustained average per tier
  RAM  < 200 MB RSS

Usage:
    ros2 run skyhunter_comm test_bridge_stress

Prerequisites:
    ros2 launch skyhunter_comm networking.launch.py
    ros2 launch skyhunter_gazebo sim.launch.py  (8 robots)
"""

import time
import threading
import psutil
import rclpy
from rclpy.node import Node
from skyhunter_msgs.msg import CommState
from skyhunter_msgs.srv import JamLink, UnjamLink, InjectFailure

# ── Config ────────────────────────────────────────────────────────────────────
NUM_ROBOTS          = 8
JAM_ATTENUATION_DB  = 60.0
MEASURE_WINDOW_S    = 30.0    # measure each tier for 30s
SAMPLE_INTERVAL_S   = 0.5     # sample CPU/RAM every 0.5s
STATE_TIMEOUT       = 20.0
LTE_FAIL_DURATION   = 60.0

CPU_THRESHOLD_PCT   = 15.0    # max acceptable sustained CPU %
RAM_THRESHOLD_MB    = 200.0   # max acceptable RSS MB

TIER_NAMES = {0: 'DISCONNECTED', 1: 'WiFi6', 2: 'LTE', 3: 'LoRa'}
PASS = '✅ PASS'
FAIL = '❌ FAIL'


def find_filter_pid() -> int:
    """Find PID of comm_traffic_filter process."""
    for proc in psutil.process_iter(['pid', 'cmdline']):
        try:
            cmdline = ' '.join(proc.info['cmdline'] or [])
            if 'comm_traffic_filter' in cmdline and 'grep' not in cmdline:
                return proc.info['pid']
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass
    return None


class StressTest(Node):

    def __init__(self):
        super().__init__('test_bridge_stress')

        self._current_tier = None
        self._tier_history = []

        self.create_subscription(CommState, '/comm_state', self._comm_state_cb, 10)

        self._jam_client      = self.create_client(JamLink,       '/jam_link')
        self._unjam_client    = self.create_client(UnjamLink,     '/unjam_link')
        self._lte_fail_client = self.create_client(InjectFailure, '/lte/inject_failure')

        self.get_logger().info('Waiting for services...')
        for c in [self._jam_client, self._unjam_client, self._lte_fail_client]:
            c.wait_for_service(timeout_sec=10.0)
        self.get_logger().info('All services ready.')

        # Find filter process
        self._pid = find_filter_pid()
        if self._pid:
            self.get_logger().info(f'Monitoring comm_traffic_filter PID: {self._pid}')
            self._proc = psutil.Process(self._pid)
            self._proc.cpu_percent(interval=None)  # prime the CPU counter
        else:
            self.get_logger().error('comm_traffic_filter process not found!')
            self._proc = None

    # ── Callbacks ─────────────────────────────────────────────────────────────

    def _comm_state_cb(self, msg: CommState) -> None:
        if msg.current_tier != self._current_tier:
            name = TIER_NAMES.get(msg.current_tier, '?')
            self.get_logger().info(f'[CommState] → {name}')
            self._current_tier = msg.current_tier
            self._tier_history.append((time.monotonic(), msg.current_tier))

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _wait_futures(self, futures, timeout=30.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if all(f.done() for f in futures):
                break
            rclpy.spin_once(self, timeout_sec=0.05)

    def _jam_all(self):
        self.get_logger().info(f'Jamming all links...')
        count = 0
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = JamLink.Request()
                req.robot_a = str(a)
                req.robot_b = str(b)
                req.attenuation_db = JAM_ATTENUATION_DB
                self._wait_futures([self._jam_client.call_async(req)])
                count += 1
        self.get_logger().info(f'Jammed {count} links')

    def _unjam_all(self):
        self.get_logger().info('Removing all jams...')
        count = 0
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = UnjamLink.Request()
                req.robot_a = str(a)
                req.robot_b = str(b)
                self._wait_futures([self._unjam_client.call_async(req)])
                count += 1
        self.get_logger().info(f'Unjammed {count} links')

    def _inject_lte_failure(self, duration_s: float):
        req = InjectFailure.Request()
        req.target_tier  = 2
        req.failure_type = 'disconnect'
        req.duration_s   = float(duration_s)
        self._wait_futures([self._lte_fail_client.call_async(req)])

    def _wait_for_tier(self, target_tier: int, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._current_tier == target_tier:
                return True
        return False

    def _spin_seconds(self, seconds: float):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)

    def _measure_window(self, label: str, window_s: float) -> dict:
        """Sample CPU and RAM over window_s seconds, return stats dict."""
        if not self._proc:
            return {}

        cpu_samples = []
        ram_samples = []
        stop = threading.Event()

        def sampler():
            while not stop.is_set():
                try:
                    cpu = self._proc.cpu_percent(interval=None)
                    ram = self._proc.memory_info().rss / (1024 * 1024)  # MB
                    cpu_samples.append(cpu)
                    ram_samples.append(ram)
                except psutil.NoSuchProcess:
                    break
                time.sleep(SAMPLE_INTERVAL_S)

        t = threading.Thread(target=sampler, daemon=True)
        t.start()
        self._spin_seconds(window_s)
        stop.set()
        t.join(timeout=2.0)

        if not cpu_samples:
            return {}

        stats = {
            'tier':     label,
            'cpu_avg':  sum(cpu_samples) / len(cpu_samples),
            'cpu_max':  max(cpu_samples),
            'ram_avg':  sum(ram_samples) / len(ram_samples),
            'ram_max':  max(ram_samples),
            'samples':  len(cpu_samples),
        }
        print(f'    CPU: avg={stats["cpu_avg"]:.1f}%  max={stats["cpu_max"]:.1f}%  '
              f'| RAM: avg={stats["ram_avg"]:.0f}MB  max={stats["ram_max"]:.0f}MB  '
              f'| samples={stats["samples"]}')
        return stats

    # ── Test ──────────────────────────────────────────────────────────────────

    def run(self):
        if not self._proc:
            print('❌ Cannot run — comm_traffic_filter process not found')
            return

        results  = []
        all_stats = []

        print('\nWaiting for initial WiFi6 state...')
        self._wait_for_tier(1, timeout=STATE_TIMEOUT)

        # ── WiFi6 stress ──────────────────────────────────────────────────────
        print('\n' + '─'*60)
        print(f'[WiFi6] Measuring {MEASURE_WINDOW_S}s under full 8-robot load...')
        stats = self._measure_window('WiFi6', MEASURE_WINDOW_S)
        all_stats.append(stats)

        cpu_ok = stats.get('cpu_avg', 999) < CPU_THRESHOLD_PCT
        ram_ok = stats.get('ram_max', 999) < RAM_THRESHOLD_MB
        results.append((f'WiFi6: CPU avg < {CPU_THRESHOLD_PCT}%', PASS if cpu_ok else FAIL))
        results.append((f'WiFi6: RAM max < {RAM_THRESHOLD_MB}MB', PASS if ram_ok else FAIL))

        # ── LTE stress ────────────────────────────────────────────────────────
        print('\n' + '─'*60)
        print('[LTE] Jamming → waiting for LTE...')
        self._jam_all()
        got_lte = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        if got_lte:
            print(f'[LTE] Measuring {MEASURE_WINDOW_S}s...')
            stats = self._measure_window('LTE', MEASURE_WINDOW_S)
            all_stats.append(stats)
            cpu_ok = stats.get('cpu_avg', 999) < CPU_THRESHOLD_PCT
            ram_ok = stats.get('ram_max', 999) < RAM_THRESHOLD_MB
            results.append((f'LTE:   CPU avg < {CPU_THRESHOLD_PCT}%', PASS if cpu_ok else FAIL))
            results.append((f'LTE:   RAM max < {RAM_THRESHOLD_MB}MB', PASS if ram_ok else FAIL))
        else:
            results.append(('LTE: transition failed', FAIL))

        # ── LoRa stress ───────────────────────────────────────────────────────
        print('\n' + '─'*60)
        print(f'[LoRa] Injecting LTE failure ({LTE_FAIL_DURATION}s) → waiting for LoRa...')
        self._inject_lte_failure(LTE_FAIL_DURATION)
        got_lora = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        if got_lora:
            print(f'[LoRa] Measuring {MEASURE_WINDOW_S}s...')
            stats = self._measure_window('LoRa', MEASURE_WINDOW_S)
            all_stats.append(stats)
            cpu_ok = stats.get('cpu_avg', 999) < CPU_THRESHOLD_PCT
            ram_ok = stats.get('ram_max', 999) < RAM_THRESHOLD_MB
            results.append((f'LoRa:  CPU avg < {CPU_THRESHOLD_PCT}%', PASS if cpu_ok else FAIL))
            results.append((f'LoRa:  RAM max < {RAM_THRESHOLD_MB}MB', PASS if ram_ok else FAIL))
        else:
            results.append(('LoRa: transition failed', FAIL))

        # ── Cleanup ───────────────────────────────────────────────────────────
        self._spin_seconds(2.0)
        self._unjam_all()

        self._print_summary(results, all_stats)

    def _print_summary(self, results, all_stats):
        passed = sum(1 for _, r in results if r == PASS)
        total  = len(results)

        print('\n' + '='*60)
        print('Results')
        print('='*60)
        for name, result in results:
            print(f'  {result}  {name}')

        print('\n' + '─'*60)
        print(f'  {"Tier":<14} {"CPU avg":>8} {"CPU max":>8} {"RAM avg":>9} {"RAM max":>9}')
        print('  ' + '─'*52)
        for s in all_stats:
            print(f'  {s["tier"]:<14} '
                  f'{s["cpu_avg"]:>7.1f}%  '
                  f'{s["cpu_max"]:>7.1f}%  '
                  f'{s["ram_avg"]:>7.0f}MB  '
                  f'{s["ram_max"]:>7.0f}MB')

        print('─'*60)
        all_pass = all(r == PASS for _, r in results)
        vv = PASS if all_pass else FAIL
        print(f'  {vv}  L11 — Bridge stress test')
        print(f'  {passed}/{total} checks passed')
        print('='*60 + '\n')


def main(args=None):
    rclpy.init(args=args)
    node = StressTest()

    for _ in range(20):
        rclpy.spin_once(node, timeout_sec=0.1)

    try:
        node.run()
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()