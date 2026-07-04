"""Tests for SRTM 30m DEM loader (P2-6)."""
from __future__ import annotations

import tempfile
from pathlib import Path

import numpy as np
import pytest

from mapping.srtm_dem_loader import SrtmDemLoader


def _make_synthetic_tile_bytes(elevation_m: int = 100) -> bytes:
    arr = np.full((1201, 1201), elevation_m, dtype=">i2")
    return arr.tobytes()


@pytest.fixture
def tmp_dem_dir():
    with tempfile.TemporaryDirectory() as td:
        path = Path(td) / "N37E127.hgt"
        with path.open("wb") as f:
            f.write(_make_synthetic_tile_bytes(elevation_m=100))
        yield td


def test_tile_name_seoul():
    assert SrtmDemLoader.tile_name(37.5, 127.05) == "N37E127.hgt"


def test_tile_name_southern():
    assert SrtmDemLoader.tile_name(-33.5, 18.4) == "S34E018.hgt"


def test_tile_name_western():
    assert SrtmDemLoader.tile_name(40.7, -74.0) == "N40W074.hgt"


def test_load_tile_returns_none_when_missing():
    loader = SrtmDemLoader(tile_dir="/nonexistent/dir")
    assert loader.load_tile(37.5, 127.5) is None


def test_load_tile_success(tmp_dem_dir):
    loader = SrtmDemLoader(tile_dir=tmp_dem_dir)
    tile = loader.load_tile(37.5, 127.05)
    assert tile is not None
    assert tile.lat0 == 37
    assert tile.lon0 == 127
    assert tile.grid.shape == (1201, 1201)


def test_load_tile_caches(tmp_dem_dir):
    loader = SrtmDemLoader(tile_dir=tmp_dem_dir)
    t1 = loader.load_tile(37.5, 127.05)
    t2 = loader.load_tile(37.7, 127.9)  # same tile (N37E127)
    assert t1 is t2


def test_elevation_at(tmp_dem_dir):
    loader = SrtmDemLoader(tile_dir=tmp_dem_dir)
    elev = loader.elevation_at(37.5, 127.05)
    assert elev is not None
    assert abs(elev - 100.0) < 0.5


def test_elevation_outside_tile(tmp_dem_dir):
    loader = SrtmDemLoader(tile_dir=tmp_dem_dir)
    # Tile not loaded for this area (no N35E127.hgt)
    assert loader.elevation_at(35.0, 127.0) is None


def test_slope_zero_in_flat_tile(tmp_dem_dir):
    loader = SrtmDemLoader(tile_dir=tmp_dem_dir)
    slope = loader.slope_at(37.5, 127.05)
    assert slope is not None
    assert slope < 0.01


def test_slope_with_gradient():
    """Tile with row-indexed elevation → measurable slope."""
    with tempfile.TemporaryDirectory() as td:
        # Each row has constant elevation = row index (north→high gradient).
        col = np.arange(1201, dtype=">i2").reshape(-1, 1)
        arr = np.tile(col, (1, 1201))
        path = Path(td) / "N37E127.hgt"
        with path.open("wb") as f:
            f.write(arr.tobytes())

        loader = SrtmDemLoader(tile_dir=td)
        slope = loader.slope_at(37.5, 127.05, delta_m=5.0)
        assert slope is not None
        assert slope > 0.001


def test_void_value_returns_none():
    """Cell with void value → returns None."""
    with tempfile.TemporaryDirectory() as td:
        arr = np.full((1201, 1201), 100, dtype=">i2")
        # SRTM cell at (v=600, u=600) covers the centre of N37E127.
        arr[600, 600] = -32768
        path = Path(td) / "N37E127.hgt"
        with path.open("wb") as f:
            f.write(arr.tobytes())

        loader = SrtmDemLoader(tile_dir=td)
        # u = 600 → lon = 127 + 600/1200 = 127.5
        # v = 600 → lat = 37 + 1 - 600/1200 = 37.5
        elev = loader.elevation_at(37.5, 127.5)
        assert elev is None
