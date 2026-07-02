"""Tests for /hub/slam/aggregated_map (AggregatedMap + dispatcher + codec)."""
import numpy as np
import pytest

from core.messages import AggregatedMap, Pose2D
from mapping.aggregated_map import (
    DEFAULT_PERIOD_S,
    MAX_PERIOD_S,
    AggregatedMapDispatcher,
    AggregatedMapInput,
    decode_aggregated_map,
    decode_png_to_grid,
    encode_grid_to_png,
)

try:
    from PIL import Image  # noqa: F401
    _PILLOW = True
except ImportError:
    _PILLOW = False

needs_pillow = pytest.mark.skipif(
    not _PILLOW, reason="Pillow not installed")


# ─── AggregatedMap dataclass ───────────────────────────────────────────

def test_validate_accepts_well_formed():
    msg = AggregatedMap(
        sequence=1,
        occupancy_grid_png=b"\x89PNG\r\n\x1a\n",   # header-only stub OK for validate()
        origin=Pose2D(1.0, 2.0, 0.0),
        resolution_m=0.10,
        width_cells=10, height_cells=10,
        contributing_robots=3,
        timestamp_ms=1_000,
    )
    msg.validate()


def test_validate_rejects_non_positive_resolution():
    with pytest.raises(ValueError):
        AggregatedMap(resolution_m=0.0).validate()
    with pytest.raises(ValueError):
        AggregatedMap(resolution_m=-0.1).validate()


def test_validate_rejects_negative_dims():
    with pytest.raises(ValueError):
        AggregatedMap(width_cells=-1).validate()
    with pytest.raises(ValueError):
        AggregatedMap(height_cells=-1).validate()


def test_validate_rejects_dims_with_empty_payload():
    msg = AggregatedMap(width_cells=10, height_cells=10,
                        occupancy_grid_png=b"")
    with pytest.raises(ValueError):
        msg.validate()


def test_validate_rejects_negative_contributors():
    with pytest.raises(ValueError):
        AggregatedMap(contributing_robots=-1).validate()


def test_is_stale_respects_max_age():
    msg = AggregatedMap(timestamp_ms=1_000)
    assert msg.is_stale(now_ms=5_000, max_age_sec=10.0) is False
    assert msg.is_stale(now_ms=12_000, max_age_sec=10.0) is True


def test_is_stale_zero_max_age_never_stale():
    msg = AggregatedMap(timestamp_ms=0)
    assert msg.is_stale(now_ms=10**10, max_age_sec=0.0) is False


# ─── PNG codec round-trip ──────────────────────────────────────────────

@needs_pillow
def test_png_roundtrip_uint8():
    grid = np.arange(256, dtype=np.uint8).reshape(16, 16)
    png = encode_grid_to_png(grid)
    decoded = decode_png_to_grid(png)
    assert decoded.shape == grid.shape
    assert np.array_equal(decoded, grid)


@needs_pillow
def test_png_roundtrip_float_with_unknown_sentinel():
    # MapTile convention: -1 unknown / 0 free / 1 occupied.
    grid = np.full((4, 4), -1.0, dtype=np.float32)
    grid[0, 0] = 0.0    # free → 0
    grid[1, 1] = 1.0    # occupied → 255
    grid[2, 2] = 0.5    # mid → ~127 (collides with unknown sentinel —
                        # documented in encoder).
    png = encode_grid_to_png(grid)
    decoded = decode_png_to_grid(png)
    assert decoded[0, 0] == 0
    assert decoded[1, 1] == 255
    # The bulk -1 cells round-trip as 127.
    assert decoded[3, 3] == 127


@needs_pillow
def test_decode_aggregated_map_dim_mismatch_raises():
    grid = np.zeros((4, 4), dtype=np.uint8)
    png = encode_grid_to_png(grid)
    msg = AggregatedMap(
        occupancy_grid_png=png,
        width_cells=8, height_cells=8,    # lies — actual PNG is 4x4
        resolution_m=0.1,
    )
    with pytest.raises(ValueError):
        decode_aggregated_map(msg)


@needs_pillow
def test_decode_aggregated_map_returns_origin_and_resolution():
    grid = np.full((3, 5), 7, dtype=np.uint8)
    png = encode_grid_to_png(grid)
    msg = AggregatedMap(
        occupancy_grid_png=png,
        origin=Pose2D(1.5, 2.5, 0.0),
        resolution_m=0.25,
        width_cells=5, height_cells=3,
    )
    decoded, origin, res = decode_aggregated_map(msg)
    assert decoded.shape == (3, 5)
    assert origin.x == 1.5 and origin.y == 2.5
    assert res == 0.25


# ─── Dispatcher ────────────────────────────────────────────────────────

def _inputs():
    return AggregatedMapInput(
        grid=np.zeros((4, 4), dtype=np.uint8),
        origin=Pose2D(0.0, 0.0, 0.0),
        resolution_m=0.1,
        contributing_robots=2,
    )


@needs_pillow
def test_dispatcher_first_call_emits():
    d = AggregatedMapDispatcher(period_s=DEFAULT_PERIOD_S)
    msg = d.due_message(now_ms=0, inputs=_inputs())
    assert msg is not None
    assert msg.sequence == 1
    assert msg.contributing_robots == 2
    assert msg.height_cells == 4 and msg.width_cells == 4


@needs_pillow
def test_dispatcher_silent_inside_period():
    d = AggregatedMapDispatcher(period_s=DEFAULT_PERIOD_S)
    d.due_message(now_ms=0, inputs=_inputs())
    # 1 s later — well inside the 30 s period
    assert d.due_message(now_ms=1_000, inputs=_inputs()) is None


@needs_pillow
def test_dispatcher_emits_after_period():
    d = AggregatedMapDispatcher(period_s=DEFAULT_PERIOD_S)
    d.due_message(now_ms=0, inputs=_inputs())
    period_ms = int(DEFAULT_PERIOD_S * 1000)
    msg = d.due_message(now_ms=period_ms, inputs=_inputs())
    assert msg is not None
    assert msg.sequence == 2


def test_dispatcher_returns_none_when_no_inputs():
    d = AggregatedMapDispatcher()
    assert d.due_message(now_ms=0, inputs=None) is None


def test_dispatcher_accepts_v13_default():
    """SAN v1.3 §9 — 5 s is the production cadence; must not be clamped
    up to the legacy 30 s floor.
    """
    d = AggregatedMapDispatcher(period_s=5.0)
    assert d.period_s == 5.0


def test_dispatcher_clamps_period_below_min():
    """Sub-second cadence is rejected — PNG round-trip dominates."""
    d = AggregatedMapDispatcher(period_s=0.1)
    # MIN_PERIOD_S = 1.0 in v1.3
    assert d.period_s == 1.0


def test_dispatcher_clamps_period_above_60s():
    d = AggregatedMapDispatcher(period_s=300.0)
    assert d.period_s == MAX_PERIOD_S


@needs_pillow
def test_event_message_force_publishes_and_resets_timer():
    d = AggregatedMapDispatcher()
    d.due_message(now_ms=0, inputs=_inputs())
    forced = d.event_message(now_ms=500, inputs=_inputs())
    assert forced.sequence == 2
    # Next periodic call inside 30 s of the *forced* publish is silent.
    assert d.due_message(now_ms=1_000, inputs=_inputs()) is None
