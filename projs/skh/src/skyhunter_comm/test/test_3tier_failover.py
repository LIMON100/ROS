#!/usr/bin/env python3
"""
test_3tier_failover.py — L14a + L14b: Full 3-tier failover (V&V 3.1)

Scenarios:
  A — WiFi6 → LTE → LoRa → LTE → WiFi6   (full degradation + full recovery)
  B — WiFi6 → LTE → WiFi6                 (partial degradation, no LoRa)
  C — WiFi6 → LoRa direct                 (LTE unavailable from start)
  D — Mid-mission failover                 (failover while comm traffic is active)

Pass criteria (V&V 3.1):
  - All 4 scenarios complete without entering DISCONNECTED state
  - Tier transitions occur within expected timeouts
  - Full recovery to WiFi6 in all scenarios
  - No unintended tier drops during stable hold periods

Usage:
    ros2 run skyhunter_comm test_3tier_failover

Prerequisites (all must be running):
    ros2 launch skyhunter_gazebo sim.launch.py   (8 robots)
    ros2 launch skyhunter_comm networking.launch.py
"""

import time
import rclpy
from rclpy.node import Node

from skyhunter_msgs.msg import CommState, MeshMetrics, LteStatus, LoraStatus
from skyhunter_msgs.srv import JamLink, UnjamLink, InjectFailure

# ── Config ────────────────────────────────────────────────────────────────────
NUM_ROBOTS              = 8
JAM_ATTENUATION_DB      = 60.0
WIFI6_FAIL_DUR          = 3.0    # must match swarm_comm_manager param
WIFI6_RECOVERY_DUR      = 5.0    # must match swarm_comm_manager param
LTE_FAIL_DUR            = 10.0   # must match swarm_comm_manager param
STATE_TIMEOUT           = 25.0   # max wait per tier transition
WIFI6_RECOVERY_TIMEOUT  = 60.0
HOLD_DURATION           = 5.0
LTE_FAILURE_DURATION    = 20.0

TIER_NAMES = {0: 'DISCONNECTED', 1: 'WiFi6', 2: 'LTE', 3: 'LoRa'}
PASS = '\u2705 PASS'
FAIL = '\u274c FAIL'


