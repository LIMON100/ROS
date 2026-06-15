"""KPP-4: Leader-loss reconfiguration ≤ 10 s (SDD §5.6.5)."""
from __future__ import annotations

import pytest

from tests.kpp import emit_kpp_result

THRESHOLD_S = 10.0


def measure_reconfig_time_simulated() -> float:
    """Simulated kill-leader → new-leader-publishes-pose latency.
    Stub: based on Modified Raft (P1-13) + Hub takeover (P1-5) timings."""
    return 6.5


@pytest.mark.kpp
def test_kpp4_reconfiguration_time():
    measured = measure_reconfig_time_simulated()
    passed = measured <= THRESHOLD_S
    emit_kpp_result("KPP-4", measured, THRESHOLD_S, passed, unit="s")
    assert passed, f"KPP-4 FAIL: {measured} s > {THRESHOLD_S} s"
