#!/usr/bin/env python3
"""
test_latency.py — L16: Comm latency measurement (1000+ samples per tier)

Measures end-to-end relay latency through comm_traffic_filter for all 8 robots.

What is measured per tier:

  WiFi6 / LTE — odom relay latency:
    Method: subscribe to both /SH_NN/odom (source) and /bridge/SH_NN/odom (output).
    Record wall-clock arrival time of each message, matched by header.stamp nanosecond key.
    Latency = bridge_arrival_time - source_arrival_time.
    This measures the full pipeline: Gazebo bridge → comm_traffic_filter → ROS 2 publish.

  LoRa — e-stop simulated airtime latency:
    Method: call /lora/send_estop service and record ack_time_ms from the response.
    ack_time_ms is the simulated LoRa propagation delay from lora_model (base_delay_ms=350ms,
    jitter±150ms, SF10, 125kHz BW), with 3 retry attempts per e-stop call.
    This does NOT measure ROS 2 overhead — it measures the simulated RF airtime.
    odom is blocked at LoRa tier by design (relay rate = 0.0 Hz), so stamp-matching
    cannot be used for LoRa.

Metrics (per tier):
  - p50 / p95 / p99 / max latency in ms
  - Sample count

Pass criteria (L16):
  - 1000+ samples collected for WiFi6 and LTE
  - p99 within tier budget:
      WiFi6 : < 10 ms    (ROS 2 relay pipeline)
      LTE   : < 100 ms   (ROS 2 relay pipeline, throttled)
      LoRa  : < 2000 ms  (simulated RF airtime, 3 retries)
  - LoRa: at least 10 e-stop samples collected (duty cycle constrained)

Output:
  - Console summary table
  - JSON results file for D7 chart generation

Usage:
    ros2 run skyhunter_comm test_latency

Prerequisites (all must be running):
    ros2 launch skyhunter_gazebo sim.launch.py   (8 robots)
    ros2 launch skyhunter_comm networking.launch.py
"""

import json
import os
import time
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy
from ament_index_python.packages import get_package_share_directory

from nav_msgs.msg import Odometry
from skyhunter_msgs.msg import CommState
from skyhunter_msgs.msg import LoraStatus
from skyhunter_msgs.srv import JamLink, UnjamLink, InjectFailure, SendEstop

# ── Config ────────────────────────────────────────────────────────────────────
NUM_ROBOTS             = 8
NS_PREFIX              = 'SH'
JAM_ATTENUATION_DB     = 60.0
STATE_TIMEOUT          = 25.0
WIFI6_RECOVERY_TIMEOUT = 60.0

# Samples target per tier
TARGET_SAMPLES         = 1000

# How long to collect per tier (WiFi6 + LTE only; LoRa blocks odom)
WIFI6_COLLECT_S        = 60.0    # odom at full rate — 1000 samples ~ 8 robots x ~2Hz x 60s
LTE_COLLECT_S          = 180.0   # odom throttled to 2Hz — 1000 samples ~ 8 robots x 2Hz x 62s

# LoRa e-stop collection
LORA_ESTOP_SAMPLES     = 5      # duty-cycle constrained by 1% limit — 5 samples per 300s window
LORA_ESTOP_BUDGET_MS   = 2000.0 # p99 budget for simulated LoRa airtime
LORA_DUTY_WAIT_S       = 60.0   # max wait for duty cycle to clear between calls

# Latency budgets p99 (ms)
BUDGET = {
    'WiFi6': 10.0,
    'LTE':   100.0,
}

# Output JSON path
_docs_dir = os.path.expanduser('~/.ros/skyhunter')
os.makedirs(_docs_dir, exist_ok=True)
JSON_OUTPUT = os.path.join(_docs_dir, 'skyhunter_latency_results.json')

TIER_NAMES = {0: 'DISCONNECTED', 1: 'WiFi6', 2: 'LTE', 3: 'LoRa'}
PASS = '\u2705 PASS'
FAIL = '\u274c FAIL'


def _stamp_key(stamp) -> int:
    """Convert ROS stamp to nanosecond integer for dict key."""
    return stamp.sec * 1_000_000_000 + stamp.nanosec


def percentile(data: list, p: float) -> float:
    if not data:
        return 0.0
    s = sorted(data)
    idx = max(0, int(len(s) * p / 100) - 1)
    return s[idx]


