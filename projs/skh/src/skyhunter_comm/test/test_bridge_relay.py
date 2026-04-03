#!/usr/bin/env python3
"""
test_bridge_relay.py — L10b: 8-robot bridge relay + auto LTE switch

Tests:
  S1 — WiFi6 pass-through  : all 8 robots' odom + camera relayed at full rate
  S2 — LTE throttle        : odom ≤ 2 Hz, camera ≤ 1 fps per robot (V&V 2.6)
  S3 — LoRa block          : all odom + camera blocked after LTE failure
  S4 — Recovery            : odom + camera resume after jams cleared

V&V 2.6: All 8 robots relayed in LTE mode — correct throttle rates, no missing robots

Usage:
    ros2 run skyhunter_comm test_bridge_relay

Prerequisites:
    ros2 launch skyhunter_comm networking.launch.py
    ros2 launch skyhunter_gazebo sim.launch.py  (8 robots)
"""

import time
import math
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from nav_msgs.msg import Odometry
from sensor_msgs.msg import Image
from skyhunter_msgs.msg import CommState
from skyhunter_msgs.srv import JamLink, UnjamLink, InjectFailure

# ── Config ────────────────────────────────────────────────────────────────────
NUM_ROBOTS         = 8
ROBOT_PREFIX       = "SH_"
JAM_ATTENUATION_DB = 60.0
STATE_TIMEOUT      = 20.0
MEASURE_WINDOW_S   = 6.0    # window to count messages and compute rate
LTE_ODOM_HZ        = 2.0    # expected throttle rate
LTE_CAMERA_FPS     = 1.0    # expected throttle rate
RATE_TOLERANCE     = 0.4    # ±40% tolerance on measured rate
LTE_FAIL_DURATION  = 30.0   # long enough to hold LoRa for measurement

TIER_NAMES = {0: 'DISCONNECTED', 1: 'WiFi6', 2: 'LTE', 3: 'LoRa'}
PASS = '✅ PASS'
FAIL = '❌ FAIL'


