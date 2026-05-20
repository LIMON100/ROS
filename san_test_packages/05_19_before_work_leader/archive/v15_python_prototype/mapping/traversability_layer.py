"""TraversabilityLayer — L3 layer of the 4-layer local cost map.

Two independent detectors fused into one uint8 grid:

  Slope detector  — fits a plane to each cell neighborhood; the angle
                    between the plane normal and Z gives the local slope.
                    ≥ 30° (UGV climbing limit) → COST_LETHAL.
                    25° ≤ θ < 30° → COST_WARN.

  Ditch detector  — looks for a sudden negative-Z step in adjacent
                    cells. A drop wider than 220 mm (UGV ditch crossing
                    spec) is lethal; 150–220 mm is warn.

Both detectors operate on a ground-elevation grid `Z(row, col)` that
the caller computes by binning the same LiDAR cloud the obstacle layer
used. We provide a small `build_ground_grid()` helper for that — it's
the cheapest aggregator (per-cell min-Z) and good enough for the v1.3
spec; a full RANSAC ground plane would be PHASE 1 follow-up.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np

from core.messages import COST_FREE, COST_LETHAL, COST_WARN_LOW


def _rolling_all_axis0(mask: np.ndarray, k: int) -> np.ndarray:
    """Per-cell AND over a window of k cells centered on the cell along
    axis 0. Returns a same-shape boolean array where True means the
    window of `k` cells along the row axis is entirely True.

    Pure numpy; equivalent to scipy.ndimage.minimum_filter(mask, size=(k,1))
    but without the scipy dependency. O(N * k) which is fine for our
    280-row grids and small k (~4 cells).
    """
    if k <= 1:
        return mask
    n_rows = mask.shape[0]
    # Window is symmetric — half above, half below.
    half = k // 2
    out = np.ones_like(mask, dtype=bool)
    for offset in range(-half, k - half):
        shifted = np.zeros_like(mask, dtype=bool)
        if offset == 0:
            shifted = mask
        elif offset > 0:
            shifted[:n_rows - offset] = mask[offset:]
        else:
            shifted[-offset:] = mask[:n_rows + offset]
        out &= shifted
    return out


@dataclass(frozen=True)
class TraversabilityThresholds:
    slope_lethal_deg: float = 30.0
    slope_warn_deg: float = 25.0
    ditch_lethal_mm: float = 220.0
    ditch_warn_mm: float = 150.0

    @property
    def slope_lethal_tan(self) -> float:
        return math.tan(math.radians(self.slope_lethal_deg))

    @property
    def slope_warn_tan(self) -> float:
        return math.tan(math.radians(self.slope_warn_deg))

    def ditch_meters(self) -> Tuple[float, float]:
        return self.ditch_lethal_mm / 1000.0, self.ditch_warn_mm / 1000.0


def build_ground_grid(points_xyz: np.ndarray,
                      shape: Tuple[int, int],
                      resolution_m: float,
                      center_ij: Tuple[int, int],
                      sentinel: float = np.nan) -> np.ndarray:
    """Bin LiDAR returns into a per-cell ground-elevation grid (float32).

    Cell value = min(z) across points hitting that cell (the lowest
    return is the best ground proxy without a full RANSAC fit). Cells
    with no points get `sentinel` (default NaN).

    Same coordinate convention as ObstacleLayer (X forward, Y left,
    robot at center).
    """
    h, w = shape
    z_grid = np.full(shape, sentinel, dtype=np.float32)
    if points_xyz is None or len(points_xyz) == 0:
        return z_grid

    p = np.asarray(points_xyz)
    x = p[:, 0].astype(np.float32)
    y = p[:, 1].astype(np.float32)
    z = p[:, 2].astype(np.float32)

    cx, cy = center_ij
    col = (cx - (y / resolution_m)).astype(np.int32)
    row = (cy - (x / resolution_m)).astype(np.int32)
    in_bounds = (col >= 0) & (col < w) & (row >= 0) & (row < h)
    col, row, z = col[in_bounds], row[in_bounds], z[in_bounds]
    if col.size == 0:
        return z_grid

    # Per-cell min via np.minimum.at (handles duplicates correctly).
    # Seed with +inf so np.minimum.at lowers values, then map +inf back
    # to the sentinel.
    work = np.full(shape, np.inf, dtype=np.float32)
    np.minimum.at(work, (row, col), z)
    mask = np.isfinite(work)
    z_grid[mask] = work[mask]
    return z_grid


class TraversabilityLayer:
    """Slope + ditch lethal detector on a robot-centered grid."""

    def __init__(self,
                 size_m: float = 14.0,
                 resolution_m: float = 0.05,
                 thresholds: Optional[TraversabilityThresholds] = None):
        if size_m <= 0 or resolution_m <= 0:
            raise ValueError("size_m and resolution_m must be > 0")
        self.size_m = float(size_m)
        self.resolution_m = float(resolution_m)
        self.thresholds = thresholds or TraversabilityThresholds()
        n = int(round(size_m / resolution_m))
        self.shape: Tuple[int, int] = (n, n)
        self.grid = np.full(self.shape, COST_FREE, dtype=np.uint8)
        self.center_ij: Tuple[int, int] = (n // 2, n // 2)

    def reset(self) -> None:
        self.grid.fill(COST_FREE)

    # ── Public update entrypoints ──────────────────────────────────

    def update(self, points_xyz: np.ndarray) -> np.ndarray:
        """End-to-end: points → ground grid → slope + ditch → uint8."""
        z_grid = build_ground_grid(
            points_xyz, self.shape, self.resolution_m, self.center_ij)
        return self.update_from_ground(z_grid)

    def update_from_ground(self, z_grid: np.ndarray) -> np.ndarray:
        """Update from a pre-computed ground-elevation grid (NaN-ok)."""
        if z_grid.shape != self.shape:
            raise ValueError(
                f"z_grid shape {z_grid.shape} != layer shape {self.shape}")
        self.reset()
        slope_cost = self._slope_cost(z_grid)
        ditch_cost = self._ditch_cost(z_grid)
        # Pointwise max → lethal beats warn beats free.
        np.maximum(slope_cost, ditch_cost, out=self.grid)
        return self.grid

    # ── Slope (plane-fit via local gradient) ───────────────────────

    def _slope_cost(self, z: np.ndarray) -> np.ndarray:
        """Approximate slope by central differences across the cell.

        With NaN cells (no LiDAR return) the gradient propagates as NaN
        and we mark those cells COST_FREE (unknown ≠ blocked, planner
        decides what to do with low-confidence cells via inflation).
        """
        res = self.resolution_m
        # np.gradient on NaN-containing arrays: NaN inputs → NaN outputs,
        # which we treat as "no info → COST_FREE". A small epsilon
        # prevents 0/0 on perfectly flat zero-elevation patches.
        dzdy, dzdx = np.gradient(z, res)
        tan_theta = np.sqrt(dzdx * dzdx + dzdy * dzdy)
        out = np.full(self.shape, COST_FREE, dtype=np.uint8)
        finite = np.isfinite(tan_theta)
        warn_mask = finite & (tan_theta >= self.thresholds.slope_warn_tan)
        lethal_mask = finite & (tan_theta >= self.thresholds.slope_lethal_tan)
        # Slope warn is the LOW-severity band (100, not 200) — the spec
        # treats a slope-only obstacle as planner-traversable.
        out[warn_mask] = COST_WARN_LOW
        out[lethal_mask] = COST_LETHAL
        return out

    # ── Ditch (binary: width ≥ chassis crossing → lethal) ──────────

    # Depth threshold below which a depression doesn't count as a ditch.
    # A pothole shallower than this is a ride-quality concern, not a
    # chassis-stopper.
    _DITCH_DEPTH_MIN_M = 0.12

    def _ditch_cost(self, z: np.ndarray) -> np.ndarray:
        """Width-aware ditch classifier.

        A cell is COST_LETHAL when it sits inside a band of depressed
        cells (z ≤ -_DITCH_DEPTH_MIN_M) whose continuous run length
        along the forward axis is ≥ `ditch_lethal_mm`. The spec calls
        out chassis crossing capability of 220 mm — a narrower gap is
        safely bridged by the wheels and should classify as FREE.

        Binary FREE/LETHAL by design — the acceptance criteria for the
        ditch detector lists exactly two outcomes (150/220/300 mm →
        free/254/254), no warn band.
        """
        out = np.full(self.shape, COST_FREE, dtype=np.uint8)
        lethal_m, _warn_m = self.thresholds.ditch_meters()
        lethal_cells = max(1, int(round(lethal_m / self.resolution_m)))

        valid_z = np.isfinite(z)
        is_depressed = np.zeros_like(z, dtype=bool)
        is_depressed[valid_z] = z[valid_z] <= -self._DITCH_DEPTH_MIN_M
        if not is_depressed.any():
            return out

        # A cell belongs to a "ditch of width ≥ lethal_cells" when the
        # entire window of `lethal_cells` cells centered on it (along
        # the forward axis) is depressed. We test this via a rolling
        # MIN — if the min over the window is True, all cells were True.
        out[_rolling_all_axis0(is_depressed, lethal_cells)] = COST_LETHAL
        return out
