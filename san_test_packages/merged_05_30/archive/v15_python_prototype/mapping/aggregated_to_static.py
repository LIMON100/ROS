"""AggregatedMap → CostMap static-layer window adapter (PHASE 1 ↔ PHASE 3).

The PHASE 3 SLAM aggregator publishes `AggregatedMap` messages (PNG-
compressed global occupancy grids at the Hub-side 0.10 m resolution,
sample rate = 5 s in v1.3). The PHASE 1 cost-map compositor expects
a robot-centered uint8 grid of `COST_*` values sized to the local
master grid (default 14 × 14 m at 0.05 m resolution = 280 × 280 cells).

This adapter:
  1. PNG-decodes the AggregatedMap into the Hub's coordinate frame.
  2. Re-samples to the local cost-map resolution.
  3. Crops the robot-centered window of `size_m / 2` radius.
  4. Maps occupancy values to COST_FREE / COST_WARN / COST_LETHAL using
     the standard ROS occupancy thresholds (free ≤ 50, lethal ≥ 200,
     mid-grey "unknown" → free so we don't over-fence the planner).

Pure compute, no IPC. Caller drives:

    adapter = AggregatedToStatic(cost_cfg)
    window = adapter.sample(latest_aggregated_map, robot_pose_xy)
    cost_map.compose(points_xyz, static_layer_window=window)
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np

from core.messages import (
    COST_FREE,
    COST_LETHAL,
    COST_WARN,
    AggregatedMap,
)

from .aggregated_map import decode_png_to_grid

# Occupancy thresholds (matches the 0..255 PNG encoding produced by
# encode_grid_to_png in aggregated_map.py):
#   < 50  : free
#   50-200: unknown / warn band
#   ≥ 200 : lethal (confident obstacle from at least one follower)
_FREE_MAX: int = 50
_LETHAL_MIN: int = 200
# Mid-grey "no observation from any robot" → free. Reasoning: an
# unobserved cell is not the same as a blocked cell; treating it as
# COST_FREE here lets the local LiDAR layers (obstacle, traversability)
# drive the decision instead of pre-fencing the planner.
_UNKNOWN_AS_FREE: bool = True


@dataclass(frozen=True)
class AggregatedToStaticConfig:
    """How big a window we crop + the local cost-map resolution.

    Defaults match CostMapConfig() — change these only if your cost map
    differs.
    """
    size_m: float = 14.0
    resolution_m: float = 0.05

    @property
    def grid_cells(self) -> int:
        return int(round(self.size_m / self.resolution_m))


class AggregatedToStatic:
    """Resample + crop an AggregatedMap to a cost-map-shaped uint8 window.

    Stateless — every `sample()` call rebuilds the window from scratch.
    Operators / tests can call it as a free function via `sample_once()`
    below when they don't want to instantiate the class.
    """

    def __init__(self,
                 cfg: Optional[AggregatedToStaticConfig] = None):
        self.cfg = cfg or AggregatedToStaticConfig()
        n = self.cfg.grid_cells
        self.shape: Tuple[int, int] = (n, n)

    def sample(self,
               agg: Optional[AggregatedMap],
               robot_xy: Tuple[float, float]) -> Optional[np.ndarray]:
        """Resample + crop. Returns a (H, W) uint8 grid of COST_* values,
        or None if `agg` is missing / empty / non-overlapping with the
        robot window.
        """
        if agg is None or not agg.occupancy_grid_png:
            return None
        if agg.width_cells <= 0 or agg.height_cells <= 0:
            return None
        try:
            global_grid = decode_png_to_grid(agg.occupancy_grid_png)
        except Exception:        # corrupt payload — surface as "no data"
            return None
        if global_grid.shape != (agg.height_cells, agg.width_cells):
            return None
        return self._extract_window(global_grid, agg, robot_xy)

    # ── Internal: crop + resample ──────────────────────────────────

    def _extract_window(self,
                        global_grid: np.ndarray,
                        agg: AggregatedMap,
                        robot_xy: Tuple[float, float]) -> Optional[np.ndarray]:
        half = self.cfg.size_m / 2.0
        rx, ry = robot_xy
        # World-coord bounds of the local window.
        x_min, y_min = rx - half, ry - half
        x_max, y_max = rx + half, ry + half

        gres = float(agg.resolution_m)
        ox, oy = float(agg.origin.x), float(agg.origin.y)
        gh, gw = global_grid.shape
        # Global-grid bounds in world coords.
        g_x_min, g_y_min = ox, oy
        g_x_max, g_y_max = ox + gw * gres, oy + gh * gres
        # Intersection — bail if the windows don't overlap at all.
        if (x_max <= g_x_min or x_min >= g_x_max
                or y_max <= g_y_min or y_min >= g_y_max):
            return None

        local_n = self.cfg.grid_cells
        local_res = self.cfg.resolution_m
        # For each local cell, sample the global cell at the
        # corresponding world coordinate (nearest-neighbour).
        # Local cell (r, c) maps to world point
        #     x = x_min + (c + 0.5) * local_res
        #     y = y_min + (r + 0.5) * local_res
        # Then global cell index:
        #     gj = floor((x - ox) / gres)
        #     gi = floor((y - oy) / gres)
        # Vectorise to keep the cost reasonable on a 280 × 280 window.
        rows = np.arange(local_n)
        cols = np.arange(local_n)
        xs = x_min + (cols + 0.5) * local_res        # (local_n,)
        ys = y_min + (rows + 0.5) * local_res        # (local_n,)
        gj = np.floor((xs - ox) / gres).astype(np.int32)
        gi = np.floor((ys - oy) / gres).astype(np.int32)
        gj_valid = (gj >= 0) & (gj < gw)
        gi_valid = (gi >= 0) & (gi < gh)
        # Clamp to safe indices; we'll mask OOB cells back to free below.
        gj_safe = np.clip(gj, 0, gw - 1)
        gi_safe = np.clip(gi, 0, gh - 1)
        # Build the (local_n, local_n) sample by outer indexing.
        sampled = global_grid[np.ix_(gi_safe, gj_safe)].astype(np.uint8)
        oob = ~np.outer(gi_valid, gj_valid)
        sampled[oob] = 0        # outside coverage → free

        return _classify(sampled)


def _classify(grid_u8: np.ndarray) -> np.ndarray:
    """Map an occupancy-grid uint8 to COST_FREE / COST_WARN / COST_LETHAL.

    Standard ROS conventions: ≤ 50 = free, ≥ 200 = lethal, in between
    = unknown (treated as free here per `_UNKNOWN_AS_FREE`).
    """
    out = np.full(grid_u8.shape, COST_FREE, dtype=np.uint8)
    out[grid_u8 >= _LETHAL_MIN] = COST_LETHAL
    warn_band = (grid_u8 > _FREE_MAX) & (grid_u8 < _LETHAL_MIN)
    if not _UNKNOWN_AS_FREE:
        out[warn_band] = COST_WARN
    return out


def sample_once(agg: Optional[AggregatedMap],
                robot_xy: Tuple[float, float],
                cfg: Optional[AggregatedToStaticConfig] = None
                ) -> Optional[np.ndarray]:
    """Convenience: build the adapter on the fly + return one window."""
    return AggregatedToStatic(cfg).sample(agg, robot_xy)


__all__ = [
    "AggregatedToStatic",
    "AggregatedToStaticConfig",
    "sample_once",
]
