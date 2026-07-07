"""Tests for OsmStaticLayer (mapping/osm_static_layer.py).

The PBF-reading path (``load_from_pbf``) needs osmium + a real PBF
file; we cover those as a single skip-if-deps-missing smoke test. The
pure-math helpers (UTM projection, polygon rasterizer, way classifier)
get full pytest coverage without any heavy dep.
"""
from __future__ import annotations

import importlib

import numpy as np
import pytest

from mapping.osm_static_layer import (
    OsmStaticLayer,
    _bresenham,
    _utm_epsg,
    _utm_zone,
    classify_way,
    rasterize_line,
    rasterize_polygon,
)

# ─── existing smoke tests (preserved) ──────────────────────────────────


def test_init_with_default_cell():
    layer = OsmStaticLayer()
    assert layer.cell_size_m == 0.20


def test_get_cost_outside_grid_returns_unknown():
    layer = OsmStaticLayer()
    assert layer.get_cost(1.0, 1.0) == 0.5


def test_get_cost_inside_grid():
    layer = OsmStaticLayer()
    layer.grid = np.zeros((10, 10), dtype=np.float32)
    layer.grid[5, 5] = 1.0
    layer.utm_origin = (0.0, 0.0)
    cost = layer.get_cost(5 * 0.20 + 0.05, 5 * 0.20 + 0.05)
    assert cost == 1.0


def test_update_cell_clamps():
    layer = OsmStaticLayer()
    layer.grid = np.zeros((10, 10), dtype=np.float32)
    layer.utm_origin = (0.0, 0.0)
    layer.update_cell(0.05, 0.05, 1.5)   # > 1.0 → clamped to 1.0
    assert layer.get_cost(0.05, 0.05) == 1.0
    layer.update_cell(0.05, 0.05, -0.5)  # < 0.0 → clamped to 0.0
    assert layer.get_cost(0.05, 0.05) == 0.0


# ─── classify_way ──────────────────────────────────────────────────────


def test_classify_way_building_wins_over_road():
    """A road-bridge with a `building` tag is structurally a building
    (most-restrictive class). The order matters — see SDD §4.7.1."""
    tags = {"building": "yes", "highway": "primary"}
    assert classify_way(tags) == "building"


@pytest.mark.parametrize("tags, expected", [
    ({"natural": "water"},        "water"),
    ({"natural": "wetland"},      "water"),
    ({"waterway": "river"},       "water"),
    ({"natural": "tree_row"},     "tree_row"),
    ({"highway": "residential"},  "road"),
    ({"foo": "bar"},              "unknown"),
    ({},                          "unknown"),
])
def test_classify_way_per_tag(tags, expected):
    assert classify_way(tags) == expected


# ─── UTM zone / epsg ───────────────────────────────────────────────────


@pytest.mark.parametrize("lon, zone", [
    (-180.0,  1),   # zone 1 starts at the antimeridian
    (-177.0,  1),
    (-3.0,   30),   # 0° E is the boundary between 30 and 31
    (3.0,    31),
    (126.978, 52),  # Seoul
    (179.999, 60),
])
def test_utm_zone_known_longitudes(lon, zone):
    assert _utm_zone(lon) == zone


def test_utm_epsg_north_vs_south():
    """Seoul (lat>0) → 32652; Sydney (lat<0, lon≈151°) → 32756."""
    assert _utm_epsg(37.5665, 126.978) == 32652
    assert _utm_epsg(-33.8688, 151.2093) == 32756


# ─── polygon rasterizer ────────────────────────────────────────────────


def test_rasterize_polygon_square_interior_filled():
    grid = np.zeros((10, 10), dtype=np.float32)
    poly = [(2, 2), (7, 2), (7, 7), (2, 7)]
    rasterize_polygon(grid, poly, value=1.0)
    # Interior cells set.
    assert grid[4, 4] == 1.0
    # Exterior cells untouched.
    assert grid[0, 0] == 0.0
    assert grid[9, 9] == 0.0
    # Sanity: ~5×5 = 25 cells filled (boundary differences allowed).
    filled = int((grid == 1.0).sum())
    assert 18 <= filled <= 36, f"unexpected fill count {filled}"


def test_rasterize_polygon_triangle_pixel_count_nonzero():
    grid = np.zeros((20, 20), dtype=np.float32)
    poly = [(2, 2), (18, 2), (10, 18)]
    rasterize_polygon(grid, poly, value=0.75)
    filled = int((grid > 0).sum())
    assert filled > 0
    # Triangle area is ~(1/2 × 16 × 16) ≈ 128. Allow generous slack
    # because rasterization undercounts at edges.
    assert 60 < filled < 160


def test_rasterize_polygon_too_few_points_is_noop():
    grid = np.zeros((5, 5), dtype=np.float32)
    rasterize_polygon(grid, [(1, 1), (2, 2)], value=1.0)
    assert grid.sum() == 0.0


def test_rasterize_polygon_rejects_1d_grid():
    with pytest.raises(ValueError, match="2D"):
        rasterize_polygon(np.zeros(10), [(0, 0), (1, 1), (2, 0)], 1.0)


# ─── line rasterizer / bresenham ───────────────────────────────────────


def test_bresenham_diagonal_endpoints_inclusive():
    pts = list(_bresenham(0, 0, 3, 3))
    assert pts[0] == (0, 0)
    assert pts[-1] == (3, 3)
    assert len(pts) == 4   # diagonal: 4 cells


