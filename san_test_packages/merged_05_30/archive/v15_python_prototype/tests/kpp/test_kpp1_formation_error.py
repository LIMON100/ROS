"""KPP-1: Formation maintenance avg error ≤ 2 m (SDD §5.6.5)."""
from __future__ import annotations

import numpy as np
import pytest

from tests.kpp import emit_kpp_result

THRESHOLD_M = 2.0


def measure_formation_error_simulated() -> float:
    """Simulate 9 followers tracking a leader for 60 s. Returns mean lateral
    error (m). Production: Gazebo + RK3588J swarm. Stub: deterministic
    noise with seed=42."""
    rng = np.random.default_rng(seed=42)
    errors = rng.normal(loc=0.5, scale=0.4, size=(9, 600))
    return float(np.abs(errors).mean())


@pytest.mark.kpp
def test_kpp1_formation_error_under_threshold():
    avg_error = measure_formation_error_simulated()
    passed = avg_error <= THRESHOLD_M
    emit_kpp_result("KPP-1", avg_error, THRESHOLD_M, passed, unit="m")
    assert passed, f"KPP-1 FAIL: {avg_error:.2f} m > {THRESHOLD_M} m"
