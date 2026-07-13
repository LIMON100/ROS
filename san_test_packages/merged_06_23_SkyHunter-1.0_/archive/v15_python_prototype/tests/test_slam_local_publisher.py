"""Tests for mapping.slam_local_publisher — follower-side SLAMLocalDelta."""
import numpy as np
import pytest

from core.messages import Pose2D
from mapping.aggregated_map import decode_png_to_grid
from mapping.slam_local_publisher import (
    MODE_DEFAULT,
    MODE_NARROW,
    MODE_OBSTACLE,
    MODE_WIDE,
    PERIOD_BY_MODE,
    UNKNOWN_CELL,
    SlamLocalPublisher,
)

# ─── helpers ───────────────────────────────────────────────────────────

_BOOT_MS = 1_700_000_000_000


def _grid(shape, fill=UNKNOWN_CELL, occupied_cells=()):
    g = np.full(shape, fill, dtype=np.uint8)
    for (r, c) in occupied_cells:
        g[r, c] = 255
    return g


def _origin(x=0.0, y=0.0):
    return Pose2D(x=x, y=y, theta_rad=0.0)


# ─── construction + period table ───────────────────────────────────────

def test_period_table_matches_aggregator():
    # SAN v1.3 §9 — publisher periods stay locked to the aggregator
    # table; mismatch would let a follower publish on a faster/slower
    # cadence than the Hub fuses on.
    assert PERIOD_BY_MODE[MODE_WIDE]     == 30.0
    assert PERIOD_BY_MODE[MODE_DEFAULT]  == 5.0
    assert PERIOD_BY_MODE[MODE_NARROW]   == 2.5
    assert PERIOD_BY_MODE[MODE_OBSTACLE] == 1.0


def test_constructor_rejects_empty_robot_id():
    with pytest.raises(ValueError):
        SlamLocalPublisher(robot_id="")


def test_constructor_rejects_unknown_mode():
    with pytest.raises(ValueError):
        SlamLocalPublisher(robot_id="r1", mode="bogus")


def test_default_mode_is_5s():
    """SAN v1.3 §9 — was 30 s in v1.1."""
    p = SlamLocalPublisher(robot_id="r1")
    assert p.mode == MODE_DEFAULT
    assert p.period_sec == 5.0


def test_explicit_period_sec_overrides_mode_default():
    p = SlamLocalPublisher(robot_id="r1", period_sec=42.0)
    assert p.period_sec == 42.0


# ─── mode + period switching ───────────────────────────────────────────

def test_set_mode_updates_period():
    p = SlamLocalPublisher(robot_id="r1")
    p.set_mode(MODE_NARROW)
    assert p.period_sec == 2.5
    p.set_mode(MODE_OBSTACLE)
    assert p.period_sec == 1.0


def test_set_mode_rejects_unknown():
    p = SlamLocalPublisher(robot_id="r1")
    with pytest.raises(ValueError):
        p.set_mode("bogus")


def test_set_period_sec_overrides_named_mode():
    p = SlamLocalPublisher(robot_id="r1", mode=MODE_DEFAULT)
    p.set_period_sec(7.5)
    assert p.period_sec == 7.5
    # Mode label is unaffected — just the period changed.
    assert p.mode == MODE_DEFAULT


def test_set_period_sec_rejects_non_positive():
    p = SlamLocalPublisher(robot_id="r1")
    with pytest.raises(ValueError):
        p.set_period_sec(0)
    with pytest.raises(ValueError):
        p.set_period_sec(-1)


# ─── ingest + accumulation ─────────────────────────────────────────────

def test_no_publish_before_first_ingest():
    p = SlamLocalPublisher(robot_id="r1")
    assert p.due_publish(now_ms=_BOOT_MS) is None


def test_first_ingest_then_due_publishes_immediately():
    # "Pre-roll" behavior — the first publish fires as soon as there's
    # something to send, without waiting an extra period.
    p = SlamLocalPublisher(robot_id="r1")
    p.ingest_local_map(_grid((4, 4)), _origin(), 0.10, now_ms=_BOOT_MS)
    msg = p.due_publish(now_ms=_BOOT_MS)
    assert msg is not None
    assert msg.robot_id == "r1"
    assert msg.coverage_start_ms == _BOOT_MS
    assert msg.coverage_end_ms == _BOOT_MS


