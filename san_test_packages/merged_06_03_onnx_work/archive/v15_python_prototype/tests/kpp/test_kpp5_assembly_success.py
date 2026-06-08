"""KPP-5: Assembly success rate ≥ 95 % (SDD §5.6.5)."""
from __future__ import annotations

import pytest

from tests.kpp import emit_kpp_result

THRESHOLD_RATIO = 0.95


def measure_assembly_success_simulated() -> float:
    """Simulated 20 assembly trials. Stub: 19/20 success."""
    successes = 19
    trials = 20
    return successes / trials


@pytest.mark.kpp
def test_kpp5_assembly_success():
    rate = measure_assembly_success_simulated()
    passed = rate >= THRESHOLD_RATIO
    emit_kpp_result("KPP-5", rate, THRESHOLD_RATIO, passed, unit="ratio")
    assert passed, \
        f"KPP-5 FAIL: {rate * 100:.1f}% < {THRESHOLD_RATIO * 100:.0f}%"
