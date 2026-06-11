"""SlamPersistentLayer — L2 layer of 3-layer cost map (SDD Rev.A.6 §4.7.5).

Bayesian update: P_new = α·P_prev + (1-α)·P_observed
α = 0.95 → roughly 20 observations (~2 s @ 10 Hz) to confirm a new
obstacle, ~50 observations for strong confidence.
"""
from __future__ import annotations

from typing import Tuple

import numpy as np


class SlamPersistentLayer:
    """Bayesian persistent occupancy layer."""

    DEFAULT_CELL_M = 0.20
    DEFAULT_ALPHA = 0.95

    # P2-5 — observation_count increments only on confident readings (|p−0.5|
    # ≥ this margin). Picked to ignore noisy mid-range observations.
    OBS_SIGNAL_MARGIN = 0.4

    def __init__(self,
                 cell_size_m: float = DEFAULT_CELL_M,
                 alpha: float = DEFAULT_ALPHA,
                 grid_shape: Tuple[int, int] = (500, 500)):
        self.cell_size_m = float(cell_size_m)
        self.alpha = float(alpha)
        # Start at 0.5 (uncertain) — Bayesian prior with no information.
        self.grid = np.full(grid_shape, 0.5, dtype=np.float32)
        # P2-5 hybrid update — counts confident observations per cell.
        self.observation_count = np.zeros(grid_shape, dtype=np.uint16)
        self.utm_origin: Tuple[float, float] = (0.0, 0.0)

    def bayesian_update(self, observed: np.ndarray) -> None:
        """Bayesian persistence update.

        observed: 2D array same shape as self.grid, values in [0, 1]
        representing per-cell occupancy probabilities from the latest
        SLAM tile.
        """
        if observed.shape != self.grid.shape:
            raise ValueError(
                f"shape mismatch: {observed.shape} vs {self.grid.shape}"
            )
        self.grid = (self.alpha * self.grid +
                     (1.0 - self.alpha) * observed.astype(np.float32))
        np.clip(self.grid, 0.0, 1.0, out=self.grid)
        # Bump observation_count where the input was a confident reading.
        signal_mask = np.abs(observed - 0.5) >= self.OBS_SIGNAL_MARGIN
        np.add(self.observation_count, signal_mask.astype(np.uint16),
               out=self.observation_count, where=signal_mask,
               casting="unsafe")

    def get_cell(self, x_m: float, y_m: float) -> float:
        """O(1) lookup at world coord. Returns 0.5 outside grid."""
        ix = int((x_m - self.utm_origin[0]) / self.cell_size_m)
        iy = int((y_m - self.utm_origin[1]) / self.cell_size_m)
        h, w = self.grid.shape
        if not (0 <= ix < w and 0 <= iy < h):
            return 0.5
        return float(self.grid[iy, ix])

    def reset(self) -> None:
        """Snap all cells back to 0.5 (uncertain).

        Triggered after RTK FIX recovery — pose discontinuity invalidates
        accumulated SLAM evidence (SDD §4.7.5).
        """
        self.grid.fill(0.5)
        self.observation_count.fill(0)