class BridgeRelayTest(Node):

    def __init__(self):
        super().__init__('test_bridge_relay')

        self._current_tier = None
        self._tier_history = []

        # Per-robot message counters — keyed by robot_id
        self._odom_counts   = {r: 0 for r in range(1, NUM_ROBOTS + 1)}
        self._camera_counts = {r: 0 for r in range(1, NUM_ROBOTS + 1)}
        self._counting = False

        # Comm state
        self.create_subscription(CommState, '/comm_state', self._comm_state_cb, 10)

        # Bridge topic subscribers — all 8 robots
        for robot_id in range(1, NUM_ROBOTS + 1):
            ns = f"{ROBOT_PREFIX}{robot_id:02d}"
            self.create_subscription(
                Odometry,
                f'/bridge/{ns}/odom',
                lambda msg, r=robot_id: self._odom_cb(msg, r),
                10,
            )
            _camera_qos = QoSProfile(
                reliability=ReliabilityPolicy.BEST_EFFORT,
                history=HistoryPolicy.KEEP_LAST,
                depth=10,
            )
            self.create_subscription(
                Image,
                f'/bridge/{ns}/rgb_camera/image_raw',
                lambda msg, r=robot_id: self._camera_cb(msg, r),
                _camera_qos,
            )

        # Service clients
        self._jam_client      = self.create_client(JamLink,        '/jam_link')
        self._unjam_client    = self.create_client(UnjamLink,      '/unjam_link')
        self._lte_fail_client = self.create_client(InjectFailure,  '/lte/inject_failure')

        self.get_logger().info('Waiting for services...')
        for c in [self._jam_client, self._unjam_client, self._lte_fail_client]:
            c.wait_for_service(timeout_sec=10.0)
        self.get_logger().info('All services ready.')

    # ── Callbacks ─────────────────────────────────────────────────────────────

    def _comm_state_cb(self, msg: CommState) -> None:
        if msg.current_tier != self._current_tier:
            name = TIER_NAMES.get(msg.current_tier, '?')
            self.get_logger().info(f'[CommState] → {name} (tier {msg.current_tier})')
            self._current_tier = msg.current_tier
            self._tier_history.append((time.monotonic(), msg.current_tier))

    def _odom_cb(self, msg: Odometry, robot_id: int) -> None:
        if self._counting:
            self._odom_counts[robot_id] += 1

    def _camera_cb(self, msg: Image, robot_id: int) -> None:
        if self._counting:
            self._camera_counts[robot_id] += 1

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _wait_futures(self, futures, timeout=30.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if all(f.done() for f in futures):
                break
            rclpy.spin_once(self, timeout_sec=0.05)

    def _jam_all(self):
        self.get_logger().info(f'Jamming all links at {JAM_ATTENUATION_DB} dB...')
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
        self.get_logger().info(f'Injecting LTE failure for {duration_s}s...')
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

    def _reset_counts(self):
        for r in range(1, NUM_ROBOTS + 1):
            self._odom_counts[r]   = 0
            self._camera_counts[r] = 0

    def _measure_rates(self, window_s: float):
        """Count messages over window_s seconds, return (odom_rates, camera_rates) dicts."""
        self._reset_counts()
        self._counting = True
        self._spin_seconds(window_s)
        self._counting = False
        odom_rates   = {r: self._odom_counts[r]   / window_s for r in range(1, NUM_ROBOTS + 1)}
        camera_rates = {r: self._camera_counts[r] / window_s for r in range(1, NUM_ROBOTS + 1)}
        return odom_rates, camera_rates

    def _check_all_robots_active(self, rates: dict, min_hz: float) -> tuple:
        """Return (pass, missing_robots). A robot is active if rate >= min_hz."""
        missing = [r for r, hz in rates.items() if hz < min_hz]
        return len(missing) == 0, missing

    def _check_all_robots_blocked(self, rates: dict, max_hz: float = 0.1) -> tuple:
        """Return (pass, leaking_robots). A robot is blocked if rate <= max_hz."""
        leaking = [r for r, hz in rates.items() if hz > max_hz]
        return len(leaking) == 0, leaking

    def _check_rate_in_range(self, rates: dict, target_hz: float, tol: float) -> tuple:
        """Return (pass, out_of_range robots). Rate must be within tol fraction of target."""
        lo = target_hz * (1.0 - tol)
        hi = target_hz * (1.0 + tol)
        bad = {r: hz for r, hz in rates.items() if not (lo <= hz <= hi)}
        return len(bad) == 0, bad

    # ── Test scenarios ────────────────────────────────────────────────────────

    def run(self):
        results = []

        # Wait for initial WiFi6 state
        print('\nWaiting for comm_state...')
        self._wait_for_tier(1, timeout=STATE_TIMEOUT)
        self.get_logger().info(f'Starting tier: {TIER_NAMES.get(self._current_tier, "?")}')

        # ── Scenario 1 — WiFi6 pass-through ──────────────────────────────────
        print('\n' + '─'*60)
        print('SCENARIO 1 — WiFi6 Pass-Through')
        print('─'*60)

        print(f'\n[1] Measuring relay rates in WiFi6 mode ({MEASURE_WINDOW_S}s)...')
        odom_rates, camera_rates = self._measure_rates(MEASURE_WINDOW_S)

        # All 8 robots must have odom activity (>0.5 Hz confirms pass-through active)
        all_active, missing = self._check_all_robots_active(odom_rates, min_hz=0.5)
        result = PASS if all_active else FAIL
        results.append(('S1: All 8 robots relaying odom in WiFi6', result))
        print(f'    {result} — odom rates: ' +
              ', '.join(f'SH_{r:02d}:{hz:.1f}Hz' for r, hz in odom_rates.items()))
        if missing:
            print(f'         Missing robots: {missing}')

        cam_active, missing_cam = self._check_all_robots_active(camera_rates, min_hz=0.5)
        result = PASS if cam_active else FAIL
        results.append(('S1: All 8 robots relaying camera in WiFi6', result))
        print(f'    {result} — camera rates: ' +
              ', '.join(f'SH_{r:02d}:{hz:.1f}fps' for r, hz in camera_rates.items()))

        # ── Scenario 2 — LTE throttle ─────────────────────────────────────────
        print('\n' + '─'*60)
        print('SCENARIO 2 — LTE Throttle (V&V 2.6)')
        print('─'*60)

        print('\n[1] Jamming all links → waiting for LTE...')
        self._jam_all()
        got_lte = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        result = PASS if got_lte else FAIL
        results.append(('S2: WiFi6 → LTE transition on jam', result))
        print(f'    {result} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

        if got_lte:
            print(f'\n[2] Measuring relay rates in LTE mode ({MEASURE_WINDOW_S}s)...')
            odom_rates, camera_rates = self._measure_rates(MEASURE_WINDOW_S)

            # All 8 robots must be relaying odom
            all_active, missing = self._check_all_robots_active(odom_rates, min_hz=0.3)
            result = PASS if all_active else FAIL
            results.append(('S2: All 8 robots relaying odom in LTE', result))
            print(f'    {result} — odom rates: ' +
                  ', '.join(f'SH_{r:02d}:{hz:.1f}Hz' for r, hz in odom_rates.items()))
            if missing:
                print(f'         Missing robots: {missing}')

            # Odom rate must be near 2 Hz
            rate_ok, bad = self._check_rate_in_range(odom_rates, LTE_ODOM_HZ, RATE_TOLERANCE)
            result = PASS if rate_ok else FAIL
            results.append((f'S2: Odom throttled to ~{LTE_ODOM_HZ}Hz in LTE', result))
            if bad:
                print(f'    {result} — odom out of range: ' +
                      ', '.join(f'SH_{r:02d}:{hz:.2f}Hz' for r, hz in bad.items()))
            else:
                print(f'    {result} — all robots odom within ±{int(RATE_TOLERANCE*100)}% of {LTE_ODOM_HZ}Hz')

            # Camera rate must be near 1 fps
            cam_active, missing_cam = self._check_all_robots_active(camera_rates, min_hz=0.1)
            result = PASS if cam_active else FAIL
            results.append(('S2: All 8 robots relaying camera in LTE', result))
            print(f'    {result} — camera rates: ' +
                  ', '.join(f'SH_{r:02d}:{hz:.1f}fps' for r, hz in camera_rates.items()))

            cam_rate_ok, bad_cam = self._check_rate_in_range(
                camera_rates, LTE_CAMERA_FPS, RATE_TOLERANCE)
            result = PASS if cam_rate_ok else FAIL
            results.append((f'S2: Camera throttled to ~{LTE_CAMERA_FPS}fps in LTE', result))
            if bad_cam:
                print(f'    {result} — camera out of range: ' +
                      ', '.join(f'SH_{r:02d}:{hz:.2f}fps' for r, hz in bad_cam.items()))
            else:
                print(f'    {result} — all robots camera within ±{int(RATE_TOLERANCE*100)}% of {LTE_CAMERA_FPS}fps')

        # ── Scenario 3 — LoRa block ───────────────────────────────────────────
        print('\n' + '─'*60)
        print('SCENARIO 3 — LoRa Block')
        print('─'*60)

        print(f'\n[1] Injecting LTE failure ({LTE_FAIL_DURATION}s) → waiting for LoRa...')
        self._inject_lte_failure(LTE_FAIL_DURATION)
        got_lora = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        result = PASS if got_lora else FAIL
        results.append(('S3: LTE → LoRa transition on LTE failure', result))
        print(f'    {result} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

        if got_lora:
            print(f'\n[2] Measuring relay rates in LoRa mode ({MEASURE_WINDOW_S}s)...')
            odom_rates, camera_rates = self._measure_rates(MEASURE_WINDOW_S)

            odom_blocked, leaking_odom = self._check_all_robots_blocked(odom_rates)
            result = PASS if odom_blocked else FAIL
            results.append(('S3: All odom blocked in LoRa', result))
            print(f'    {result} — odom rates: ' +
                  ', '.join(f'SH_{r:02d}:{hz:.2f}Hz' for r, hz in odom_rates.items()))
            if leaking_odom:
                print(f'         Leaking robots: {leaking_odom}')

            cam_blocked, leaking_cam = self._check_all_robots_blocked(camera_rates)
            result = PASS if cam_blocked else FAIL
            results.append(('S3: All camera blocked in LoRa', result))
            print(f'    {result} — camera rates: ' +
                  ', '.join(f'SH_{r:02d}:{hz:.2f}fps' for r, hz in camera_rates.items()))

        # ── Scenario 4 — Recovery ─────────────────────────────────────────────
        print('\n' + '─'*60)
        print('SCENARIO 4 — Recovery')
        print('─'*60)

        print('\n[1] Removing all jams → waiting for WiFi6 recovery...')
        # Wait for LTE failure to auto-expire then unjam
        self._spin_seconds(2.0)
        self._unjam_all()
        got_wifi6 = self._wait_for_tier(1, timeout=60.0)
        result = PASS if got_wifi6 else FAIL
        results.append(('S4: Full recovery to WiFi6 after LoRa', result))
        print(f'    {result} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

        if got_wifi6:
            self._spin_seconds(2.0)
            print(f'\n[2] Confirming relay resumes in WiFi6 ({MEASURE_WINDOW_S}s)...')
            odom_rates, _ = self._measure_rates(MEASURE_WINDOW_S)
            all_active, missing = self._check_all_robots_active(odom_rates, min_hz=0.5)
            result = PASS if all_active else FAIL
            results.append(('S4: All 8 robots relaying odom after recovery', result))
            print(f'    {result}' + (f' — missing: {missing}' if missing else ''))

        # ── V&V 2.6 summary ───────────────────────────────────────────────────
        vv_checks = [r for n, r in results if 'S2' in n]
        vv_pass = all(r == PASS for r in vv_checks)
        vv_result = PASS if vv_pass else FAIL
        results.append(('V&V 2.6 — 8-robot bridge relay', vv_result))

        self._print_summary(results)

    def _print_summary(self, results):
        total = len(results)
        passed = sum(1 for _, r in results if r == PASS)

        print('\n' + '='*60)
        print('Results')
        print('='*60)
        for name, result in results:
            print(f'  {result}  {name}')
        print('─'*60)
        vv = next((r for n, r in results if 'V&V' in n), FAIL)
        elapsed = time.monotonic()
        print(f'  {vv}  V&V 2.6 — 8-robot bridge relay')
        print(f'  {passed}/{total} checks passed')
        print('\nTier history:')
        for ts, tier in self._tier_history:
            print(f'  t+{ts:.1f}s → {TIER_NAMES.get(tier, "?")}')
        print('='*60 + '\n')


def main(args=None):
    rclpy.init(args=args)
    node = BridgeRelayTest()

    for _ in range(30):
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