#!/usr/bin/env python3
"""
test_failover.py — L8b: WiFi6→LTE failover test via jammer

Tests:
  1. Verify initial state is WiFi6
  2. Jam all robot links → RSSI drops below threshold → FSM transitions to LTE
  3. Unjam → WiFi6 recovers after hysteresis window
  4. V&V 2.5: WiFi6→LTE trigger confirmed

Usage:
    ros2 run skyhunter_comm test_failover

Prerequisites (all must be running):
    ros2 launch skyhunter_comm networking.launch.py
    ros2 launch skyhunter_gazebo sim.launch.py  (8 robots)
"""

import time
import rclpy
from rclpy.node import Node
from skyhunter_msgs.msg import CommState
from skyhunter_msgs.srv import JamLink, UnjamLink


# ── Config ────────────────────────────────────────────────────────────────────
NUM_ROBOTS          = 8
JAM_ATTENUATION_DB  = 60.0    # enough to push RSSI below -85 threshold
WIFI6_FAIL_DURATION = 3.0     # must match swarm_comm_manager param
WIFI6_RECOVERY_DUR  = 5.0     # hysteresis recovery window
STATE_TIMEOUT       = 15.0    # max wait for state transition

TIER_NAMES = {1: 'WiFi6', 2: 'LTE', 3: 'LoRa', 4: 'DISCONNECTED'}

# ── Results ───────────────────────────────────────────────────────────────────
PASS = '✅ PASS'
FAIL = '❌ FAIL'


class FailoverTest(Node):

    def __init__(self):
        super().__init__('test_failover')

        self._current_tier = None
        self._tier_history = []

        # Subscribe to comm_state
        self.create_subscription(CommState, '/comm_state', self._comm_state_cb, 10)

        # Jammer clients
        self._jam_client   = self.create_client(JamLink,   '/jam_link')
        self._unjam_client = self.create_client(UnjamLink, '/unjam_link')

        self.get_logger().info('test_failover: waiting for services...')
        self._jam_client.wait_for_service(timeout_sec=10.0)
        self._unjam_client.wait_for_service(timeout_sec=10.0)

    def _comm_state_cb(self, msg: CommState) -> None:
        if msg.current_tier != self._current_tier:
            name = TIER_NAMES.get(msg.current_tier, '?')
            self.get_logger().info(f'[CommState] → {name} (tier {msg.current_tier})')
            self._current_tier = msg.current_tier
            self._tier_history.append((time.monotonic(), msg.current_tier))

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _wait_for_futures(self, futures: list, timeout: float = 5.0):
        """Fire all futures concurrently then wait for all to complete."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if all(f.done() for f in futures):
                break
            rclpy.spin_once(self, timeout_sec=0.05)

    def _jam_all(self):
        """Jam all link pairs sequentially — concurrent calls drop on single-threaded executor."""
        self.get_logger().info(f'Jamming all links at {JAM_ATTENUATION_DB} dB...')
        count = 0
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = JamLink.Request()
                req.robot_a        = str(a)
                req.robot_b        = str(b)
                req.attenuation_db = JAM_ATTENUATION_DB
                future = self._jam_client.call_async(req)
                self._wait_for_futures([future])
                count += 1
        self.get_logger().info(f'Jammed {count} links')

    def _unjam_all(self):
        """Unjam all link pairs sequentially — concurrent calls drop on single-threaded executor."""
        self.get_logger().info('Removing all jams...')
        count = 0
        for a in range(1, NUM_ROBOTS + 1):
            for b in range(a + 1, NUM_ROBOTS + 1):
                req = UnjamLink.Request()
                req.robot_a = str(a)
                req.robot_b = str(b)
                future = self._unjam_client.call_async(req)
                self._wait_for_futures([future])
                count += 1
        self.get_logger().info(f'Unjammed {count} links')

    def _wait_for_tier(self, target_tier: int, timeout: float) -> bool:
        """Spin until comm_state reaches target_tier or timeout."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.2)
            if self._current_tier == target_tier:
                return True
        return False

    def _spin_seconds(self, seconds: float):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)

    # ── Test steps ────────────────────────────────────────────────────────────

    def run(self):
        results = []

        print('\n' + '='*60)
        print('L8b — WiFi6→LTE Failover Test')
        print('='*60)

        # ── Step 1: Verify initial WiFi6 state ───────────────────────────────
        print('\n[1] Waiting for initial WiFi6 state...')
        got_wifi6 = self._wait_for_tier(1, timeout=STATE_TIMEOUT)
        result = PASS if got_wifi6 else FAIL
        results.append(('Initial state = WiFi6', result))
        print(f'    {result} — initial tier: {TIER_NAMES.get(self._current_tier, "?")}')

        if not got_wifi6:
            print('    Cannot proceed — stack not in WiFi6. Is networking.launch.py running?')
            self._print_summary(results)
            return

        # ── Step 2: Jam all links → expect LTE transition ────────────────────
        print(f'\n[2] Jamming all links (attenuation={JAM_ATTENUATION_DB} dB)...')
        self._jam_all()
        print(f'    Waiting up to {STATE_TIMEOUT}s for LTE transition '
              f'(FSM needs {WIFI6_FAIL_DURATION}s sustained failure)...')

        transitioned_to_lte = self._wait_for_tier(2, timeout=STATE_TIMEOUT)
        result = PASS if transitioned_to_lte else FAIL
        results.append(('WiFi6 → LTE on sustained jam', result))
        print(f'    {result} — current tier: {TIER_NAMES.get(self._current_tier, "?")}')

        # ── Step 3: Hold jammed state — LTE should stay stable ───────────────
        print('\n[3] Holding jammed state for 5s — LTE should remain stable...')
        tier_before = self._current_tier
        self._spin_seconds(5.0)
        stayed_lte = self._current_tier == 2
        result = PASS if stayed_lte else FAIL
        results.append(('LTE stable while jammed', result))
        print(f'    {result} — tier: {TIER_NAMES.get(self._current_tier, "?")}')

        # ── Step 4: Unjam → expect WiFi6 recovery ────────────────────────────
        print(f'\n[4] Removing jams — waiting up to {STATE_TIMEOUT}s for WiFi6 recovery '
              f'(hysteresis = {WIFI6_RECOVERY_DUR}s)...')
        self._unjam_all()

        recovered_wifi6 = self._wait_for_tier(1, timeout=STATE_TIMEOUT)
        result = PASS if recovered_wifi6 else FAIL
        results.append(('LTE → WiFi6 recovery after unjam', result))
        print(f'    {result} — current tier: {TIER_NAMES.get(self._current_tier, "?")}')

        # ── Step 5: V&V 2.5 summary ──────────────────────────────────────────
        all_pass = all(r == PASS for _, r in results)
        vv_result = PASS if all_pass else FAIL
        results.append(('V&V 2.5 — WiFi6→LTE trigger', vv_result))

        self._print_summary(results)

    def _print_summary(self, results):
        print('\n' + '='*60)
        print('Results')
        print('='*60)
        for name, result in results:
            print(f'  {result}  {name}')
        print()
        print('Tier history:')
        for ts, tier in self._tier_history:
            print(f'  t+{ts:.1f}s → {TIER_NAMES.get(tier, "?")}')
        print('='*60 + '\n')


def main(args=None):
    rclpy.init(args=args)
    node = FailoverTest()

    # Spin briefly to get initial state
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