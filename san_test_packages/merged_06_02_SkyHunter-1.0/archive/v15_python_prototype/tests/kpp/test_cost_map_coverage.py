"""KPP cost_map_coverage ≥ 7 m forward (SAN v1.5 §4.5.1).

The local cost map must cover at least 7 m in front of the robot so
the planner has a meaningful 2.5 s lookahead at the 2.78 m/s top speed
(v1.5 DCN-2026-001 D-002: 10 km/h, was v1.3-era 12 km/h / 3.33 m/s).
The production grid is 14 m square with the robot at center → 7 m
forward coverage by construction; this test pins the geometry so a
config change can't silently shrink the planning horizon.
"""
from __future__ import annotations

import numpy as np
import pytest

from core.messages import COST_LETHAL
from mapping.cost_map import CostMap, CostMapConfig

pytestmark = pytest.mark.kpp


def test_grid_size_280x280_at_50mm_resolution():
    cfg = CostMapConfig()
    assert cfg.size_m == 14.0
    assert cfg.resolution_m == 0.05
    assert cfg.grid_cells == 280


def test_forward_coverage_geometric_invariant():
    cfg = CostMapConfig()
    cm = CostMap(cfg)
    assert cm.forward_coverage_m == pytest.approx(7.0, abs=1e-6)


def test_lethal_cell_at_6_99m_forward_registers():
    """An obstacle 6.99 m in front must land inside the grid (with
    margin) — covers the spec's 7 m forward minimum.
    """
    cfg = CostMapConfig()
    cm = CostMap(cfg)
    # Put 50 returns at x = 6.99 m, z = 0.30 m (lethal height).
    pts = np.zeros((50, 3), dtype=np.float32)
    pts[:, 0] = 6.99
    pts[:, 1] = 0.0
    pts[:, 2] = 0.30
    grid, _ = cm.compose(pts)
    # At least one lethal cell must appear at the far edge.
    rows, cols = np.where(grid == COST_LETHAL)
    assert rows.size >= 1, "no lethal cell registered at 6.99 m forward"
    # Verify the cell is near the top row (forward direction in our
    # coordinate convention: row decreases as x increases).
    min_row = int(rows.min())
    assert min_row < 5, (
        f"lethal cell at 6.99 m landed at row {min_row}; expected near "
        f"row 0 (forward edge of grid)")


def test_lethal_cell_at_8m_forward_clipped():
    """An obstacle 8 m forward must NOT appear (outside the grid) —
    confirms the grid is sized to exactly 7 m forward, not 8 m+.
    """
    cfg = CostMapConfig()
    cm = CostMap(cfg)
    pts = np.zeros((50, 3), dtype=np.float32)
    pts[:, 0] = 8.0
    pts[:, 2] = 0.30
    grid, _ = cm.compose(pts)
    assert (grid != COST_LETHAL).all(), (
        "obstacle at 8 m forward should not appear in a 7 m-coverage grid")
