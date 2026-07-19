"""OsmStaticLayer — L1 layer of 3-layer cost map (SDD Rev.A.6 §4.7.1).

OSM PBF → 20 cm rasterized occupancy grid. Replaces the HD-map
dependency that mapping/hd_map_store.py used to satisfy.

Cost mapping:
  building, water       → 1.0  (blocked)
  highway (any)         → 0.2  (preferred)
  natural=tree_row      → 0.6
  unknown               → 0.5
"""
from __future__ import annotations

from typing import Optional, Tuple

import numpy as np


class OsmStaticLayer:
    """OSM static occupancy grid at 20 cm resolution."""

    DEFAULT_CELL_M = 0.20
    DEFAULT_COSTS = {
        "building": 1.0,
        "water":    1.0,
        "road":     0.2,
        "tree_row": 0.6,
        "unknown":  0.5,
    }

    def __init__(self, cell_size_m: float = DEFAULT_CELL_M):
        self.cell_size_m = float(cell_size_m)
        # (lat_min, lon_min, lat_max, lon_max) in WGS84
        self.bbox: Optional[Tuple[float, float, float, float]] = None
        # 2D float32, [0, 1] cost
        self.grid: Optional[np.ndarray] = None
        # (east, north) m — UTM origin of grid[0, 0]
        self.utm_origin: Optional[Tuple[float, float]] = None

    def load_from_pbf(self, pbf_path: str,
                      bbox: Tuple[float, float, float, float]) -> None:
        """Load PBF, extract bbox, rasterize to 20 cm grid.

        bbox: (lat_min, lon_min, lat_max, lon_max) in WGS84.

        Implementation outline:
          1. osmium handler reads ways/nodes inside bbox
          2. WGS84 → UTM zone (pyproj)
          3. Allocate grid: ceil((east_max - east_min) / cell_size_m) ×
             ceil((north_max - north_min) / cell_size_m)
          4. For each way: classify (building/water/road/tree_row) +
             rasterize polygon (interior fill) or line (Bresenham)
          5. Default unmapped cells = 0.5 (unknown)
        """
        # Heavy deps imported lazily so unit tests that never call this
        # don't pay the GDAL/osmium import cost.
        import osmium  # noqa: F401  — Reference: https://docs.osmcode.org/pyosmium/
        import pyproj  # noqa: F401

        raise NotImplementedError(
            "Implement: osmium handler + pyproj transform + raster fill"
        )

    def get_cost(self, x_m: float, y_m: float) -> float:
        """Lookup cost at world coord (UTM east, north). Returns 0.5
        when grid is unloaded or query falls outside grid extent."""
        if self.grid is None or self.utm_origin is None:
            return 0.5
        ix = int((x_m - self.utm_origin[0]) / self.cell_size_m)
        iy = int((y_m - self.utm_origin[1]) / self.cell_size_m)
        h, w = self.grid.shape
        if not (0 <= ix < w and 0 <= iy < h):
            return 0.5
        return float(self.grid[iy, ix])

    def update_cell(self, x_m: float, y_m: float, value: float) -> None:
        """Hybrid update support (SDD §4.7.6) — clamp to [0, 1]."""
        if self.grid is None or self.utm_origin is None:
            return
        ix = int((x_m - self.utm_origin[0]) / self.cell_size_m)
        iy = int((y_m - self.utm_origin[1]) / self.cell_size_m)
        h, w = self.grid.shape
        if 0 <= ix < w and 0 <= iy < h:
            self.grid[iy, ix] = float(np.clip(value, 0.0, 1.0))
