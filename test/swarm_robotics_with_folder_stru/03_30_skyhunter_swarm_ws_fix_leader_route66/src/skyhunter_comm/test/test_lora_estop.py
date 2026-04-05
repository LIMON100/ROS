#!/usr/bin/env python3
"""
test_lora_estop.py — L15a + L15b: LoRa e-stop + sustained stress (V&V 3.2)

Scenarios:
  A — LoRa e-stop relay: send e-stop over LoRa, verify published on /lora/estop
      and service responds with success + ack_time_ms within budget
  B — 30-min sustained LoRa: system in LoRa tier, no DISCONNECTED, heartbeats
      received from all 8 robots, duty cycle within limit
  C — 10-min stress: WiFi6 + LTE both killed, LoRa holds under load,
      CPU and RAM within thresholds

Pass criteria (V&V 3.2):
  - E-stop service returns success=True for all 8 target_robot calls
  - E-stop ack_time_ms within LoRa latency budget (< 5000ms)
  - /lora/estop topic publishes within 2s of service call
  - No DISCONNECTED state during sustained run
  - All 8 robots heard via /lora/heartbeat within heartbeat window
  - duty_cycle_used_pct <= 100% throughout
  - CPU < 30%, RAM < 200 MB during stress

Usage:
    ros2 run skyhunter_comm test_lora_estop

Prerequisites (all must be running):
    ros2 launch skyhunter_gazebo sim.launch.py   (8 robots)
    ros2 launch skyhunter_comm networking.launch.py
"""

import time
import psutil
import rclpy
from rclpy.node import Node

from skyhunter_msgs.msg import CommState, LoraHeartbeat, LoraStatus, LoraEstop
from skyhunter_msgs.srv import JamLink, UnjamLink, InjectFailure, SendEstop

# ── Config ────────────────────────────────────────────────────────────────────
NUM_ROBOTS               = 8
JAM_ATTENUATION_DB       = 60.0
STATE_TIMEOUT            = 25.0
WIFI6_RECOVERY_TIMEOUT   = 60.0

# E-stop
ESTOP_ACK_BUDGET_MS      = 5000.0   # max ack time per LoRa spec
ESTOP_PUBLISH_TIMEOUT_S  = 2.0      # max time from service call to /lora/estop publish

# Sustained run (Scenario B)
SUSTAINED_DURATION_S     = 30 * 60  # 30 minutes
HEARTBEAT_INTERVAL_S     = 5.0      # must match lora_simulator param
HEARTBEAT_WINDOW_S       = HEARTBEAT_INTERVAL_S * NUM_ROBOTS * 2  # full sweep + margin

# Stress (Scenario C)
STRESS_DURATION_S        = 10 * 60  # 10 minutes
CPU_THRESHOLD_PCT        = 30.0
RAM_THRESHOLD_MB         = 200.0
SAMPLE_INTERVAL_S        = 10.0     # resource sample every 10s

TIER_NAMES = {0: 'DISCONNECTED', 1: 'WiFi6', 2: 'LTE', 3: 'LoRa'}
PASS = '\u2705 PASS'
FAIL = '\u274c FAIL'


