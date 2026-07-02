"""Tests for mapping.slam_aggregator — Hub UGV SBC#1 SLAM fusion."""
import numpy as np
import pytest

from core.messages import Pose2D, SLAMLocalDelta
from mapping.aggregated_map import (
    decode_png_to_grid,
    encode_grid_to_png,
)
from mapping.slam_aggregator import (
    MODE_DEFAULT,
    MODE_NARROW,
    MODE_OBSTACLE,
    MODE_WIDE,
    PERIOD_BY_MODE,
    UNKNOWN_CELL,
    SlamAggregator,
    merge_deltas,
    optimize_pose_graph,
)

# ─── fixtures / helpers ────────────────────────────────────────────────

def _delta_from_grid(
    robot_id: str,
    grid: np.ndarray,
    origin_x: float = 0.0,
    origin_y: float = 0.0,
    resolution_m: float = 0.10,
    timestamp_ms: int = 1_700_000_000_000,
):
    """Timestamp defaults to a plausible epoch-ms so coverage_start_ms
    stays non-negative even after the 30 s window subtraction."""
    return SLAMLocalDelta(
        sequence=1,
        robot_id=robot_id,
        occupancy_grid_delta_png=encode_grid_to_png(grid),
        origin=Pose2D(x=origin_x, y=origin_y, theta_rad=0.0),
        resolution_m=resolution_m,
        coverage_start_ms=max(timestamp_ms - 30_000, 0),
        coverage_end_ms=timestamp_ms,
        timestamp_ms=timestamp_ms,
    )


def _checkerboard(h: int, w: int, hot: int = 255) -> np.ndarray:
    g = np.full((h, w), UNKNOWN_CELL, dtype=np.uint8)
    g[::2, ::2] = hot
    return g


# ─── mode + period table ───────────────────────────────────────────────

def test_period_table_matches_spec():
    # SAN v1.3 §9 cadence — Hub UGV dual-SBC reduces the legacy v1.1
    # 30 s default to 5 s. The mode ratios are preserved 6:1:0.5:0.2.
    assert PERIOD_BY_MODE[MODE_WIDE]     == 30.0
    assert PERIOD_BY_MODE[MODE_DEFAULT]  == 5.0
    assert PERIOD_BY_MODE[MODE_NARROW]   == 2.5
    assert PERIOD_BY_MODE[MODE_OBSTACLE] == 1.0


def test_constructor_rejects_unknown_mode():
    with pytest.raises(ValueError):
        SlamAggregator(mode="bogus")


def test_constructor_rejects_zero_min_contributors():
    with pytest.raises(ValueError):
        SlamAggregator(min_contributors=0)


def test_default_mode_period_5s():
    """SAN v1.3 §9 — default cadence is 5 s, not the v1.1 30 s."""
    agg = SlamAggregator()
    assert agg.mode == MODE_DEFAULT
    assert agg.period_s == 5.0


def test_set_mode_updates_period():
    agg = SlamAggregator()
    agg.set_mode(MODE_NARROW)
    assert agg.period_s == 2.5
    agg.set_mode(MODE_OBSTACLE)
    assert agg.period_s == 1.0
    agg.set_mode(MODE_WIDE)
    assert agg.period_s == 30.0


def test_set_mode_rejects_unknown():
    agg = SlamAggregator()
    with pytest.raises(ValueError):
        agg.set_mode("bogus")


# ─── ingest / forget ───────────────────────────────────────────────────

def test_ingest_caches_delta_per_robot():
    agg = SlamAggregator()
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.ingest(_delta_from_grid("follower2", _checkerboard(4, 4)))
    assert agg.contributors == 2


def test_ingest_replaces_latest_per_robot():
    agg = SlamAggregator()
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4),
                                 timestamp_ms=1_000))
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4),
                                 timestamp_ms=2_000))
    # Same robot — should only count once.
    assert agg.contributors == 1


def test_ingest_rejects_invalid_delta():
    # empty robot_id is invalid per SLAMLocalDelta.validate().
    agg = SlamAggregator()
    bad = _delta_from_grid("", _checkerboard(4, 4))
    with pytest.raises(ValueError):
        agg.ingest(bad)


