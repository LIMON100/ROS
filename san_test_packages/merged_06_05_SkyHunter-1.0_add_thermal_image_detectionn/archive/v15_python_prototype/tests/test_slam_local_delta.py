"""Tests for SLAMLocalDelta (replaces 1 Hz SLAMDelta with 30–60 s aggregates)."""
import pytest

from core.messages import Pose2D, SLAMLocalDelta


def _delta(**overrides):
    base = dict(
        sequence=1,
        robot_id="follower1",
        occupancy_grid_delta_png=b"\x89PNG...",
        origin=Pose2D(x=10.0, y=-5.0, theta_rad=0.0),
        resolution_m=0.10,
        coverage_start_ms=1_700_000_000_000,
        coverage_end_ms=1_700_000_030_000,
        timestamp_ms=1_700_000_030_500,
    )
    base.update(overrides)
    return SLAMLocalDelta(**base)


def test_validate_accepts_defaults_with_filled_robot_id():
    # Default ctor has empty robot_id → must reject.
    with pytest.raises(ValueError):
        SLAMLocalDelta().validate()


def test_validate_accepts_well_formed_delta():
    _delta().validate()


def test_validate_rejects_non_positive_resolution():
    with pytest.raises(ValueError):
        _delta(resolution_m=0.0).validate()
    with pytest.raises(ValueError):
        _delta(resolution_m=-0.1).validate()


def test_validate_rejects_negative_coverage_timestamp():
    with pytest.raises(ValueError):
        _delta(coverage_start_ms=-1).validate()


def test_validate_rejects_end_before_start():
    with pytest.raises(ValueError):
        _delta(coverage_start_ms=2_000, coverage_end_ms=1_000).validate()


def test_validate_rejects_empty_robot_id():
    with pytest.raises(ValueError):
        _delta(robot_id="").validate()


def test_coverage_duration_ms_returns_window_length():
    d = _delta(coverage_start_ms=10_000, coverage_end_ms=40_000)
    assert d.coverage_duration_ms() == 30_000


def test_coverage_duration_ms_zero_when_endpoints_equal():
    d = _delta(coverage_start_ms=5_000, coverage_end_ms=5_000)
    assert d.coverage_duration_ms() == 0