def test_accumulation_preserves_obstacle_across_window():
    # Two ingests in the same window; the obstacle from the FIRST tick
    # must survive into the published delta even though the SECOND tick
    # didn't observe it.
    p = SlamLocalPublisher(robot_id="r1")
    g1 = _grid((4, 4), occupied_cells=[(1, 1)])
    g2 = _grid((4, 4))    # all unknown — no obstacle reported this tick
    p.ingest_local_map(g1, _origin(), 0.10, now_ms=_BOOT_MS)
    p.ingest_local_map(g2, _origin(), 0.10, now_ms=_BOOT_MS + 1_000)
    msg = p.due_publish(now_ms=_BOOT_MS + 1_000)
    decoded = decode_png_to_grid(msg.occupancy_grid_delta_png)
    assert decoded[1, 1] == 255


def test_accumulation_overlay_picks_max():
    # First tick: cell (0,0) free (0). Second: same cell occupied (255).
    # Per-cell max wins → published cell is 255.
    p = SlamLocalPublisher(robot_id="r1")
    g1 = np.full((2, 2), 0, dtype=np.uint8)
    g2 = np.full((2, 2), UNKNOWN_CELL, dtype=np.uint8)
    g2[0, 0] = 255
    p.ingest_local_map(g1, _origin(), 0.10, now_ms=_BOOT_MS)
    p.ingest_local_map(g2, _origin(), 0.10, now_ms=_BOOT_MS + 1_000)
    msg = p.due_publish(now_ms=_BOOT_MS + 1_000)
    decoded = decode_png_to_grid(msg.occupancy_grid_delta_png)
    assert decoded[0, 0] == 255


def test_geometry_change_resets_canvas():
    # Local map grew between ticks (different shape) — the previous
    # canvas is discarded since the cells no longer align.
    p = SlamLocalPublisher(robot_id="r1")
    g_small = _grid((2, 2), occupied_cells=[(0, 0)])
    g_big   = _grid((4, 4))   # all unknown
    p.ingest_local_map(g_small, _origin(), 0.10, now_ms=_BOOT_MS)
    p.ingest_local_map(g_big,   _origin(), 0.10, now_ms=_BOOT_MS + 1_000)
    assert p.stats["geometry_resets"] == 1
    msg = p.due_publish(now_ms=_BOOT_MS + 1_000)
    decoded = decode_png_to_grid(msg.occupancy_grid_delta_png)
    # The earlier obstacle is gone (canvas was reset to the bigger grid).
    assert decoded.shape == (4, 4)
    assert decoded[0, 0] == UNKNOWN_CELL


def test_origin_change_resets_canvas():
    # Same shape + resolution but a different origin → reset; the new
    # patch's cells aren't aligned with the old canvas.
    p = SlamLocalPublisher(robot_id="r1")
    p.ingest_local_map(_grid((2, 2), occupied_cells=[(0, 0)]),
                       _origin(x=0.0), 0.10, now_ms=_BOOT_MS)
    p.ingest_local_map(_grid((2, 2)),
                       _origin(x=5.0), 0.10, now_ms=_BOOT_MS + 1_000)
    assert p.stats["geometry_resets"] == 1


# ─── period gating ─────────────────────────────────────────────────────

def test_publish_silent_within_period_after_first():
    """SAN v1.3 §9 — default cadence 5 s; 4 999 ms is still inside."""
    p = SlamLocalPublisher(robot_id="r1", mode=MODE_DEFAULT)  # 5 s
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10, now_ms=_BOOT_MS)
    p.due_publish(now_ms=_BOOT_MS)
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10,
                       now_ms=_BOOT_MS + 4_999)
    assert p.due_publish(now_ms=_BOOT_MS + 4_999) is None


def test_publish_fires_at_period_boundary():
    """SAN v1.3 §9 — default boundary 5 s exactly."""
    p = SlamLocalPublisher(robot_id="r1", mode=MODE_DEFAULT)
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10, now_ms=_BOOT_MS)
    p.due_publish(now_ms=_BOOT_MS)
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10,
                       now_ms=_BOOT_MS + 5_000)
    assert p.due_publish(now_ms=_BOOT_MS + 5_000) is not None


