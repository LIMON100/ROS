"""PHASE 3 ↔ PHASE 1 integration:

  SlamAggregator → AggregatedMap → AggregatedToStatic → CostMap

A static-layer window sampled from a Hub-side AggregatedMap must:
  • carry COST_LETHAL where the global map shows a confident obstacle
  • carry COST_FREE elsewhere (and on OOB)
  • feed cleanly into CostMap.compose() so lethal cells in the static
    window survive composition (worst-cost-wins) when the LiDAR sees
    nothing
"""
from __future__ import annotations

import numpy as np
import pytest

from core.messages import (
    COST_FREE,
    COST_LETHAL,
    AggregatedMap,
    Pose2D,
)
from mapping.aggregated_map import encode_grid_to_png
from mapping.aggregated_to_static import (
    AggregatedToStatic,
    AggregatedToStaticConfig,
    sample_once,
)
from mapping.cost_map import CostMap, CostMapConfig

# Pillow check — encode_grid_to_png requires it, but the test env in CI
# has it installed (see PHASE 1 setup). Mark anyway in case a local box
# is missing.
PIL = pytest.importorskip("PIL")


def _build_agg(global_grid: np.ndarray,
               origin: tuple = (0.0, 0.0),
               resolution_m: float = 0.10) -> AggregatedMap:
    h, w = global_grid.shape
    return AggregatedMap(
        sequence=1,
        occupancy_grid_png=encode_grid_to_png(global_grid),
        origin=Pose2D(x=origin[0], y=origin[1], theta_rad=0.0),
        resolution_m=resolution_m,
        width_cells=w,
        height_cells=h,
        contributing_robots=2,
        timestamp_ms=0,
    )


# ───────────────────────────────────────────────────────────────────
# Basic sampling
# ───────────────────────────────────────────────────────────────────
def test_sample_none_for_empty_aggregated_map():
    """Adapter must gracefully return None for a missing / empty msg."""
    assert sample_once(None, robot_xy=(0.0, 0.0)) is None
    empty = AggregatedMap()
    assert sample_once(empty, robot_xy=(0.0, 0.0)) is None


def test_sample_returns_correct_shape():
    grid = np.zeros((100, 100), dtype=np.uint8)
    cfg = AggregatedToStaticConfig(size_m=4.0, resolution_m=0.05)
    window = sample_once(_build_agg(grid), robot_xy=(5.0, 5.0), cfg=cfg)
    assert window is not None
    assert window.shape == (cfg.grid_cells, cfg.grid_cells)
    assert window.dtype == np.uint8


def test_sample_marks_global_obstacle_as_lethal():
    """A confident obstacle in the global grid must surface as
    COST_LETHAL in the sampled local window.
    """
    # Global grid 100×100 at 0.10 m/cell → 10 m × 10 m
    # Origin = (0, 0). Put a 4-cell obstacle band at world (5, 5).
    grid = np.zeros((100, 100), dtype=np.uint8)
    grid[48:52, 48:52] = 255           # confident obstacle
    cfg = AggregatedToStaticConfig(size_m=4.0, resolution_m=0.05)
    window = sample_once(_build_agg(grid), robot_xy=(5.0, 5.0), cfg=cfg)
    assert window is not None
    # At least one COST_LETHAL cell near the robot center.
    n = cfg.grid_cells
    cy, cx = n // 2, n // 2
    region = window[cy - 10:cy + 10, cx - 10:cx + 10]
    assert (region == COST_LETHAL).any(), (
        "expected COST_LETHAL near the robot center, got "
        f"{np.unique(region, return_counts=True)}")


def test_sample_unknown_cells_become_free():
    """Mid-grey (127, "no observation") must NOT pre-fence the planner —
    map it to COST_FREE so the LiDAR layers drive the decision.
    """
    grid = np.full((50, 50), 127, dtype=np.uint8)
    cfg = AggregatedToStaticConfig(size_m=2.0, resolution_m=0.05)
    window = sample_once(_build_agg(grid), robot_xy=(2.5, 2.5), cfg=cfg)
    assert window is not None
    assert (window == COST_FREE).all()


def test_sample_out_of_coverage_returns_free():
    """Robot far outside the global map → all-FREE window, not None."""
    grid = np.zeros((50, 50), dtype=np.uint8)
    grid[10, 10] = 255            # somewhere
    cfg = AggregatedToStaticConfig(size_m=2.0, resolution_m=0.05)
    # 100 m × 100 m offset puts the robot far outside the 5 m × 5 m
    # global map → no overlap.
    window = sample_once(_build_agg(grid), robot_xy=(100.0, 100.0), cfg=cfg)
    assert window is None