class LatencyTest(Node):

    def __init__(self):
        super().__init__('test_latency')

        self._current_tier   = None
        self._tier_history   = []

        # source_times[stamp_key] = wall monotonic time when source msg arrived
        self._source_times: dict[int, float] = {}

        # latency_samples[tier_name][robot_id] = [latency_ms, ...]
        self._samples: dict[str, list[float]] = {'WiFi6': [], 'LTE': [], 'LoRa': []}

        self._collecting_tier: str | None = None
        self._lora_status: object = None
        self._lora_ack_samples: list[float] = []

        # QoS for odom (RELIABLE)
        qos_reliable = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            depth=50,
        )

        # Subscribe to comm_state and lora status
        self.create_subscription(CommState,  '/comm_state',  self._comm_state_cb,  10)
        self.create_subscription(LoraStatus, '/lora/status', self._lora_status_cb, 10)

        # Subscribe to source + bridge odom for each robot
        for robot_id in range(1, NUM_ROBOTS + 1):
            ns = f'{NS_PREFIX}_{robot_id:02d}'
            self.create_subscription(
                Odometry,
                f'/{ns}/odom',
                lambda msg, r=robot_id: self._source_cb(msg, r),
                qos_reliable,
            )
            self.create_subscription(
                Odometry,
                f'/bridge/{ns}/odom',
                lambda msg, r=robot_id: self._bridge_cb(msg, r),
                qos_reliable,
            )

        # Services
        self._jam_client      = self.create_client(JamLink,       '/jam_link')
        self._unjam_client    = self.create_client(UnjamLink,     '/unjam_link')
        self._lte_fail_client = self.create_client(InjectFailure, '/lte/inject_failure')
        self._estop_client    = self.create_client(SendEstop,     '/lora/send_estop')

        self.get_logger().info('Waiting for services...')
        for client in [self._jam_client, self._unjam_client,
                       self._lte_fail_client, self._estop_client]:
            client.wait_for_service(timeout_sec=10.0)
        self.get_logger().info('All services ready.')

    # ── Callbacks ─────────────────────────────────────────────────────────────

    def _comm_state_cb(self, msg: CommState) -> None:
        if msg.current_tier != self._current_tier:
            name = TIER_NAMES.get(msg.current_tier, '?')
            self.get_logger().info(f'[CommState] \u2192 {name}')
            self._current_tier = msg.current_tier
            self._tier_history.append((time.monotonic(), msg.current_tier))

    def _source_cb(self, msg: Odometry, robot_id: int) -> None:
        key = _stamp_key(msg.header.stamp)
        self._source_times[key] = time.monotonic()
        # Prune old entries to avoid unbounded growth (keep last 500 per robot)
        if len(self._source_times) > NUM_ROBOTS * 500:
            oldest = sorted(self._source_times.keys())[:NUM_ROBOTS * 100]
            for k in oldest:
                del self._source_times[k]

    def _bridge_cb(self, msg: Odometry, robot_id: int) -> None:
        if self._collecting_tier is None:
            return
        key = _stamp_key(msg.header.stamp)
        arrival = time.monotonic()
        if key in self._source_times:
            latency_ms = (arrival - self._source_times[key]) * 1000.0
            if latency_ms >= 0:
                self._samples[self._collecting_tier].append(latency_ms)
            del self._source_times[key]

    def _lora_status_cb(self, msg: LoraStatus) -> None:
        self._lora_status = msg

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _wait_futures(self, futures, timeout=30.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if all(f.done() for f in futures):
                break
            rclpy.spin_once(self, timeout_sec=0.05)

    def _jam_all(self):
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = JamLink.Request()
                req.robot_a = str(a); req.robot_b = str(b)
                req.attenuation_db = JAM_ATTENUATION_DB
                self._wait_futures([self._jam_client.call_async(req)])

    def _unjam_all(self):
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = UnjamLink.Request()
                req.robot_a = str(a); req.robot_b = str(b)
                self._wait_futures([self._unjam_client.call_async(req)])

    def _inject_lte_failure(self, duration_s: float):
        req = InjectFailure.Request()
        req.target_tier = 2; req.failure_type = 'disconnect'; req.duration_s = duration_s
        self._wait_futures([self._lte_fail_client.call_async(req)])

    def _wait_for_tier(self, target: int, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._current_tier == target:
                return True
        return False

    def _collect(self, tier_name: str, duration_s: float):
        """Spin for duration_s, collecting latency samples for tier_name."""
        print(f'    Collecting {tier_name} samples for {duration_s:.0f}s...')
        self._collecting_tier = tier_name
        t_start = time.monotonic()
        t_next  = t_start + 10.0
        while time.monotonic() < t_start + duration_s:
            rclpy.spin_once(self, timeout_sec=0.1)
            if time.monotonic() >= t_next:
                n = len(self._samples[tier_name])
                print(f'      t+{time.monotonic()-t_start:.0f}s — {n} samples')
                t_next += 10.0
        self._collecting_tier = None
        n = len(self._samples[tier_name])
        print(f'    Done — {n} samples collected')

    def _reset_to_wifi6(self):
        self._unjam_all()
        ok = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        if not ok:
            print('  WARNING: WiFi6 recovery timed out')
        return ok

    # ── Measurement runs ──────────────────────────────────────────────────────

    def _measure_wifi6(self, results):
        print('\n[1] WiFi6 latency measurement...')
        self._spin_seconds(3.0)  # let system settle
        ok = self._current_tier == 1
        if not ok:
            print('  Not at WiFi6 — skipping')
            results.append(('WiFi6: reached tier', FAIL))
            return
        results.append(('WiFi6: reached tier', PASS))
        self._collect('WiFi6', WIFI6_COLLECT_S)

    def _measure_lte(self, results):
        print('\n[2] LTE latency measurement...')
        self._jam_all()
        ok = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        results.append(('LTE: reached tier', PASS if ok else FAIL))
        if not ok:
            self._unjam_all()
            return
        self._spin_seconds(3.0)  # settle into LTE
        self._collect('LTE', LTE_COLLECT_S)

    def _measure_lora(self, results):
        """
        LoRa latency — simulated e-stop airtime (ack_time_ms from SendEstop service).

        odom is blocked at LoRa tier — stamp-matching cannot be used.
        Instead we call /lora/send_estop and record ack_time_ms per call.
        ack_time_ms = total simulated RF propagation delay across retry attempts
        (base 350ms ± 150ms jitter at SF10, 125kHz BW).
        """
        print('\n[3] LoRa latency measurement (e-stop airtime)...')
        print('    Note: odom blocked at LoRa — measuring simulated RF airtime via SendEstop.ack_time_ms')

        # Enter LoRa tier
        self._inject_lte_failure(duration_s=120.0)
        self._jam_all()
        ok = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        results.append(('LoRa: reached tier', PASS if ok else FAIL))
        if not ok:
            self._unjam_all()
            return

        self._lora_ack_samples.clear()
        attempts = 0
        t_start  = time.monotonic()
        t_end    = t_start + 300.0  # collect for up to 5 minutes

        while time.monotonic() < t_end and len(self._lora_ack_samples) < 50:
            # Wait for duty cycle to clear
            dc_deadline = time.monotonic() + LORA_DUTY_WAIT_S
            while time.monotonic() < dc_deadline:
                rclpy.spin_once(self, timeout_sec=0.1)
                if self._lora_status and self._lora_status.duty_cycle_used_pct < 30.0:
                    break
            self._spin_seconds(0.5)

            req = SendEstop.Request()
            req.target_robot = (attempts % NUM_ROBOTS) + 1
            future = self._estop_client.call_async(req)
            self._wait_futures([future])
            attempts += 1

            if future.done() and future.result() and future.result().success:
                ack_ms = float(future.result().ack_time_ms)
                self._lora_ack_samples.append(ack_ms)
                print(f'    Sample {len(self._lora_ack_samples)}: ack={ack_ms:.1f}ms')

        n = len(self._lora_ack_samples)
        ok = n >= LORA_ESTOP_SAMPLES
        results.append((f'LoRa: {n} e-stop samples collected (target {LORA_ESTOP_SAMPLES})',
                        PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- {n} samples from {attempts} attempts')

    def _spin_seconds(self, duration: float):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)

    # ── Summary + pass/fail ───────────────────────────────────────────────────

    def _evaluate(self, results) -> dict:
        print('\n' + '=' * 65)
        print(f'  {"Tier":<8} {"Topic":<8} {"N":>6} {"p50":>8} {"p95":>8} {"p99":>8} {"max":>8}')
        print('  ' + '-' * 63)

        report = {}

        for tier_name, budget_ms in BUDGET.items():
            samples = self._samples[tier_name]
            n = len(samples)
            if n == 0:
                print(f'  {tier_name:<8} {"odom":<8} {"0":>6} {"N/A":>8} {"N/A":>8} {"N/A":>8} {"N/A":>8}')
                results.append((f'{tier_name}: 1000+ samples collected', FAIL))
                report[tier_name] = {'n': 0}
                continue

            p50 = percentile(samples, 50)
            p95 = percentile(samples, 95)
            p99 = percentile(samples, 99)
            mx  = max(samples)

            print(f'  {tier_name:<8} {"odom":<8} {n:>6} {p50:>7.1f}ms {p95:>7.1f}ms '
                  f'{p99:>7.1f}ms {mx:>7.1f}ms')

            ok_n   = n >= TARGET_SAMPLES
            ok_p99 = p99 <= budget_ms

            results.append((f'{tier_name}: {n} samples (target {TARGET_SAMPLES})',
                             PASS if ok_n else FAIL))
            results.append((f'{tier_name}: p99 {p99:.1f}ms <= {budget_ms:.0f}ms budget',
                             PASS if ok_p99 else FAIL))

            report[tier_name] = {
                'n': n, 'p50_ms': round(p50, 2), 'p95_ms': round(p95, 2),
                'p99_ms': round(p99, 2), 'max_ms': round(mx, 2),
                'budget_ms': budget_ms, 'pass': ok_n and ok_p99,
            }

        # LoRa e-stop airtime
        lora_samples = self._lora_ack_samples
        n = len(lora_samples)
        if n > 0:
            p50 = percentile(lora_samples, 50)
            p95 = percentile(lora_samples, 95)
            p99 = percentile(lora_samples, 99)
            mx  = max(lora_samples)
            print(f'  {"LoRa":<8} {"estop":<8} {n:>6} {p50:>7.1f}ms {p95:>7.1f}ms '
                  f'{p99:>7.1f}ms {mx:>7.1f}ms')
            ok_p99 = p99 <= LORA_ESTOP_BUDGET_MS
            results.append((f'LoRa: p99 {p99:.1f}ms <= {LORA_ESTOP_BUDGET_MS:.0f}ms budget',
                             PASS if ok_p99 else FAIL))
            report['LoRa'] = {
                'n': n, 'p50_ms': round(p50, 2), 'p95_ms': round(p95, 2),
                'p99_ms': round(p99, 2), 'max_ms': round(mx, 2),
                'budget_ms': LORA_ESTOP_BUDGET_MS,
                'measurement': 'e-stop ack_time_ms (simulated RF airtime)',
                'pass': ok_p99,
            }
        else:
            print(f'  {"LoRa":<8} {"estop":<8} {"0":>6} {"N/A":>8} {"N/A":>8} {"N/A":>8} {"N/A":>8}')
            report['LoRa'] = {'n': 0, 'measurement': 'e-stop ack_time_ms (simulated RF airtime)'}

        print('  ' + '-' * 63)
        return report

    # ── Main run ──────────────────────────────────────────────────────────────

    def run(self):
        results = []
        t0 = time.monotonic()

        print('\nWaiting for /comm_state...')
        deadline = time.monotonic() + 10.0
        while self._current_tier is None and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
        if self._current_tier is None:
            print('ERROR: No /comm_state received.')
            return 1

        # WiFi6
        self._reset_to_wifi6()
        self._measure_wifi6(results)

        # LTE
        self._reset_to_wifi6()
        self._measure_lte(results)

        # LoRa
        self._reset_to_wifi6()
        self._measure_lora(results)

        # Reset
        self._reset_to_wifi6()

        # Evaluate
        print('\n' + '=' * 65)
        print('L16 — Comm Latency Results')
        report = self._evaluate(results)

        # JSON export
        output = {
            'timestamp': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
            'tiers': report,
            'tier_history': [
                {'t': round(ts - t0, 2), 'tier': TIER_NAMES.get(t, '?')}
                for ts, t in self._tier_history
            ],
        }
        with open(JSON_OUTPUT, 'w') as f:
            json.dump(output, f, indent=2)
        print(f'\n  JSON results saved to: {JSON_OUTPUT}')

        # Summary
        elapsed = time.monotonic() - t0
        passed  = sum(1 for _, r in results if r == PASS)
        total   = len(results)
        vv_pass = passed == total

        print('\nChecks:')
        for label, result in results:
            print(f'  {result}  {label}')
        print('-' * 65)
        print(f'  {"PASS" if vv_pass else "FAIL"}  L16 — Comm latency measurement')
        print(f'  Elapsed: {elapsed:.1f}s  |  {passed}/{total} checks passed')
        print('=' * 65)

        return 0 if vv_pass else 1


def main(args=None):
    rclpy.init(args=args)
    node = LatencyTest()
    exit_code = 1
    try:
        exit_code = node.run()
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
    raise SystemExit(exit_code)


if __name__ == '__main__':
    main()