def test_publish_resets_window_so_coverage_start_advances():
    p = SlamLocalPublisher(robot_id="r1", mode=MODE_DEFAULT)
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10, now_ms=_BOOT_MS)
    p.due_publish(now_ms=_BOOT_MS)
    # Next window — first ingest after the publish becomes the new
    # coverage_start_ms. Use a comfortably-past-period offset.
    second_ms = _BOOT_MS + 6_000
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10, now_ms=second_ms)
    msg = p.due_publish(now_ms=second_ms)
    assert msg.coverage_start_ms == second_ms
    assert msg.coverage_end_ms == second_ms


# ─── sequence + stats ──────────────────────────────────────────────────

def test_sequence_increments():
    """SAN v1.3 §9 — narrow cadence 2.5 s."""
    p = SlamLocalPublisher(robot_id="r1", mode=MODE_NARROW)
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10, now_ms=_BOOT_MS)
    a = p.due_publish(now_ms=_BOOT_MS)
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10,
                       now_ms=_BOOT_MS + 2_500)
    b = p.due_publish(now_ms=_BOOT_MS + 2_500)
    assert (a.sequence, b.sequence) == (1, 2)


def test_stats_count_ingests_and_publishes():
    p = SlamLocalPublisher(robot_id="r1")
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10, now_ms=_BOOT_MS)
    p.ingest_local_map(_grid((2, 2)), _origin(), 0.10, now_ms=_BOOT_MS + 500)
    p.due_publish(now_ms=_BOOT_MS + 500)
    assert p.stats["ingests"] == 2
    assert p.stats["publishes"] == 1


# ─── bandwidth budget (SAN v1.3 §9: 6-20 KB/s, ≤ 3% of Wi-Fi 6 mesh) ──
#
# v1.1 budget was 1-3 KB/s (steady) / 5 KB/s (cap) at 30 s cadence.
# v1.3 trades 6× bandwidth for 6× freshness — the Hub UGV dual-SBC
# upgrade absorbs the per-frame compute, and the mesh has ~660 KB/s
# of headroom (3% of theoretical Wi-Fi 6 throughput).
_V13_STEADY_CAP_BYTES_PER_S    = 20_000   # SAN v1.3 §9 upper bound
_V13_BURST_CAP_BYTES_PER_S    = 100_000   # obstacle-mode 1 s burst ceiling

def _realistic_local_grid(size: int = 200) -> np.ndarray:
    """200x200 grid (~20 m x 20 m at 0.1 m/cell). Sparse occupancy
    (~5%) like a real indoor environment with walls + clutter."""
    rng = np.random.default_rng(seed=42)
    g = np.full((size, size), UNKNOWN_CELL, dtype=np.uint8)
    # ~5% occupied, ~30% free, ~65% unknown — matches a realistic
    # mid-mission slam_toolbox snapshot.
    n_cells = size * size
    occupied_idx = rng.choice(n_cells, size=int(n_cells * 0.05), replace=False)
    free_idx     = rng.choice(n_cells, size=int(n_cells * 0.30), replace=False)
    flat = g.ravel()
    flat[occupied_idx] = 255
    flat[free_idx]     = 0
    return flat.reshape(size, size)


def test_bandwidth_under_20kbps_per_swarm_at_default_5s():
    """7 followers × 5 s cadence × encoded msg size → must stay below
    the SAN v1.3 §9 steady-state ceiling of 20 KB/s.

    The Wi-Fi 6 mesh has roughly 660 KB/s of practical headroom; the
    spec budgets 3% of that for SLAM aggregation, so 20 KB/s is the
    upper bound. PNG compression on the realistic 200×200 grid
    (sparse occupancy) keeps payloads small enough to satisfy this
    even at 6× the v1.1 cadence.

    Followers = 7 because v1.3 swarm has 8 robots and the leader (S1)
    does not contribute a local SLAM delta. The Hub (S2) DOES — it
    runs slam_toolbox too — so the actual contributor count is 7.
    """
    p = SlamLocalPublisher(robot_id="r1", mode=MODE_DEFAULT)
    p.ingest_local_map(
        _realistic_local_grid(), _origin(), 0.10, now_ms=_BOOT_MS)
    msg = p.due_publish(now_ms=_BOOT_MS)
    encoded_bytes = len(msg.occupancy_grid_delta_png)
    n_followers = 7
    period_sec = PERIOD_BY_MODE[MODE_DEFAULT]
    bytes_per_sec = n_followers * encoded_bytes / period_sec
    assert bytes_per_sec < _V13_STEADY_CAP_BYTES_PER_S, (
        f"SLAM bandwidth {bytes_per_sec:.0f} B/s exceeds the v1.3 "
        f"steady-state cap {_V13_STEADY_CAP_BYTES_PER_S} B/s "
        f"(payload {encoded_bytes} B × {n_followers} followers / "
        f"{period_sec}s period)")


