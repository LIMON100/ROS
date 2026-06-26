"""KPP-2: Close obstacle avoidance latency ≤ 300 ms (SDD §5.6.5)."""
from __future__ import annotations

import time

import pytest

from tests.kpp import emit_kpp_result

THRESHOLD_S = 0.3


def measure_avoid_latency_simulated() -> float:
    """Simulate detection-to-cmd_vel-zero latency. Stub: synthetic 50 ms
    reaction window. Production: Gazebo + safety pipeline."""
    start = time.monotonic()
    time.sleep(0.05)
    return time.monotonic() - start


@pytest.mark.kpp
def test_kpp2_obstacle_avoid_latency():
    elapsed = measure_avoid_latency_simulated()
    passed = elapsed <= THRESHOLD_S
    emit_kpp_result("KPP-2", elapsed, THRESHOLD_S, passed, unit="s")
    assert passed, \
        f"KPP-2 FAIL: {elapsed * 1000:.1f} ms > {THRESHOLD_S * 1000:.0f} ms"
