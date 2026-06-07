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

from typing import Iterable, List, Optional, Sequence, Tuple

import numpy as np

# ─── Cost classification (pure, no deps) ───────────────────────────────


def classify_way(tags: dict) -> str:
    """Map OSM way tags to one of {"building", "water", "road",
    "tree_row", "unknown"}.

    Order matters: a single way can carry multiple tags (e.g. a road
    bridge with `building=yes`), so the most-restrictive class wins.
    The order below is exactly the worst→best traversal so a `building`
    tag never gets demoted by a `highway` tag on the same way.
    """
    if tags.get("building"):
        return "building"
    natural = tags.get("natural")
    if natural in ("water", "wetland"):
        return "water"
    if tags.get("waterway"):
        return "water"
    if natural == "tree_row":
        return "tree_row"
    if tags.get("highway"):
        return "road"
    return "unknown"


# ─── WGS84 ↔ UTM (lazy import of pyproj) ───────────────────────────────


def _utm_zone(lon_deg: float) -> int:
    """Standard UTM zone for a given longitude (1..60)."""
    return int((lon_deg + 180.0) / 6.0) % 60 + 1


def _utm_epsg(lat_deg: float, lon_deg: float) -> int:
    """Return the EPSG code for the UTM zone covering (lat, lon).

    Northern hemisphere: 32601..32660 (zone 1..60).
    Southern hemisphere: 32701..32760.
    """
    zone = _utm_zone(lon_deg)
    return (32600 if lat_deg >= 0 else 32700) + zone


# ─── Polygon rasterization (pure numpy, no Pillow) ─────────────────────


def rasterize_polygon(
    grid: np.ndarray,
    polygon_ij: Sequence[Tuple[int, int]],
    value: float,
) -> None:
    """Fill ``grid`` in-place with ``value`` everywhere inside the
    closed polygon ``polygon_ij`` (list of (col, row) tuples in grid
    coordinates).

    Implementation: scanline (even-odd rule) — for each row that the
    polygon's bounding box covers, compute the x-intersections of every
    non-horizontal edge with that row's y-center and fill between
    consecutive x-pairs. O(rows × edges).

    No Pillow/shapely dependency. Works with any closed-or-open ring;
    the closing segment is implied.
    """
    if grid.ndim != 2:
        raise ValueError("grid must be 2D")
    if len(polygon_ij) < 3:
        return
    pts = np.asarray(polygon_ij, dtype=np.float64)
    ys = pts[:, 1]
    y_min = int(max(0, np.floor(ys.min())))
    y_max = int(min(grid.shape[0] - 1, np.ceil(ys.max())))
    h, w = grid.shape
    n = len(pts)

    for y in range(y_min, y_max + 1):
        y_center = y + 0.5
        xints: List[float] = []
        for k in range(n):
            x1, y1 = pts[k]
            x2, y2 = pts[(k + 1) % n]
            if y1 == y2:               # horizontal edge — skip
                continue
            if (y1 > y_center) == (y2 > y_center):
                continue               # edge doesn't straddle this row
            t = (y_center - y1) / (y2 - y1)
            xints.append(x1 + t * (x2 - x1))
        xints.sort()
        for a, b in zip(xints[0::2], xints[1::2], strict=False):
            xa = int(max(0, np.ceil(a)))
            xb = int(min(w - 1, np.floor(b)))
            if xb >= xa:
                grid[y, xa:xb + 1] = value


def rasterize_line(
    grid: np.ndarray,
    line_ij: Sequence[Tuple[int, int]],
    value: float,
    width: int = 1,
) -> None:
    """Rasterize a poly-line (open) onto ``grid`` with Bresenham.

    ``width`` is the half-thickness in cells (1 = single-pixel line,
    2 = a 3-cell thick stroke, etc.) — used so a road's centerline
    paints a few cells wide instead of a hairline.
    """
    if len(line_ij) < 2:
        return
    h, w = grid.shape
    for k in range(len(line_ij) - 1):
        x1, y1 = line_ij[k]
        x2, y2 = line_ij[k + 1]
        for px, py in _bresenham(int(x1), int(y1), int(x2), int(y2)):
            # Square brush of half-width `width`.
            for dy in range(-width + 1, width):
                for dx in range(-width + 1, width):
                    xx, yy = px + dx, py + dy
                    if 0 <= xx < w and 0 <= yy < h:
                        grid[yy, xx] = value


def _bresenham(x0: int, y0: int, x1: int, y1: int) -> Iterable[Tuple[int, int]]:
    """Integer Bresenham line generator — pure Python."""
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    x, y = x0, y0
    while True:
        yield x, y
        if x == x1 and y == y1:
            return
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x += sx
        if e2 <= dx:
            err += dx
            y += sy