def test_bandwidth_obstacle_burst_stays_under_mesh_saturation():
    """Obstacle mode (v1.3: 1 s period) is a documented temporary
    burst — "일시 단축". It's allowed to exceed the steady-state
    20 KB/s budget but must stay under the mesh-saturation ceiling
    so other traffic (control, video, audit) doesn't suffer.

    100 KB/s is 15% of practical Wi-Fi 6 mesh throughput — comfortably
    below saturation while leaving the planner room to react when a
    cluster of obstacles triggers the burst on multiple followers
    simultaneously.
    """
    p = SlamLocalPublisher(robot_id="r1", mode=MODE_OBSTACLE)
    p.ingest_local_map(
        _realistic_local_grid(), _origin(), 0.10, now_ms=_BOOT_MS)
    msg = p.due_publish(now_ms=_BOOT_MS)
    encoded_bytes = len(msg.occupancy_grid_delta_png)
    bytes_per_sec = 7 * encoded_bytes / PERIOD_BY_MODE[MODE_OBSTACLE]
    assert bytes_per_sec < _V13_BURST_CAP_BYTES_PER_S, (
        f"SLAM burst bandwidth at obstacle/1s = {bytes_per_sec:.0f} B/s "
        f"exceeds {_V13_BURST_CAP_BYTES_PER_S} B/s mesh-saturation "
        f"ceiling (payload {encoded_bytes} B per follower)")


def test_bandwidth_v13_default_cheaper_than_v10_firehose():
    """Smoke check vs the v1.0 1 Hz path: at v1.3's 5 s cadence, we
    use roughly 1/5 the bandwidth of the v1.0 firehose (was 1/30 in
    v1.1 — v1.3 trades some of that savings back for freshness).

    Prevents accidental reversion to a true 1 Hz local-delta cadence
    slipping past review, which would blow the mesh budget.
    """
    p = SlamLocalPublisher(robot_id="r1", mode=MODE_DEFAULT)
    p.ingest_local_map(
        _realistic_local_grid(), _origin(), 0.10, now_ms=_BOOT_MS)
    msg = p.due_publish(now_ms=_BOOT_MS)
    payload = len(msg.occupancy_grid_delta_png)
    n_followers = 7
    v1_0_rate = n_followers * payload * 1.0       # 1 Hz × N followers
    v1_3_rate = n_followers * payload / PERIOD_BY_MODE[MODE_DEFAULT]
    # v1.3 must still be at least 4× cheaper than the v1.0 firehose.
    assert v1_0_rate / max(v1_3_rate, 1) >= 4.0


# ─── validate output passes round-trip ─────────────────────────────────

def test_published_message_validates():
    p = SlamLocalPublisher(robot_id="follower7")
    p.ingest_local_map(_grid((8, 8), occupied_cells=[(0, 0)]),
                       _origin(x=10.0, y=-5.0), 0.10, now_ms=_BOOT_MS)
    msg = p.due_publish(now_ms=_BOOT_MS)
    msg.validate()
    assert msg.robot_id == "follower7"
    assert msg.origin.x == 10.0
    assert msg.origin.y == -5.0


def test_published_grid_round_trips_through_png():
    p = SlamLocalPublisher(robot_id="r1")
    src = _grid((6, 6), occupied_cells=[(2, 2), (3, 4), (5, 0)])
    p.ingest_local_map(src, _origin(), 0.10, now_ms=_BOOT_MS)
    msg = p.due_publish(now_ms=_BOOT_MS)
    decoded = decode_png_to_grid(msg.occupancy_grid_delta_png)
    assert decoded.shape == src.shape
    assert np.array_equal(decoded, src)