def test_forget_drops_cached_delta():
    agg = SlamAggregator()
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.ingest(_delta_from_grid("follower2", _checkerboard(4, 4)))
    assert agg.forget("follower1") is True
    assert agg.contributors == 1
    # Idempotent — second call returns False.
    assert agg.forget("follower1") is False


# ─── due_aggregate gating ──────────────────────────────────────────────

def test_due_aggregate_returns_none_below_min_contributors():
    agg = SlamAggregator(min_contributors=2)
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    assert agg.due_aggregate(now_ms=10_000_000) is None


def test_due_aggregate_first_call_publishes_immediately():
    agg = SlamAggregator()
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.ingest(_delta_from_grid("follower2", _checkerboard(4, 4)))
    msg = agg.due_aggregate(now_ms=10_000_000)
    assert msg is not None
    assert msg.contributing_robots == 2


def test_due_aggregate_silent_within_period():
    """SAN v1.3 §9 — default cadence 5 s; gate holds at 4 999 ms."""
    agg = SlamAggregator(mode=MODE_DEFAULT)  # 5 s period
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.ingest(_delta_from_grid("follower2", _checkerboard(4, 4)))
    assert agg.due_aggregate(now_ms=10_000_000) is not None
    # 4 999 ms later — still inside the v1.3 period, no message.
    assert agg.due_aggregate(now_ms=10_004_999) is None


def test_due_aggregate_fires_again_after_period():
    agg = SlamAggregator(mode=MODE_DEFAULT)
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.ingest(_delta_from_grid("follower2", _checkerboard(4, 4)))
    assert agg.due_aggregate(now_ms=10_000_000) is not None
    # 5 s later — exactly on the boundary, must fire.
    assert agg.due_aggregate(now_ms=10_005_000) is not None


def test_due_aggregate_narrow_mode_short_period():
    """SAN v1.3 §9 — narrow cadence 2.5 s (was 15 s in v1.1)."""
    agg = SlamAggregator(mode=MODE_NARROW)  # 2.5 s
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.ingest(_delta_from_grid("follower2", _checkerboard(4, 4)))
    assert agg.due_aggregate(now_ms=0) is not None
    assert agg.due_aggregate(now_ms=2_499) is None
    assert agg.due_aggregate(now_ms=2_500) is not None


# ─── force_event ───────────────────────────────────────────────────────

def test_force_event_publishes_immediately_mid_period():
    agg = SlamAggregator(mode=MODE_DEFAULT)
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.ingest(_delta_from_grid("follower2", _checkerboard(4, 4)))
    agg.due_aggregate(now_ms=10_000_000)
    # 1 s after publish — would normally be silent.
    agg.force_event()
    msg = agg.due_aggregate(now_ms=10_001_000)
    assert msg is not None


def test_force_event_auto_clears_after_publish():
    agg = SlamAggregator(mode=MODE_DEFAULT)
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.ingest(_delta_from_grid("follower2", _checkerboard(4, 4)))
    agg.force_event()
    assert agg.due_aggregate(now_ms=1_000) is not None
    # Force-event flag should not "stick" — next mid-period call silent.
    assert agg.due_aggregate(now_ms=1_500) is None


def test_force_event_still_requires_min_contributors():
    # force_event alone doesn't bypass the contributor floor.
    agg = SlamAggregator(min_contributors=2)
    agg.ingest(_delta_from_grid("follower1", _checkerboard(4, 4)))
    agg.force_event()
    assert agg.due_aggregate(now_ms=0) is None


# ─── merge correctness ────────────────────────────────────────────────

def test_merge_deltas_empty_returns_none():
    assert merge_deltas([], resolution_m=0.1) is None


def test_merge_deltas_single_delta_passthrough():
    g1 = np.full((4, 4), 200, dtype=np.uint8)
    d1 = _delta_from_grid("follower1", g1)
    agg = SlamAggregator(min_contributors=1)
    agg.ingest(d1)
    msg = agg.due_aggregate(now_ms=0)
    assert msg is not None
    grid = decode_png_to_grid(msg.occupancy_grid_png)
    assert grid.shape == (4, 4)
    assert np.all(grid == 200)