# ─── Main class ────────────────────────────────────────────────────────


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

    # Width of rasterized road centerlines, in cells. At 20 cm/cell a
    # half-width of 8 covers a ~3.2 m lane on each side of the line —
    # close to a residential street.
    ROAD_HALF_WIDTH_CELLS = 8

    def __init__(self, cell_size_m: float = DEFAULT_CELL_M):
        self.cell_size_m = float(cell_size_m)
        # (lat_min, lon_min, lat_max, lon_max) in WGS84
        self.bbox: Optional[Tuple[float, float, float, float]] = None
        # 2D float32, [0, 1] cost
        self.grid: Optional[np.ndarray] = None
        # (east, north) m — UTM origin of grid[0, 0]
        self.utm_origin: Optional[Tuple[float, float]] = None
        # Cached pyproj epsg so callers can introspect after load.
        self.utm_epsg: Optional[int] = None

    def allocate_grid(
        self,
        bbox: Tuple[float, float, float, float],
    ) -> Tuple[int, int]:
        """Project bbox to UTM, allocate the grid, and remember its
        origin. Returns ``(height, width)`` in cells.

        Pulled out as a public method so unit tests can exercise the
        sizing/projection math without needing a real PBF.
        """
        import pyproj  # lazy — heavy import

        lat_min, lon_min, lat_max, lon_max = bbox
        lat_mid = 0.5 * (lat_min + lat_max)
        lon_mid = 0.5 * (lon_min + lon_max)
        epsg = _utm_epsg(lat_mid, lon_mid)
        transformer = pyproj.Transformer.from_crs(
            "EPSG:4326", f"EPSG:{epsg}", always_xy=True)

        east_min, north_min = transformer.transform(lon_min, lat_min)
        east_max, north_max = transformer.transform(lon_max, lat_max)
        # Account for the bbox corners not being axis-aligned in UTM.
        east_lo  = min(east_min, east_max)
        east_hi  = max(east_min, east_max)
        north_lo = min(north_min, north_max)
        north_hi = max(north_min, north_max)

        width  = int(np.ceil((east_hi  - east_lo)  / self.cell_size_m))
        height = int(np.ceil((north_hi - north_lo) / self.cell_size_m))
        if width <= 0 or height <= 0:
            raise ValueError(
                f"degenerate bbox → {width}×{height} grid (lat/lon swap?)")

        unknown = self.DEFAULT_COSTS["unknown"]
        self.grid = np.full((height, width), unknown, dtype=np.float32)
        self.utm_origin = (east_lo, north_lo)
        self.utm_epsg = epsg
        self.bbox = bbox
        return height, width

    def _ij_for_utm(self, east: float, north: float) -> Tuple[int, int]:
        """UTM (east, north) → integer (col, row)."""
        if self.utm_origin is None:
            raise RuntimeError("allocate_grid() must be called first")
        ix = int((east  - self.utm_origin[0]) / self.cell_size_m)
        iy = int((north - self.utm_origin[1]) / self.cell_size_m)
        return ix, iy

    def stamp_way(
        self,
        way_class: str,
        nodes_utm: Sequence[Tuple[float, float]],
        *,
        closed: bool,
    ) -> None:
        """Burn one OSM way onto the grid.

        ``nodes_utm`` is a list of (east, north) UTM coords. ``closed``
        means polygon-fill; otherwise it's a line rasterization (road,
        tree_row).
        """
        if self.grid is None:
            raise RuntimeError("allocate_grid() must be called first")
        if way_class not in self.DEFAULT_COSTS:
            return
        cost = self.DEFAULT_COSTS[way_class]
        nodes_ij = [self._ij_for_utm(e, n) for e, n in nodes_utm]
        if closed and len(nodes_ij) >= 3:
            rasterize_polygon(self.grid, nodes_ij, cost)
        else:
            # Roads + tree rows: thick brush. Buildings/water rings
            # without a `closed` flag are degenerate — skip them.
            if way_class in ("road", "tree_row"):
                rasterize_line(
                    self.grid, nodes_ij, cost,
                    width=self.ROAD_HALF_WIDTH_CELLS)

    def load_from_pbf(
        self,
        pbf_path: str,
        bbox: Tuple[float, float, float, float],
    ) -> None:
        """Load PBF, extract bbox, rasterize to 20 cm grid.

        bbox: (lat_min, lon_min, lat_max, lon_max) in WGS84.

        Heavy deps (osmium, pyproj) are imported lazily so unit tests
        that exercise only the math helpers above don't need them
        installed. The osmium handler walks ways; each way's nodes are
        projected to UTM and burned via ``stamp_way()``.
        """
        import osmium
        import pyproj

        self.allocate_grid(bbox)
        # pyproj transformer instance is reusable; the allocate_grid()
        # call already chose the UTM zone — reuse the same EPSG so the
        # ways line up with the grid origin.
        transformer = pyproj.Transformer.from_crs(
            "EPSG:4326", f"EPSG:{self.utm_epsg}", always_xy=True)

        layer = self  # closure capture

        class _WayHandler(osmium.SimpleHandler):
            """Walk every way in the PBF, classify, project, and burn.

            Filtering on ``bbox`` is left to the caller (typically the
            PBF is pre-cut with osmium-tool's `extract`) — we accept the
            extra cost of scanning a slightly larger file rather than
            doing point-in-bbox testing per node.
            """

            def way(self, w) -> None:
                tags = {t.k: t.v for t in w.tags}
                way_class = classify_way(tags)
                if way_class == "unknown":
                    return
                # is_closed is the cheap structural check; the building/
                # water classifications still apply on open rings, but
                # we only polygon-fill when the ring closes — open
                # polygons get the line rasterizer fallback.
                try:
                    closed = bool(w.is_closed())
                except Exception:                # noqa: BLE001
                    closed = False
                nodes_utm: List[Tuple[float, float]] = []
                for n in w.nodes:
                    try:
                        lon, lat = float(n.lon), float(n.lat)
                    except (osmium.InvalidLocationError, AttributeError):
                        continue
                    e, north = transformer.transform(lon, lat)
                    nodes_utm.append((e, north))
                if len(nodes_utm) < 2:
                    return
                layer.stamp_way(way_class, nodes_utm, closed=closed)

        handler = _WayHandler()
        # locations=True is required so n.lon / n.lat are populated
        # without a separate node pass.
        handler.apply_file(pbf_path, locations=True)

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