def test_rasterize_line_horizontal_width_one():
    grid = np.zeros((5, 10), dtype=np.float32)
    rasterize_line(grid, [(1, 2), (8, 2)], value=1.0, width=1)
    assert (grid[2, 1:9] == 1.0).all()
    # No bleed into neighbouring rows.
    assert (grid[1, :] == 0.0).all()
    assert (grid[3, :] == 0.0).all()


def test_rasterize_line_thick_brush():
    grid = np.zeros((10, 10), dtype=np.float32)
    rasterize_line(grid, [(0, 5), (9, 5)], value=0.5, width=2)
    # 3-cell vertical thickness around row 5.
    assert (grid[5, :] == 0.5).all()
    assert (grid[6, :] == 0.5).all()


# ─── OsmStaticLayer integration (allocate + stamp) ─────────────────────


@pytest.fixture
def tiny_layer():
    pytest.importorskip("pyproj")
    return OsmStaticLayer(cell_size_m=1.0)   # 1 m/cell for round numbers


def test_allocate_grid_seoul_bbox_produces_sane_dimensions(tiny_layer):
    """A 100 m × 100 m bbox near Seoul should land in a ~100×100 grid
    at 1 m/cell. Allow tolerance for UTM scale distortion."""
    # Roughly 100 m × 100 m centred near city hall (lat 37.5665).
    # 0.0009° lat ≈ 100 m; 0.0011° lon ≈ 97 m at that latitude.
    bbox = (37.5665, 126.978, 37.5665 + 0.0009, 126.978 + 0.0011)
    h, w = tiny_layer.allocate_grid(bbox)
    assert 80 < h < 130, f"unexpected height {h}"
    assert 80 < w < 130, f"unexpected width  {w}"
    assert tiny_layer.utm_epsg == 32652
    assert tiny_layer.grid is not None
    # Grid initialised to "unknown" (0.5).
    assert (tiny_layer.grid == 0.5).all()


def test_stamp_way_polygon_paints_building(tiny_layer):
    bbox = (37.5665, 126.978, 37.5665 + 0.0009, 126.978 + 0.0011)
    tiny_layer.allocate_grid(bbox)
    east0, north0 = tiny_layer.utm_origin
    # 5 m × 5 m square inside the grid.
    poly_utm = [
        (east0 + 10, north0 + 10),
        (east0 + 15, north0 + 10),
        (east0 + 15, north0 + 15),
        (east0 + 10, north0 + 15),
    ]
    tiny_layer.stamp_way("building", poly_utm, closed=True)
    # Centre of square → cost 1.0.
    assert tiny_layer.get_cost(east0 + 12.5, north0 + 12.5) == 1.0
    # Far corner → still unknown.
    assert tiny_layer.get_cost(east0 + 50, north0 + 50) == 0.5


def test_stamp_way_road_line_paints_thick(tiny_layer):
    bbox = (37.5665, 126.978, 37.5665 + 0.0009, 126.978 + 0.0011)
    tiny_layer.allocate_grid(bbox)
    east0, north0 = tiny_layer.utm_origin
    line_utm = [(east0 + 5, north0 + 30), (east0 + 50, north0 + 30)]
    tiny_layer.stamp_way("road", line_utm, closed=False)
    # On the line → road cost 0.2.
    assert tiny_layer.get_cost(east0 + 25, north0 + 30) == 0.2
    # A few metres off the line — still inside the road's thick brush.
    assert tiny_layer.get_cost(east0 + 25, north0 + 31) == 0.2


def test_stamp_way_unknown_class_is_noop(tiny_layer):
    bbox = (37.5665, 126.978, 37.5665 + 0.0009, 126.978 + 0.0011)
    tiny_layer.allocate_grid(bbox)
    assert tiny_layer.grid is not None
    original = tiny_layer.grid.copy()
    east0, north0 = tiny_layer.utm_origin
    tiny_layer.stamp_way(
        "unknown",
        [(east0 + 10, north0 + 10), (east0 + 30, north0 + 30)],
        closed=False,
    )
    assert np.array_equal(tiny_layer.grid, original)


def test_allocate_grid_rejects_degenerate_bbox(tiny_layer):
    with pytest.raises(ValueError, match="degenerate"):
        tiny_layer.allocate_grid((37.5, 127.0, 37.5, 127.0))


# ─── PBF path (only if osmium is installed) ────────────────────────────


@pytest.mark.skipif(
    importlib.util.find_spec("osmium") is None,
    reason="osmium not installed — PBF integration path can't be exercised "
           "in CI by design; production install brings it via apt+conda")
def test_load_from_pbf_smoke_minimal_extract():
    """Smoke test: real PBF read path. Skipped on CI by default.

    The runner only opens the file and runs ``apply_file`` against the
    classifier — we don't ship a baked PBF in-tree, so this test needs
    the user to point at a real extract via the
    ``PATROL_TEST_OSM_PBF`` environment variable.
    """
    import os
    pbf = os.environ.get("PATROL_TEST_OSM_PBF")
    if not pbf:
        pytest.skip("set PATROL_TEST_OSM_PBF to a real .pbf extract")
    layer = OsmStaticLayer(cell_size_m=0.5)
    bbox_str = os.environ.get("PATROL_TEST_OSM_BBOX", "")
    bbox = tuple(float(x) for x in bbox_str.split(","))
    assert len(bbox) == 4, (
        "PATROL_TEST_OSM_BBOX must be lat_min,lon_min,lat_max,lon_max")
    layer.load_from_pbf(pbf, bbox)  # type: ignore[arg-type]
    assert layer.grid is not None
    # At least *some* cells should be classified non-unknown.
    assert (layer.grid != 0.5).any(), "PBF produced no classified cells"
