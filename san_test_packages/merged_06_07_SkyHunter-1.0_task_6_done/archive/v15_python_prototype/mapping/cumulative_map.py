"""
Cumulative point cloud accumulator with time awareness.

Per-cell metadata tracked:
  • observation_count
  • first_seen, last_seen
  • persistence score (rolling)
  • layer auto-classification:
      L1 permanent     (persistence >= 0.9)
      L2 semi-permanent(0.5 ≤ p < 0.9)
      L3 temporary     (0.1 ≤ p < 0.5)
      L4 noise         (p < 0.1, dropped)

This is the place where Voxblox / OpenVDB would normally be plugged in.
For pure-Python prototype, we use a 3D voxel grid with float32 stats.
"""
from __future__ import annotations

import time
from typing import Dict, Tuple

import numpy as np


class CumulativeMap:
    """
    Per-tile (10m × 10m) cumulative voxel statistics.

    For each (ix, iy) tile:
      hits[H, W]:        number of times occupied
      misses[H, W]:      number of times observed free
      first_seen[H, W]:  earliest timestamp
      last_seen[H, W]:   latest timestamp
    """

    def __init__(self, tile_size_m: float = 10.0, resolution: float = 0.05):
        self.tile_size_m = tile_size_m
        self.resolution = resolution
        self.cells_per_side = int(tile_size_m / resolution)
        self._tiles: Dict[Tuple[int, int], dict] = {}
        # Lock created lazily on first use (spawn-safe).
        self._lock = None

    def _ensure_lock(self):
        if self._lock is None:
            import threading
            self._lock = threading.Lock()
        return self._lock

    def _get_or_create_tile(self, ix: int, iy: int) -> dict:
        if (ix, iy) not in self._tiles:
            n = self.cells_per_side
            self._tiles[(ix, iy)] = {
                "hits": np.zeros((n, n), dtype=np.uint16),
                "misses": np.zeros((n, n), dtype=np.uint16),
                "first_seen": np.zeros((n, n), dtype=np.float32),
                "last_seen": np.zeros((n, n), dtype=np.float32),
            }
        return self._tiles[(ix, iy)]

    def update(self, points_xyz: np.ndarray, sensor_xy: Tuple[float, float]) -> None:
        """
        points_xyz: (N, 3) float32 in world frame.
        sensor_xy: position of the sensor (for raycast — placeholder here).

        Real impl: voxel-based ray casting (e.g., Bresenham 3D).
        Here: only mark hit cells. (Free-space update is more expensive.)
        """
        if points_xyz.size == 0:
            return
        t = time.time()
        # bucket points by tile
        tile_ix = (points_xyz[:, 0] / self.tile_size_m).astype(np.int32)
        tile_iy = (points_xyz[:, 1] / self.tile_size_m).astype(np.int32)
        unique_tiles = np.unique(np.stack([tile_ix, tile_iy], axis=1), axis=0)

        with self._ensure_lock():
            for ix, iy in unique_tiles:
                mask = (tile_ix == ix) & (tile_iy == iy)
                pts = points_xyz[mask]
                cell_x = ((pts[:, 0] - ix * self.tile_size_m) / self.resolution).astype(np.int32)
                cell_y = ((pts[:, 1] - iy * self.tile_size_m) / self.resolution).astype(np.int32)
                # clamp
                n = self.cells_per_side
                ok = (cell_x >= 0) & (cell_x < n) & (cell_y >= 0) & (cell_y < n)
                cell_x, cell_y = cell_x[ok], cell_y[ok]
                if cell_x.size == 0:
                    continue
                tile = self._get_or_create_tile(int(ix), int(iy))
                np.add.at(tile["hits"], (cell_y, cell_x), 1)
                # first_seen: only if zero
                first = tile["first_seen"]
                first_mask = first[cell_y, cell_x] == 0
                first[cell_y[first_mask], cell_x[first_mask]] = t
                tile["last_seen"][cell_y, cell_x] = t

    def persistence(self, ix: int, iy: int) -> np.ndarray:
        """Persistence ∈ [0,1] = hits / (hits + misses + 1)."""
        with self._ensure_lock():
            tile = self._tiles.get((ix, iy))
            if tile is None:
                n = self.cells_per_side
                return np.zeros((n, n), dtype=np.float32)
            h = tile["hits"].astype(np.float32)
            m = tile["misses"].astype(np.float32)
            return h / (h + m + 1.0)

    def classify_layers(self, ix: int, iy: int) -> np.ndarray:
        """Cell layer code: 1=L1, 2=L2, 3=L3, 4=L4 (noise)."""
        p = self.persistence(ix, iy)
        out = np.full(p.shape, 4, dtype=np.uint8)
        out[p >= 0.1] = 3
        out[p >= 0.5] = 2
        out[p >= 0.9] = 1
        return out

    def time_window_occupancy(
        self, ix: int, iy: int, since: float, until: float
    ) -> np.ndarray:
        """Occupancy considering only observations within [since, until]."""
        with self._ensure_lock():
            tile = self._tiles.get((ix, iy))
            if tile is None:
                n = self.cells_per_side
                return np.full((n, n), -1.0, dtype=np.float32)
            ls = tile["last_seen"]
            mask = (ls >= since) & (ls <= until)
            occ = np.where(mask, 1.0, -1.0).astype(np.float32)
            return occ
