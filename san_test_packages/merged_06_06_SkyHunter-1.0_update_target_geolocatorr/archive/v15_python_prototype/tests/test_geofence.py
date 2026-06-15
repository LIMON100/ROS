"""Tests for geofence (P1-18, SDD Rev.A.6 §9.3)."""
from __future__ import annotations

import pytest

from safety.geofence import FenceState, Geofence

# Square fence in Seoul-ish area (~88 m × 111 m at lat 37.5).
# 0.0001° lon ≈ 8.85 m at lat 37.5; 0.0001° lat ≈ 11.1 m anywhere.
SQUARE = [
    (37.5000, 127.0000),
    (37.5000, 127.0010),       # ~88 m east
    (37.5010, 127.0010),       # ~111 m north
    (37.5010, 127.0000),
]


def test_polygon_min_3_points():
    with pytest.raises(ValueError):
        Geofence([(37.5, 127.0), (37.51, 127.0)])    # 2 points


def test_inside_safe():
    f = Geofence(SQUARE)
    ev = f.check(37.5005, 127.0005)                  # center
    assert ev.state == FenceState.SAFE


def test_outside_violation():
    f = Geofence(SQUARE)
    ev = f.check(37.5020, 127.0020)                  # outside NE
    assert ev.state == FenceState.VIOLATION
    assert "OUTSIDE" in ev.message


def test_approaching_within_buffer():
    """About 1 m from the east boundary with buffer=2 m → APPROACHING.

    east_edge = 127.0010; ~1 m west = 127.0010 − 0.0000113.
    """
    f = Geofence(SQUARE, buffer_m=2.0, hard_stop_m=0.5)
    ev = f.check(37.5005, 127.0010 - 0.0000113)
    assert ev.state == FenceState.APPROACHING


def test_hard_stop_within_threshold():
    """About 0.3 m from boundary → VIOLATION (hard_stop)."""
    f = Geofence(SQUARE, buffer_m=2.0, hard_stop_m=0.5)
    ev = f.check(37.5005, 127.0010 - 0.0000034)      # ~0.3 m inside
    assert ev.state == FenceState.VIOLATION
    assert "hard_stop" in ev.message.lower()


def test_dev_override_skips_check():
    f = Geofence(SQUARE, dev_override=True)
    ev = f.check(37.5020, 127.0020)                  # would be outside
    assert ev.state == FenceState.SAFE
    assert "dev_mode" in ev.message.lower()


def test_distance_field_set_for_inside():
    f = Geofence(SQUARE)
    ev = f.check(37.5005, 127.0005)
    assert ev.distance_to_boundary_m > 0


def test_outside_distance_is_signed_negative():
    f = Geofence(SQUARE)
    ev = f.check(37.5020, 127.0005)                  # north of fence
    assert ev.state == FenceState.VIOLATION
    assert ev.distance_to_boundary_m < 0
