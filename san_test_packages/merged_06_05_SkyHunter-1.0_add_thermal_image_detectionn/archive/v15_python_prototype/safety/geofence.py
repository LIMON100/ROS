"""Geofence violation detection (SDD Rev.A.6 §9.3).

Polygon-in-point + minimum distance to boundary, computed once per
1 Hz tick alongside the other safety checks. Two thresholds:

  buffer_m (default 2 m)    — APPROACHING: warn the operator and
                               flag the next planner cycle to steer
                               away.
  hard_stop_m (default 0.5) — VIOLATION: cmd_vel forced to (0, 0)
                               and locomotion to Stand mode.

dev_override (PIN-authenticated only) skips the entire check. The
override is intentionally explicit: a developer in the office shouldn't
be able to disable the fence without a recorded PIN auth event in the
audit log.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from enum import IntEnum
from typing import List, Tuple


class FenceState(IntEnum):
    SAFE = 0
    APPROACHING = 1     # within buffer_m of boundary
    VIOLATION = 2       # within hard_stop_m or outside polygon


@dataclass
class GeofenceEvent:
    state: FenceState
    distance_to_boundary_m: float    # negative when outside polygon
    pose_lat: float
    pose_lon: float
    message: str


class Geofence:
    """Polygon-in-point check at 1 Hz."""

    EARTH_R = 6_378_137.0   # m, WGS84 mean

    def __init__(self,
                 polygon: List[Tuple[float, float]],
                 buffer_m: float = 2.0,
                 hard_stop_m: float = 0.5,
                 dev_override: bool = False):
        if len(polygon) < 3:
            raise ValueError("polygon needs ≥ 3 points")
        self.polygon = list(polygon)
        self.buffer_m = float(buffer_m)
        self.hard_stop_m = float(hard_stop_m)
        self.dev_override = bool(dev_override)

    def check(self, lat: float, lon: float) -> GeofenceEvent:
        if self.dev_override:
            return GeofenceEvent(
                state=FenceState.SAFE,
                distance_to_boundary_m=999.0,
                pose_lat=lat, pose_lon=lon,
                message="geofence overridden (dev_mode)",
            )

        inside = self._point_in_polygon(lat, lon)
        dist = self._min_distance_to_boundary(lat, lon)

        if not inside:
            return GeofenceEvent(
                state=FenceState.VIOLATION,
                distance_to_boundary_m=-dist,    # signed: negative = outside
                pose_lat=lat, pose_lon=lon,
                message=f"OUTSIDE fence by {dist:.1f}m",
            )
        if dist <= self.hard_stop_m:
            return GeofenceEvent(
                state=FenceState.VIOLATION,
                distance_to_boundary_m=dist,
                pose_lat=lat, pose_lon=lon,
                message=f"hard_stop within {dist:.2f}m of boundary",
            )
        if dist <= self.buffer_m:
            return GeofenceEvent(
                state=FenceState.APPROACHING,
                distance_to_boundary_m=dist,
                pose_lat=lat, pose_lon=lon,
                message=f"approaching boundary ({dist:.1f}m)",
            )
        return GeofenceEvent(
            state=FenceState.SAFE,
            distance_to_boundary_m=dist,
            pose_lat=lat, pose_lon=lon,
            message="",
        )

    # ─── Geometry helpers ───
    def _point_in_polygon(self, lat: float, lon: float) -> bool:
        """Ray-casting algorithm in lat/lon space (small areas only)."""
        n = len(self.polygon)
        inside = False
        j = n - 1
        for i in range(n):
            yi, xi = self.polygon[i]
            yj, xj = self.polygon[j]
            crosses = ((yi > lat) != (yj > lat)) and (
                lon < (xj - xi) * (lat - yi) / (yj - yi + 1e-12) + xi)
            if crosses:
                inside = not inside
            j = i
        return inside

    def _min_distance_to_boundary(self,
                                   lat: float,
                                   lon: float) -> float:
        n = len(self.polygon)
        min_d = float("inf")
        for i in range(n):
            la1, lo1 = self.polygon[i]
            la2, lo2 = self.polygon[(i + 1) % n]
            d = self._distance_to_segment(lat, lon, la1, lo1, la2, lo2)
            if d < min_d:
                min_d = d
        return min_d

    def _distance_to_segment(self,
                              lat: float, lon: float,
                              la1: float, lo1: float,
                              la2: float, lo2: float) -> float:
        """Local-planar distance from (lat, lon) to segment.

        Valid for sub-kilometre fences — switch to spherical math for
        bigger polygons.
        """
        cos_lat = math.cos(math.radians(la1))
        x = math.radians(lon - lo1) * self.EARTH_R * cos_lat
        y = math.radians(lat - la1) * self.EARTH_R
        x2 = math.radians(lo2 - lo1) * self.EARTH_R * cos_lat
        y2 = math.radians(la2 - la1) * self.EARTH_R
        seg_len2 = x2 * x2 + y2 * y2
        if seg_len2 < 1e-9:
            return math.hypot(x, y)
        t = max(0.0, min(1.0, (x * x2 + y * y2) / seg_len2))
        proj_x = t * x2
        proj_y = t * y2
        return math.hypot(x - proj_x, y - proj_y)