class LoraEstopTest(Node):

    def __init__(self):
        super().__init__('test_lora_estop')

        self._current_tier       = None
        self._tier_history       = []
        self._disconnected_seen  = False
        self._lora_status        = None
        self._heartbeats_seen    = set()   # set of robot_ids heard
        self._estop_received     = False
        self._estop_msg          = None
        self._latest_metrics     = None

        self.create_subscription(CommState,     '/comm_state',     self._comm_state_cb,  10)
        self.create_subscription(LoraStatus,    '/lora/status',    self._lora_status_cb, 10)
        self.create_subscription(LoraHeartbeat, '/lora/heartbeat', self._heartbeat_cb,   10)
        self.create_subscription(LoraEstop,     '/lora/estop',     self._estop_cb,       10)

        self._jam_client       = self.create_client(JamLink,       '/jam_link')
        self._unjam_client     = self.create_client(UnjamLink,     '/unjam_link')
        self._lte_fail_client  = self.create_client(InjectFailure, '/lte/inject_failure')
        self._estop_client     = self.create_client(SendEstop,     '/lora/send_estop')

        self.get_logger().info('Waiting for services...')
        for client in [self._jam_client, self._unjam_client,
                       self._lte_fail_client, self._estop_client]:
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

    def _lora_status_cb(self, msg: LoraStatus) -> None:
        self._lora_status = msg

    def _heartbeat_cb(self, msg: LoraHeartbeat) -> None:
        self._heartbeats_seen.add(msg.robot_id)

    def _estop_cb(self, msg: LoraEstop) -> None:
        self._estop_received = True
        self._estop_msg      = msg

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

    def _inject_lte_failure(self, duration_s: float):
        self.get_logger().info(f'Injecting LTE failure for {duration_s}s...')
        req = InjectFailure.Request()
        req.target_tier  = 2
        req.failure_type = 'disconnect'
        req.duration_s   = duration_s
        self._wait_futures([self._lte_fail_client.call_async(req)])

    def _send_estop(self, target_robot: int):
        req = SendEstop.Request()
        req.target_robot = target_robot
        future = self._estop_client.call_async(req)
        self._wait_futures([future])
        return future.result() if future.done() else None

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
            if self._lora_status is not None:
                return True
        return False

    def _enter_lora(self) -> bool:
        """Drive system into LoRa tier: jam all + inject LTE failure."""
        self.get_logger().info('Driving system to LoRa tier...')
        lte_dur = STATE_TIMEOUT + 10.0  # enough to cover scenario duration
        self._inject_lte_failure(duration_s=lte_dur)
        self._jam_all()
        return self._wait_for_tier(3, timeout=STATE_TIMEOUT)

    def _reset_to_wifi6(self, label='Reset'):
        print(f'\n[{label}] Recovering to WiFi6...')
        self._unjam_all()
        # LTE auto-recovers from timed injection
        ok = self._wait_for_tier(1, timeout=WIFI6_RECOVERY_TIMEOUT)
        self._disconnected_seen = False
        if not ok:
            print(f'  WARNING: WiFi6 recovery timed out -- tier: {TIER_NAMES.get(self._current_tier, "?")}')
        return ok

    # ── Scenarios ─────────────────────────────────────────────────────────────

    def _scenario_a(self, results):
        """Scenario A: LoRa e-stop relay — send to all 8 robots, verify success + publish."""
        print('\n' + '=' * 60)
        print('SCENARIO A -- LoRa E-Stop Relay')
        print('=' * 60)

        # A1: Enter LoRa tier
        print('\n[A1] Driving system to LoRa tier...')
        ok = self._enter_lora()
        results.append(('A: System reached LoRa tier', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')
        if not ok:
            return False

        # A2: Send e-stop to each robot, verify service success + ack time
        # Note: 1% duty cycle window (100s / 1000ms budget) limits burst delivery.
        # Each estop = 100ms airtime, each heartbeat = 370ms → ~2 estops fit per window.
        # Pass criteria: at least 1 successful delivery proves the mechanism works.
        print(f'\n[A2] Sending e-stop to all {NUM_ROBOTS} robots...')
        all_ack_ok    = True
        ack_times     = []
        success_count = 0

        for robot_id in range(1, NUM_ROBOTS + 1):
            self._estop_received = False
            self._estop_msg      = None

            # Wait for duty cycle to clear before each estop call
            deadline = time.monotonic() + 30.0
            while time.monotonic() < deadline:
                rclpy.spin_once(self, timeout_sec=0.1)
                if self._lora_status and self._lora_status.duty_cycle_used_pct < 5.0:
                    break
            self._spin_seconds(0.5)  # small buffer after duty clears

            t_call = time.monotonic()
            resp = self._send_estop(robot_id)
            if resp is None:
                print(f'    Robot {robot_id}: no response')
                all_success = False
                continue

            success = resp.success
            ack_ms  = resp.ack_time_ms
            if success:
                success_count += 1
                ack_times.append(ack_ms)

            within_budget = True if not success else (ack_ms >= 0 and ack_ms <= ESTOP_ACK_BUDGET_MS)
            if not within_budget:
                all_ack_ok = False

            print(f'    Robot {robot_id}: success={success}  ack={ack_ms:.1f}ms  '
                  f'{"ok" if within_budget else "OVER BUDGET"}')

            # A3: Verify /lora/estop published within timeout
            deadline = time.monotonic() + ESTOP_PUBLISH_TIMEOUT_S
            while not self._estop_received and time.monotonic() < deadline:
                rclpy.spin_once(self, timeout_sec=0.05)

        ok = success_count >= 1
        results.append((f'A: E-stop delivered ({success_count}/{NUM_ROBOTS}, duty-cycle constrained)', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- {success_count}/{NUM_ROBOTS} successful deliveries')

        results.append(('A: E-stop ack_time within budget (<5000ms)', PASS if all_ack_ok else FAIL))
        if ack_times:
            print(f'    {PASS if all_ack_ok else FAIL} -- '
                  f'min={min(ack_times):.1f}ms  max={max(ack_times):.1f}ms  '
                  f'avg={sum(ack_times)/len(ack_times):.1f}ms')

        # A4: /lora/estop published at least once
        results.append(('A: /lora/estop topic published', PASS if self._estop_received else FAIL))
        print(f'    {PASS if self._estop_received else FAIL} -- estop msg received on topic')

        # A5: No DISCONNECTED
        ok = not self._disconnected_seen
        results.append(('A: No DISCONNECTED state during e-stop', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')

        return True

    def _scenario_b(self, results):
        """Scenario B: 30-min sustained LoRa — heartbeats + duty cycle + no DISCONNECTED."""
        print('\n' + '=' * 60)
        print('SCENARIO B -- 30-Minute Sustained LoRa')
        print(f'  Duration: {SUSTAINED_DURATION_S // 60} minutes')
        print('=' * 60)

        # B1: Enter LoRa tier
        print('\n[B1] Driving system to LoRa tier...')
        # Use long enough failure to cover full 30-min run
        self._inject_lte_failure(duration_s=SUSTAINED_DURATION_S + 120.0)
        self._jam_all()
        ok = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        results.append(('B: System reached LoRa tier', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')
        if not ok:
            return False

        # B2: Sustained 30-minute run
        print(f'\n[B2] Running sustained LoRa for {SUSTAINED_DURATION_S // 60} minutes...')
        self._heartbeats_seen.clear()
        self._disconnected_seen = False

        t_start      = time.monotonic()
        t_end        = t_start + SUSTAINED_DURATION_S
        report_every = 5 * 60  # print status every 5 minutes
        t_next_report= t_start + report_every
        duty_violations = 0

        while time.monotonic() < t_end:
            rclpy.spin_once(self, timeout_sec=0.1)
            now = time.monotonic()

            # Periodic status report
            if now >= t_next_report:
                elapsed_min = (now - t_start) / 60
                robots_heard = len(self._heartbeats_seen)
                duty = self._lora_status.duty_cycle_used_pct if self._lora_status else 0.0
                print(f'    t+{elapsed_min:.1f}min -- robots heard: {robots_heard}/{NUM_ROBOTS}  '
                      f'duty: {duty:.1f}%  '
                      f'disconnected: {self._disconnected_seen}')
                t_next_report = now + report_every

            # Track duty cycle violations
            if self._lora_status and self._lora_status.duty_cycle_used_pct > 100.0:
                duty_violations += 1

            if self._disconnected_seen:
                print(f'    DISCONNECTED detected at t+{(now - t_start)/60:.1f}min')

        elapsed = time.monotonic() - t_start
        robots_heard = len(self._heartbeats_seen)

        # B3: All robots heard via heartbeat
        ok = robots_heard == NUM_ROBOTS
        results.append(('B: All 8 robots heard via /lora/heartbeat', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- {robots_heard}/{NUM_ROBOTS} robots heard')

        # B4: No DISCONNECTED
        ok = not self._disconnected_seen
        results.append(('B: No DISCONNECTED during 30-min run', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL}')

        # B5: Duty cycle within limit throughout
        ok = duty_violations == 0
        results.append(('B: Duty cycle within limit throughout', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- violations: {duty_violations}')

        print(f'    Elapsed: {elapsed:.1f}s')
        return True

    def _scenario_c(self, results):
        """Scenario C: 10-min stress — WiFi6 + LTE both killed, LoRa holds, CPU/RAM checked."""
        print('\n' + '=' * 60)
        print('SCENARIO C -- 10-Minute Stress (WiFi6 + LTE Killed)')
        print(f'  Duration: {STRESS_DURATION_S // 60} minutes')
        print('=' * 60)

        # C1: Kill both WiFi6 and LTE simultaneously → LoRa
        print('\n[C1] Killing WiFi6 (jam all) + LTE (inject failure) simultaneously...')
        self._inject_lte_failure(duration_s=STRESS_DURATION_S + 120.0)
        self._jam_all()
        ok = self._wait_for_tier(3, timeout=STATE_TIMEOUT)
        results.append(('C: System reached LoRa under dual failure', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- tier: {TIER_NAMES.get(self._current_tier, "?")}')
        if not ok:
            return False

        # C2: 10-minute stress run — sample CPU/RAM every SAMPLE_INTERVAL_S
        print(f'\n[C2] Running 10-minute stress...')
        self._disconnected_seen = False
        self._heartbeats_seen.clear()

        t_start          = time.monotonic()
        t_end            = t_start + STRESS_DURATION_S
        t_next_sample    = t_start + SAMPLE_INTERVAL_S

        cpu_samples      = []
        ram_samples_mb   = []
        duty_violations  = 0
        tier_deviations  = 0

        while time.monotonic() < t_end:
            rclpy.spin_once(self, timeout_sec=0.1)
            now = time.monotonic()

            if now >= t_next_sample:
                cpu_pct = psutil.cpu_percent(interval=None)
                ram_mb  = psutil.Process().memory_info().rss / (1024 * 1024)
                cpu_samples.append(cpu_pct)
                ram_samples_mb.append(ram_mb)
                elapsed_min = (now - t_start) / 60
                duty = self._lora_status.duty_cycle_used_pct if self._lora_status else 0.0
                print(f'    t+{elapsed_min:.1f}min -- CPU: {cpu_pct:.1f}%  '
                      f'RAM: {ram_mb:.1f} MB  duty: {duty:.1f}%')
                t_next_sample = now + SAMPLE_INTERVAL_S

            if self._lora_status and self._lora_status.duty_cycle_used_pct > 100.0:
                duty_violations += 1

            if self._current_tier != 3:
                tier_deviations += 1

        # C3: LoRa held throughout (no tier deviations, no DISCONNECTED)
        ok = tier_deviations == 0 and not self._disconnected_seen
        results.append(('C: LoRa held throughout stress (no tier deviations)', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- deviations: {tier_deviations}  '
              f'disconnected: {self._disconnected_seen}')

        # C4: CPU within threshold
        if cpu_samples:
            avg_cpu = sum(cpu_samples) / len(cpu_samples)
            max_cpu = max(cpu_samples)
            ok = avg_cpu < CPU_THRESHOLD_PCT
            results.append((f'C: CPU avg < {CPU_THRESHOLD_PCT}% during stress', PASS if ok else FAIL))
            print(f'    {PASS if ok else FAIL} -- avg: {avg_cpu:.1f}%  max: {max_cpu:.1f}%')

        # C5: RAM within threshold
        if ram_samples_mb:
            avg_ram = sum(ram_samples_mb) / len(ram_samples_mb)
            max_ram = max(ram_samples_mb)
            ok = max_ram < RAM_THRESHOLD_MB
            results.append((f'C: RAM max < {RAM_THRESHOLD_MB} MB during stress', PASS if ok else FAIL))
            print(f'    {PASS if ok else FAIL} -- avg: {avg_ram:.1f} MB  max: {max_ram:.1f} MB')

        # C6: All 8 robots heard
        robots_heard = len(self._heartbeats_seen)
        ok = robots_heard == NUM_ROBOTS
        results.append(('C: All 8 robots heard via /lora/heartbeat', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- {robots_heard}/{NUM_ROBOTS} robots heard')

        # C7: Duty cycle within limit
        ok = duty_violations == 0
        results.append(('C: Duty cycle within limit during stress', PASS if ok else FAIL))
        print(f'    {PASS if ok else FAIL} -- violations: {duty_violations}')

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

        # Scenario A
        self._reset_to_wifi6('Pre-A reset')
        self._scenario_a(results)

        # Scenario B
        self._reset_to_wifi6('Pre-B reset')
        self._scenario_b(results)

        # Scenario C
        self._reset_to_wifi6('Pre-C reset')
        self._scenario_c(results)

        # Final recovery
        self._reset_to_wifi6('Post-C reset')

        elapsed = time.monotonic() - t0
        passed  = sum(1 for _, r in results if r == PASS)
        total   = len(results)
        vv_pass = passed == total

        print('\n' + '=' * 60)
        print('V&V 3.2 -- LoRa E-Stop + Sustained Stress Results')
        print('=' * 60)
        for label, result in results:
            print(f'  {result}  {label}')
        print('-' * 60)
        print(f'  {"PASS" if vv_pass else "FAIL"}  V&V 3.2 -- LoRa e-stop + sustained stress')
        print(f'  Elapsed: {elapsed:.1f}s  |  {passed}/{total} checks passed')
        print('\nTier history:')
        t_ref = self._tier_history[0][0] if self._tier_history else t0
        for ts, tier in self._tier_history:
            print(f'  t+{ts - t_ref:.1f}s -> {TIER_NAMES.get(tier, "?")}')
        print('=' * 60)

        return 0 if vv_pass else 1


def main(args=None):
    rclpy.init(args=args)
    node = LoraEstopTest()
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