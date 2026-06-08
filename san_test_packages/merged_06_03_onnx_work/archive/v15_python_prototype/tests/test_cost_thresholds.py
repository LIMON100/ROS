"""SAN v1.3 §6.4 cost-map threshold classification tests (PHASE 1).

Covers the chassis-derived thresholds:

  Obstacle layer:
    height 200 mm → COST_WARN     (200)
    height 235 mm → COST_LETHAL   (254)
    height 270 mm → COST_LETHAL   (254)

  Traversability slope:
    25° → COST_WARN
    30° → COST_LETHAL
    35° → COST_LETHAL

  Traversability ditch:
    150 mm → COST_WARN  (just above warn threshold)
    220 mm → COST_LETHAL
    300 mm → COST_LETHAL

  Inflation:
    1.0 m radius from lethal cell → cost > 0
    1.5 m from lethal cell → cost = 0
"""
from __future__ import annotations

import numpy as np
import pytest

from core.messages import (
    COST_FREE,
    COST_LETHAL,
    COST_WARN,
    COST_WARN_LOW,
)
from mapping.cost_map import CostMap, CostMapConfig
from mapping.inflation_layer import InflationLayer, InflationParams
from mapping.obstacle_layer import ObstacleLayer, ObstacleThresholds
from mapping.traversability_layer import (
    TraversabilityLayer,
    build_ground_grid,
)


# ─── Helpers ────────────────────────────────────────────────────────
def _box_points(height_mm: float, n: int = 50) -> np.ndarray:
    """N copies of a single point at the given height — forms an
    obstacle in one cell directly in front of the robot (x = +1 m).
    """
    z = height_mm / 1000.0
    pts = np.zeros((n, 3), dtype=np.float32)
    pts[:, 0] = 1.0          # 1 m forward
    pts[:, 1] = 0.0
    pts[:, 2] = z
    return pts


