"""KPP cost_map_latency_p99 ≤ 5 s (SAN v1.3 §6.4).

Measures end-to-end producer latency through the 4-layer compose() at
the v1.3 production grid size (14 × 14 m, 50 mm cells → 280 × 280
master grid). Spec target is 5 s with the real ROS2 stack on RK3588;
we set a much tighter 1 s envelope here because pure-Python compute on
the CI runner is what's being measured, and a 5× margin spots any
algorithmic regression early.
"""
from __future__ import annotations

import time

import numpy as np
import pytest

from mapping.cost_map import CostMap, CostMapConfig

pytestmark = pytest.mark.kpp


def _make_scan(n_points: int = 10_000,
               seed: int = 0xC057CA9) -> np.ndarray:
    """Synthetic LiDAR scan in front of the robot (X ∈ [0, 7] m,
    Y ∈ [-3, 3] m, Z ∈ [0, 0.5] m). A handful of returns push above
    the lethal height so the obstacle + inflation layers do real work.
    """
    rng = np.random.default_rng(seed)
    n = n_points
    pts = np.empty((n, 3), dtype=np.float32)
    pts[:, 0] = rng.uniform(0.5, 7.0, size=n)
    pts[:, 1] = rng.uniform(-3.0, 3.0, size=n)
    pts[:, 2] = rng.uniform(0.0, 0.50, size=n)
    # Drop a 30 cm pillar in the middle to give the compositor real
    # lethal cells to inflate around.
    pillar_idx = rng.choice(n, size=200, replace=False)
    pts[pillar_idx, 0] = 3.0
    pts[pillar_idx, 1] = 0.0
    pts[pillar_idx, 2] = 0.30
    return pts


def test_cost_map_latency_p99_within_5s():
    cfg = CostMapConfig()        # production-size (14 m, 50 mm cells)
    cm = CostMap(cfg)
    n_iter = 25
    latencies_s = []
    for i in range(n_iter):
        scan = _make_scan(n_points=10_000, seed=i)
        t0 = time.monotonic()
        _grid, producer_latency = cm.compose(scan, t_input_mono=t0)
        latencies_s.append(producer_latency)

    # P99 — use a high percentile since n_iter is small. Caller's spec
    # is p99 ≤ 5 s; we additionally assert a tighter 1 s envelope to
    # catch algorithmic regressions on the CI runner.
    p99 = float(np.percentile(latencies_s, 99))
    p50 = float(np.percentile(latencies_s, 50))
    assert p99 <= 5.0, f"p99 = {p99:.3f} s > KPP threshold 5 s"
    assert p99 <= 1.0, (
        f"p99 = {p99:.3f} s exceeds the local CI margin (1 s, 5× the "
        f"spec target). Suggests an algorithmic regression. p50 = {p50:.3f} s")


def test_cost_map_publish_rate_1hz_pm_10pct():
    """1 Hz publish budget: compose() must complete fast enough that
    a 1 Hz pipeline (1.0 s period) has at least 10% headroom.
    """
    cfg = CostMapConfig()
    cm = CostMap(cfg)
    scan = _make_scan(n_points=10_000)
    # Three warm-up calls (Python JIT-y caches, numpy lazy allocation).
    for _ in range(3):
        cm.compose(scan)
    # Measure the steady-state cost of one compose().
    t0 = time.monotonic()
    cm.compose(scan)
    dt = time.monotonic() - t0
    # 90% of the 1 s budget = 0.9 s. Compose must beat that.
    assert dt < 0.9, (
        f"compose() took {dt*1000:.1f} ms — leaves < 10% of a 1 Hz "
        f"period free for queue + serialize + publish overhead.")
