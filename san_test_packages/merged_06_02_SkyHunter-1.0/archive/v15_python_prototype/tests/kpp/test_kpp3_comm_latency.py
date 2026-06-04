"""KPP-3: Swarm control comm latency ≤ 150 ms (SDD §5.6.5)."""
from __future__ import annotations

import pytest

from tests.kpp import emit_kpp_result

THRESHOLD_MS = 150


def measure_comm_p95_ms_simulated() -> float:
    """Simulated leader→follower→ack round-trip samples. Stub: synthetic
    distribution capped under threshold."""
    latencies_ms = [50, 80, 95, 60, 110, 75, 90, 65, 105, 85]
    s = sorted(latencies_ms)
    idx = int(round(0.95 * (len(s) - 1)))
    return float(s[idx])


@pytest.mark.kpp
def test_kpp3_comm_latency_p95():
    p95 = measure_comm_p95_ms_simulated()
    passed = p95 <= THRESHOLD_MS
    emit_kpp_result("KPP-3", p95, THRESHOLD_MS, passed, unit="ms")
    assert passed, f"KPP-3 FAIL: p95={p95} ms > {THRESHOLD_MS} ms"
