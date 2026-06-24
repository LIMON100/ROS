"""ObstacleLayer — L2 layer of the 4-layer local cost map (SAN v1.3 §6.4).

Pure compute: takes a deskewed LiDAR point cloud (N, 4) [x, y, z, intensity]
expressed in the robot frame (X forward, Y left, Z up, sensor mount height
already subtracted so z=0 == ground) and produces a (H, W) uint8 grid of
COST_FREE / COST_WARN / COST_LETHAL values.

Thresholds come from `cost_map.obstacle_lethal_mm` / `obstacle_warn_mm`
in system.yaml — they encode the UGV's chassis spec:

  height ≥ 235 mm → COST_LETHAL  (cannot step over)
  200 ≤ height < 235 mm → COST_WARN  (risky, planner may try)
  height < 200 mm → COST_FREE        (drivable)

A cell flips state only when at least `obstacle_min_points` returns
land in it — a single stray return doesn't pollute the grid.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np

from core.messages import COST_FREE, COST_LETHAL, COST_WARN


@dataclass(frozen=True)
class ObstacleThresholds:
    """UGV chassis-derived thresholds. Defaults match SAN v1.3 §4.5."""
    lethal_mm: float = 235.0      # cannot climb
    warn_mm: float = 200.0        # high but maybe traversable
    min_points: int = 3           # cluster size for a cell to register

    def as_meters(self) -> Tuple[float, float]:
        return self.lethal_mm / 1000.0, self.warn_mm / 1000.0


class ObstacleLayer:
    """Robot-centered uint8 occupancy from LiDAR returns above ground.

    Grid is square; the robot sits at the center cell (cx, cy) facing +X.
    Forward coverage = size_m / 2.
    """

    def __init__(self,
                 size_m: float = 14.0,
                 resolution_m: float = 0.05,
                 thresholds: Optional[ObstacleThresholds] = None):
        if size_m <= 0 or resolution_m <= 0:
            raise ValueError("size_m and resolution_m must be > 0")
        self.size_m = float(size_m)
        self.resolution_m = float(resolution_m)
        self.thresholds = thresholds or ObstacleThresholds()
        n = int(round(size_m / resolution_m))
        self.shape: Tuple[int, int] = (n, n)
        self.grid = np.full(self.shape, COST_FREE, dtype=np.uint8)
        # Robot is at the grid center, facing +X.
        self.center_ij: Tuple[int, int] = (n // 2, n // 2)

    def reset(self) -> None:
        self.grid.fill(COST_FREE)

    def update(self, points_xyz: np.ndarray) -> np.ndarray:
        """Recompute the grid from a single LiDAR scan.

        Returns a view of `self.grid` (uint8, shape == self.shape).

        Points are expected in the robot frame with z measured against
        ground (sensor mount height already subtracted). Returns above
        the chassis ceiling (5 m) are dropped — they're tree canopy or
        building overhangs the UGV can drive under.
        """
        self.reset()
        if points_xyz is None or len(points_xyz) == 0:
            return self.grid

        p = np.asarray(points_xyz)
        if p.shape[1] < 3:
            raise ValueError("points_xyz must have ≥ 3 columns (x,y,z)")
        x = p[:, 0].astype(np.float32)
        y = p[:, 1].astype(np.float32)
        z = p[:, 2].astype(np.float32)

        # Drop overhead returns + below-ground (sensor noise) early.
        z_lethal_m, z_warn_m = self.thresholds.as_meters()
        in_height = (z >= z_warn_m) & (z <= 5.0)
        x, y, z = x[in_height], y[in_height], z[in_height]
        if x.size == 0:
            return self.grid

        # World → grid index. Robot at center, X forward (rows ↑),
        # Y left (cols ←). The grid is row-major (y, x) so we map:
        #   col = cx + (-y / res)    (positive y → smaller col)
        #   row = cy - ( x / res)    (positive x → smaller row, forward)
        h, w = self.shape
        cx, cy = self.center_ij
        col = (cx - (y / self.resolution_m)).astype(np.int32)
        row = (cy - (x / self.resolution_m)).astype(np.int32)

        in_bounds = (col >= 0) & (col < w) & (row >= 0) & (row < h)
        col, row, z = col[in_bounds], row[in_bounds], z[in_bounds]
        if col.size == 0:
            return self.grid

        # Count points per cell, separately for warn and lethal heights.
        # Use bincount over a flattened cell index for O(N) accumulation.
        flat = row * w + col
        is_lethal = z >= z_lethal_m
        n_cells = h * w

        counts_warn = np.bincount(flat, minlength=n_cells).reshape(self.shape)
        counts_lethal = np.bincount(flat[is_lethal],
                                    minlength=n_cells).reshape(self.shape)

        min_pts = max(1, int(self.thresholds.min_points))
        # Lethal wins over warn.
        self.grid[counts_warn >= min_pts] = COST_WARN
        self.grid[counts_lethal >= min_pts] = COST_LETHAL
        return self.grid

    def world_to_cell(self, x_m: float, y_m: float) -> Tuple[int, int]:
        """Return (row, col) for a robot-frame point, or (-1, -1) if OOB."""
        cx, cy = self.center_ij
        col = cx - int(round(y_m / self.resolution_m))
        row = cy - int(round(x_m / self.resolution_m))
        h, w = self.shape
        if not (0 <= col < w and 0 <= row < h):
            return -1, -1
        return row, col