def test_merge_deltas_disjoint_grids_canvas_covers_bbox():
    # Two 4x4 grids, second offset by +0.4 m in x at 0.1 m/cell → 4 cells
    # right. Canvas width should be 8, height 4.
    g_left  = np.full((4, 4), 100, dtype=np.uint8)
    g_right = np.full((4, 4), 200, dtype=np.uint8)
    agg = SlamAggregator()
    agg.ingest(_delta_from_grid("a", g_left, origin_x=0.0, origin_y=0.0))
    agg.ingest(_delta_from_grid("b", g_right, origin_x=0.4, origin_y=0.0))
    msg = agg.due_aggregate(now_ms=0)
    grid = decode_png_to_grid(msg.occupancy_grid_png)
    assert grid.shape == (4, 8)
    # Left half from a, right half from b — no overlap.
    assert np.all(grid[:, :4] == 100)
    assert np.all(grid[:, 4:] == 200)


def test_merge_deltas_overlapping_picks_max():
    # Two 4x4 grids fully overlapping; per-cell max wins.
    g_a = np.full((4, 4), 100, dtype=np.uint8)
    g_b = np.full((4, 4), 200, dtype=np.uint8)
    g_b[1, 1] = 50    # one cell where a (100) wins
    agg = SlamAggregator()
    agg.ingest(_delta_from_grid("a", g_a))
    agg.ingest(_delta_from_grid("b", g_b))
    msg = agg.due_aggregate(now_ms=0)
    grid = decode_png_to_grid(msg.occupancy_grid_png)
    assert grid.shape == (4, 4)
    assert grid[1, 1] == 100        # a wins this cell
    assert np.all(grid[grid != 100] == 200)


def test_merge_deltas_origin_at_min_corner():
    # Origin of the merged grid is the (min_x, min_y) of all bboxes.
    g = np.full((2, 2), 200, dtype=np.uint8)
    agg = SlamAggregator()
    agg.ingest(_delta_from_grid("a", g, origin_x=-1.0, origin_y=-0.5))
    agg.ingest(_delta_from_grid("b", g, origin_x=+1.0, origin_y=+0.5))
    msg = agg.due_aggregate(now_ms=0)
    assert msg.origin.x == pytest.approx(-1.0)
    assert msg.origin.y == pytest.approx(-0.5)


def test_optimize_pose_graph_v1_is_passthrough():
    # v1 stub returns the same array; verify identity.
    g = np.full((3, 3), 123, dtype=np.uint8)
    out = optimize_pose_graph(g)
    assert np.array_equal(out, g)


# ─── interaction with mode change ──────────────────────────────────────

def test_mode_change_uses_new_period_after_next_publish():
    """SAN v1.3 §9 — default 5 s → wide 30 s extends the gate.

    Default→wide is the right transition to show "new period applies"
    because wide (30 s) is longer than default (5 s); a default→obstacle
    transition would shorten the gate and complicate the assertion.
    """
    agg = SlamAggregator(mode=MODE_DEFAULT)
    agg.ingest(_delta_from_grid("a", _checkerboard(2, 2)))
    agg.ingest(_delta_from_grid("b", _checkerboard(2, 2)))
    agg.due_aggregate(now_ms=0)
    # Switch to wide (30 s). Should remain silent until 30 s, not 5 s.
    agg.set_mode(MODE_WIDE)
    assert agg.due_aggregate(now_ms=10_000) is None
    assert agg.due_aggregate(now_ms=30_000) is not None


def test_mode_change_alone_does_not_force_publish():
    # set_mode is configuration only — must not trigger an immediate emit.
    agg = SlamAggregator(mode=MODE_DEFAULT)
    agg.ingest(_delta_from_grid("a", _checkerboard(2, 2)))
    agg.ingest(_delta_from_grid("b", _checkerboard(2, 2)))
    agg.due_aggregate(now_ms=0)
    agg.set_mode(MODE_NARROW)
    # 1 s later — narrow period is 2.5 s, so still silent.
    assert agg.due_aggregate(now_ms=1_000) is None
