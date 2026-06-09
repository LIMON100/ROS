#!/usr/bin/env python3
# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
test_all.py — SkyHunter Comm Stack V&V Test Runner

Runs each test as an isolated subprocess — clean rclpy context per test,
no state leakage, crashed tests cannot affect subsequent ones.

Tests:
  V&V 2.1 — Delay injection 5-20ms         (test_delay.py)
  V&V 2.2 — Packet loss model              (test_packet_loss.py)
  V&V 2.3 — Jammer service                 (test_jammer.py)
  V&V 2.5 — WiFi6 → LTE failover          (test_failover.py)
  V&V 2.6 — 8-robot bridge relay           (test_bridge_relay.py)
  V&V 3.0 — Full 3-tier chain + recovery   (test_full_chain.py)
  V&V L11 — Bridge stress test CPU/RAM     (test_bridge_stress.py)
  V&V 3.1 — Full 3-tier failover            (test_3tier_failover.py)
  V&V 3.2 — LoRa e-stop + sustained stress  (test_lora_estop.py)
  V&V L16 — Comm latency 1000+ samples      (test_latency.py)

Prerequisites:
    export ROS_DOMAIN_ID=42
    ros2 launch skyhunter_bringup sim.launch.py num_robots:=8
    ros2 launch skyhunter_bringup networking.launch.py

Usage:
    python3 test_all.py
    python3 test_all.py --tests 2.1 2.2 3.1
    python3 test_all.py --verbose
"""

import argparse
import os
import subprocess
import sys
import time

PASS = '✅ PASS'
FAIL = '❌ FAIL'

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

TESTS = [
    {
        'id':      '2.1',
        'name':    'Delay Injection',
        'script':  'test_delay.py',
        'args':    ['--samples', '100', '--timeout', '60'],
        'timeout': 90,
    },
    {
        'id':      '2.2',
        'name':    'Packet Loss Model',
        'script':  'test_packet_loss.py',
        'args':    ['--samples', '100', '--timeout', '60'],
        'timeout': 90,
    },
    {
        'id':      '2.3',
        'name':    'Jammer Service',
        'script':  'test_jammer.py',
        'args':    [
            '--robot-a', '1', '--robot-b', '2',
            '--duration', '30', '--samples', '100', '--timeout', '120',
        ],
        'timeout': 150,
    },
    {
        'id':      '2.5',
        'name':    'WiFi6 → LTE Failover',
        'script':  'test_failover.py',
        'args':    [],
        'timeout': 120,
    },
    {
        'id':      '2.6',
        'name':    '8-Robot Bridge Relay',
        'script':  'test_bridge_relay.py',
        'args':    [],
        'timeout': 120,
    },
    {
        'id':      '3.0',
        'name':    'Full 3-Tier Chain + Recovery',
        'script':  'test_full_chain.py',
        'args':    [],
        'timeout': 180,
    },
    {
        'id':      'L11',
        'name':    'Bridge Stress Test CPU/RAM',
        'script':  'test_bridge_stress.py',
        'args':    [],
        'timeout': 120,
    },
    {
        'id':      '3.1',
        'name':    'Full 3-Tier Failover',
        'script':  'test_3tier_failover.py',
        'args':    [],
        'timeout': 600,   # ~114s typical + margin
    },
    {
        'id':      '3.2',
        'name':    'LoRa E-Stop + Sustained Stress',
        'script':  'test_lora_estop.py',
        'args':    [],
        'timeout': 3000,  # ~45 min (30-min B + 10-min C + margin)
    },
    {
        'id':      'L16',
        'name':    'Comm Latency (1000+ samples)',
        'script':  'test_latency.py',
        'args':    [],
        'timeout': 600,   # WiFi6 60s + LTE 180s + transitions + margin
    },
]


def run_test(test: dict, verbose: bool) -> tuple[bool, str, float]:
    """Run a single test as a subprocess. Returns (passed, output, elapsed)."""
    script = os.path.join(SCRIPT_DIR, test['script'])
    cmd    = [sys.executable, script] + test['args']

    t0 = time.time()
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=test['timeout'],
        )
        output = result.stdout + (result.stderr if result.stderr else '')
        passed = result.returncode == 0
    except subprocess.TimeoutExpired:
        output = f'  ⚠  Timed out after {test["timeout"]}s\n'
        passed = False

    elapsed = time.time() - t0

    if verbose:
        print(output)

    return passed, output, elapsed


def extract_score(output: str) -> str:
    """Extract N/M score line from test output."""
    for line in reversed(output.splitlines()):
        stripped = line.strip()
        if '/' in stripped and ('check' in stripped.lower() or 'passed' in stripped.lower()):
            for part in stripped.split():
                if '/' in part and all(c.isdigit() or c == '/' for c in part):
                    return part
    return ''


def main():
    parser = argparse.ArgumentParser(description='SkyHunter Comm V&V Runner (W3)')
    parser.add_argument('--tests',   type=str, nargs='+',
                        help='Run subset e.g. --tests 2.1 2.2 L11')
    parser.add_argument('--verbose', action='store_true',
                        help='Print full output from each test')
    args = parser.parse_args()

    tests = TESTS
    if args.tests:
        tests = [t for t in TESTS if t['id'] in args.tests]
        if not tests:
            print(f'No matching tests for: {args.tests}')
            print(f'Available: {[t["id"] for t in TESTS]}')
            sys.exit(1)

    print()
    print('=' * 60)
    print('  SkyHunter Comm V&V — Test Runner')
    print('=' * 60)
    print(f'  tests : {", ".join(t["id"] for t in tests)}')
    print('=' * 60)

    summary = []

    for test in tests:
        print(f'\n  ▶  V&V {test["id"]} — {test["name"]}  (timeout {test["timeout"]}s)...')
        passed, output, elapsed = run_test(test, args.verbose)
        score     = extract_score(output)
        score_str = f'  {score}' if score else ''
        sym       = '✅' if passed else '❌'
        print(f'  {sym}  V&V {test["id"]} — {test["name"]}  [{elapsed:.1f}s]{score_str}')
        summary.append((test['id'], test['name'], passed, elapsed, score))

    # ── Consolidated report ───────────────────────────────────────────────────
    passed_count = sum(1 for *_, p, _, _ in summary if p)
    total_count  = len(summary)
    all_pass     = passed_count == total_count
    total_time   = sum(e for *_, e, _ in summary)

    print()
    print('=' * 60)
    print('  Results Summary')
    print('=' * 60)
    for vid, name, passed, elapsed, score in summary:
        sym       = '✅ PASS' if passed else '❌ FAIL'
        score_str = f'  {score}' if score else ''
        print(f'  {sym}  V&V {vid} — {name:<32} [{elapsed:.1f}s]{score_str}')
    print('─' * 60)
    print(f'  {"✅ ALL PASS" if all_pass else "❌ SOME FAILED"}  '
          f'{passed_count}/{total_count} tests passed  '
          f'[{total_time:.1f}s total]')
    print('=' * 60)
    print()

    sys.exit(0 if all_pass else 1)


if __name__ == '__main__':
    main()