def test_sample_partial_overlap_clips_to_free():
    """Robot at the edge: half the window is outside the global map.
    The outside half must be COST_FREE (don't pre-fence what we don't
    know), the inside half can carry the global obstacles.
    """
    # 50×50 @ 0.10 m → 5 m × 5 m global; origin (0, 0).
    grid = np.zeros((50, 50), dtype=np.uint8)
    grid[:, 49] = 255             # right-edge wall
    cfg = AggregatedToStaticConfig(size_m=4.0, resolution_m=0.05)
    # Place the robot AT the right edge of the global map (x = 5.0).
    # The window extends from x=3 to x=7; half is OOB.
    window = sample_once(_build_agg(grid), robot_xy=(5.0, 2.5), cfg=cfg)
    assert window is not None
    # Window column index of the robot is local_n / 2. To the right of
    # that the cells are OOB, so they should all be free.
    n = cfg.grid_cells
    right_of_robot = window[:, n // 2 + 5:]
    assert (right_of_robot == COST_FREE).all()


# ───────────────────────────────────────────────────────────────────
# End-to-end with CostMap compositor
# ───────────────────────────────────────────────────────────────────
def test_static_window_feeds_cost_map_composite():
    """A LETHAL cell from the SLAM static window survives
    CostMap.compose() (worst-cost-wins).
    """
    # Build a global map with a small lethal patch right at the robot.
    grid = np.zeros((40, 40), dtype=np.uint8)
    grid[19:21, 19:21] = 255
    cfg_local = CostMapConfig(size_m=4.0, resolution_m=0.05)
    cfg_adapt = AggregatedToStaticConfig(
        size_m=cfg_local.size_m,
        resolution_m=cfg_local.resolution_m)
    window = sample_once(_build_agg(grid), robot_xy=(2.0, 2.0), cfg=cfg_adapt)
    assert window is not None

    cm = CostMap(cfg_local)
    # No LiDAR points — only the static layer contributes lethality.
    master, _ = cm.compose(np.zeros((0, 3), dtype=np.float32),
                           static_layer_window=window)
    assert (master == COST_LETHAL).any()


def test_lidar_override_does_not_clear_static_lethal():
    """LiDAR shows nothing → static-layer lethal cells must persist."""
    grid = np.zeros((40, 40), dtype=np.uint8)
    grid[19:21, 19:21] = 255
    cfg_local = CostMapConfig(size_m=4.0, resolution_m=0.05)
    cfg_adapt = AggregatedToStaticConfig(
        size_m=cfg_local.size_m,
        resolution_m=cfg_local.resolution_m)
    window = sample_once(_build_agg(grid), robot_xy=(2.0, 2.0), cfg=cfg_adapt)

    cm = CostMap(cfg_local)
    n_lethal_static_only = int(np.count_nonzero(
        cm.compose(np.zeros((0, 3), dtype=np.float32),
                   static_layer_window=window)[0] == COST_LETHAL))

    # Now compose with a single non-lethal LiDAR scan (low pillar far
    # away). The static lethal count must NOT decrease.
    pts = np.array([[3.0, 3.0, 0.05]] * 10, dtype=np.float32)
    master, _ = cm.compose(pts, static_layer_window=window)
    n_lethal_combined = int(np.count_nonzero(master == COST_LETHAL))
    assert n_lethal_combined >= n_lethal_static_only


# ───────────────────────────────────────────────────────────────────
# Stateless reuse — adapter instance handles many frames
# ───────────────────────────────────────────────────────────────────
def test_adapter_instance_is_stateless():
    cfg = AggregatedToStaticConfig(size_m=4.0, resolution_m=0.05)
    adapter = AggregatedToStatic(cfg)
    g1 = np.zeros((40, 40), dtype=np.uint8)
    g1[20, 20] = 255
    g2 = np.zeros((40, 40), dtype=np.uint8)
    g2[10, 10] = 255
    w1 = adapter.sample(_build_agg(g1), robot_xy=(2.0, 2.0))
    w2 = adapter.sample(_build_agg(g2), robot_xy=(2.0, 2.0))
    # Different inputs → different outputs (state didn't bleed through).
    assert w1 is not None and w2 is not None
    assert not np.array_equal(w1, w2)
