"""SRTM 30m Digital Elevation Model loader (SDD §10.5.1, P2-6).

NASA SRTM (Shuttle Radar Topography Mission) 30 m tiles.
File format: HGT (16-bit big-endian signed int, meters above sea level).
Tile naming: N37E127.hgt (lat-lon corner of 1°×1° tile).
Tile size: 1201 × 1201 samples (3 arc-seconds = ~30 m).

Used by:
- P2-3 Auto terrain switch (DEM elevation triggers)
- Mission planning (slope-aware path)

Korea coverage: ~25 tiles (lat 33-39, lon 124-132) ~50 MB onboard.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Tuple

import numpy as np


@dataclass
class DemTile:
    lat0: int                # SW corner lat
    lon0: int                # SW corner lon
    grid: np.ndarray         # 1201×1201 int16, meters
    void_value: int = -32768  # SRTM 'no data' marker


class SrtmDemLoader:
    """Load and query SRTM 30m HGT tiles."""

    TILE_SIZE = 1201
    DEFAULT_DIR = "/opt/patrol_maps/srtm/"

    def __init__(self, tile_dir: str = DEFAULT_DIR) -> None:
        self.tile_dir = Path(tile_dir)
        self._cache: Dict[Tuple[int, int], DemTile] = {}

    @staticmethod
    def tile_name(lat_deg: float, lon_deg: float) -> str:
        """Compute HGT filename for given lat/lon."""
        lat0 = int(math.floor(lat_deg))
        lon0 = int(math.floor(lon_deg))
        ns = "N" if lat0 >= 0 else "S"
        ew = "E" if lon0 >= 0 else "W"
        return f"{ns}{abs(lat0):02d}{ew}{abs(lon0):03d}.hgt"

    def load_tile(self, lat_deg: float, lon_deg: float) -> Optional[DemTile]:
        lat0 = int(math.floor(lat_deg))
        lon0 = int(math.floor(lon_deg))
        if (lat0, lon0) in self._cache:
            return self._cache[(lat0, lon0)]

        path = self.tile_dir / self.tile_name(lat_deg, lon_deg)
        if not path.exists():
            return None
        try:
            with path.open("rb") as f:
                raw = f.read(self.TILE_SIZE * self.TILE_SIZE * 2)
            grid = np.frombuffer(raw, dtype=">i2").reshape(
                (self.TILE_SIZE, self.TILE_SIZE))
            tile = DemTile(lat0=lat0, lon0=lon0, grid=grid)
            self._cache[(lat0, lon0)] = tile
            return tile
        except (OSError, ValueError):
            return None

    def elevation_at(self, lat_deg: float, lon_deg: float) -> Optional[float]:
        """Bilinear interpolation. Returns meters or None."""
        tile = self.load_tile(lat_deg, lon_deg)
        if tile is None:
            return None
        # Local coords within the tile (0..TILE_SIZE-1).
        u = (lon_deg - tile.lon0) * (self.TILE_SIZE - 1)
        # SRTM rows go north → south, so latitude axis is reversed.
        v = (tile.lat0 + 1 - lat_deg) * (self.TILE_SIZE - 1)
        if u < 0 or u > self.TILE_SIZE - 1 or v < 0 or v > self.TILE_SIZE - 1:
            return None
        u0, u1 = int(math.floor(u)), int(math.ceil(u))
        v0, v1 = int(math.floor(v)), int(math.ceil(v))
        u1 = min(u1, self.TILE_SIZE - 1)
        v1 = min(v1, self.TILE_SIZE - 1)
        du, dv = u - u0, v - v0

        v00 = int(tile.grid[v0, u0])
        v10 = int(tile.grid[v0, u1])
        v01 = int(tile.grid[v1, u0])
        v11 = int(tile.grid[v1, u1])

        if tile.void_value in (v00, v10, v01, v11):
            return None

        a = v00 * (1 - du) + v10 * du
        b = v01 * (1 - du) + v11 * du
        return float(a * (1 - dv) + b * dv)

    def slope_at(self, lat_deg: float, lon_deg: float,
                 delta_m: float = 5.0) -> Optional[float]:
        """Slope (gradient magnitude) m/m at given point.

        delta_m is the lateral sampling distance for finite-difference.
        Returns abs gradient (no direction). None if any sample is void.
        """
        d_lat = delta_m / 111_111.0
        d_lon = delta_m / (111_111.0 * math.cos(math.radians(lat_deg)))

        h_n = self.elevation_at(lat_deg + d_lat, lon_deg)
        h_s = self.elevation_at(lat_deg - d_lat, lon_deg)
        h_e = self.elevation_at(lat_deg, lon_deg + d_lon)
        h_w = self.elevation_at(lat_deg, lon_deg - d_lon)
        if any(v is None for v in (h_n, h_s, h_e, h_w)):
            return None
        dh_dy = (h_n - h_s) / (2 * delta_m)
        dh_dx = (h_e - h_w) / (2 * delta_m)
        return math.hypot(dh_dx, dh_dy)