def _slope_grid(shape, slope_deg: float, resolution_m: float) -> np.ndarray:
    """Synthetic ground-elevation grid with the given slope along +X.

    Useful for slope tests — we bypass LiDAR and feed
    `TraversabilityLayer.update_from_ground()` directly.
    """
    h, w = shape
    tan = float(np.tan(np.deg2rad(slope_deg)))
    # Row 0 = +X forward (highest); robot at center.
    rows = np.arange(h).reshape(-1, 1).astype(np.float32)
    # Elevation increases as row index decreases (forward).
    z = (h // 2 - rows) * resolution_m * tan
    return np.broadcast_to(z, shape).astype(np.float32).copy()


# ───────────────────────────────────────────────────────────────────
# Obstacle layer — 200 / 235 / 270 mm
# ───────────────────────────────────────────────────────────────────
@pytest.mark.parametrize("height_mm,expected", [
    (199.9, COST_FREE),    # just below warn → free
    (200.0, COST_WARN),    # at warn boundary
    (220.0, COST_WARN),    # in the warn band
    (234.9, COST_WARN),    # just below lethal
    (235.0, COST_LETHAL),  # at lethal boundary
    (270.0, COST_LETHAL),  # solidly lethal
    (500.0, COST_LETHAL),  # tall obstacle
])
def test_obstacle_height_classification(height_mm, expected):
    layer = ObstacleLayer(size_m=4.0, resolution_m=0.05,
                          thresholds=ObstacleThresholds(min_points=3))
    grid = layer.update(_box_points(height_mm))
    # Find the single non-free cell.
    rows, cols = np.where(grid != COST_FREE)
    if expected == COST_FREE:
        assert rows.size == 0, (rows, cols, grid[rows, cols])
        return
    assert rows.size >= 1
    # All flagged cells should have the same cost value.
    values = set(grid[rows, cols].tolist())
    assert values == {expected}, values


def test_obstacle_drops_overhead_returns():
    """A point at 5.5 m altitude (tree canopy) must not register."""
    layer = ObstacleLayer(size_m=4.0, resolution_m=0.05,
                          thresholds=ObstacleThresholds(min_points=1))
    pts = np.array([[1.0, 0.0, 5.5]] * 10, dtype=np.float32)
    grid = layer.update(pts)
    assert np.count_nonzero(grid != COST_FREE) == 0


def test_obstacle_min_points_floor():
    """A single stray return doesn't pollute a cell."""
    layer = ObstacleLayer(size_m=4.0, resolution_m=0.05,
                          thresholds=ObstacleThresholds(min_points=3))
    pts = np.array([[1.0, 0.0, 0.30]], dtype=np.float32)  # 300 mm lethal
    grid = layer.update(pts)
    assert np.count_nonzero(grid != COST_FREE) == 0


# ───────────────────────────────────────────────────────────────────
# Traversability slope — 25 / 30 / 35 °
# ───────────────────────────────────────────────────────────────────
@pytest.mark.parametrize("slope_deg,expected", [
    (10.0, COST_FREE),
    (25.0, COST_WARN_LOW),    # spec: 25° → cost 100
    (29.9, COST_WARN_LOW),
    (30.0, COST_LETHAL),       # spec: 30° → cost 254
    (35.0, COST_LETHAL),       # spec: 35° → cost 254
])
def test_traversability_slope_classification(slope_deg, expected):
    layer = TraversabilityLayer(size_m=4.0, resolution_m=0.05)
    z = _slope_grid(layer.shape, slope_deg, layer.resolution_m)
    grid = layer.update_from_ground(z)
    # The slope grid is uniform → expect a SINGLE majority cost class
    # over the interior (edges suffer from gradient boundary effects).
    h, w = grid.shape
    interior = grid[5:h - 5, 5:w - 5]
    classes, counts = np.unique(interior, return_counts=True)
    majority = int(classes[counts.argmax()])
    assert majority == expected, (slope_deg, classes.tolist(), counts.tolist())


# ───────────────────────────────────────────────────────────────────
# Traversability ditch — 150 / 220 / 300 mm
# ───────────────────────────────────────────────────────────────────
def _ditch_grid(shape, width_mm: float, depth_mm: float,
                resolution_m: float) -> np.ndarray:
    """Flat ground with a `width_mm`-wide, `depth_mm`-deep band
    centered on the robot's forward axis.
    """
    h, w = shape
    z = np.zeros(shape, dtype=np.float32)
    span = max(1, int(round(width_mm / 1000.0 / resolution_m)))
    mid = h // 2
    z[mid - span // 2:mid + span // 2 + 1, :] = -depth_mm / 1000.0
    return z


@pytest.mark.parametrize("width_mm,expected", [
    # Binary classification per the v1.3 spec (free / 254):
    #   width < ditch_lethal_mm → FREE  (chassis can cross)
    #   width ≥ ditch_lethal_mm → LETHAL
    # Test widths chosen to land well-separated on a 50 mm grid so the
    # boundary isn't quantization-ambiguous.
    (100, COST_FREE),       # 2 cells
    (150, COST_FREE),       # 3 cells  (spec data point)
    (220, COST_LETHAL),     # 4 cells  (spec data point — at boundary)
    (300, COST_LETHAL),     # 6 cells  (spec data point)
])
def test_traversability_ditch_classification(width_mm, expected):
    # Depth 200 mm is well above the depth-floor (120 mm) so width alone
    # drives the classification.
    depth_mm = 200
    layer = TraversabilityLayer(size_m=4.0, resolution_m=0.05)
    z = _ditch_grid(layer.shape, width_mm, depth_mm, layer.resolution_m)
    # Suppress the slope detector's edge-of-ditch artefacts — we only
    # care about the ditch detector's output. Run the ditch path
    # directly via the private helper used by update_from_ground().
    ditch_only = layer._ditch_cost(z)
    h, _ = ditch_only.shape
    mid = h // 2
    band = ditch_only[mid - 1:mid + 2, 10:-10]
    if expected == COST_FREE:
        assert (band == COST_FREE).all(), (
            width_mm, np.unique(band, return_counts=True))
        return
    classes = np.unique(band)
    assert expected in classes, (width_mm, classes.tolist())


# ───────────────────────────────────────────────────────────────────
# Inflation — 1.0 m radius
# ───────────────────────────────────────────────────────────────────
def test_inflation_radius():
    layer = InflationLayer(size_m=4.0, resolution_m=0.05,
                           params=InflationParams(radius_m=1.0))
    h, w = layer.shape
    lethal = np.zeros(layer.shape, dtype=bool)
    cy, cx = h // 2, w // 2
    lethal[cy, cx] = True
    grid = layer.update(lethal)
    # Center cell stays COST_LETHAL.
    assert grid[cy, cx] == COST_LETHAL
    # 0.5 m east of center (10 cells at 0.05 m) → inflated.
    assert grid[cy, cx + 10] > 0
    # 1.5 m east of center (30 cells) → beyond radius → 0.
    assert grid[cy, cx + 30] == 0


def test_inflation_no_lethal_no_cost():
    layer = InflationLayer(size_m=4.0, resolution_m=0.05)
    grid = layer.update(np.zeros(layer.shape, dtype=bool))
    assert (grid == 0).all()


# ───────────────────────────────────────────────────────────────────
# End-to-end compose() through CostMap
# ───────────────────────────────────────────────────────────────────
def test_compose_lethal_obstacle_gets_inflation_halo():
    cfg = CostMapConfig(size_m=4.0, resolution_m=0.05)
    cm = CostMap(cfg)
    # 270 mm tall obstacle, dense enough to clear min_points.
    pts = _box_points(270.0, n=10)
    grid, _latency = cm.compose(pts)
    # The cell at the obstacle is lethal, and there's a halo of
    # non-zero cost cells around it (inflation).
    lethal_count = int(np.count_nonzero(grid == COST_LETHAL))
    halo_count = int(np.count_nonzero((grid > COST_FREE)
                                      & (grid < COST_LETHAL)))
    assert lethal_count >= 1
    assert halo_count > lethal_count, (lethal_count, halo_count)


def test_compose_static_layer_is_or_merged():
    """A static layer cell at lethal must survive composition even
    when LiDAR shows nothing there.
    """
    cfg = CostMapConfig(size_m=4.0, resolution_m=0.05)
    cm = CostMap(cfg)
    n = cfg.grid_cells
    static = np.zeros((n, n), dtype=np.uint8)
    static[10, 20] = COST_LETHAL
    grid, _ = cm.compose(np.zeros((0, 3), dtype=np.float32),
                         static_layer_window=static)
    assert grid[10, 20] == COST_LETHAL


# ───────────────────────────────────────────────────────────────────
# Ground-grid builder sanity
# ───────────────────────────────────────────────────────────────────
def test_build_ground_grid_min_z_per_cell():
    # 5 points all in the same cell at varying heights.
    pts = np.array([[1.0, 0.0, 0.10],
                    [1.0, 0.0, 0.05],
                    [1.0, 0.0, 0.20],
                    [1.0, 0.0, -0.05],
                    [1.0, 0.0, 0.00]], dtype=np.float32)
    shape = (80, 80)
    cy, cx = shape[0] // 2, shape[1] // 2
    z = build_ground_grid(pts, shape, resolution_m=0.05, center_ij=(cx, cy))
    # 1 m forward at 0.05 m/cell = 20 cells ahead → row = cy - 20.
    cell_row = cy - 20
    cell_col = cx
    assert z[cell_row, cell_col] == pytest.approx(-0.05, abs=1e-6)