class ThreeTierFailoverTest(Node):

    def __init__(self):
        super().__init__('test_3tier_failover')

        self._current_tier      = None
        self._tier_history      = []
        self._latest_metrics    = None
        self._lte_state         = -1
        self._lora_active       = False
        self._disconnected_seen = False

        self.create_subscription(CommState,   '/comm_state',   self._comm_state_cb,  10)
        self.create_subscription(LteStatus,   '/lte_status',   self._lte_status_cb,  10)
        self.create_subscription(MeshMetrics, '/mesh_metrics', self._metrics_cb,     10)
        self.create_subscription(LoraStatus,  '/lora/status',  self._lora_status_cb, 10)

        self._jam_client       = self.create_client(JamLink,       '/jam_link')
        self._unjam_client     = self.create_client(UnjamLink,     '/unjam_link')
        self._lte_fail_client  = self.create_client(InjectFailure, '/lte/inject_failure')


        self.get_logger().info('Waiting for services...')
        for client in [self._jam_client, self._unjam_client, self._lte_fail_client]:
            client.wait_for_service(timeout_sec=10.0)
        self.get_logger().info('All services ready.')

    # ── Callbacks ─────────────────────────────────────────────────────────────

    def _comm_state_cb(self, msg: CommState) -> None:
        if msg.current_tier != self._current_tier:
            name = TIER_NAMES.get(msg.current_tier, '?')
            self.get_logger().info(f'[CommState] \u2192 {name} (tier {msg.current_tier})')
            self._current_tier = msg.current_tier
            self._tier_history.append((time.monotonic(), msg.current_tier))
        if msg.current_tier == 0:
            self._disconnected_seen = True

    def _metrics_cb(self, msg: MeshMetrics) -> None:
        self._latest_metrics = msg

    def _lte_status_cb(self, msg: LteStatus) -> None:
        self._lte_state = msg.state

    def _lora_status_cb(self, msg: LoraStatus) -> None:
        self._lora_active = msg.active

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _wait_futures(self, futures, timeout=30.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if all(f.done() for f in futures):
                break
            rclpy.spin_once(self, timeout_sec=0.05)

    def _jam_all(self):
        self.get_logger().info(f'Jamming all links at {JAM_ATTENUATION_DB} dB...')
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = JamLink.Request()
                req.robot_a        = str(a)
                req.robot_b        = str(b)
                req.attenuation_db = JAM_ATTENUATION_DB
                self._wait_futures([self._jam_client.call_async(req)])

    def _unjam_all(self):
        self.get_logger().info('Removing all jams...')
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = UnjamLink.Request()
                req.robot_a = str(a)
                req.robot_b = str(b)
                self._wait_futures([self._unjam_client.call_async(req)])

    def _inject_lte_failure(self, duration_s: float = LTE_FAILURE_DURATION):
        self.get_logger().info(f'Injecting LTE failure for {duration_s}s...')
        req = InjectFailure.Request()
        req.target_tier  = 2
        req.failure_type = 'disconnect'
        req.duration_s   = duration_s
        self._wait_futures([self._lte_fail_client.call_async(req)])

    def _wait_for_tier(self, target: int, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._current_tier == target:
                return True
        return False

    def _monitor_tier(self, expected: int, duration: float) -> bool:
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._current_tier != expected:
                return False
        return True

    def _spin_seconds(self, duration: float):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)

    def _wait_all_unjammed(self, timeout: float = 15.0) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._latest_metrics and not any(lnk.jammed for lnk in self._latest_metrics.links):
                self.get_logger().info('All links confirmed unjammed')
                return True
        return False

    def _reset_to_wifi6(self, label='Reset'):
        print(f'\n[{label}] Recovering to WiFi6...')
        self._unjam_all()
        self._wait_all_unjammed()
        # LTE auto-recovers from timed failure injection — no explicit service call needed
        ok = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        self._disconnected_seen = False
        if not ok:
            print(f'  WARNING: WiFi6 recovery timed out -- tier: {TIER_NAMES.get(self._current_tier, "?")}')
        return ok

    # ── Scenarios ─────────────────────────────────────────────────────────────

    def _scenario_a(self, results):
        """Scenario A: WiFi6 -> LTE -> LoRa -> LTE -> WiFi6"""
        print('\n' + '=' * 60)
        print('SCENARIO A -- Full Degradation + Full Recovery')
        print('  WiFi6 -> LTE -> LoRa -> LTE -> WiFi6')
        print('=' * 60)

        print('\n[A1] Confirming WiFi6 initial state...')
        self._spin_seconds(2.0)
        ok = self._current_tier == 1
        results.append(('A: Initial state = WiFi6', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        if not ok:
            return False

        print(f'\n[A2] Jamming all links -> waiting for LTE (timeout {STATE_TIMEOUT}s)...')
        self._jam_all()
        ok = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        results.append(('A: WiFi6 -> LTE on full jam', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')
        if not ok:
            self._unjam_all()
            return False

        print(f'\n[A3] Holding LTE for {HOLD_DURATION}s...')
        ok = self._monitor_tier(2, duration=HOLD_DURATION)
        results.append(('A: LTE stable under jam', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')

        print(f'\n[A4] Injecting LTE failure -> waiting for LoRa (timeout {STATE_TIMEOUT}s)...')
        self._inject_lte_failure(duration_s=LTE_FAILURE_DURATION)
        ok = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        results.append(('A: LTE -> LoRa on LTE failure', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        print(f'\n[A5] Holding LoRa for {HOLD_DURATION}s...')
        ok = self._monitor_tier(3, duration=HOLD_DURATION)
        results.append(('A: LoRa stable after full degradation', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')

        print(f'\n[A6] Waiting for LTE auto-recovery -> LoRa -> LTE (timeout {STATE_TIMEOUT}s)...')
        ok = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        results.append(('A: LoRa -> LTE on LTE recovery', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        print(f'\n[A7] Removing jams -> waiting for WiFi6 (timeout {WIFI6_RECOVERY_TIMEOUT}s)...')
        self._unjam_all()
        self._wait_all_unjammed()
        ok = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        results.append(('A: LTE -> WiFi6 on unjam', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        ok = not self._disconnected_seen
        results.append(('A: No DISCONNECTED state entered', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        return True

    def _scenario_b(self, results):
        """Scenario B: WiFi6 -> LTE -> WiFi6 (no LoRa)"""
        print('\n' + '=' * 60)
        print('SCENARIO B -- Partial Degradation (no LoRa)')
        print('  WiFi6 -> LTE -> WiFi6')
        print('=' * 60)

        print('\n[B1] Confirming WiFi6 initial state...')
        self._spin_seconds(2.0)
        ok = self._current_tier == 1
        results.append(('B: Initial state = WiFi6', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        if not ok:
            return False

        print(f'\n[B2] Jamming all links -> waiting for LTE (timeout {STATE_TIMEOUT}s)...')
        self._jam_all()
        ok = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        results.append(('B: WiFi6 -> LTE on jam', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')
        if not ok:
            self._unjam_all()
            return False

        print(f'\n[B3] Holding LTE for {HOLD_DURATION}s -- must NOT drop to LoRa...')
        ok = self._monitor_tier(2, duration=HOLD_DURATION)
        results.append(('B: Stays at LTE (no LoRa drop)', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        print(f'\n[B4] Removing jams -> waiting for WiFi6 (timeout {WIFI6_RECOVERY_TIMEOUT}s)...')
        self._unjam_all()
        self._wait_all_unjammed()
        ok = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        results.append(('B: LTE -> WiFi6 on unjam', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        ok = not self._disconnected_seen
        results.append(('B: No DISCONNECTED state entered', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        return True

    def _scenario_c(self, results):
        """Scenario C: WiFi6 -> LoRa direct (LTE unavailable from start)"""
        print('\n' + '=' * 60)
        print('SCENARIO C -- Direct WiFi6 -> LoRa (LTE unavailable from start)')
        print('=' * 60)

        print('\n[C1] Confirming WiFi6 initial state...')
        self._spin_seconds(2.0)
        ok = self._current_tier == 1
        results.append(('C: Initial state = WiFi6', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        if not ok:
            return False

        # Duration covers C3 jam wait + C4 hold only — expires naturally before Scenario D reset
        c_lte_fail_dur = STATE_TIMEOUT + HOLD_DURATION
        print(f'\n[C2] Making LTE unavailable ({c_lte_fail_dur}s = STATE_TIMEOUT + HOLD_DURATION)...')
        self._inject_lte_failure(duration_s=c_lte_fail_dur)
        self._spin_seconds(2.0)

        print(f'\n[C3] Jamming all links -> expecting direct skip to LoRa (timeout {STATE_TIMEOUT}s)...')
        self._jam_all()
        ok = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        results.append(('C: WiFi6 -> LoRa direct (LTE unavailable)', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        print(f'\n[C4] Holding LoRa for {HOLD_DURATION}s...')
        ok = self._monitor_tier(3, duration=HOLD_DURATION)
        results.append(('C: LoRa stable (LTE unavailable)', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')

        print(f'\n[C5] Recovering LTE + removing jams -> waiting for WiFi6 (timeout {WIFI6_RECOVERY_TIMEOUT}s)...')
        # LTE auto-recovers after 120s — just unjam and wait for WiFi6
        self._unjam_all()
        self._wait_all_unjammed()
        ok = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        results.append(('C: Full recovery to WiFi6', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        ok = not self._disconnected_seen
        results.append(('C: No DISCONNECTED state entered', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        return True

    def _scenario_d(self, results):
        """Scenario D: Mid-mission failover with active comm traffic"""
        print('\n' + '=' * 60)
        print('SCENARIO D -- Mid-Mission Failover')
        print('  Active comm traffic -> WiFi6 -> LTE -> LoRa -> WiFi6')
        print('=' * 60)

        print('\n[D1] Confirming WiFi6 and simulating active mission (10s)...')
        self._spin_seconds(2.0)
        ok = self._current_tier == 1
        results.append(('D: Initial state = WiFi6', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        if not ok:
            return False

        t_start = time.monotonic()
        msgs_seen = 0
        while time.monotonic() < t_start + 10.0:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._latest_metrics:
                msgs_seen += 1
        print(f'    {msgs_seen} mesh_metrics messages received during mission window')
        results.append(('D: Active comm traffic confirmed during WiFi6', PASS if msgs_seen > 0 else FAIL))
        print(f'    {PASS if msgs_seen > 0 else FAIL}')

        print(f'\n[D2] Mid-mission jam -> waiting for LTE (timeout {STATE_TIMEOUT}s)...')
        self._jam_all()
        ok = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        results.append(('D: Mid-mission WiFi6 -> LTE', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')
        if not ok:
            self._unjam_all()
            return False

        print(f'\n[D3] Injecting LTE failure -> waiting for LoRa (timeout {STATE_TIMEOUT}s)...')
        self._inject_lte_failure(duration_s=LTE_FAILURE_DURATION)
        ok = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        results.append(('D: Mid-mission LTE -> LoRa', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        print(f'\n[D4] Holding LoRa for {HOLD_DURATION}s...')
        ok = self._monitor_tier(3, duration=HOLD_DURATION)
        results.append(('D: LoRa stable during mid-mission', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')

        print(f'\n[D5] Recovering all -> waiting for WiFi6 (timeout {WIFI6_RECOVERY_TIMEOUT}s)...')
        self._unjam_all()
        self._wait_all_unjammed()
        ok = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        results.append(('D: Full recovery to WiFi6 after mid-mission', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')

        ok = not self._disconnected_seen
        results.append(('D: No DISCONNECTED state entered', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        return True

    # ── Main run ──────────────────────────────────────────────────────────────

    def run(self):
        results = []
        t0 = time.monotonic()

        print('\nWaiting for /comm_state...')
        deadline = time.monotonic() + 10.0
        while self._current_tier is None and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
        if self._current_tier is None:
            print('ERROR: No /comm_state received. Is networking.launch.py running?')
            return 1

        self._reset_to_wifi6('Pre-A reset')
        self._scenario_a(results)

        self._reset_to_wifi6('Pre-B reset')
        self._scenario_b(results)

        self._reset_to_wifi6('Pre-C reset')
        self._scenario_c(results)

        self._reset_to_wifi6('Pre-D reset')
        self._scenario_d(results)

        elapsed = time.monotonic() - t0
        passed  = sum(1 for _, r in results if r == PASS)
        total   = len(results)
        vv_pass = passed == total

        print('\n' + '=' * 60)
        print('V&V 3.1 -- Full 3-Tier Failover Results')
        print('=' * 60)
        for label, result in results:
            print(f'  {result}  {label}')
        print('-' * 60)
        print(f'  {"PASS" if vv_pass else "FAIL"}  V&V 3.1 -- Full 3-tier failover')
        print(f'  Elapsed: {elapsed:.1f}s  |  {passed}/{total} checks passed')
        print('\nTier history:')
        t_ref = self._tier_history[0][0] if self._tier_history else t0
        for ts, tier in self._tier_history:
            print(f'  t+{ts - t_ref:.1f}s -> {TIER_NAMES.get(tier, "?")}')
        print('=' * 60)

        return 0 if vv_pass else 1


def main(args=None):
    rclpy.init(args=args)
    node = ThreeTierFailoverTest()
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