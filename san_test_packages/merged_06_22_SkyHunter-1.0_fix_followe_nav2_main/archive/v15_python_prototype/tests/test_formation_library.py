"""Tests for 9 formations library (P2-1, SDD §7.2)."""
from __future__ import annotations

import math

import pytest

from mission.formation_library import FormationLibrary, FormationType


def test_all_9_formation_types_defined():
    expected = {"column", "line", "v_shape", "diamond",
                "echelon_left", "echelon_right",
                "box", "vee_inverted", "free_spread"}
    actual = {ft.value for ft in FormationType}
    assert actual == expected
    assert len(actual) == 9


# ───── Column ─────
def test_column_n3_d5():
    offs = FormationLibrary.column(3, 5.0)
    assert offs == [(-5.0, 0.0), (-10.0, 0.0), (-15.0, 0.0)]


def test_column_n5_d3():
    offs = FormationLibrary.column(5, 3.0)
    assert len(offs) == 5
    for i, (x, y) in enumerate(offs, 1):
        assert abs(x - (-i * 3.0)) < 1e-9
        assert y == 0.0


# ───── Line ─────
def test_line_alternates_sides():
    offs = FormationLibrary.line(4, 5.0)
    assert len(offs) == 4
    assert offs[0][1] > 0
    assert offs[1][1] < 0


# ───── V-shape ─────
def test_v_shape_n2_symmetric():
    offs = FormationLibrary.v_shape(2, 5.0, theta_deg=90)
    assert abs(offs[0][0] - offs[1][0]) < 1e-9
    assert offs[0][1] == -offs[1][1]


def test_v_shape_90_deg_at_d5():
    offs = FormationLibrary.v_shape(2, 5.0, theta_deg=90)
    expected_x = -5.0 * math.cos(math.radians(45))
    expected_y = 5.0 * math.sin(math.radians(45))
    assert abs(offs[0][0] - expected_x) < 1e-6
    assert abs(abs(offs[0][1]) - expected_y) < 1e-6


# ───── Diamond ─────
def test_diamond_n4_cardinal_points():
    offs = FormationLibrary.diamond(4, 5.0)
    assert len(offs) == 4
    distances = [math.hypot(x, y) for x, y in offs]
    for d in distances:
        assert abs(d - 5.0) < 1e-6


# ───── Echelon ─────
def test_echelon_left_increasing_distance():
    offs = FormationLibrary.echelon_left(3, 5.0)
    for i in range(2):
        x_i, y_i = offs[i]
        x_n, y_n = offs[i + 1]
        assert x_n < x_i
        assert y_n > y_i


def test_echelon_right_mirrors_left():
    left = FormationLibrary.echelon_left(3, 5.0)
    right = FormationLibrary.echelon_right(3, 5.0)
    for (le, ri) in zip(left, right, strict=True):
        assert abs(le[0] - ri[0]) < 1e-6
        assert abs(le[1] + ri[1]) < 1e-6


# ───── Box ─────
def test_box_n4_corners_at_distance():
    offs = FormationLibrary.box(4, 5.0)
    assert len(offs) == 4
    expected_d = 5.0 * math.sqrt(2) / 2.0
    for x, y in offs:
        assert abs(math.hypot(x, y) - expected_d) < 1e-6


# ───── Vee Inverted ─────
def test_vee_inverted_opens_forward():
    offs = FormationLibrary.vee_inverted(2, 5.0, theta_deg=90)
    for x, _ in offs:
        assert x > 0


def test_vee_inverted_vs_v_shape_x_negation():
    v = FormationLibrary.v_shape(2, 5.0, theta_deg=90)
    vi = FormationLibrary.vee_inverted(2, 5.0, theta_deg=90)
    for ((x_v, y_v), (x_vi, y_vi)) in zip(v, vi, strict=True):
        assert abs(x_v + x_vi) < 1e-6
        assert abs(y_v - y_vi) < 1e-6


# ───── Free Spread ─────
def test_free_spread_within_radius():
    offs = FormationLibrary.free_spread(5, 5.0, area_radius=10.0)
    for x, y in offs:
        assert math.hypot(x, y) <= 10.0
        assert math.hypot(x, y) >= 5.0


def test_free_spread_deterministic_with_seed():
    a = FormationLibrary.free_spread(5, 5.0, seed=42)
    b = FormationLibrary.free_spread(5, 5.0, seed=42)
    assert a == b
    c = FormationLibrary.free_spread(5, 5.0, seed=99)
    assert a != c


# ───── Dispatch ─────
def test_compute_dispatches_correctly():
    offs = FormationLibrary.compute(FormationType.V_SHAPE, 4,
                                    d_m=5.0, theta_deg=90)
    assert len(offs) == 4


def test_compute_zero_followers():
    offs = FormationLibrary.compute(FormationType.V_SHAPE, 0)
    assert offs == []


def test_compute_unknown_type_raises():
    with pytest.raises(ValueError):
        FormationLibrary.compute("unknown_type", 4)


def test_to_planner_dict_format():
    offs = FormationLibrary.compute(FormationType.COLUMN, 3, d_m=5.0)
    result = FormationLibrary.to_planner_dict(offs, start_id=1)
    assert isinstance(result, dict)
    assert set(result.keys()) == {1, 2, 3}
    assert result[1] == (-5.0, 0.0)


# ───── Integration with PredictivePlanner ─────
def test_integration_with_predictive_planner():
    from mission.predictive_planner import PredictivePlanner
    offs = FormationLibrary.compute(FormationType.V_SHAPE, 4,
                                    d_m=5.0, theta_deg=90)
    planner_dict = FormationLibrary.to_planner_dict(offs)
    pp = PredictivePlanner(formation_offsets=planner_dict)
    targets = pp.compute_follower_targets(
        leader_pred=(0.0, 0.0),
        leader_heading_rad=0.0,
        valid_until_ts=0.0,
    )
    assert len(targets) == 4
