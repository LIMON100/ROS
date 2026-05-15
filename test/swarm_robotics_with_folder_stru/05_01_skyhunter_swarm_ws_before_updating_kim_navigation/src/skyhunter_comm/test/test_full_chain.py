#!/usr/bin/env python3
"""
test_full_chain.py — L9b: Full 3-tier chain test + recovery + rapid cycling

Scenarios:
  1. Full degradation chain  — WiFi6 → LTE → LoRa
  2. Full recovery chain     — LoRa → LTE → WiFi6
  3. Rapid cycling           — jam/unjam 3x quickly, FSM must not flap to LoRa

V&V target: precursor to V&V 3.1 (full 3-tier failover)

Usage:
    ros2 run skyhunter_comm test_full_chain

Prerequisites (all must be running):
    ros2 launch skyhunter_gazebo sim.launch.py  (8 robots)
    ros2 launch skyhunter_comm networking.launch.py
"""

import time
import rclpy
from rclpy.node import Node

from skyhunter_msgs.msg import CommState, MeshMetrics, LteStatus
from skyhunter_msgs.srv import JamLink, UnjamLink, InjectFailure

# ── Config ────────────────────────────────────────────────────────────────────
NUM_ROBOTS          = 8
JAM_ATTENUATION_DB  = 60.0   # dB — pushes RSSI well below disconnect threshold
WIFI6_FAIL_DUR      = 3.0    # must match swarm_comm_manager param
WIFI6_RECOVERY_DUR  = 5.0    # must match swarm_comm_manager param
LTE_FAIL_DUR        = 10.0   # must match swarm_comm_manager param
STATE_TIMEOUT            = 20.0   # max wait per tier transition
WIFI6_RECOVERY_TIMEOUT   = 60.0   # WiFi6 re-establishes RSSI + 5s hysteresis after LTE
RAPID_CYCLE_COUNT   = 3      # number of jam/unjam cycles
RAPID_CYCLE_GAP_S   = 1.5    # seconds between jam and unjam (shorter than fail duration)

TIER_NAMES = {0: 'DISCONNECTED', 1: 'WiFi6', 2: 'LTE', 3: 'LoRa'}
PASS = '✅ PASS'
FAIL = '❌ FAIL'


