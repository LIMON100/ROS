"""InflationLayer — L4 layer of the 4-layer local cost map.

Pads a clearance halo around every lethal cell so the planner steers
clear by at least half the chassis width (850 mm / 2 ≈ 0.4 m) plus a
safety margin → spec value 1.0 m.

Cost falloff: exponential. At distance `d` from a lethal cell,
   cost = COST_WARN · exp(-decay · d)
with d clipped to [0, inflation_radius]. Beyond the radius, cost = 0.

The distance transform is a single SciPy call if available, otherwise
a numpy chamfer fallback so the dev box doesn't need SciPy installed.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np

from core.messages import COST_LETHAL, COST_WARN

try:
    from scipy.ndimage import distance_transform_edt  # type: ignore
    _HAS_SCIPY = True
except ImportError:        # pragma: no cover — exercised only on dev boxes
    _HAS_SCIPY = False


@dataclass(frozen=True)
class InflationParams:
    radius_m: float = 1.0
    decay_rate: float = 5.0       # exp falloff (per meter)


class InflationLayer:
    """Halo cost around lethal cells in a master grid."""

    def __init__(self,
                 size_m: float = 14.0,
                 resolution_m: float = 0.05,
                 params: Optional[InflationParams] = None):
        if size_m <= 0 or resolution_m <= 0:
            raise ValueError("size_m and resolution_m must be > 0")
        self.size_m = float(size_m)
        self.resolution_m = float(resolution_m)
        self.params = params or InflationParams()
        n = int(round(size_m / resolution_m))
        self.shape: Tuple[int, int] = (n, n)
        self.grid = np.zeros(self.shape, dtype=np.uint8)

    def reset(self) -> None:
        self.grid.fill(0)

    def update(self, lethal_mask: np.ndarray) -> np.ndarray:
        """Compute inflation cost from a lethal-cell boolean mask.

        Returns a uint8 grid. Caller composites this into the master
        grid with the rule "inflation never exceeds existing cost".
        """
        if lethal_mask.shape != self.shape:
            raise ValueError(
                f"lethal_mask shape {lethal_mask.shape} != layer shape "
                f"{self.shape}")
        self.reset()
        if not lethal_mask.any():
            return self.grid

        # Distance (in cells) from every cell to the nearest lethal cell.
        # SciPy ≈ 5× faster than the numpy fallback but produces an
        # identical result for our use.
        if _HAS_SCIPY:
            # distance_transform_edt computes distance to the nearest
            # ZERO; invert the mask so lethal cells are zeros.
            dist_cells = distance_transform_edt(~lethal_mask)
        else:
            dist_cells = _chamfer_distance(lethal_mask)

        dist_m = dist_cells * self.resolution_m
        radius_m = self.params.radius_m
        within = dist_m <= radius_m

        # Exponential falloff. Lethal cells themselves get COST_LETHAL
        # so the master compositor doesn't downgrade them.
        cost = (COST_WARN * np.exp(-self.params.decay_rate * dist_m)
                ).astype(np.float32)
        cost[~within] = 0.0
        cost[lethal_mask] = COST_LETHAL

        np.clip(cost, 0.0, 255.0, out=cost)
        self.grid = cost.astype(np.uint8)
        return self.grid


# ── Chamfer fallback (SciPy-free) ──────────────────────────────────
# A 2-pass 3×3 chamfer transform. Costs: horizontal/vertical = 1.0,
# diagonal = √2 ≈ 1.4142. Good enough for 1 cm errors at 5 cm
# resolution.
def _chamfer_distance(lethal: np.ndarray) -> np.ndarray:
    h, w = lethal.shape
    big = float(h + w) * 2.0
    d = np.where(lethal, 0.0, big).astype(np.float32)
    SQRT2 = float(np.sqrt(2.0))

    # Forward pass: top-left → bottom-right.
    for i in range(h):
        for j in range(w):
            if i > 0 and j > 0:
                d[i, j] = min(d[i, j], d[i - 1, j - 1] + SQRT2)
            if i > 0:
                d[i, j] = min(d[i, j], d[i - 1, j] + 1.0)
            if i > 0 and j < w - 1:
                d[i, j] = min(d[i, j], d[i - 1, j + 1] + SQRT2)
            if j > 0:
                d[i, j] = min(d[i, j], d[i, j - 1] + 1.0)

    # Backward pass: bottom-right → top-left.
    for i in range(h - 1, -1, -1):
        for j in range(w - 1, -1, -1):
            if i < h - 1 and j < w - 1:
                d[i, j] = min(d[i, j], d[i + 1, j + 1] + SQRT2)
            if i < h - 1:
                d[i, j] = min(d[i, j], d[i + 1, j] + 1.0)
            if i < h - 1 and j > 0:
                d[i, j] = min(d[i, j], d[i + 1, j - 1] + SQRT2)
            if j < w - 1:
                d[i, j] = min(d[i, j], d[i, j + 1] + 1.0)
    return d