class FullChainTest(Node):

    def __init__(self):
        super().__init__('test_full_chain')

        self._current_tier  = None
        self._tier_history  = []
        self._latest_metrics = None
        self._lte_state: int = -1
        self._lte_rtt_ms: float = 0.0

        self.create_subscription(CommState,    '/comm_state',   self._comm_state_cb, 10)
        self.create_subscription(LteStatus,    '/lte_status',   self._lte_status_cb,  10)
        self.create_subscription(MeshMetrics,  '/mesh_metrics', self._metrics_cb,    10)

        self._jam_client    = self.create_client(JamLink,       '/jam_link')
        self._unjam_client  = self.create_client(UnjamLink,     '/unjam_link')
        self._lte_fail_client   = self.create_client(InjectFailure, '/lte/inject_failure')

        self.get_logger().info('Waiting for services...')
        for client in [self._jam_client, self._unjam_client, self._lte_fail_client]:
            client.wait_for_service(timeout_sec=10.0)
        self.get_logger().info('All services ready.')

    # ── Callbacks ─────────────────────────────────────────────────────────────

    def _comm_state_cb(self, msg: CommState) -> None:
        if msg.current_tier != self._current_tier:
            name = TIER_NAMES.get(msg.current_tier, '?')
            self.get_logger().info(f'[CommState] → {name} (tier {msg.current_tier})')
            self._current_tier = msg.current_tier
            self._tier_history.append((time.monotonic(), msg.current_tier))

    def _metrics_cb(self, msg: MeshMetrics) -> None:
        self._latest_metrics = msg

    def _lte_status_cb(self, msg: LteStatus) -> None:
        if msg.state != self._lte_state:
            state_name = {0: 'DISCONNECTED', 1: 'CONNECTED', 2: 'DEGRADED'}.get(msg.state, '?')
            self.get_logger().info(f'[LteStatus] state → {state_name}  rtt={msg.rtt_ms:.1f}ms')
        self._lte_state = msg.state
        self._lte_rtt_ms = msg.rtt_ms

    # ── Service helpers ───────────────────────────────────────────────────────

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
                req.robot_a        = str(a)
                req.robot_b        = str(b)
                req.attenuation_db = JAM_ATTENUATION_DB
                future = self._jam_client.call_async(req)
                self._wait_futures([future])
                count += 1
        self.get_logger().info(f'Jammed {count} links')

    def _unjam_all(self):
        self.get_logger().info('Removing all jams...')
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = UnjamLink.Request()
                req.robot_a = str(a)
                req.robot_b = str(b)
                future = self._unjam_client.call_async(req)
                self._wait_futures([future])

    def _inject_lte_failure(self, duration_s: float = 25.0):
        """Inject a timed LTE failure. Auto-recovers after duration_s."""
        self.get_logger().info(f'Injecting LTE failure for {duration_s}s...')
        req = InjectFailure.Request()
        req.target_tier   = 2
        req.failure_type  = 'disconnect'
        req.duration_s    = duration_s
        future = self._lte_fail_client.call_async(req)
        self._wait_futures([future])

    def _wait_all_jammed(self, timeout=10.0) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._latest_metrics is not None:
                if all(not lnk.connected for lnk in self._latest_metrics.links):
                    self.get_logger().info('All links confirmed jammed')
                    return True
        self.get_logger().warn('Timeout waiting for all links to be jammed')
        return False

    def _wait_all_unjammed(self, timeout: float = 10.0) -> bool:
        """Wait until no links report jammed=True in mesh_metrics."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._latest_metrics is not None:
                if not any(lnk.jammed for lnk in self._latest_metrics.links):
                    self.get_logger().info('All links confirmed unjammed')
                    return True
        self.get_logger().warn('Timeout waiting for all links to be unjammed')
        return False

    def _wait_for_tier(self, target: int, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._current_tier == target:
                return True
        return False

    def _spin_seconds(self, duration: float):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)

    def _monitor_tier(self, expected: int, duration: float):
        """Spin for duration, return False if tier deviates from expected."""
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
            if self._current_tier != expected:
                return False
        return True

    # ── Test scenarios ────────────────────────────────────────────────────────

    def _scenario_1_degradation(self, results):
        """WiFi6 → LTE → LoRa full degradation chain."""
        print('\n' + '─' * 60)
        print('SCENARIO 1 — Full Degradation Chain')
        print('─' * 60)

        # Step 1: Confirm WiFi6
        print('\n[1] Verifying initial WiFi6 state...')
        self._spin_seconds(2.0)
        ok = self._current_tier == 1
        results.append(('S1: Initial state = WiFi6', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} — tier: {TIER_NAMES.get(self._current_tier, "?")}')
        if not ok:
            return False

        # Step 2: Jam all → WiFi6 → LTE
        print(f'\n[2] Jamming all links → waiting for LTE transition...')
        self._jam_all()
        self._wait_all_jammed()
        ok = self._wait_for_tier(2, timeout=35.0)  # waits for LTE timer expiry + FSM reaction
        results.append(('S1: WiFi6 → LTE on jam', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} — tier: {TIER_NAMES.get(self._current_tier, "?")}')
        if not ok:
            self._unjam_all()
            return False

        # Step 3: Inject LTE failure → LTE → LoRa
        print(f'\n[3] Injecting LTE failure → waiting for LoRa transition...')
        self._inject_lte_failure(duration_s=20.0)
        ok = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        results.append(('S1: LTE → LoRa on LTE failure', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

        # Step 4: Hold LoRa stable for 5s
        print('\n[4] Holding LoRa state for 5s — must remain stable...')
        ok = self._monitor_tier(3, duration=5.0)
        results.append(('S1: LoRa stable after full degradation', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

        return True

    def _scenario_2_recovery(self, results):
        """LoRa → LTE → WiFi6 full recovery chain."""
        print('\n' + '─' * 60)
        print('SCENARIO 2 — Full Recovery Chain')
        print('─' * 60)

        # Step 1: Recover LTE → LoRa → LTE
        print('\n[1] Recovering LTE → expecting LoRa → LTE transition...')
        ok = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        results.append(('S2: LoRa → LTE on LTE recovery', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

        # Step 2: Unjam all → LTE → WiFi6
        print(f'\n[2] Removing all jams → waiting for WiFi6 recovery (up to {WIFI6_RECOVERY_TIMEOUT}s)...')
        self._unjam_all()
        ok = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        results.append(('S2: LTE → WiFi6 on unjam', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

        # Step 3: Hold WiFi6 stable for 5s
        print('\n[3] Holding WiFi6 state for 5s — must remain stable...')
        ok = self._monitor_tier(1, duration=5.0)
        results.append(('S2: WiFi6 stable after full recovery', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

    def _scenario_3_rapid_cycling(self, results):
        """Rapid jam/unjam cycling — FSM must not drop to LoRa."""
        print('\n' + '─' * 60)
        print('SCENARIO 3 — Rapid Cycling')
        print('─' * 60)
        print(f'    Cycling {RAPID_CYCLE_COUNT}x with {RAPID_CYCLE_GAP_S}s jam window')
        print(f'    (jam window < wifi6_fail_duration {WIFI6_FAIL_DUR}s → FSM should not trip)')

        reached_lora = False

        for i in range(1, RAPID_CYCLE_COUNT + 1):
            print(f'\n[cycle {i}/{RAPID_CYCLE_COUNT}] Jamming...')
            self._jam_all()
            self._spin_seconds(RAPID_CYCLE_GAP_S)

            if self._current_tier == 3:
                reached_lora = True
                print(f'    ⚠️  Dropped to LoRa on cycle {i}')

            print(f'    Unjamming...')
            self._unjam_all()
            self._spin_seconds(1.0)
            print(f'    Tier after unjam: {TIER_NAMES.get(self._current_tier, "?")}')

        # After cycling, wait for WiFi6 to settle
        # Final flush — unjam again and spin to let jammer_service publish clean /jammed_links
        self.get_logger().info('Final unjam flush...')
        self._unjam_all()
        self._spin_seconds(2.0)
        self._wait_all_unjammed(timeout=15.0)
        print(f'\n    Waiting for WiFi6 to settle (up to {WIFI6_RECOVERY_TIMEOUT}s)...')
        ok_wifi6 = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        ok_no_lora = not reached_lora

        results.append(('S3: No LoRa drop during rapid cycling', PASS if ok_no_lora else FAIL))
        results.append(('S3: WiFi6 restored after cycling', PASS if ok_wifi6 else FAIL))
        print(f'    {PASS if ok_no_lora else FAIL} — no LoRa drop')
        print(f'    {PASS if ok_wifi6 else FAIL} — WiFi6 restored')

    # ── Main run ──────────────────────────────────────────────────────────────

    def run(self):
        results = []
        t0 = time.monotonic()

        # Wait for first comm_state message
        print('\nWaiting for comm_state...')
        deadline = time.monotonic() + 10.0
        while self._current_tier is None and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
        if self._current_tier is None:
            print('ERROR: No /comm_state received. Is networking.launch.py running?')
            return

        s1_ok = self._scenario_1_degradation(results)
        if s1_ok:
            self._scenario_2_recovery(results)

        # Reset to clean state before rapid cycling
        print('\n[Reset] Ensuring clean WiFi6 state before rapid cycling...')
        self._unjam_all()
        self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        self._scenario_3_rapid_cycling(results)

        # ── Summary ───────────────────────────────────────────────────────────
        elapsed = time.monotonic() - t0
        passed  = sum(1 for _, r in results if r == PASS)
        total   = len(results)
        vv_pass = passed == total

        print('\n' + '=' * 60)
        print('Results')
        print('=' * 60)
        for label, result in results:
            print(f'  {result}  {label}')
        print('─' * 60)
        print(f'  {"✅ PASS" if vv_pass else "❌ FAIL"}  V&V 3.1 precursor — Full 3-tier chain')
        print(f'  Elapsed: {elapsed:.1f}s  |  {passed}/{total} checks passed')

        print('\nTier history:')
        for ts, tier in self._tier_history:
            print(f'  t+{ts:.1f}s → {TIER_NAMES.get(tier, "?")}')
        print('=' * 60)


def main(args=None):
    rclpy.init(args=args)
    node = FullChainTest()
    try:
        node.run()
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()